// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_vmfunc.c
 *
 * `bgpgrep`-specific BGP VM extended functions
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "bgp/vmintrin.h"
#include "sys/endian.h"

#include <assert.h>
#include <string.h>

typedef struct Asnnode Asnnode;
struct Asnnode {
	Asn32    asn;
	Sint32   pos;
	Asnnode *children[2];
};

typedef struct Asntree Asntree;
struct Asntree {
	Uint32   n;
	Asnnode *root;
};

// NOTE: Need this macro so we can safely Bgp_VmTempFree() a bunch of
//       nodes with a single call, instead of using a loop
#define ALIGNEDNODESIZ ALIGN(sizeof(Asnnode), ALIGNMENT)

Asnnode *GetAsnTreeNode(Bgpvm *vm, Asntree *t, Asn32 asn)
{
	Asnnode **p, *i;

	// Search suitable spot inside tree
	p = &t->root;
	while ((i = *p) != NULL && asn != i->asn)
		p = &i->children[asn > i->asn];

	if (!i) {
		// No node with this ASN
		i = (Asnnode *) Bgp_VmTempAlloc(vm, ALIGNEDNODESIZ);
		if (!i)
			return NULL;

		i->asn = asn;
		i->pos = -1;
		i->children[0] = i->children[1] = NULL;

		*p = i;

		t->n++;
	}
	return i;
}

void BgpgrepF_PeerAddrMatch(Bgpvm *vm)
{
	/*
	 * POPS:
	 * -1 - Peerlistop *
	 *
	 * PUSHES:
	 * TRUE if S.peerAddr and S.peerAs match peer list, FALSE otherwise
	 */
	if (!BGP_VMCHKSTKSIZ(vm, 1))
		return;

	Boolean foundPeer = FALSE;  // unless found otherwise
	for (Peerlistop *i = (Peerlistop *) BGP_VMPOPA(vm); i; i = i->next) {
		switch (i->opc) {
		default:
		case PEER_ADDR_NOP:
			break;
		case PEER_ADDR_EQ:
			if (!Ip_Equal(&i->addr, &S.peerAddr)) continue;

			break;
		case PEER_ADDR_NE:
			if (Ip_Equal(&i->addr, &S.peerAddr)) continue;

			break;
		}
		if (i->asn == ASN_ANY)
			foundPeer = TRUE;
		else if (ISASNNOT(i->asn))
			foundPeer = ASN(i->asn) != ASN(S.peerAs);
		else
			foundPeer = ASN(i->asn) == ASN(S.peerAs);

		if (foundPeer)
			break;
	}

	// XXX: include match info

	BGP_VMPUSH(vm, foundPeer);
}

FORCE_INLINE int TIMESTAMP_CMP(const Timestampop *stamp)
{
	int res = (S.timestampSecs > stamp->secs) - (S.timestampSecs < stamp->secs);
	if (res == 0)
		res = (S.timestampMicrosecs > stamp->microsecs) -
		      (S.timestampMicrosecs < stamp->microsecs);

	return res;
}

void BgpgrepF_TimestampCompare(Bgpvm *vm)
{
	/*
	 * POPS:
	 * -1 - Timestampop *
	 *
	 * PUSHES:
	 * TRUE if S.timestampSecs and S.timestampMicrosecs matches argument,
	 * FALSE otherwise
	 */
	if (!BGP_VMCHKSTKSIZ(vm, 1))
		return;

	Boolean res = FALSE;

	Timestampop *stamp = (Timestampop *) BGP_VMPOPA(vm);
	if (!stamp)  // should never happen
		goto nomatch;

	switch (stamp->opc) {
	case TIMESTAMP_LE:
		res = (TIMESTAMP_CMP(stamp) <= 0);
		break;
	case TIMESTAMP_LT:
		res = (TIMESTAMP_CMP(stamp) <  0);
		break;
	case TIMESTAMP_NE:
		res = (TIMESTAMP_CMP(stamp) != 0);
		break;
	case TIMESTAMP_EQ:
		res = (TIMESTAMP_CMP(stamp) == 0);
		break;
	case TIMESTAMP_GT:
		res = (TIMESTAMP_CMP(stamp) >  0);
		break;
	case TIMESTAMP_GE:
		res = (TIMESTAMP_CMP(stamp) >= 0);
		break;
	default:
		UNREACHABLE;
		return;
	}

nomatch:
	// XXX: include match info

	BGP_VMPUSH(vm, res);
}

