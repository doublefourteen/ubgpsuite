// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/bgp.c
 *
 * BGP message decoding.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"
#include "sys/sys_local.h"
#include "sys/dbg.h"
#include "sys/endian.h"
#include "sys/ip.h"
#include "sys/con.h"
#include "argv.h"
#include "numlib.h"
#include "smallbytecopy.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Uint8 bgp_marker[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

STATIC_ASSERT(sizeof(bgp_marker) == BGP_MARKER_LEN, "Malformed 'bgp_marker'");

static THREAD_LOCAL BgpErrStat bgp_errs;

const char *Bgp_ErrorString(BgpRet code)
{
	switch (code) {
	case BGPEBADVM:            return "Attempting operation on BGP VM under error state";
	case BGPEVMNOPROG:         return "Attempt to execute BGP VM with empty bytecode";
	case BGPEVMBADCOMTCH:      return "Bad COMMUNITY match expression";
	case BGPEVMASMTCHESIZE:    return "BGP VM AS_PATH match expression too complex";
	case BGPEVMASNGRPLIM:      return "BGP VM AS_PATH match expression group limit hit";
	case BGPEVMBADASMTCH:      return "Bad AS_PATH match expression";
	case BGPEVMBADJMP:         return "BGP VM jump instruction target is out of bounds";
	case BGPEVMILL:            return "Illegal instruction in BGP VM bytecode";
	case BGPEVMOOM:            return "BGP VM heap memory exhausted";
	case BGPEVMBADENDBLK:      return "Encountered ENDBLK with no corresponding BLK";
	case BGPEVMUFLOW:          return "BGP VM stack underflow";
	case BGPEVMOFLOW:          return "BGP VM stack overflow";
	case BGPEVMBADFN:          return "CALL instruction index is out of bounds";
	case BGPEVMBADK:           return "LOADK instruction index is out of bounds";
	case BGPEVMMSGERR:         return "Error encountered during BGP message access";
	case BGPEVMBADOP:          return "BGP VM instruction has invalid operand";
	case BGPENOERR:            return "No error";
	case BGPEIO:               return "Input/Output error";
	case BGPEBADTYPE:          return "Provided BGP message has inconsistent type";
	case BGPENOADDPATH:        return "Provided BGP message contains no ADD_PATH information";
	case BGPEBADATTRTYPE:      return "Provided BGP attribute has inconsistent type";
	case BGPEBADMARKER:        return "BGP message marker mismatch";
	case BGPENOMEM:            return "Memory allocation failure";
	case BGPETRUNCMSG:         return "Truncated BGP message";
	case BGPEOVRSIZ:           return "Oversized BGP message";
	case BGPEBADOPENLEN:       return "BGP OPEN message has inconsistent length";
	case BGPEDUPNLRIATTR:      return "Duplicate NLRI attribute detected inside UPDATE message";
	case BGPEBADPFXWIDTH:      return "Bad prefix width";
	case BGPETRUNCPFX:         return "Truncated prefix";
	case BGPETRUNCATTR:        return "Truncated BGP attribute";
	case BGPEBADAGGR:          return "Malformed AGGREGATOR attribute";
	case BGPEBADAGGR4:         return "Malformed AS4_AGGREGATOR attribute";
	case BGPEAFIUNSUP:         return "Unsupported Address Family Identifier";
	case BGPESAFIUNSUP:        return "Unsupported Subsequent Address Family Identifier";
	case BGPEBADMRTTYPE:       return "Provided MRT record has inconsistent type";
	case BGPETRUNCMRT:         return "Truncated MRT record";
	case BGPEBADPEERIDXCNT:    return "TABLE_DUMPV2 Peer Index Table record has incoherent peer entry count";
	case BGPETRUNCPEERV2:      return "Truncated peer entry in TABLE_DUMPV2 Peer Index Table record";
	case BGPEBADRIBV2CNT:      return "TABLE_DUMPV2 RIB record has incoherent RIB entry count";
	case BGPETRUNCRIBV2:       return "Truncated entry in TABLE_DUMPV2 RIB record";
	case BGPEBADRIBV2MPREACH:  return "TABLE_DUMPV2 RIB record contains an illegal MP_REACH attribute";
	case BGPERIBNOMPREACH:     return "IPv6 RIB entry lacks MP_REACH_NLRI attribute";
	case BGPEBADPEERIDX:       return "Peer entry index is out of bounds";
	default:                   return "Unknown BGP error";
	}
}

