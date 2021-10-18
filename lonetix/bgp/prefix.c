// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/prefix.c
 *
 * Deal with network prefixes.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"
#include "sys/endian.h"
#include "sys/ip.h"
#include "numlib.h"
#include "smallbytecopy.h"

#include <assert.h>
#include <string.h>

// ===========================================================================
// Some performance oriented macros to avoid branching during prefix iteration

/// Calculate the minimum size of a possibly ADD_PATH enabled prefix.
#define MINPFXSIZ(isAddPath) \
	((((isAddPath) != 0) << 2) + 1)

/// Extract the prefix portion out of a possibly ADD_PATH enabled prefix pointer.
#define RAWPFXPTR(base, isAddPath) \
	((RawPrefix *) ((Uint8 *) (base) + (((isAddPath) != 0) << 2)))

/// Calculate maximum prefix width in bits given an address family
#define MAXPFXWIDTH(family) (((family) == AFI_IP6) ? IPV6_WIDTH : IPV4_WIDTH)  // simple CMOV

// ===========================================================================

char *Bgp_PrefixToString(Afi afi, const RawPrefix *prefix, char *dest)
{
	Ipv4adr adr;
	Ipv6adr adr6;

	switch (afi) {
	case AFI_IP:
		memset(&adr, 0, sizeof(adr));
		_smallbytecopy4(adr.bytes, prefix->bytes, PFXLEN(prefix->width));
		dest = Ipv4_AdrToString(&adr, dest);
		break;

	case AFI_IP6:
		memset(&adr6, 0, sizeof(adr6));
		_smallbytecopy16(adr6.bytes, prefix->bytes, PFXLEN(prefix->width));
		dest = Ipv6_AdrToString(&adr6, dest);
		break;

	default:
		return NULL;  // invalid argument
	}

	*dest++ = '/';
	dest    = Utoa(prefix->width, dest);
	return dest;
}

char *Bgp_ApPrefixToString(Afi afi, const ApRawPrefix *prefix, char *dest)
{
	// NOTE: Test early to avoid polluting `dest` in case of invalid argument;
	//       hopefully compilers will flatten this function to
	//       eliminate duplicate test inside switch
	if (afi != AFI_IP && afi != AFI_IP6)
		return NULL;  // invalid argument

	dest    = Utoa(beswap32(prefix->pathId), dest);
	*dest++ = ' ';
	return Bgp_PrefixToString(afi, PLAINPFX(prefix), dest);
}

Judgement Bgp_StringToPrefix(const char *s, Prefix *dest)
{
	Ipadr      adr;
	unsigned   width;
	NumConvRet res;

	const char *ptr = s;
	while (*ptr != '/' && *ptr != '\0') ptr++;

	size_t len = ptr - s;
	char  *buf = (char *) alloca(len + 1);

	memcpy(buf, s, len);
	buf[len] = '\0';

	if (Ip_StringToAdr(buf, &adr) != OK)
		return NG;  // Bad IP string

	if (*ptr == '/') {
		ptr++;  // skip '/' separator

		char *eptr;
		width = Atou(ptr, &eptr, 10, &res);

		if (res != NCVENOERR || *eptr != '\0')
			return NG;
	} else
		width = (adr.family == IP6) ? IPV6_WIDTH : IPV4_WIDTH;  // implicit full prefix

	switch (adr.family) {
	case IP4:
		if (width > IPV4_WIDTH) return NG;  // illegal prefix length

		dest->afi = AFI_IP;
		break;

	case IP6:
		if (width > IPV6_WIDTH) return NG;  // illegal prefix length

		dest->afi = AFI_IP6;
		break;

	default:
		UNREACHABLE;
		return NG;
	}

	dest->isAddPath = FALSE;
	dest->width     = width;
	_smallbytecopy16(dest->bytes, adr.bytes, PFXLEN(width));
	return OK;
}

Judgement Bgp_StartPrefixes(Prefixiter *it,
                            Afi         afi,
                            Safi        safi,
                            const void *data,
                            size_t      nbytes,
                            Boolean     isAddPath)
{
	if (afi != AFI_IP && afi != AFI_IP6)
		return Bgp_SetErrStat(BGPEAFIUNSUP);
	if (safi != SAFI_UNICAST && safi != SAFI_MULTICAST)
		return Bgp_SetErrStat(BGPESAFIUNSUP);

	it->afi       = afi;
	it->safi      = safi;
	it->isAddPath = isAddPath;
	it->base      = (Uint8 *) data;
	it->lim       = it->base + nbytes;
	it->ptr       = it->base;

	return Bgp_SetErrStat(BGPENOERR);
}

void *Bgp_NextPrefix(Prefixiter *it)
{
	if (it->ptr >= it->lim) {
		Bgp_SetErrStat(BGPENOERR);
		return NULL;  // end of iteration
	}

	// Basic check for prefix initial bytes
	size_t left = it->lim - it->ptr;
	size_t siz  = MINPFXSIZ(it->isAddPath);
	if (left < siz) {
		Bgp_SetErrStat(BGPETRUNCPFX);
		return NULL;
	}

	// Adjust a pointer to skip Path identifier info if necessary
	const RawPrefix *rawPfx = RAWPFXPTR(it->ptr, it->isAddPath);
	if (rawPfx->width > MAXPFXWIDTH(it->afi)) {
		Bgp_SetErrStat(BGPEBADPFXWIDTH);
		return NULL;
	}

	// Ensure all the necessary prefix bytes are present
	siz += PFXLEN(rawPfx->width);
	if (left < siz) {
		Bgp_SetErrStat(BGPETRUNCPFX);
		return NULL;
	}

	// All good, advance and return
	void *pfx = it->ptr;
	it->ptr  += siz;
	Bgp_SetErrStat(BGPENOERR);

	return pfx;
}
