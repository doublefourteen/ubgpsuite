// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/attribute.c
 *
 * Deals with BGP Path Attributes.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"
#include "bgp/mrt.h"
#include "sys/endian.h"
#include "sys/interlocked.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if 0

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int AttrCodeComp(const void *lhs, const void *rhs)
{
	int a = *(BgpAttrCode *) lhs;
	int b = *(BgpAttrCode *) rhs;

	return a - b;
}

int main(void)
{
	BgpAttrCode attributes[] = {
		BGP_ATTR_ORIGIN,
		BGP_ATTR_NEXT_HOP,
		BGP_ATTR_ATOMIC_AGGREGATE,
		BGP_ATTR_AGGREGATOR,
		BGP_ATTR_AS4_AGGREGATOR,
		BGP_ATTR_AS_PATH,
		BGP_ATTR_AS4_PATH,
		BGP_ATTR_MP_REACH_NLRI,
		BGP_ATTR_MP_UNREACH_NLRI,
		BGP_ATTR_COMMUNITY,
		BGP_ATTR_EXTENDED_COMMUNITY,
		BGP_ATTR_LARGE_COMMUNITY
	};

	assert(BGP_ATTRTAB_LEN == ARRAY_SIZE(attributes));
	assert(ARRAY_SIZE(attributes) < 0x80);  // we store indexes inside Sint8

	qsort(attributes, ARRAY_SIZE(attributes), sizeof(*attributes), AttrCodeComp);

	// Make attribute to lookup index table, assign an
	// incremental index to each notable attribute in ascending order.
	printf("const Sint8 bgp_attrTabIdx[256] = {\n");

	const int LINE_ELS   = 16;
	const int LINE_COUNT = 256 / LINE_ELS;
	assert(256 % LINE_ELS == 0);

	const int LAST_LINE = LINE_COUNT - 1;

	int nextIdx = 0;
	BgpAttrCode code = 0;
	for (int i = 0; i < LINE_COUNT; i++) {
		for (int j = 0; j < LINE_ELS; j++) {
			if (j == 0)
				printf("\t");
			else
				printf(", ");

			if (bsearch(&code, attributes, ARRAY_SIZE(attributes), sizeof(*attributes), AttrCodeComp))
				printf("%2d", nextIdx++);
			else
				printf("%d", -1);

			code++;
		}

		if (i != LAST_LINE)
			printf(",");

		printf("\n");
	}

	printf("};\n");
	return 0;
}

#endif

