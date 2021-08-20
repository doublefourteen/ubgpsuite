// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_dump.c
 *
 * BGP message dump logic.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "sys/con.h"
#include "sys/endian.h"

#include <assert.h>
#include <string.h>

// NOTE: TABLE_DUMPV2 RIBs may alter S.dropMsgFrame

static void FixBgpAttributeTableForRib(Bgpattrtab tab, Boolean isRibv2)
{
	// HACK ALERT:
	//
	// This is an innocent hack to speed up RIBv2 dumps on BGP messages
	// that were already rebuilt (e.g. non-trivial filtering was necessary).
	//
	// The optimization is based on the fact that we know all offsets
	// we've calculated inside the rebuilt BGP attribute list are valid up to
	// the occurrence of MP_REACH_NLRI or MP_UNREACH_NLRI, whichever comes
	// first. Moreover what we haven't found inside the BGP message attribute
	// list isn't in the RIB either.
	//
	// NOTE: A LOT of RIBs also include the MP_UNREACH attribute,
	// which is unfortunate. We clear the MP_UNREACH_NLRI attribute during
	// filtering (to avoid printing false positives), but it still messes
	// with the offsets of the subsequent attributes.
	//
	// NOTE: There is no chance to get BGP_ATTR_UNKNOWN inside the table,
	// when BGP messages are rebuilt their offset table is filled up entirely.

	// We don't need thread safety over Bgpattrtab, as bgpgrep is single-threaded.

	Sint16 off    = tab[bgp_attrTabIdx[BGP_ATTR_MP_UNREACH_NLRI]];
	Uint16 maxoff = 0xffffu;
	if (off != BGP_ATTR_NOTFOUND)
		maxoff = (Uint16) off;

	if (isRibv2) {
		off = tab[bgp_attrTabIdx[BGP_ATTR_MP_REACH_NLRI]];
		if (off != BGP_ATTR_NOTFOUND && maxoff > (Uint16) off)
			maxoff = (Uint16) off;
	}
	if (maxoff == 0xffffu)  // no MP_REACH or MP_UNREACH, table is perfectly ok
		return;

	// Reset any offset after maxoff
	for (int i = 0; i < BGP_ATTRTAB_LEN; i++) {
		off = tab[i];
		if (off == BGP_ATTR_NOTFOUND)
			continue;
		if ((Uint16) off > maxoff)
			tab[i] = BGP_ATTR_UNKNOWN;
	}
}

static void NormalizeExtendedTimestamp(void)
{
	S.timestampSecs      += S.timestampMicrosecs / 1000000;
	S.timestampMicrosecs %= 1000000;
}

static void OutputBgp4mp(const Mrthdr *hdr, Bgpattrtab tab)
{
	S.lenientBgpErrors = TRUE;
	S.outFmt->DumpBgp4mp(hdr, S.outf, S.outfOps, tab);
	S.lenientBgpErrors = FALSE;
}

void BgpgrepD_Bgp4mp(void)
{
	const Mrthdr    *hdr    = MRT_HDR(&S.rec);
	const Bgp4mphdr *bgp4mp = Bgp_GetBgp4mpHdr(&S.rec, NULL);

	if (BGP4MP_ISSTATECHANGE(hdr->subtype)) {
		OutputBgp4mp(hdr, NULL);
		return;
	}
	if (!BGP4MP_ISMESSAGE(hdr->subtype))
		return;  // don't care for anything else

	// NOTE: Optimizing BGP4MP to avoid message rebuild isn't worth the effort

	// Setup for BGP4MP
	S.peerAs             = BGP4MP_GETPEERADDR(hdr->subtype, &S.peerAddr, bgp4mp);
	S.timestampSecs      = beswap32(hdr->timestamp);
	S.timestampMicrosecs = 0;
	if (hdr->type == MRT_BGP4MP_ET) {
		S.timestampMicrosecs = beswap32(((const Mrthdrex *) hdr)->microsecs);
		NormalizeExtendedTimestamp();
	}

	// Dump MRT data
	Bgp_UnwrapBgp4mp(&S.rec, &S.msg, /*flags=*/BGPF_UNOWNED);
	if (Bgp_VmExec(&S.vm, &S.msg))
		OutputBgp4mp(hdr, S.msg.table);

	Bgp_ClearMsg(&S.msg);
}

static void OutputZebra(const Mrthdr *hdr, Bgpattrtab tab)
{
	S.lenientBgpErrors = TRUE;
	S.outFmt->DumpZebra(hdr, S.outf, S.outfOps, tab);
	S.lenientBgpErrors = FALSE;
}

void BgpgrepD_Zebra(void)
{
	const Mrthdr   *hdr   = MRT_HDR(&S.rec);
	const Zebrahdr *zebra = Bgp_GetZebraHdr(&S.rec, NULL);

	if (hdr->subtype == ZEBRA_STATE_CHANGE) {
		OutputZebra(hdr, NULL);
		return;
	}
	if (!ZEBRA_ISMESSAGE(hdr->subtype))
		return;  // don't care for anything else

	if (S.isTrivialFilter) {
		// FAST PATH - avoid rebuilding original message
		BGP_CLRATTRTAB(S.msg.table);
		OutputZebra(hdr, S.msg.table);
		return;
	}

	// FILTERING PATH - Setup filter
	S.peerAs          = ASN16BIT(zebra->peerAs);
	S.peerAddr.family = IP4;
	S.peerAddr.v4     = zebra->peerAddr;

	S.timestampSecs      = beswap32(hdr->timestamp);
	S.timestampMicrosecs = 0;

	// Filter and dump BGP data
	Bgp_UnwrapZebra(&S.rec, &S.msg, /*flags=*/0);
	if (Bgp_VmExec(&S.vm, &S.msg))
		OutputZebra(hdr, S.msg.table);

	Bgp_ClearMsg(&S.msg);
}