static NOINLINE NORETURN void Bgp_Quit(BgpRet code, Srcloc *loc, void *obj)
{
	USED(obj);

	loc->call_depth++;  // include Bgp_Quit() itself

	if (com_progName) {
		Sys_Print(STDERR, com_progName);
		Sys_Print(STDERR, ": ");
	}

	Sys_Print(STDERR, "Terminate called in response to an unrecoverable BGP error.\n\t");

	if (loc) {
#ifndef NDEBUG
		if (loc->filename && loc->line > 0) {
			char buf[64];

			Utoa(loc->line, buf);
			Sys_Print(STDERR, "Error occurred in file ");
			Sys_Print(STDERR, loc->filename);
			Sys_Print(STDERR, ":");
			Sys_Print(STDERR, buf);
			Sys_Print(STDERR, "\n\t");
		}
#endif
		if (loc->func) {
			Sys_Print(STDERR, "[");
			Sys_Print(STDERR, loc->func);
			Sys_Print(STDERR, "()]: ");
		}
	}

	Sys_Print(STDERR, Bgp_ErrorString(code));
	Sys_Print(STDERR, ".\n");

	exit(EXIT_FAILURE);
}

Judgement _Bgp_SetErrStat(BgpRet             code,
                          const char        *filename,
                          const char        *func,
                          unsigned long long line,
                          unsigned           depth)
{
	// Don't clobber error code on BGP message access error inside filtering VM
	if (code != BGPEVMMSGERR)
		bgp_errs.code = code;
	if (code == BGPENOERR)
		return OK;  // usual case

	void (*err_func)(BgpRet, Srcloc *, void *);
	Srcloc loc;

	err_func = bgp_errs.func;
	if (err_func) {
		loc.filename   = filename;
		loc.func       = func;
		loc.line       = line;
		loc.call_depth = depth + 1;

		if (err_func == BGP_ERR_QUIT)
			err_func = Bgp_Quit;

		err_func(code, &loc, bgp_errs.obj);
	}

	return NG;
}

void Bgp_SetErrFunc(void (*func)(BgpRet, Srcloc *, void *),
                    void  *arg)
{
	bgp_errs.func = func;
	bgp_errs.obj  = arg;
}

BgpRet Bgp_GetErrStat(BgpErrStat *stat)
{
	if (stat)
		*stat = bgp_errs;

	return bgp_errs.code;
}

Uint16 Bgp_CheckMsgHdr(const void *data,
                       size_t      nbytes,
                       Boolean     allowExtendedSize)
{
	if (nbytes < BGP_HDRSIZ) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return 0;
	}

	const Bgphdr *hdr = (const Bgphdr *) data;
	if (memcmp(hdr->marker, bgp_marker, BGP_MARKER_LEN) != 0) {
		Bgp_SetErrStat(BGPEBADMARKER);
		return 0;
	}

	Uint16 len = beswap16(hdr->len);
	if (len < BGP_HDRSIZ) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return 0;
	}
	if (len > BGP_MSGSIZ && !allowExtendedSize) {
		Bgp_SetErrStat(BGPEOVRSIZ);
		return 0;
	}
	if (len > nbytes) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return 0;
	}

	return len;
}

Judgement Bgp_MsgFromBuf(Bgpmsg     *msg,
                         const void *data,
                         size_t      nbytes,
                         unsigned    flags)
{
	// Immediately initialize flags (mask away superflous ones)
	msg->flags = flags & (BGPF_ADDPATH|BGPF_ASN32BIT|BGPF_EXMSG|BGPF_UNOWNED);

	// Check header data for correctness
	Uint16 len = Bgp_CheckMsgHdr(data, nbytes, BGP_ISEXMSG(msg->flags));
	if (len == 0)
		return NG;  // error already set by Bgp_CheckHdr()

	if (BGP_ISUNOWNED(msg->flags))
		msg->buf = (Uint8 *) data;  // don't copy data over

	else {
		// Copy over relevant data
		const MemOps *memOps = BGP_MEMOPS(msg);

		msg->buf = (Uint8 *) memOps->Alloc(msg->allocp, len, NULL);
		if (!msg->buf)
			return Bgp_SetErrStat(BGPENOMEM);

		memcpy(msg->buf, data, len);
	}

	BGP_CLRATTRTAB(msg->table);
	return Bgp_SetErrStat(BGPENOERR);
}