const Sint8 bgp_attrTabIdx[256] = {
	-1,  0,  1,  2, -1, -1,  3,  4,  5, -1, -1, -1, -1, -1,  6,  7,
	 8,  9, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

#define BGP_MP_REACH_MINLEN   (2 + 1 + 1 + 1)
#define BGP_MP_UNREACH_MINLEN (2 + 1)

#define BGP_ISATTRSET(mask, code) \
	(((mask)[((Uint8) (code)) >> 6] & (1uLL << (((Uint8) (code)) & 0x3f))) != 0)
#define BGP_SETATTR(mask, code) \
	((void) ((mask)[((Uint8) (code)) >> 6] |= (1uLL << (((Uint8) (code)) & 0x3f))))

static Boolean Bgp_IsRibv2RfcCompliant(const Uint8 *data,
                                       size_t       len,
                                       Afi          afi,
                                       Safi         safi)
{
	if (len > 2 + 1 + 1) { // AFI-SAFI and NEXT HOP length
		// Could be non-compliant - we determine that the attribute
		// is NOT compliant if:
		//
		// AFI and SAFI are coherent with `afi` and `safi` and
		// the next byte that follows AFI-SAFI can be interpreted as a
		// NEXT HOP length without overflowing the attribute length
		const Bgpmpfam *fam = (const Bgpmpfam *) data;
		if (fam->afi  == afi &&
		    fam->safi == safi &&
		    2 + 1 + (size_t) data[3] <= len)
			return FALSE;
	}

	return TRUE;
}

static Boolean Bgp_EnsureMsgBuffer(Bgpmsg *msg, size_t *bufsiz, size_t nbytes)
{
	if (*bufsiz < nbytes) {
		if (!BGP_ISEXMSG(msg->flags) || nbytes > BGP_EXMSGSIZ) {
			Bgp_SetErrStat(BGPEOVRSIZ);
			return FALSE;
		}

		const MemOps *ops    = BGP_MEMOPS(msg);
		void         *newBuf = ops->Alloc(msg->allocp, BGP_EXMSGSIZ, msg->buf);
		if (!newBuf) {
			Bgp_SetErrStat(BGPENOMEM);
			return FALSE;
		}

		msg->buf = (Uint8 *) newBuf;
		*bufsiz  = BGP_EXMSGSIZ;
	}

	return TRUE;
}

Judgement Bgp_RebuildMsgFromRib(const Prefix     *nlri,
                                const Bgpattrseg *tpa,
                                Bgpmsg           *msg,
                                unsigned          flags)
{
	/* NOTE: TABLE_DUMP_V2 imposes every AS to be 32 bits wide,
	 *       regardless of the ASN32BIT flag, so any attempt to consistently
	 *       rebuild a message that originally contained 16 bits ASes is
	 *       futile.
	 *       Legacy TABLE_DUMP, on the other hand, mandates 16 bits AS numbers,
	 *       it is incapable of storing AS_PATH with ASN32BIT.
	 *
	 *       It looks like they couldn't find a way to have both.
	 */
	if (flags & BGPF_RIBV2) {
		if (nlri->isAddPath)
			flags |= BGPF_ADDPATH;

		flags |= BGPF_ASN32BIT;  // always the case for TABLE_DUMPV2
	} else {
		flags &= ~(BGPF_ASN32BIT|BGPF_ADDPATH);
	}

	// Prepare pointers for NLRI inclusion (prefix pointer and length)
	const void *nlriPtr = &nlri->width;
	size_t      nlriLen = 1 + PFXLEN(nlri->width);
	if (flags & BGPF_ADDPATH) {
		nlriPtr  = &nlri->pathId;
		nlriLen += 4;
	} else
		flags &= ~BGPF_ADDPATH;

	// Write down message flags
	msg->flags = flags & (BGPF_ADDPATH|BGPF_ASN32BIT|BGPF_EXMSG);

	// Prepare an attribute offset table:
	// We scan the entire attribute set, hence what we don't see isn't there
	for (unsigned i = 0; i < BGP_ATTRTAB_LEN; i++)
		msg->table[i] = BGP_ATTR_NOTFOUND;

	// Allocate initial buffer
	size_t bufsiz = BGP_MSGSIZ;

	msg->buf = (Uint8 *) BGP_MEMOPS(msg)->Alloc(msg->allocp, bufsiz, NULL);
	if (!msg->buf)
		return Bgp_SetErrStat(BGPENOMEM);

	// Begin recreating UPDATE message
	Bgphdr *hdr = BGP_HDR(msg);
	memset(hdr->marker, 0xff, sizeof(hdr->marker));
	// hdr->len is updated after rebuild process
	hdr->type = BGP_UPDATE;

	// No withdrawn, zero-out 2 bytes
	size_t off = BGP_HDRSIZ;

	msg->buf[off++] = 0;
	msg->buf[off++] = 0;

	// Leave out 2 bytes for attributes length (updated after rebuild)
	Uint16 tpaLen;

	size_t tpaLenOff = off;
	off += 2;

	// Rebuild UPDATE message's TPA (we also populate lookup table ourselves)
	Bgpattriter it;
	const Bgpattr *attr;

	Boolean shouldAppendNlri = TRUE;  // unless proven otherwise

	Bgp_StartUpdateAttributes(&it, tpa, NULL);

	tpaLen = 0;
	while ((attr = Bgp_NcNextAttribute(&it)) != NULL) {
		size_t hdrSiz  = BGP_ATTRHDRSIZ(attr);
		size_t attrLen = BGP_ATTRLEN(attr);
		size_t attrSiz = hdrSiz + attrLen;

		int idx = bgp_attrTabIdx[attr->code];

		if (attr->code == BGP_ATTR_MP_REACH_NLRI && (flags & BGPF_RIBV2) != 0) {
			// Rebuild a BGP compliant MP_REACH attribute
			if (attrLen < 1) {
				Bgp_SetErrStat(BGPEBADRIBV2MPREACH);
				goto error;
			}

			// TABLE_DUMPV2 MP_REACH lacks NLRI and removes reserved field,
			// leaving only NEXT HOP length and contents
			// ...by RFC at least, following code takes care of tolerating some
			// common relaxed formats

			const Uint8 *nextHop = (Uint8 *) BGP_ATTRPTR(attr);

			Boolean isRfcCompliant;
			if (flags & BGPF_STRICT_RFC6396)
				isRfcCompliant = TRUE;
			else
				isRfcCompliant = Bgp_IsRibv2RfcCompliant(nextHop, attrLen, nlri->afi, nlri->safi);

			if (!isRfcCompliant) {
				// Non-conforming MP_REACH with extraneous AFI-SAFI:
				// many MRT dumps leave them inside MP_REACH, despite being
				// against RFC 6396.
				// RFC also mandates that NEXT HOP length and NEXT HOP
				// data should be the only contents included in MP_REACH_NLRI,
				// but when the encoding is NOT compliant, sometimes even
				// RESERVED field and NLRI are left there, so we also make
				// attrLen consistent.
				//
				// NOTE: We already made sure that AFI, SAFI and NEXT HOP length
				//       inside the dump are plausible in
				//       Bgp_IsRibv2RfcCompliant().

				nextHop += 2 + 1;
				attrLen  = 1 + *nextHop;
			}

			if ((size_t) *nextHop + 1 != attrLen) {
				Bgp_SetErrStat(BGPEBADRIBV2MPREACH);
				goto error;
			}

			// Calculate attribute size and ensure sufficient buffer size
			size_t mpReachLen     = 2 + 1 + attrLen + 1 + nlriLen;
			Boolean isExtendedLen = (mpReachLen > 0xff);

			attrSiz = 3 + ((size_t) isExtendedLen) + mpReachLen;
			if (!Bgp_EnsureMsgBuffer(msg, &bufsiz, off + attrSiz))
				goto error;

			// Proceed with rebuild
			if (isExtendedLen) {
				msg->buf[off++] = attr->flags | BGP_ATTR_EXTENDED;
				msg->buf[off++] = BGP_ATTR_MP_REACH_NLRI;
				msg->buf[off++] = mpReachLen >> 8;
			} else {
				msg->buf[off++] = attr->flags & ~BGP_ATTR_EXTENDED;
				msg->buf[off++] = BGP_ATTR_MP_REACH_NLRI;
			}

			msg->buf[off++] = mpReachLen & 0xff;

			memcpy(msg->buf + off, &nlri->afi, sizeof(nlri->afi));
			off += sizeof(nlri->afi);

			msg->buf[off++] = nlri->safi;

			memcpy(msg->buf + off, nextHop, attrLen);
			off += attrLen;

			msg->buf[off++] = 0; // reserved field

			memcpy(msg->buf + off, nlriPtr, nlriLen);
			off += nlriLen;

			// We shouldn't add NLRI if we've seen an MP_REACH
			// inside a TABLE_DUMPV2
			shouldAppendNlri = FALSE;

		} else {
			if (attr->code == BGP_ATTR_MP_UNREACH_NLRI) {
				if (flags & BGPF_STRIPUNREACH) {
					// Kill off a spurious MP_UNREACH_NLRI
					assert(idx >= 0);
					msg->table[idx] = BGP_ATTR_NOTFOUND;
					continue;
				}
				if (flags & BGPF_CLEARUNREACH) {
					// Keep the attribute, but clear its contents
					const Bgpmpfam *srcFam = Bgp_GetMpFamily(attr);
					if (!srcFam)
						goto error;  // error already set

					// Rebuild attribute on the fly with original AFI and SAFI
					attrSiz = 3 + 2 + 1;

					Bgpattr *mpUnreach = (Bgpattr *) alloca(attrSiz);
					mpUnreach->flags = attr->flags & ~BGP_ATTR_EXTENDED;
					mpUnreach->code  = BGP_ATTR_MP_UNREACH_NLRI;
					mpUnreach->len   = 3;

					Bgpmpfam *dstFam = (Bgpmpfam *) BGP_ATTRPTR(mpUnreach);
					dstFam->afi      = srcFam->afi;
					dstFam->safi     = srcFam->safi;

					attr = mpUnreach;
				}
			}

			// If we're seeing a MP_REACH_NLRI, then we're
			// dealing with a TABLE_DUMP, AFI should be consistent with
			// our NLRI, otherwise attribute should be evicted
			if (attr->code == BGP_ATTR_MP_REACH_NLRI) {
				const Bgpmpfam *family = Bgp_GetMpFamily(attr);
				if (!family)
					goto error;  // error already set

				if (family->afi != nlri->afi) {
					// Discard spurious AFI from dump
					assert(idx >= 0);
					msg->table[idx] = BGP_ATTR_NOTFOUND;
					continue;
				}

				// Force MP_REACH_NLRI to only contain the requested NLRI
				// We create an attribute on the fly removing every extraneous
				// prefix, leaving only the relevant one

				size_t nextHopLen;
				void *nextHop = Bgp_GetMpNextHop(attr, &nextHopLen);
				if (!nextHop)
					goto error;  // error already set

				size_t mpReachLen     = 2 + 1 + 1 + nextHopLen + 1 + nlriLen;
				Boolean isExtendedLen = (mpReachLen > 0xff);

				attrSiz = 3 + ((size_t) isExtendedLen) + mpReachLen;

				Bgpattr *mpReach = (Bgpattr *) alloca(attrSiz);

				Uint8 *ptr = (Uint8 *) mpReach;
				if (isExtendedLen) {
					*ptr++ = attr->flags | BGP_ATTR_EXTENDED;
					*ptr++ = BGP_ATTR_MP_REACH_NLRI;
					*ptr++ = mpReachLen >> 8;
				} else {
					*ptr++ = attr->flags & ~BGP_ATTR_EXTENDED;
					*ptr++ = BGP_ATTR_MP_REACH_NLRI;
				}

				*ptr++ = mpReachLen & 0xff;

				memcpy(ptr, &nlri->afi, sizeof(nlri->afi));
				ptr += sizeof(nlri->afi);

				*ptr++ = family->safi;  // preserve attribute SAFI, TABLE_DUMP RIB doesn't provide such info

				*ptr++ = nextHopLen;

				memcpy(ptr, nextHop, nextHopLen);
				ptr += nextHopLen;

				*ptr++ = 0;  // reserved

				memcpy(ptr, nlriPtr, nlriLen);

				attr             = mpReach;  // copy from newly created attribute
				shouldAppendNlri = FALSE;    // ...and don't generate NLRI
			}

			if (!Bgp_EnsureMsgBuffer(msg, &bufsiz, off + attrSiz))
				goto error;

			memcpy(msg->buf + off, attr, attrSiz);
			off += attrSiz;
		}

		// Update offset table if necessary and move on
		if (idx >= 0)
			msg->table[idx] = tpaLen;

		tpaLen += attrSiz;
	}
	if (Bgp_GetErrStat(NULL))
		goto error;  // iteration failed

	// Write out attributes length
	tpaLen = beswap16(tpaLen);
	memcpy(msg->buf + tpaLenOff, &tpaLen, sizeof(tpaLen));

	// Include NLRI if MP_REACH_NLRI wasn't seen anywhere
	if (shouldAppendNlri) {
		// MP_REACH is mandatory in case of an AFI_IP6 address
		if (nlri->afi == AFI_IP6) {
			Bgp_SetErrStat(BGPERIBNOMPREACH);
			goto error;
		}

		if (!Bgp_EnsureMsgBuffer(msg, &bufsiz, off + nlriLen))
			goto error;

		memcpy(msg->buf + off, nlriPtr, nlriLen);
		off += nlriLen;
	}

	// Write out UPDATE message length
	hdr->len = beswap16(off);

	// Attempt buffer shrinking
	void *newBuf = BGP_MEMOPS(msg)->Alloc(msg->allocp, off, msg->buf);
	if (newBuf)  // PARANOID
		msg->buf = (Uint8 *) newBuf;

	return OK;  // error status already cleared by the iterator

error:
	Bgp_ClearMsg(msg);
	return NG;  // error status already set
}

Bgpmpfam *Bgp_GetMpFamily(const Bgpattr *attr)
{
	if (attr->code != BGP_ATTR_MP_REACH_NLRI &&
	    attr->code != BGP_ATTR_MP_UNREACH_NLRI) {
		Bgp_SetErrStat(BGPEBADATTRTYPE);
		return NULL;
	}
	if (BGP_ATTRLEN(attr) < 2 + 1) {
		Bgp_SetErrStat(BGPETRUNCATTR);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return (Bgpmpfam *) BGP_ATTRPTR(attr);
}

void *Bgp_GetMpNextHop(const Bgpattr *attr, size_t *nbytes)
{
	if (attr->code != BGP_ATTR_MP_REACH_NLRI) {
		Bgp_SetErrStat(BGPEBADATTRTYPE);
		return NULL;
	}

	size_t off = 2 + 1; // skip MP_REACH
	size_t len = BGP_ATTRLEN(attr);

	Uint8 *data = (Uint8 *) BGP_ATTRPTR(attr);
	if (len < off + 1 || len < off + 1 + data[off]) {
		Bgp_SetErrStat(BGPETRUNCATTR);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	if (nbytes)
		*nbytes = data[off];

	return data + off + 1;
}

Bgpattr *Bgp_GetRealAggregator(const Bgpattrseg *tpa,
                               Boolean          *isAsn32bit,
                               Bgpattrtab        table)
{
	/* RFC 6793
	 * A NEW BGP speaker MUST also be prepared to receive the AS4_AGGREGATOR
	 * attribute along with the AGGREGATOR attribute from an OLD BGP
	 * speaker.  When both of the attributes are received, if the AS number
	 * in the AGGREGATOR attribute is not AS_TRANS, then:
	 *
	 * -  the AS4_AGGREGATOR attribute and the AS4_PATH attribute SHALL be ignored,
	 * -  the AGGREGATOR attribute SHALL be taken as the information
	 *    about the aggregating node, and
	 * -  the AS_PATH attribute SHALL be taken as the AS path
	 *    information.
	 *
	 * Otherwise,
	 * -  the AGGREGATOR attribute SHALL be ignored,
	 * -  the AS4_AGGREGATOR attribute SHALL be taken as the information
	 *    about the aggregating node, and
	 * -  the AS path information would need to be constructed, as in all
	 *    other cases.
	 */

	assert(isAsn32bit != NULL);

	Bgpattr *attr = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_AGGREGATOR, table);
	if (!attr)
		return NULL;  // error, if any, already set

	Boolean asn32bit = *isAsn32bit;
	if (!BGP_CHKAGGRSIZ(attr, asn32bit)) {
		Bgp_SetErrStat(BGPEBADAGGR);
		return NULL;
	}

	Bgpaggr *aggr = (Bgpaggr *) BGP_ATTRPTR(attr);
	if (ISASTRANS(BGP_AGGRAS(aggr, asn32bit))) {
		Bgpattr *attr4 = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_AS4_AGGREGATOR, table);
		if (Bgp_GetErrStat(NULL))
			return NULL;  // forward error

		if (!BGP_CHKAGGRSIZ(attr4, TRUE)) {
			Bgp_SetErrStat(BGPEBADAGGR4);
			return NULL;
		}

		attr     = attr4;
		asn32bit = TRUE;
	}

	*isAsn32bit = asn32bit;
	return attr;  // error already cleared
}

Bgpattr *Bgp_GetRealMsgAggregator(Bgpmsg *msg, Boolean *isAsn32bit)
{
	const Bgpupdate *update = Bgp_GetMsgUpdate(msg);
	if (!update)
		return NULL;  // error already set

	const Bgpattrseg *tpa = Bgp_GetUpdateAttributes(update);
	if (!tpa)
		return NULL;  // error already set

	Boolean  flag = BGP_ISASN32BIT(msg->flags);
	Bgpattr *attr = Bgp_GetRealAggregator(tpa, &flag, msg->table);
	if (!attr)
		return NULL;

	if (isAsn32bit)
		*isAsn32bit = flag;

	return attr;
}

#define BGP_ASSEGSIZ(seg, isAsn32bit) \
	(2 + (((size_t) (seg)->len) << (1 + ((isAsn32bit) != 0))))

static void Bgp_DoStartAsSegments(Assegiter *it, const Bgpattr *attr, Boolean asn32bit)
{
	it->base     = (Uint8 *) BGP_ATTRPTR(attr);
	it->lim      = it->base + BGP_ATTRLEN(attr);
	it->ptr      = it->base;
	it->asn32bit = asn32bit;
}

Judgement Bgp_StartAsSegments(Assegiter *it, const Bgpattr *attr, Boolean asn32bit)
{
	if (attr->code != BGP_ATTR_AS_PATH)
		return Bgp_SetErrStat(BGPEBADATTRTYPE);

	Bgp_DoStartAsSegments(it, attr, asn32bit);
	return Bgp_SetErrStat(BGPENOERR);
}

Judgement Bgp_StartAs4Segments(Assegiter *it, const Bgpattr *attr)
{
	if (attr->code != BGP_ATTR_AS4_PATH)
		return Bgp_SetErrStat(BGPEBADATTRTYPE);

	Bgp_DoStartAsSegments(it, attr, /*asn32bit=*/TRUE);
	return Bgp_SetErrStat(BGPENOERR);
}

Asseg *Bgp_NextAsSegment(Assegiter *it)
{
	if (it->ptr >= it->lim) {
		Bgp_SetErrStat(BGPENOERR);
		return NULL;
	}

	size_t left = it->lim - it->ptr;
	if (left < 2) {
		Bgp_SetErrStat(BGPETRUNCATTR);
		return NULL;
	}

	Asseg *seg = (Asseg *) it->ptr;
	size_t siz = BGP_ASSEGSIZ(seg, it->asn32bit);
	if (left < siz) {
		Bgp_SetErrStat(BGPETRUNCATTR);
		return NULL;
	}

	it->ptr += siz;

	Bgp_SetErrStat(BGPENOERR);
	return seg;
}

Judgement Bgp_StartMsgRealAsPath(Aspathiter *it, Bgpmsg *msg)
{
	const Bgpupdate *update = Bgp_GetMsgUpdate(msg);
	if (!update)
		return NG;  // error already set

	const Bgpattrseg *tpa = Bgp_GetUpdateAttributes(update);
	if (!tpa)
		return NG;  // error already set

	return Bgp_StartRealAsPath(it, tpa, BGP_ISASN32BIT(msg->flags), msg->table);
}

Judgement Bgp_StartRealAsPath(Aspathiter       *it,
                              const Bgpattrseg *tpa,
                              Boolean           isAsn32bit,
                              Bgpattrtab        table)
{
	Bgpattr *asPath = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_AS_PATH, table);
	if (!asPath) {
		memset(it, 0, sizeof(*it));   // set to empty iterator
		it->maxCount = 0xffffu;
		it->asIdx    = 0xffffu;
		return Bgp_GetErrStat(NULL) ? NG : OK;  // propagate error
	}

	assert(asPath->code == BGP_ATTR_AS_PATH);

	// Setup for iteration at AS_PATH
	if (Bgp_StartAsSegments(&it->segs, asPath, isAsn32bit) != OK)
		return NG;  // propagate error

	it->nextAttr = NULL;
	it->maxCount = 0xffffu;
	it->asIdx    = 0xffffu;

	// Peel off first segment
	Asseg *seg = Bgp_NextAsSegment(&it->segs);
	if (seg) {
		// We've got at least one segment, setup base and limit
		it->base = (Uint8 *) seg;
		it->lim  = it->base + BGP_ASSEGSIZ(seg, isAsn32bit);
		it->ptr  = seg->data;
	} else {
		if (Bgp_GetErrStat(NULL))
			return NG;  // propagate error

		// AS_PATH is empty
		it->lim = it->base = it->ptr = NULL;
	}

	// Check whether we have to account for AS4_PATH
	Bgpattr *attr = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_AGGREGATOR, table);
	if (Bgp_GetErrStat(NULL))
		return NG;  // propagate error

	Bgpattr *attr4 = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_AS4_AGGREGATOR, table);
	if (Bgp_GetErrStat(NULL))
		return NG;  // propagate error

	if (!attr || !attr4)
		return OK;  // we're fine as it is

	if (!BGP_CHKAGGRSIZ(attr, isAsn32bit))
		return Bgp_SetErrStat(BGPEBADAGGR);
	if (!BGP_CHKAGGRSIZ(attr4, TRUE))
		return Bgp_SetErrStat(BGPEBADAGGR4);

	Bgpattr *as4Path = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_AS4_PATH, table);
	if (!as4Path)
		return Bgp_GetErrStat(NULL) ? NG : OK;  // propagate error

	assert(as4Path->code == BGP_ATTR_AS4_PATH);

	// Count AS numbers available inside both AS_PATH and AS4_PATH
	unsigned asCount = 0, as4Count = 0;

	Assegiter segs;

	Judgement res = Bgp_StartAsSegments(&segs, asPath, isAsn32bit); USED(res);
	assert(res == OK);  // definitely succeeds (we've done it before)

	while ((seg = Bgp_NextAsSegment(&segs)) != NULL) {
		if (seg->type == AS_SET)
			asCount++;
		else
			asCount += seg->len;
	}
	if (Bgp_GetErrStat(NULL))
		return NG;  // propagate error

	if (Bgp_StartAs4Segments(&segs, as4Path) != OK)
		return NG;  // propagate error

	while ((seg = Bgp_NextAsSegment(&segs)) != NULL) {
		if (seg->type == AS_SET)
			as4Count++;
		else
			as4Count += seg->len;
	}
	if (Bgp_GetErrStat(NULL))
		return NG;  // propagate error

	if (asCount >= as4Count) {
		// Must read some ASes from asPath and then switch to as4Path
		it->nextAttr = as4Path;
		it->maxCount = asCount - as4Count;
	}
	return OK;  // error already cleared
}

