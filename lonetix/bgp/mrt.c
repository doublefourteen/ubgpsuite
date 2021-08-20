// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/mrt.c
 *
 * Deals with Multi-Threaded Routing Toolkit (MRT) format.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"
#include "sys/endian.h"
#include "sys/interlocked.h"

#include <assert.h>
#include <string.h>

static void *MRT_DATAPTR(const Mrtrecord *rec, size_t *len)
{
	const Mrthdr *hdr = MRT_HDR(rec);
	Uint32        off = MRT_ISEXHDRTYPE(hdr->type) << 2;

	*len = beswap32(hdr->len) - off;
	return rec->buf + MRT_HDRSIZ + off;
}

Judgement Bgp_MrtFromBuf(Mrtrecord *rec, const void *buf, size_t nbytes)
{
	if (nbytes < MRT_HDRSIZ)
		return Bgp_SetErrStat(BGPETRUNCMRT);

	const Mrthdr *hdr = (const Mrthdr *) buf;

	size_t left = beswap32(hdr->len);            // NOTE: Includes extended timestamp size, if any!
	if (left < 4 && MRT_ISEXHDRTYPE(hdr->type))  // ...so check it
		return Bgp_SetErrStat(BGPETRUNCMRT);

	size_t siz = sizeof(*hdr) + left;
	if (siz > nbytes)
		return Bgp_SetErrStat(BGPETRUNCMRT);

	const MemOps *memOps = MRT_MEMOPS(rec);

	rec->buf = (Uint8 *) memOps->Alloc(rec->allocp, siz, NULL);
	if (!rec->buf)
		return Bgp_SetErrStat(BGPENOMEM);

	rec->peerOffTab = NULL;
	memcpy(rec->buf, buf, siz);
	return Bgp_SetErrStat(BGPENOERR);
}

Judgement Bgp_ReadMrt(Mrtrecord *rec, void *streamp, const StmOps *ops)
{
	Mrthdr hdr;

	// Read header
	Sint64 n = ops->Read(streamp, &hdr, sizeof(hdr));
	if (n == 0) {
		// Precisely at end of file, no error, just no more records
		Bgp_SetErrStat(BGPENOERR);
		return NG;
	}
	if (n < 0)
		return Bgp_SetErrStat(BGPEIO);
	if ((size_t) n != sizeof(hdr))
		return Bgp_SetErrStat(BGPEIO);

	size_t left = beswap32(hdr.len);            // NOTE: Includes extended timestamp size, if any!
	if (left < 4 && MRT_ISEXHDRTYPE(hdr.type))  // ...so check it
		return Bgp_SetErrStat(BGPETRUNCMRT);

	// Allocate buffer
	// NOTE: ...but MRT header length doesn't account for header size itself
	size_t siz           = sizeof(hdr) + left;
	const MemOps *memOps = MRT_MEMOPS(rec);

	rec->buf = (Uint8 *) memOps->Alloc(rec->allocp, siz, NULL);
	if (!rec->buf)
		return Bgp_SetErrStat(BGPENOMEM);

	// Populate buffer
	memcpy(rec->buf, &hdr, sizeof(hdr));
	n = ops->Read(streamp, rec->buf + sizeof(hdr), left);
	if (n < 0) {
		Bgp_SetErrStat(BGPEIO);
		goto fail;
	}
	if ((size_t) n != left) {
		Bgp_SetErrStat(BGPEIO);
		goto fail;
	}

	rec->peerOffTab = NULL;
	return Bgp_SetErrStat(BGPENOERR);

fail:
	memOps->Free(rec->allocp, rec->buf);
	return NG;
}

#define MRT_PEERIDX_MINSIZ (4 + 2 + 2)

Mrtpeeridx *Bgp_GetMrtPeerIndex(Mrtrecord *rec)
{
	const Mrthdr *hdr = MRT_HDR(rec);
	if (hdr->type != MRT_TABLE_DUMPV2 || hdr->subtype != TABLE_DUMPV2_PEER_INDEX_TABLE) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	// Basic size check (only for fixed size portion)
	size_t len = beswap32(hdr->len);
	size_t siz = MRT_PEERIDX_MINSIZ;
	if (len < siz) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	// NOTE: PEER_INDEX_TABLE cannot have extended timestamp
	assert(!MRT_ISEXHDRTYPE(hdr->subtype));
	Mrtpeeridx *peerIdx = (Mrtpeeridx *)  (hdr + 1);

	// View Name field size check
	siz += beswap16(peerIdx->viewNameLen);
	if (len < siz) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return peerIdx;
}