void BgpgrepF_FindAsLoops(Bgpvm *vm)
{
	/* POPS:
	 * NONE
	 *
	 * PUSHES:
	 * TRUE if loops are found inside AS PATH, FALSE otherwise
	 */

	Aspathiter it;

	Asntree t;
	Asn     asn;

	Sint32  pos = 0;
	Boolean foundLoop = FALSE;

	const Bgphdr *hdr = BGP_HDR(vm->msg);
	if (hdr->type != BGP_UPDATE)
		goto nomatch;

	if (Bgp_StartMsgRealAsPath(&it, vm->msg) != OK)
		goto nomatch;

	memset(&t, 0, sizeof(t));
	while ((asn = Bgp_NextAsPath(&it)) != -1) {
		Asn32 as32 = ASN(asn);
		if (as32 == AS4_TRANS) {
			// AS_TRANS are irrelevant for loops (they are, in fact, bogons)
			pos++;
			continue;
		}

		Asnnode *n = GetAsnTreeNode(vm, &t, as32);
		if (!n)
			return;  // out of temporary memory

		if (n->pos != -1 && n->pos < pos - 1) {
			foundLoop = TRUE;
			break;
		}

		n->pos = pos++;  // record last position of this ASN and move on
	}
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

	Bgp_VmTempFree(vm, t.n * ALIGNEDNODESIZ);

nomatch:
	// XXX: include match info
	if (BGP_VMCHKSTK(vm, 1))
		BGP_VMPUSH(vm, foundLoop);
}

void BgpgrepF_BogonAsn(Bgpvm *vm)
{
	/* POPS:
	 * NONE
	 *
	 * PUSHES:
	 * TRUE if bogons are found inside AS PATH, FALSE otherwise
	 */

	Aspathiter it;
	Asn asn;

	Boolean foundBogon = FALSE;

	const Bgphdr *hdr = BGP_HDR(vm->msg);
	if (hdr->type != BGP_UPDATE)
		goto nomatch;

	if (Bgp_StartMsgRealAsPath(&it, vm->msg) != OK)
		goto nomatch;

	while ((asn = Bgp_NextAsPath(&it)) != -1) {
		Uint32 n = beswap32(ASN(asn));

		// https://ripe72.ripe.net/wp-content/uploads/presentations/151-RIPE72_bogon_ASNs_JobSnijders.pdf
		// https://www.manrs.org/2021/01/routing-security-terms-bogons-vogons-and-martians/
		// Breakdown:
		// 0                      Reserved can't use in BGP RFC 7607
		// 23456                  AS_TRANS RFC 6793
		// 64496-64511            Reserved for use in docs and code RFC 5398
		// 64512-65534            Reserved for Private Use RFC 6996
		// 65535                  Reserved RFC 7300
		// 65536-65551            Reserved for use in docs and code RFC 5398
		// 65552-131071           Reserved by IANA
		// 4200000000-4294967294  Reserved for Private Use RFC 6996
		// 4294967295             Reserved RFC 7300
		foundBogon |= (n == 0u || n == 23456u);
		foundBogon |= (n >= 64496u && n <= 131071u);
		foundBogon |= (n >= 4200000000u && n <= 4294967295u);
		if (foundBogon)
			break;
	}
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

nomatch:
	// XXX: include match info
	if (BGP_VMCHKSTK(vm, 1))
		BGP_VMPUSH(vm, foundBogon);
}