Asn Bgp_NextAsPath(Aspathiter *it)
{
	if (it->maxCount == 0) {
		// Force attribute swap (if any)
		it->segs.ptr = it->segs.lim;
		it->ptr = it->lim;
	}

	while (it->ptr >= it->lim) {
		// Need to fetch another segment
		Asseg *seg = Bgp_NextAsSegment(&it->segs);

		if (seg) {
			// New segment acquired, move on
			it->base = (Uint8 *) seg;
			it->lim  = it->base + BGP_ASSEGSIZ(seg, it->segs.asn32bit);
			it->ptr  = seg->data;
		} else {
			if (Bgp_GetErrStat(NULL))
				return -1;  // report error

			if (!it->nextAttr) {
				// End of iteration, all good
				Bgp_SetErrStat(BGPENOERR);
				it->asIdx = 0xffffu;
				return -1;
			}

			// Need to switch iterator
			if (Bgp_StartAs4Segments(&it->segs, it->nextAttr) != OK)
				return -1;  // error already set

			it->base = it->lim = it->ptr = NULL;  // force a new segment fetch
			it->nextAttr = NULL;                  // no more attributes follow
			it->maxCount = 0xffffu;               // no ASN limit
		}
	}

	size_t left = it->lim - it->ptr;
	assert(left > 0);

	Asn32 a32;
	Asn16 a16;
	Asn   asn;

	if (it->segs.asn32bit) {
		if (left < sizeof(a32)) {
			Bgp_SetErrStat(BGPETRUNCATTR);
			return -1;
		}

		memcpy(&a32, it->ptr, sizeof(a32));
		it->ptr += sizeof(a32);

		asn = ASN32BIT(a32);
	} else {
		if (left < sizeof(a16)) {
			Bgp_SetErrStat(BGPETRUNCATTR);
			return -1;
		}

		memcpy(&a16, it->ptr, sizeof(a16));
		it->ptr += sizeof(a16);

		asn = ASN16BIT(a16);
	}

	if (BGP_CURASSEG(it)->type == AS_SEQUENCE || it->ptr == it->lim)
		it->maxCount--;  // decrease max ASN limit, if any

	it->asIdx++;  // increment returned ASN counter

	Bgp_SetErrStat(BGPENOERR);
	return asn;
}

