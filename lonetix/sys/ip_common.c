// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/ip_common.c
 *
 * Cross-system Internet Protocol functionality implementation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/ip.h"

#include "sys/endian.h"
#include "numlib.h"

#include <string.h>

char *Ipv4_AdrToString(const Ipv4adr *adr, char *dest)
{
	char *ptr = dest;
	ptr = Utoa(adr->bytes[0], ptr); *ptr++ = '.';
	ptr = Utoa(adr->bytes[1], ptr); *ptr++ = '.';
	ptr = Utoa(adr->bytes[2], ptr); *ptr++ = '.';
	ptr = Utoa(adr->bytes[3], ptr);
	return ptr;
}

char *Ipv6_AdrToString(const Ipv6adr *adr, char *dest)
{
	char *ptr = dest;
	if (IS_IPV6_V4MAPPED(adr)) {
		// IPv4 mapped to IPv6 (starts with 0:0:0:0:0:0:ffff)
		memcpy(ptr, "::ffff:", 7);
		ptr += 7;

		ptr = Utoa(adr->bytes[0], ptr); *ptr++ = '.';
		ptr = Utoa(adr->bytes[1], ptr); *ptr++ = '.';
		ptr = Utoa(adr->bytes[2], ptr); *ptr++ = '.';
		ptr = Utoa(adr->bytes[3], ptr);
	} else {
		// Regular IPv6
		ptr = Xtoa(beswap16(adr->words[0]), ptr); *ptr++ = ':';
		ptr = Xtoa(beswap16(adr->words[1]), ptr); *ptr++ = ':';
		ptr = Xtoa(beswap16(adr->words[2]), ptr); *ptr++ = ':';
		ptr = Xtoa(beswap16(adr->words[3]), ptr); *ptr++ = ':';
		ptr = Xtoa(beswap16(adr->words[4]), ptr); *ptr++ = ':';
		ptr = Xtoa(beswap16(adr->words[5]), ptr); *ptr++ = ':';
		ptr = Xtoa(beswap16(adr->words[6]), ptr); *ptr++ = ':';
		ptr = Xtoa(beswap16(adr->words[7]), ptr);

		// Replace longest /(^0|:)[:0]{2,}/ with "::"
		unsigned i, j, n, best, max;
		for (i = best = 0, max = 2; dest[i] != '\0'; i++) {
			if (i > 0 && dest[i] != ':')
				continue;

			// Count the number of consecutive 0 in this substring
			n = 0;
			for (j = i; dest[j] != '\0'; j++) {
				if (dest[j] != ':' && dest[j] != '0')
					break;

				n++;
			}

			// Store the position if this is the best we've seen so far
			if (n > max) {
				best = i;
				max = n;
			}
		}

		ptr = dest + i;
		if (max > 3) {
			// Compress the string
			dest[best] = ':';
			dest[best + 1] = ':';

			unsigned len = i - best - max;
			memmove(&dest[best + 2], &dest[best + max], len + 1);
			ptr = &dest[best + 2 + len];
		}
	}

	return ptr;
}

char *Ip_AdrToString(const Ipadr *addr, char *dest)
{
	switch (addr->family) {
	case IP4: return Ipv4_AdrToString(&addr->v4, dest);
	case IP6: return Ipv6_AdrToString(&addr->v6, dest);
	default:  return NULL;
	}
}

static Boolean Ip_IsStringIpv4(const char *s)
{
	// A well formed IPv4 address contains one dot in chars [1,3]
	// x.
	// xx.
	// xxx.

	for (int i = 1; i < 4; i++) {
		if (s[i-1] == '\0')
			return FALSE;
		if (s[i] == '.')
			return TRUE;
	}
	return FALSE;
}

Judgement Ip_StringToAdr(const char *s, Ipadr *dest)
{
	if (Ip_IsStringIpv4(s)) {
		if (Ipv4_StringToAdr(s, &dest->v4) != OK)
			return NG;

		dest->family = IP4;
	} else {
		if (Ipv6_StringToAdr(s, &dest->v6) != OK)
			return NG;

		dest->family = IP6;
	}

	return OK;
}