Judgement Bgp_ReadMsg(Bgpmsg       *msg,
                      void         *streamp,
                      const StmOps *ops,
                      unsigned      flags)
{
	// Immediately initialize flags (mask away superflous ones)
	msg->flags = flags & (BGPF_ADDPATH|BGPF_ASN32BIT|BGPF_EXMSG|BGPF_UNOWNED);

	Uint8 buf[BGP_HDRSIZ];
	Sint64 n = ops->Read(streamp, buf, BGP_HDRSIZ);
	if (n == 0) {
		// precisely at end of stream, no error, just no more BGP messages
		Bgp_SetErrStat(BGPENOERR);
		return NG;
	}
	if (n < 0)
		return Bgp_SetErrStat(BGPEIO);
	if ((size_t) n != BGP_HDRSIZ)
		return Bgp_SetErrStat(BGPEIO);

	// Retrieve memory allocator
	const MemOps *memOps = BGP_MEMOPS(msg);

	// Check header and allocate message
	Uint16 len = Bgp_CheckMsgHdr(buf, BGP_HDRSIZ, BGP_ISEXMSG(msg->flags));
	if (len == 0)
		return NG;

	msg->buf = (Uint8 *) memOps->Alloc(msg->allocp, len, NULL);
	if (!msg->buf)
		return Bgp_SetErrStat(BGPENOMEM);

	// Copy over the message
	memcpy(msg->buf, buf, BGP_HDRSIZ);
	len -= BGP_HDRSIZ;

	n = ops->Read(streamp, msg->buf + BGP_HDRSIZ, len);
	if (n < 0) {
		Bgp_SetErrStat(BGPEIO);
		goto fail;
	}
	if ((size_t) n != len) {
		Bgp_SetErrStat(BGPEIO);
		goto fail;
	}

	BGP_CLRATTRTAB(msg->table);
	return Bgp_SetErrStat(BGPENOERR);

fail:
	memOps->Free(msg->allocp, msg->buf);
	return NG;
}

#define BGP_OPEN_MINSIZ (BGP_HDRSIZ + 1uLL + 2uLL + 2uLL + 4uLL + 1uLL)