static Boolean Bgp_SwitchNextHop(Nexthopiter *it)
{
	Boolean res = FALSE;  // iteration end, unless found otherwise

	if (it->nextHop) {
		// Setup for read from NEXT_HOP
		if (!BGP_CHKNEXTHOPLEN(it->nextHop)) {
			Bgp_SetErrStat(BGPETRUNCATTR);  // TODO BGPEBADNEXTHOP
			return FALSE;
		}

		it->base = (Uint8 *) BGP_ATTRPTR(it->nextHop);
		it->ptr  = it->base;
		it->lim  = it->ptr + IPV4_SIZE;
		it->afi  = AFI_IP;
		it->safi = SAFI_UNICAST;

		it->nextHop = NULL;  // consume NEXT_HOP
		res = TRUE;

	} else if (it->mpReach) {
		// Setup for read from MP_REACH_NLRI
		Bgpmpnexthop *nh;

		if (it->isRibv2 &&
		    Bgp_IsRibv2RfcCompliant(BGP_ATTRPTR(it->mpReach),
		                            BGP_ATTRLEN(it->mpReach),
		                            it->afiRibv2, it->safiRibv2)) {
			// We're dealing with a truncated MP_REACH_NLRI,
			// it only has next hop data
			it->afi  = it->afiRibv2;
			it->safi = it->safiRibv2;

			nh = (Bgpmpnexthop *) BGP_ATTRPTR(it->mpReach);
		} else {
			// Full MP_REACH_NLRI, or non-compliant TABLE_DUMPV2 RIB MP_REACH_NLRI
			if (!BGP_CHKMPREACH(it->mpReach)) {
				Bgp_SetErrStat(BGPETRUNCATTR);
				return FALSE;
			}

			Bgpmpfam *fam = BGP_MPFAMILY(it->mpReach);
			if (it->isRibv2 && (fam->afi != it->afiRibv2 || it->safi != it->safiRibv2)) {
				// discard MP_REACH_NLRI if it doesn't match with RIB AFI/SAFI
				it->mpReach = NULL;
				goto done;
			}

			nh = BGP_MPNEXTHOP(it->mpReach);
			it->afi  = fam->afi;
			it->safi = fam->safi;
		}

		it->base    = (Uint8 *) nh;
		it->ptr     = nh->data;
		it->lim     = &nh->data[nh->len];

		it->mpReach = NULL;  // consume MP_REACH_NLRI
		res = TRUE;
	}

done:
	Bgp_SetErrStat(BGPENOERR);
	return res;
}