void *Bgp_GetMrtPeerIndexPeers(Mrtrecord *rec, size_t *peersCount, size_t *nbytes)
{
	const Mrthdr *hdr = MRT_HDR(rec);
	if (hdr->type != MRT_TABLE_DUMPV2 || hdr->subtype != TABLE_DUMPV2_PEER_INDEX_TABLE) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	// Basic size check
	size_t len = beswap32(hdr->len);
	size_t off = 4;       // BGP Identifier
	if (len < off + 2) {  // view name length
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	// NOTE: PEER_INDEX_TABLE cannot have extended timestamp
	assert(!MRT_ISEXHDRTYPE(hdr->subtype));
	Mrtpeeridx *peerIdx = (Mrtpeeridx *)  (hdr + 1);

	// View Name size check
	off += 2 + beswap16(peerIdx->viewNameLen);  // skip view name
	if (len < off + 2) {  // entry count
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);

	// Calculate relevant sizes and return peers chunk
	if (peersCount) {
		Uint16 count;

		memcpy(&count, (Uint8 *) peerIdx + off, sizeof(count));
		*peersCount = beswap16(count);
	}

	off += 2;  // skip peers count

	if (nbytes)
		*nbytes = len - off;

	return (Uint8 *) peerIdx + off;
}

static size_t MRT_PEERENTSIZ(const Mrtpeerentv2 *ent)
{
	size_t len = 1 + 4;
	len       += MRT_ISPEERASN32BIT(ent->type) ? 4 : 2;
	len       += MRT_ISPEERIPV6(ent->type) ? IPV6_SIZE : IPV4_SIZE;
	return len;
}

static Mrtpeertabv2 *Bgp_GetPeerOffsetTable(Mrtrecord *rec)
{
	Mrtpeertabv2 *tab;
	while (TRUE) {
		tab = (Mrtpeertabv2 *) Smp_AtomicLoadPtrAcq(&rec->peerOffTab);
		if (tab)
			break;  // already allocated

		// Must allocate the table anew
		size_t peerCount;
		if (!Bgp_GetMrtPeerIndexPeers(rec, &peerCount, /*nbytes=*/NULL))
			return NULL;  // bad record type or corrupted PEER_INDEX_TABLE

		const MemOps *memOps = MRT_MEMOPS(rec);

		tab = (Mrtpeertabv2 *) memOps->Alloc(rec->allocp, offsetof(Mrtpeertabv2, offsets[peerCount]), NULL);
		if (!tab) {
			Bgp_SetErrStat(BGPENOMEM);
			return NULL;
		}

		Smp_AtomicStore16Rx(&tab->validCount, 0);
		Smp_AtomicStore16Rx(&tab->peerCount, peerCount);
		if (Smp_AtomicCasPtrRel(&rec->peerOffTab, NULL, tab))
			break;  // all good

		memOps->Free(rec->allocp, tab);  // ...somebody just allocated the table for us
	}

	return tab;
}

Mrtpeerentv2 *Bgp_GetMrtPeerByIndex(Mrtrecord *rec, Uint16 idx)
{
	// NOTE: no extended timestamp TABLE_DUMPV2 exists, so we can simplify
	//       record access

	Mrtpeertabv2 *tab = Bgp_GetPeerOffsetTable(rec);
	if (!tab)
		return NULL;

	// If we have a `Mrtpeertabv2` we're positively sure that `Mrtpeeridx`
	// is well formed.
	const Mrthdr     *hdr     = MRT_HDR(rec);
	const Mrtpeeridx *peerIdx = (const Mrtpeeridx *) (hdr + 1);

	size_t baseOff = MRT_HDRSIZ + MRT_PEERIDX_MINSIZ + beswap16(peerIdx->viewNameLen);

	// IMPORTANT INVARIANT: `tab->validCount` may change, but will only ever be
	// incremented.

	Uint16 validCount = Smp_AtomicLoad16Acq(&tab->validCount);
	if (idx < validCount) {
		// FAST PATH: Offset was cached
		Uint32 off = Smp_AtomicLoad32Rx(&tab->offsets[idx]);

		Bgp_SetErrStat(BGPENOERR);
		return (Mrtpeerentv2 *) (rec->buf + baseOff + off);
	}

	// SLOW PATH: Must scan PEER_INDEX_TABLE and update offsets

	// Check that a valid peer was actually requested
	Uint16 peerCount = Smp_AtomicLoad16Rx(&tab->peerCount);
	if (idx >= peerCount) {
		Bgp_SetErrStat(BGPEBADPEERIDX);
		return NULL;
	}

	/* NOTE: We cheat a bit:
	 *     - if we have a `peerOffTab`, then we know PEER_INDEX_TABLE header is
	 *       well formed, so we can build a `Mrtpeeriterv2` confidently
	 *       without checking data integrity;
	 *     - we know the data was well formed at least up to the last valid
	 *       peer entry, so we can resume iteration there;
	 */

	Mrtpeeriterv2 it;

	Mrtpeerentv2 *ent;
	Uint32 lastOff;

	// Initialize iterator to last known offset
	if (validCount == 0)
		lastOff = 0;

	else {
		lastOff = Smp_AtomicLoad32Rx(&tab->offsets[validCount - 1]);
		ent     = (Mrtpeerentv2 *) (rec->buf + baseOff + lastOff);

		lastOff += MRT_PEERENTSIZ(ent);
	}

	it.base = rec->buf + baseOff;
	it.lim  = rec->buf + MRT_HDRSIZ + beswap32(hdr->len);
	it.ptr  = it.base + lastOff;

	it.peerCount = peerCount;
	it.nextIdx   = validCount;

	// Keep iterating to find the new entry, update table in the process

	/* NOTE: We don't care if we concurrently write offsets to the table
	 *       while some other thread also updates that, we know we'll be writing
	 *       the same offsets in the same slots there.
	 */

	Uint16 newValidCount = validCount;
	do {
		ent = Bgp_NextMrtPeerv2(&it);
		if (!ent)
			return NULL;  // error status already set by iterator

		Uint32 off = (Uint8 *) ent - it.base;
		Smp_AtomicStore32Rx(&tab->offsets[newValidCount], off);

		newValidCount++;
	} while (idx >= newValidCount);

	// Signal what we've done to the world, don't update anything
	// if somebody else changed the table under our feet.
	Smp_AtomicCas16Rel(&tab->validCount, validCount, newValidCount);

	return ent;  // success status already set by iterator
}

Judgement Bgp_StartMrtPeersv2(Mrtpeeriterv2 *it, Mrtrecord *rec)
{
	size_t peerCount, nbytes;
	void *peers = (Uint8 *) Bgp_GetMrtPeerIndexPeers(rec, &peerCount, &nbytes);
	if (!peers)
		return NG;

	it->base = (Uint8 *) peers;
	it->lim  = it->base + nbytes;
	it->ptr  = it->base;

	it->peerCount = peerCount;
	it->nextIdx   = 0;
	return OK; // success already set
}

Mrtpeerentv2 *Bgp_NextMrtPeerv2(Mrtpeeriterv2 *it)
{
	if (it->ptr >= it->lim) {
		// End of iteration, check for correct peer count
		if (it->nextIdx == it->peerCount)
			Bgp_SetErrStat(BGPENOERR);
		else
			Bgp_SetErrStat(BGPEBADPEERIDXCNT);

		return NULL;
	}

	size_t left = it->lim - it->ptr;
	assert(left > 0);

	Mrtpeerentv2 *ent = (Mrtpeerentv2 *) it->ptr;

	size_t len = MRT_PEERENTSIZ(ent);
	if (left < len) {
		Bgp_SetErrStat(BGPETRUNCPEERV2);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	it->ptr += len;
	it->nextIdx++;
	return ent;
}

Mrtribhdrv2 *Bgp_GetMrtRibHdrv2(Mrtrecord *rec, size_t *nbytes)
{
	const Mrthdr *hdr = MRT_HDR(rec);
	if (hdr->type != MRT_TABLE_DUMPV2) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	Mrtribhdrv2 *rib    = RIBV2_HDR(hdr);
	size_t       len    = beswap32(hdr->len);
	size_t       offset = 0;

	offset += 4;  // sequence number

	size_t maxPfxWidth;
	if (TABLE_DUMPV2_ISGENERICRIB(hdr->subtype)) {
		offset += 2 + 1;  // AFI, SAFI
		if (len < offset) {
			Bgp_SetErrStat(BGPETRUNCMRT);
			return NULL;
		}

		switch (rib->gen.afi) {
		case AFI_IP:  maxPfxWidth = IPV4_WIDTH; break;
		case AFI_IP6: maxPfxWidth = IPV6_WIDTH; break;
		default:
			Bgp_SetErrStat(BGPEAFIUNSUP);
			return NULL;
		}
		if (rib->gen.safi != SAFI_UNICAST && rib->gen.safi != SAFI_MULTICAST) {
			Bgp_SetErrStat(BGPESAFIUNSUP);
			return NULL;
		}

	} else if (TABLE_DUMPV2_ISIPV4RIB(hdr->subtype)) {
		maxPfxWidth = IPV4_WIDTH;
	} else if (TABLE_DUMPV2_ISIPV6RIB(hdr->subtype)) {
		maxPfxWidth = IPV6_WIDTH;
	} else {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	const RawPrefix *pfx = (const RawPrefix *) ((Uint8 *) rib + offset);

	offset++;  // prefix width
	if (len < offset) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}
	if (pfx->width > maxPfxWidth) {
		Bgp_SetErrStat(BGPEBADPFXWIDTH);
		return NULL;
	}

	offset += PFXLEN(pfx->width);
	if (len < offset) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	if (nbytes)
		*nbytes = offset;

	return rib;
}

Mrtribentriesv2 *Bgp_GetMrtRibEntriesv2(Mrtrecord *rec, size_t *nbytes)
{
	const Mrthdr *hdr = MRT_HDR(rec);
	if (hdr->type != MRT_TABLE_DUMPV2) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}
	if (!TABLE_DUMPV2_ISRIB(hdr->subtype)) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	size_t len = beswap32(hdr->len);

	size_t offset;
	Mrtribhdrv2 *rib = Bgp_GetMrtRibHdrv2(rec, &offset);
	if (!rib)
		return NULL;  // error already set

	Mrtribentriesv2 *ents = (Mrtribentriesv2 *) ((Uint8 *) rib + offset);

	offset += 2;  // entries count
	if (len < offset) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	if (nbytes)
		*nbytes = len - offset;

	return ents;
}

Judgement Bgp_StartMrtRibEntriesv2(Mrtribiterv2 *it, Mrtrecord *rec)
{
	size_t nbytes;
	Mrtribentriesv2 *ents = Bgp_GetMrtRibEntriesv2(rec, &nbytes);
	if (!ents)
		return NG;

	it->base = ents->entries;
	it->lim  = it->base + nbytes;
	it->ptr  = it->base;

	it->isAddPath  = TABLE_DUMPV2_ISADDPATHRIB(MRT_HDR(rec)->subtype);
	it->entryCount = beswap16(ents->entryCount);
	it->nextIdx    = 0;
	return OK;
}

Mrtribentv2 *Bgp_NextRibEntryv2(Mrtribiterv2 *it)
{
	if (it->ptr >= it->lim) {
		if (it->nextIdx == it->entryCount)
			Bgp_SetErrStat(BGPENOERR);
		else
			Bgp_SetErrStat(BGPEBADRIBV2CNT);

		return NULL;
	}

	size_t left = it->lim - it->ptr;
	assert(left > 0);

	Mrtribentv2 *ent = (Mrtribentv2 *) it->ptr;

	size_t offset = 2 + 4;  // peer index, originated time
	if (it->isAddPath)
		offset += 4;  // path id

	if (left < offset + 2) {  // attributes length
		Bgp_SetErrStat(BGPETRUNCRIBV2);
		return NULL;
	}

	Bgpattrseg *tpa = (Bgpattrseg *) ((Uint8 *) ent + offset);
	offset         += 2 + beswap16(tpa->len);
	if (left < offset) {
		Bgp_SetErrStat(BGPETRUNCRIBV2);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	it->ptr += offset;
	it->nextIdx++;
	return ent;
}

Bgp4mphdr *Bgp_GetBgp4mpHdr(Mrtrecord *rec, size_t *nbytes)
{
	const Mrthdr *hdr = MRT_HDR(rec);
	if (!MRT_ISBGP4MP(hdr->type)) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	size_t len;
	Afi    afi;

	size_t offset = 0;

	Bgp4mphdr *bgp4mp = (Bgp4mphdr *) MRT_DATAPTR(rec, &len);
	if (BGP4MP_ISASN32BIT(hdr->subtype)) {
		offset += 2 * 4;
		offset += 2 + 2;
		if (len < offset) {
			Bgp_SetErrStat(BGPETRUNCMRT);
			return NULL;
		}

		afi = bgp4mp->a32.afi;
	} else {
		offset += 2 * 2;
		offset += 2 + 2;
		if (len < offset) {
			Bgp_SetErrStat(BGPETRUNCMRT);
			return NULL;
		}

		afi = bgp4mp->a16.afi;
	}

	switch (afi) {
	case AFI_IP:  offset += 2 * IPV4_SIZE; break;
	case AFI_IP6: offset += 2 * IPV6_SIZE; break;
	default:
		Bgp_SetErrStat(BGPEAFIUNSUP);
		return NULL;
	}
	if (BGP4MP_ISSTATECHANGE(hdr->subtype))
		offset += 2 * 2;

	if (len < offset) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	if (nbytes)
		*nbytes = offset;

	return bgp4mp;
}

Judgement Bgp_UnwrapBgp4mp(Mrtrecord *rec, Bgpmsg *dest, unsigned flags)
{
	const Mrthdr *hdr = MRT_HDR(rec);
	if (!MRT_ISBGP4MP(hdr->type))
		return Bgp_SetErrStat(BGPEBADMRTTYPE);
	if (!BGP4MP_ISMESSAGE(hdr->subtype))
		return Bgp_SetErrStat(BGPEBADMRTTYPE);

	size_t len;
	Uint8 *base = (Uint8 *) MRT_DATAPTR(rec, &len);

	// Skip header
	size_t siz = BGP4MP_ISASN32BIT(hdr->subtype) ? 2*4 : 2*2;  // skip ASN
	siz += 2;  // skip interface index

	Afi afi;
	if (len < siz + sizeof(afi))
		return Bgp_SetErrStat(BGPETRUNCMRT);

	// Skip AFI and addresses
	memcpy(&afi, base + siz, sizeof(afi));
	siz += sizeof(afi);

	switch (afi) {
	case AFI_IP:
		siz += 2 * sizeof(Ipv4adr);
		break;

	case AFI_IP6:
		siz += 2 * sizeof(Ipv6adr);
		break;

	default:
		return Bgp_SetErrStat(BGPEAFIUNSUP);
	}
	if (len < siz)
		return Bgp_SetErrStat(BGPETRUNCMRT);

	size_t msgsiz = len - siz;
	void  *buf    = base + siz;

	// Mask away ignored flags
	flags &= ~(BGPF_ADDPATH|BGPF_ASN32BIT);

	// ...and automatically reset them as defined by BGP4MP subtype
	if (BGP4MP_ISASN32BIT(hdr->subtype))
		flags |= BGPF_ASN32BIT;
	if (BGP4MP_ISADDPATH(hdr->subtype))
		flags |= BGPF_ADDPATH;

	// Unwrap BGP message
	return Bgp_MsgFromBuf(dest, buf, msgsiz, flags);
}

#define TABLE_DUMP_MINSIZ (2 + 2 /*+ PFX*/ + 1 + 1 + 4 /*+ IP*/ + 2 + 2)

Mrtribent *Bgp_GetMrtRibHdr(Mrtrecord *rec)
{
	Mrthdr *hdr = MRT_HDR(rec);
	if (hdr->type != MRT_TABLE_DUMP) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	size_t len = beswap32(hdr->len);
	size_t siz = TABLE_DUMP_MINSIZ - 2;  // do not include attribute length
	switch (hdr->subtype) {
	case AFI_IP:  siz += 2 * IPV4_SIZE; break;
	case AFI_IP6: siz += 2 * IPV6_SIZE; break;

	default:
		Bgp_SetErrStat(BGPEAFIUNSUP);
		return NULL;
	}

	if (len < siz + 2) {  // include attribute length in size check
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	// NOTE: TABLE_DUMP has no extended timestamp variant
	Mrtribent *ent = (Mrtribent *) (hdr + 1);

	Uint16 attrLen;
	memcpy(&attrLen, (Uint8 *) ent + siz, sizeof(attrLen));
	siz += 2;  // now include offset

	siz += beswap16(attrLen);
	if (len < siz) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return (Mrtribent *) (hdr + 1);
}

Zebrahdr *Bgp_GetZebraHdr(Mrtrecord *rec, size_t *nbytes)
{
	Mrthdr *hdr = MRT_HDR(rec);
	if (hdr->type != MRT_BGP) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NULL;
	}

	size_t len    = beswap32(hdr->len);
	size_t offset = 2 + IPV4_SIZE;
	if (ZEBRA_ISMESSAGE(hdr->subtype))
		offset += 2 + IPV4_SIZE;
	else if (hdr->subtype == ZEBRA_STATE_CHANGE)
		offset += 2 * 2;

	if (len < offset) {
		Bgp_SetErrStat(BGPETRUNCMRT);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	if (nbytes)
		*nbytes = offset;

	// NOTE: Legacy ZEBRA type doesn't have extended timestamp variants
	return (Zebrahdr *) (hdr + 1);
}

#define ZEBRA_MSGSIZ (2uLL + IPV4_SIZE + 2uLL + IPV4_SIZE)

Judgement Bgp_UnwrapZebra(Mrtrecord *rec, Bgpmsg *dest, unsigned flags)
{
	Mrthdr *hdr = MRT_HDR(rec);
	if (hdr->type != MRT_BGP)
		return Bgp_SetErrStat(BGPEBADMRTTYPE);

	// Resolve BGP type from ZEBRA subtype
	BgpType type;
	switch (hdr->subtype) {
	case ZEBRA_UPDATE:    type = BGP_UPDATE;       break;
	case ZEBRA_OPEN:      type = BGP_OPEN;         break;
	case ZEBRA_KEEPALIVE: type = BGP_KEEPALIVE;    break;
	case ZEBRA_NOTIFY:    type = BGP_NOTIFICATION; break;

	default:              return Bgp_SetErrStat(BGPEBADMRTTYPE);
	}

	Zebramsghdr *zebra = (Zebramsghdr *) (hdr + 1);  // NOTE: ZEBRA doesn't have extended timestamp variants
	size_t       len   = beswap32(hdr->len);
	if (len < ZEBRA_MSGSIZ)
		return Bgp_SetErrStat(BGPETRUNCMRT);

	// ZEBRA dumps don't include BGP message header
	size_t zebralen = len - ZEBRA_MSGSIZ;
	size_t msglen   = BGP_HDRSIZ + zebralen;

	// Validate message size
	if (msglen > BGP_EXMSGSIZ || (msglen > BGP_MSGSIZ && !BGP_ISEXMSG(flags)))
		return Bgp_SetErrStat(BGPEOVRSIZ);

	// Allocate new message
	const MemOps *ops = BGP_MEMOPS(dest);
	Bgphdr *msg       = (Bgphdr *) ops->Alloc(dest->allocp, msglen, NULL);
	if (!msg)
		return Bgp_SetErrStat(BGPENOMEM);

	// Build a valid BGP header
	memset(msg->marker, 0xff, sizeof(msg->marker));
	msg->len  = beswap16(msglen);
	msg->type = type;
	memcpy(msg + 1, zebra->msg, zebralen);

	// Populate `dest`
	dest->buf   = (Uint8 *) msg;
	dest->flags = flags & BGPF_EXMSG;  // only acceptable flag
	BGP_CLRATTRTAB(dest->table);
	return Bgp_SetErrStat(BGPENOERR);
}

void Bgp_ClearMrt(Mrtrecord *rec)
{
	const MemOps *memOps = MRT_MEMOPS(rec);
	memOps->Free(rec->allocp, rec->buf);
	memOps->Free(rec->allocp, rec->peerOffTab);

	rec->buf        = NULL;
	rec->peerOffTab = NULL;
}