Bgpopen *Bgp_GetMsgOpen(Bgpmsg *msg)
{
	Bgphdr *hdr = BGP_HDR(msg);
	if (hdr->type != BGP_OPEN) {
		Bgp_SetErrStat(BGPEBADTYPE);
		return NULL;
	}

	size_t len = beswap16(hdr->len);
	if (len < BGP_OPEN_MINSIZ) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	Bgpopen *open   = (Bgpopen *) hdr;
	size_t   nbytes = BGP_OPEN_MINSIZ + open->parmsLen;
	if (nbytes != len) {
		Bgp_SetErrStat((nbytes > len) ? BGPETRUNCMSG : BGPEBADOPENLEN);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return open;
}

#define BGP_UPDATE_MINSIZ (BGP_HDRSIZ + 2uLL + 2uLL)

Bgpupdate *Bgp_GetMsgUpdate(Bgpmsg *msg)
{
	Bgphdr *hdr = BGP_HDR(msg);
	if (hdr->type != BGP_UPDATE) {
		Bgp_SetErrStat(BGPEBADTYPE);
		return NULL;
	}

	Uint16 len = beswap16(hdr->len);
	if (len < BGP_UPDATE_MINSIZ) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return (Bgpupdate *) hdr;
}

static Boolean Bgp_SwitchMpIterator(Bgpmpiter *it)
{
	if (!it->nextAttr)
		return FALSE;  // no additional attribute to iterate, we're done

	// Switch iterator
	const Bgpmpfam *family = Bgp_GetMpFamily(it->nextAttr);  // sets error
	if (!family)
		return FALSE;  // corrupted attribute

	size_t nbytes;
	void  *routes = Bgp_GetMpRoutes(it->nextAttr, &nbytes);
	if (!routes)
		return FALSE;  // corrupted message

	// Begin subsequent iteration
	Afi afi   = family->afi;
	Safi safi = family->safi;
	if (Bgp_StartPrefixes(&it->rng, afi, safi, routes, nbytes, it->rng.isAddPath) != OK)
		return FALSE;  // shouldn't happen

	// Switch complete, clear attribute and move on
	it->nextAttr = NULL;
	return TRUE;
}

Judgement Bgp_StartMsgWithdrawn(Prefixiter *it, Bgpmsg *msg)
{
	Bgpupdate *update = Bgp_GetMsgUpdate(msg);
	if (!update)
		return NG;

	Bgpwithdrawnseg *withdrawn = Bgp_GetUpdateWithdrawn(update);
	if (!withdrawn)
		return NG;

	return Bgp_StartPrefixes(it, AFI_IP, SAFI_UNICAST,
	                         withdrawn->nlri, beswap16(withdrawn->len),
	                         BGP_ISADDPATH(msg->flags));
}

Judgement Bgp_StartAllMsgWithdrawn(Bgpmpiter *it, Bgpmsg *msg)
{
	it->nextAttr = Bgp_GetMsgAttribute(msg, BGP_ATTR_MP_UNREACH_NLRI);
	if (!it->nextAttr && Bgp_GetErrStat(NULL))
		return NG;

	return Bgp_StartMsgWithdrawn(&it->rng, msg);
}

void Bgp_InitMpWithdrawn(Bgpmpiter             *it,
                         const Bgpwithdrawnseg *withdrawn,
                         const Bgpattr         *mpUnreach,
                         Boolean                isAddPath)
{
	it->nextAttr = (Bgpattr *) mpUnreach;
	Bgp_StartPrefixes(&it->rng,
	                  AFI_IP, SAFI_UNICAST,
	                  withdrawn->nlri, beswap16(withdrawn->len),
	                  isAddPath);
}

void Bgp_InitMpNlri(Bgpmpiter     *it,
                    const void    *nlri,
                    size_t         nbytes,
                    const Bgpattr *mpReach,
                    Boolean        isAddPath)
{
	it->nextAttr = (Bgpattr *) mpReach;
	Bgp_StartPrefixes(&it->rng, AFI_IP, SAFI_UNICAST, nlri, nbytes, isAddPath);
}

Judgement Bgp_StartMsgNlri(Prefixiter *it, Bgpmsg *msg)
{
	Bgpupdate *update = Bgp_GetMsgUpdate(msg);
	if (!update)
		return NG;

	size_t nbytes;
	void *nlri = Bgp_GetUpdateNlri(update, &nbytes);
	if (!nlri)
		return NG;

	return Bgp_StartPrefixes(it, AFI_IP, SAFI_UNICAST,
	                         nlri, nbytes,
	                         BGP_ISADDPATH(msg->flags));
}

Judgement Bgp_StartAllMsgNlri(Bgpmpiter *it, Bgpmsg *msg)
{
	it->nextAttr = Bgp_GetMsgAttribute(msg, BGP_ATTR_MP_REACH_NLRI);
	if (!it->nextAttr && Bgp_GetErrStat(NULL))
		return NG;

	return Bgp_StartMsgNlri(&it->rng, msg);
}

Prefix *Bgp_NextMpPrefix(Bgpmpiter *it)
{
	void *rawPfx;
	do {
		rawPfx = Bgp_NextPrefix(&it->rng);  // sets error
		if (!rawPfx) {
			// Swap iterator if necessary and try again
			if (Bgp_GetErrStat(NULL))
				return NULL;
			if (!Bgp_SwitchMpIterator(it))
				return NULL;
		}
	} while (!rawPfx);

	// Extended prefix info
	Prefix *cur    = &it->pfx;
	cur->afi       = it->rng.afi;
	cur->safi      = it->rng.safi;
	if (it->rng.isAddPath) {
		// ADD-PATH enabled prefix
		const ApRawPrefix *pfx = (const ApRawPrefix *) rawPfx;

		cur->isAddPath = TRUE;
		cur->pathId    = pfx->pathId;
		cur->width     = pfx->width;
		_smallbytecopy16(cur->bytes, pfx->bytes, PFXLEN(pfx->width));
	} else {
		// Regular prefix
		const RawPrefix *pfx = (const RawPrefix *) rawPfx;

		cur->isAddPath = FALSE;
		cur->pathId    = 0;
		cur->width     = pfx->width;
		_smallbytecopy16(cur->bytes, pfx->bytes, PFXLEN(pfx->width));
	}

	return cur;
}

#define BGP_NOTIFICATION_MINSIZ (BGP_HDRSIZ + 1uLL + 1uLL)

Bgpnotification *Bgp_GetNotification(Bgpmsg *msg)
{
	Bgphdr *hdr = BGP_HDR(msg);
	if (hdr->type != BGP_NOTIFICATION) {
		Bgp_SetErrStat(BGPEBADTYPE);
		return NULL;
	}

	Uint16 len = beswap16(hdr->len);
	if (len < BGP_NOTIFICATION_MINSIZ) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return (Bgpnotification *) hdr;
}

Bgpparmseg *Bgp_GetParmsFromMemory(const void *data, size_t size)
{
	const size_t BGP_OPEN_PARMSOFF = BGP_OPEN_MINSIZ - BGP_HDRSIZ;

	if (size < BGP_OPEN_PARMSOFF) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	Bgpparmseg *parms  = (Bgpparmseg *) ((Uint8 *) data + BGP_OPEN_PARMSOFF - 1);
	size_t      nbytes = BGP_OPEN_PARMSOFF + parms->len;
	if (nbytes != size) {
		Bgp_SetErrStat((nbytes > size) ? BGPETRUNCMSG : BGPEBADOPENLEN);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return parms;
}

Bgpparmseg *Bgp_GetOpenParms(const Bgpopen *open)
{
	assert(open->hdr.type == BGP_OPEN);

	size_t len = beswap16(open->hdr.len) - BGP_HDRSIZ;

	return Bgp_GetParmsFromMemory(&open->version, len);
}

Bgpwithdrawnseg *Bgp_GetWithdrawnFromMemory(const void *data, size_t size)
{
	if (size < 2uLL) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	Bgpwithdrawnseg *withdrawn = (Bgpwithdrawnseg *) data;
	size -= 2;

	if (size < beswap16(withdrawn->len) + 2uLL) {  // also accounts for TPA length
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return withdrawn;
}

Bgpwithdrawnseg *Bgp_GetUpdateWithdrawn(const Bgpupdate *msg)
{
	assert(msg->hdr.type == BGP_UPDATE);

	size_t len = beswap16(msg->hdr.len);

	return Bgp_GetWithdrawnFromMemory(msg->data, len - BGP_HDRSIZ);
}

Bgpattrseg *Bgp_GetAttributesFromMemory(const void *data, size_t size)
{
	Bgpwithdrawnseg *withdrawn = Bgp_GetWithdrawnFromMemory(data, size);
	if (!withdrawn)
		return NULL;  // sets error

	size_t withdrawnLen = beswap16(withdrawn->len);

	Bgpattrseg *tpa    = (Bgpattrseg *) &withdrawn->nlri[withdrawnLen];
	size_t      tpaLen = beswap16(tpa->len);
	if (size < 2uLL + withdrawnLen + 2uLL + tpaLen) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	return tpa;  // error already cleared
}

Bgpattrseg *Bgp_GetUpdateAttributes(const Bgpupdate *msg)
{
	assert(msg->hdr.type == BGP_UPDATE);

	size_t len = beswap16(msg->hdr.len);

	return Bgp_GetAttributesFromMemory(msg->data, len - BGP_HDRSIZ);
}

void *Bgp_GetNlriFromMemory(const void *nlri, size_t size, size_t *nbytes)
{
	Bgpattrseg *tpa = Bgp_GetAttributesFromMemory(nlri, size);
	if (!tpa)
		return NULL;  // error already set

	size_t tpaLen = beswap16(tpa->len);
	if (nbytes) {
		size_t offset = &tpa->attrs[tpaLen] - (Uint8 *) nlri;

		*nbytes = size - offset;
	}

	return &tpa->attrs[tpaLen];
}

void *Bgp_GetUpdateNlri(const Bgpupdate *msg, size_t *nbytes)
{
	assert(msg->hdr.type == BGP_UPDATE);

	size_t len = beswap16(msg->hdr.len);

	return Bgp_GetNlriFromMemory(msg->data, len - BGP_HDRSIZ, nbytes);
}

void Bgp_ClearMsg(Bgpmsg *msg)
{
	if (!BGP_ISUNOWNED(msg->flags))
		BGP_MEMOPS(msg)->Free(msg->allocp, msg->buf);

	msg->flags  = 0;
	msg->buf    = NULL;
}