Judgement Bgp_StartAllNextHops(Nexthopiter      *it,
                               const Bgpattrseg *tpa,
                               Bgpattrtab        table)
{
	// Lookup relevant attributes
	it->nextHop = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_NEXT_HOP, table);
	if (Bgp_GetErrStat(NULL))
		return NG;

	it->mpReach = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_MP_REACH_NLRI, table);
	if (Bgp_GetErrStat(NULL))
		return NG;

	// Force segment setup on first call to Bgp_NextNextHop()
	it->base = it->lim = it->ptr = NULL;
	it->afi          = (Afi) 0;   // meaningful only after a Bgp_NextNextHop()
	it->safi         = (Safi) 0;  // ditto
	it->isRibv2      = FALSE;
	it->afiRibv2     = (Afi) 0;
	it->safiRibv2    = (Safi) 0;
	return OK;  // error already cleared by lookup
}

Judgement Bgp_StartAllRibv2NextHops(Nexthopiter      *it,
                                    const Mrthdr     *hdr,
                                    const Bgpattrseg *tpa,
                                    Bgpattrtab table)
{
	if (hdr->type != MRT_TABLE_DUMPV2 || !TABLE_DUMPV2_ISRIB(hdr->subtype)) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return NG;
	}

	const Mrtribhdrv2 *ribhdr = RIBV2_HDR(hdr);
	if (Bgp_StartAllNextHops(it, tpa, table) != OK)
		return NG;

	it->isRibv2 = TRUE;
	if (TABLE_DUMPV2_ISIPV4RIB(hdr->subtype)) {
		it->afiRibv2  = AFI_IP;
		it->safiRibv2 = TABLE_DUMPV2_ISMULTICASTRIB(hdr->subtype) ?
		                SAFI_MULTICAST : SAFI_UNICAST;

	} else if (TABLE_DUMPV2_ISIPV6RIB(hdr->subtype)) {
		it->afiRibv2  = AFI_IP6;
		it->safiRibv2 = TABLE_DUMPV2_ISMULTICASTRIB(hdr->subtype) ?
		                SAFI_MULTICAST : SAFI_UNICAST;

	} else {
		assert(TABLE_DUMPV2_ISGENERICRIB(hdr->subtype));

		it->afiRibv2  = ribhdr->gen.afi;
		it->safiRibv2 = ribhdr->gen.safi;
	}
	return OK;
}