static void OutputRibv2(const Mrthdr       *hdr,
                        const Mrtpeerentv2 *peerent,
                        const Mrtribentv2  *ent,
                        Bgpattrtab          tab)
{
	S.lenientBgpErrors = TRUE;
	S.outFmt->DumpRibv2(hdr, peerent, ent, S.outf, S.outfOps, tab);
	S.lenientBgpErrors = FALSE;
}

void BgpgrepD_TableDumpv2(void)
{
	const Mrthdr *hdr = MRT_HDR(&S.rec);
	if (hdr->subtype == TABLE_DUMPV2_PEER_INDEX_TABLE) {
		// Store record as PEER_INDEX_TABLE
		Bgp_ClearMrt(&S.peerIndex);

		MRT_MOVEREC(&S.peerIndex, &S.rec);
		S.hasPeerIndex = TRUE;
		return;
	}
	if (!TABLE_DUMPV2_ISRIB(hdr->subtype))
		return;  // don't care for anything but RIBs

	// We may only dump record if we've got a PEER_INDEX_TABLE
	if (!S.hasPeerIndex)
		Bgpgrep_DropRecord("SKIPPING TABLE_DUMPV2 RECORD - No PEER_INDEX_TABLE found yet");

	// Scan every entry inside RIBv2
	const Mrtribentv2 *ent;
	const Mrtribhdrv2 *ribhdr = Bgp_GetMrtRibHdrv2(&S.rec, NULL);

	Mrtribiterv2 ribents;
	Bgp_StartMrtRibEntriesv2(&ribents, &S.rec);
	while ((ent = Bgp_NextRibEntryv2(&ribents)) != NULL) {
		// If we get a corrupted entry, we must still scan what's next
		if (setjmp_fast(S.dropMsgFrame))
			continue;

		// Fetch Peer entry
		Uint16 idx = beswap16(ent->peerIndex);
		const Mrtpeerentv2 *peerent = Bgp_GetMrtPeerByIndex(&S.peerIndex, idx);
		if (S.isTrivialFilter) {
			// FAST PATH - avoid BGP message rebuild
			BGP_CLRATTRTAB(S.msg.table);
			OutputRibv2(hdr, peerent, ent, S.msg.table);
			continue;
		}

		// FILTERING PATH

		Prefix pfx;
		RIBV2_GETNLRI(hdr->subtype, &pfx, ribhdr, ent);

		// Setup filter
		S.peerAs             = MRT_GETPEERADDR(&S.peerAddr, peerent);
		S.timestampSecs      = beswap32(ent->originatedTime);
		S.timestampMicrosecs = 0;

		// Execute filter and dump RIBv2s
		const Bgpattrseg *tpa = RIBV2_GETATTRIBS(hdr->subtype, ent);

		Bgp_RebuildMsgFromRib(&pfx, tpa, &S.msg, /*flags=*/BGPF_RIBV2|BGPF_CLEARUNREACH);
		if (Bgp_VmExec(&S.vm, &S.msg)) {
			FixBgpAttributeTableForRib(S.msg.table, /*isRibv2=*/TRUE);

			OutputRibv2(hdr, peerent, ent, S.msg.table);
		}

		Bgp_ClearMsg(&S.msg);
	}
}

static void OutputRib(const Mrthdr *hdr, const Mrtribent *ent, Bgpattrtab tab)
{
	S.lenientBgpErrors = TRUE;
	S.outFmt->DumpRib(hdr, ent, S.outf, S.outfOps, tab);
	S.lenientBgpErrors = FALSE;
}

void BgpgrepD_TableDump(void)
{
	const Mrthdr    *hdr = MRT_HDR(&S.rec);
	const Mrtribent *ent = Bgp_GetMrtRibHdr(&S.rec);

	if (S.isTrivialFilter) {
		// FAST PATH - No need to rebuild BGP
		BGP_CLRATTRTAB(S.msg.table);
		OutputRib(hdr, ent, S.msg.table);
		return;
	}

	// FILTERING PATH - Setup VM for TABLE_DUMP
	S.peerAs             = RIB_GETPEERADDR(hdr->subtype, &S.peerAddr, ent);
	S.timestampSecs      = beswap32(RIB_GETORIGINATED(hdr->subtype, ent));
	S.timestampMicrosecs = 0;

	// Rebuild message and execute filter
	Prefix pfx;

	pfx.afi       = hdr->subtype;
	pfx.safi      = SAFI_UNICAST;
	pfx.isAddPath = FALSE;
	pfx.pathId    = 0;  // unimportant
	RIB_GETPFX(hdr->subtype, PLAINPFX(&pfx), ent);

	const Bgpattrseg *tpa = RIB_GETATTRIBS(hdr->subtype, ent);

	Bgp_RebuildMsgFromRib(&pfx, tpa, &S.msg, /*flags=*/BGPF_CLEARUNREACH);
	if (Bgp_VmExec(&S.vm, &S.msg)) {
		FixBgpAttributeTableForRib(S.msg.table, /*isRibv2=*/FALSE);

		OutputRib(hdr, ent, S.msg.table);
	}

	Bgp_ClearMsg(&S.msg);
}