Judgement Bgp_StartAllMsgNextHops(Nexthopiter *it, Bgpmsg *msg)
{
	const Bgpupdate *update = Bgp_GetMsgUpdate(msg);
	if (!update)
		return NG;  // error already set

	const Bgpattrseg *tpa = Bgp_GetUpdateAttributes(update);
	if (!tpa)
		return NG;  // error already set

	return Bgp_StartAllNextHops(it, tpa, msg->table);
}

Ipadr *Bgp_NextNextHop(Nexthopiter *it)
{
	if (it->ptr >= it->lim && !Bgp_SwitchNextHop(it))
		return NULL;  // end of iteration or failure, status set

	size_t left = it->lim - it->ptr;
	assert(left > 0);

	switch (it->afi) {
	case AFI_IP:
		if (left < IPV4_SIZE) {
			Bgp_SetErrStat(BGPETRUNCATTR);
			return NULL;
		}

		it->addr.family = IP4;
		memcpy(it->addr.bytes, it->ptr, IPV4_SIZE);
		it->ptr += IPV4_SIZE;
		break;

	case AFI_IP6:
		if (left < IPV6_SIZE) {
			Bgp_SetErrStat(BGPETRUNCATTR);
			return NULL;
		}

		it->addr.family = IP6;
		memcpy(it->addr.bytes, it->ptr, IPV6_SIZE);
		it->ptr += IPV6_SIZE;
		break;

	default:
		// Should never happen
		Bgp_SetErrStat(BGPEAFIUNSUP);
		return NULL;
	}

	Bgp_SetErrStat(BGPENOERR);
	return &it->addr;
}

#define BGP_COMMATTR(it) ((Bgpattr *) (it)->base)

static void *Bgp_DoNextCommunity(Bgpcommiter *it, BgpAttrCode code, size_t siz)
{
	if (BGP_COMMATTR(it)->code != code) {
		// Bad operation
		Bgp_SetErrStat(BGPEBADATTRTYPE);
		return NULL;
	}
	if (it->ptr >= it->lim) {
		// End of iteration
		Bgp_SetErrStat(BGPENOERR);
		return NULL;
	}
	
	size_t left = it->lim - it->ptr;
	assert(left > 0);
	if (left < siz) {
		Bgp_SetErrStat(BGPETRUNCATTR);
		return NULL;
	}
	
	void *res = it->ptr;
	it->ptr += siz;
	
	Bgp_SetErrStat(BGPENOERR);
	return res;
}

Judgement Bgp_StartCommunity(Bgpcommiter *it, const Bgpattr *attr)
{
	if (attr->code != BGP_ATTR_COMMUNITY &&
	    attr->code != BGP_ATTR_EXTENDED_COMMUNITY &&
	    attr->code != BGP_ATTR_LARGE_COMMUNITY)
		return Bgp_SetErrStat(BGPEBADATTRTYPE);

	it->base = (Uint8 *) attr;
	it->ptr  = (Uint8 *) BGP_ATTRPTR(attr);
	it->lim  = it->ptr + BGP_ATTRLEN(attr);
	return Bgp_SetErrStat(BGPENOERR);
}

Bgpcomm *Bgp_NextCommunity(Bgpcommiter *it)
{
	return (Bgpcomm *) Bgp_DoNextCommunity(it, BGP_ATTR_COMMUNITY, 4);
}

Bgpexcomm *Bgp_NextExtendedCommunity(Bgpcommiter *it)
{
	return (Bgpexcomm *) Bgp_DoNextCommunity(it, BGP_ATTR_EXTENDED_COMMUNITY, 2+6);
}

Bgplgcomm *Bgp_NextLargeCommunity(Bgpcommiter *it)
{
	return (Bgplgcomm *) Bgp_DoNextCommunity(it, BGP_ATTR_LARGE_COMMUNITY, 3*4);
}

void *Bgp_GetMpRoutes(const Bgpattr *attr, size_t *nbytes)
{
	if (attr->code != BGP_ATTR_MP_REACH_NLRI
	    && attr->code != BGP_ATTR_MP_UNREACH_NLRI) {
		Bgp_SetErrStat(BGPEBADATTRTYPE);
		return NULL;
	}

	size_t off = 2 + 1;  // skip AFI-SAFI
	size_t len = BGP_ATTRLEN(attr);

	Uint8 *data = (Uint8 *) BGP_ATTRPTR(attr);
	if (len < off) {
		Bgp_SetErrStat(BGPETRUNCATTR);
		return NULL;
	}
	if (attr->code == BGP_ATTR_MP_REACH_NLRI) {
		// skip NEXT-HOP and RESERVED octet
		if (len == off || len < off + 1 + data[off] + 1) {
			Bgp_SetErrStat(BGPETRUNCATTR);
			return NULL;
		}

		off += 1 + data[off] + 1;
	}

	Bgp_SetErrStat(BGPENOERR);
	if (nbytes)
		*nbytes = len - off;

	return data + off;
}

void Bgp_StartUpdateAttributes(Bgpattriter      *it,
                               const Bgpattrseg *tpa,
                               Bgpattrtab        table)
{
	it->base  = (Uint8 *) tpa->attrs;
	it->lim   = it->base + beswap16(tpa->len);
	it->ptr   = it->base;
	memset(it->attrMask, 0, sizeof(it->attrMask));
	it->table = table;
}

Judgement Bgp_StartMsgAttributes(Bgpattriter *it, Bgpmsg *msg)
{
	const Bgpupdate *update = Bgp_GetMsgUpdate(msg);
	if (!update)
		return NG;  // error already set

	const Bgpattrseg *tpa = Bgp_GetUpdateAttributes(update);
	if (!tpa)
		return NG;  // error already set

	Bgp_StartUpdateAttributes(it, tpa, msg->table);
	return OK;  // error already cleared
}

Bgpattr *Bgp_NcNextAttribute(Bgpattriter *it)
{
	size_t left, hdrSiz, len, siz;
	Bgpattr *attr;

nextAttribute:  // Label only used when skipping duplicates

	if (it->ptr >= it->lim) {
		Bgp_SetErrStat(BGPENOERR);
		return NULL;
	}

	// Ensure size is correct (account for extended attributes)
	left = it->lim - it->ptr;
	attr = (Bgpattr *) it->ptr;
	if (left < 3 || left < (hdrSiz = BGP_ATTRHDRSIZ(attr))) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}
	if (left < hdrSiz + (len = BGP_ATTRLEN(attr))) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	siz = hdrSiz + len;

	// Deal with duplicate attributes
	if (BGP_ISATTRSET(it->attrMask, attr->code)) {
		/* RFC 7606 3.g Revision to BGP UPDATE Message Error Handling
		 *
		 * If the MP_REACH_NLRI attribute or the MP_UNREACH_NLRI [RFC4760]
		 * attribute appears more than once in the UPDATE  message, then a
		 * NOTIFICATION message MUST be sent with the Error Subcode
		 * "Malformed Attribute List".  If any other attribute (whether
		 * recognized or unrecognized) appears more than once in an UPDATE
		 * message, then all the occurrences of the attribute other than the
		 * first one SHALL be discarded and the UPDATE message will continue
		 * to be processed.
		 */
		if (attr->code != BGP_ATTR_MP_REACH_NLRI &&
		    attr->code != BGP_ATTR_MP_UNREACH_NLRI) {
			// Attribute must be ignored
			it->ptr += siz;
			goto nextAttribute;
		}

		Bgp_SetErrStat(BGPEDUPNLRIATTR);
		return NULL;
	}
	BGP_SETATTR(it->attrMask, attr->code);  // seen new attribute type

	// Advance and return
	it->ptr += siz;
	Bgp_SetErrStat(BGPENOERR);
	return attr;
}

Bgpattr *Bgp_NextAttribute(Bgpattriter *it)
{
	Bgpattr *attr = Bgp_NcNextAttribute(it);
	if (!attr)
		return NULL;  // error already set if any

	// Include in attribute table if needed
	int idx = bgp_attrTabIdx[attr->code];
	if (idx >= 0)
		Smp_AtomicStore16Rx(&it->table[idx], (Uint8 *) attr - it->base);

	return attr;
}

Bgpattr *Bgp_GetUpdateAttribute(const Bgpattrseg *tpa,
                                BgpAttrCode       code,
                                Bgpattrtab        table)
{
	Bgpattr *attr;

	// Attempt quick lookup from table
	int idx = bgp_attrTabIdx[code];
	if (idx >= 0) {
		// Attribute is indexed inside table
		Sint16 off = Smp_AtomicLoad16Rx(&table[idx]);
		if (off == BGP_ATTR_NOTFOUND)
			return NULL;  // not available
		if (off != BGP_ATTR_UNKNOWN) {
			// Attribute cached in table
			attr = (Bgpattr *) (tpa->attrs + (Uint16) off);

			assert(attr->code == code);
			return attr;
		}
	}

	// Slow scan for the attribute
	Bgpattriter it;

	Bgp_StartUpdateAttributes(&it, tpa, table);
	while ((attr = Bgp_NextAttribute(&it)) != NULL) {
		if (attr->code == code)
			return attr;  // found
	}
	if (Bgp_GetErrStat(NULL))
		return NULL;  // iteration failed

	// If the iteration was successful we know we've scanned the whole
	// attribute list, so set all unknown attributes in table to not found
	for (unsigned i = 0; i < BGP_ATTRTAB_LEN; i++)
		Smp_AtomicCas16Rx(&table[i], BGP_ATTR_UNKNOWN, BGP_ATTR_NOTFOUND);

	return NULL;
}

Bgpattr *Bgp_GetMsgAttribute(Bgpmsg *msg, BgpAttrCode code)
{
	Bgpupdate *update = Bgp_GetMsgUpdate(msg);
	if (!update)
		return NULL;  // error already set

	Bgpattrseg *tpa = Bgp_GetUpdateAttributes(update);
	if (!tpa)
		return NULL;  // error already set

	return Bgp_GetUpdateAttribute(tpa, code, msg->table);
}
