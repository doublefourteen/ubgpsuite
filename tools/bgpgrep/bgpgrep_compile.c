// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_compile.c
 *
 * Filtering expression predicate compilation to VM bytecode.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "sys/con.h"
#include "sys/endian.h"
#include "sys/fs.h"
#include "sys/sys.h"
#include "lexer.h"
#include "numlib.h"
#include "strlib.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MAXCODE    8192
#define MAXLEAFLEN 8

// Expression types, in order of ascending precedence
typedef enum {
	OP_NONE,  // no operation
	OP_OR,    // -or, -o
	OP_AND,   // -and, -a (implicit when 2 consecutive expressions are found)
	OP_NOT,   // -not, !
	OP_LEAF   // instruction block, leaf node
} Expropc;

typedef Uint16 Expridx;

typedef struct {
	Expropc opc;
	union {
		// Expression node
		struct {
			Expridx l, r;  // left-right subnodes
		} n;
		// Expression leaf
		struct {
			Uint8      nc;
			Bgpvmbytec c[MAXLEAFLEN];
		} leaf;
	};
} Exprop;

typedef struct {
	// Argument list
	int    argc;
	int    argidx;
	char **argv;
	char  *curterm;

	Sint32 loopsFn;
	Sint32 peerMatchFn;
	Sint32 timestampCmpFn;
	Sint32 bogonAsnFn;

	Uint16   ncode;
	Boolean8 wasImplicitAnd;
	Exprop   code[MAXCODE];
} BgpgrepC;

static BgpgrepC C;

static Expridx BgpgrepC_ParseExpression(void);  // Forward decl

static char *BgpgrepC_LastToken(void)
{
	assert(C.argidx > 0);

	return C.argv[C.argidx - 1];
}

static void BgpgrepC_UngetToken(void)
{
	assert(C.argidx > 0 || C.wasImplicitAnd);
	if (C.wasImplicitAnd)
		C.wasImplicitAnd = FALSE;
	else
		C.argidx--;
}

static Boolean IsEndOfParse(void)
{
	return C.argidx == C.argc && !C.wasImplicitAnd;
}

char *BgpgrepC_GetToken(void)
{
	C.wasImplicitAnd = FALSE;
	return (C.argidx < C.argc) ? C.argv[C.argidx++] : NULL;
}

char *BgpgrepC_CurTerm(void)
{
	return C.curterm;
}

char *BgpgrepC_ExpectAnyToken(void)
{
	if (C.argidx >= C.argc)
		Bgpgrep_Fatal("Unexpected match expression end after '%s'", BgpgrepC_LastToken());

	return C.argv[C.argidx++];
}

char *BgpgrepC_ExpectToken(const char *what)
{
	if (C.argidx >= C.argc)
		Bgpgrep_Fatal("Unexpected match expression end after '%s', while expecting '%s'", BgpgrepC_LastToken(), what);

	char *tok = C.argv[C.argidx++];
	if (strcmp(tok, what) != 0)
		Bgpgrep_Fatal("Unexpected token '%s' while expecting '%s'", tok, what);

	return tok;
}

static Expridx PushOp(Expridx l, Expropc opc, Expridx r)
{
	if (C.ncode >= MAXCODE)
		Bgpgrep_Fatal("At '%s': Expression operations limit reached, please simplify the input expression", BgpgrepC_LastToken());

	Expridx idx = C.ncode++;
	Exprop *op  = &C.code[idx];
	op->opc = opc;
	op->n.l = l;
	op->n.r = r;
	return idx;
}

static Expridx PushLeaf(const Bgpvmbytec *c, size_t n)
{
	assert(n <= MAXLEAFLEN);

	if (C.ncode >= MAXCODE)
		Bgpgrep_Fatal("At '%s': Expression operations limit reached, please simplify the input expression", BgpgrepC_LastToken());

	Expridx idx = C.ncode++;
	Exprop *op  = &C.code[idx];

	op->opc = OP_LEAF;

	op->leaf.nc = n;
	memcpy(op->leaf.c, c, n * sizeof(*op->leaf.c));
	return idx;
}

static BgpType ParseBgpType(void)
{
	const char *tok = BgpgrepC_ExpectAnyToken();

	if (Df_stricmp(tok, "OPEN") == 0)
		return BGP_OPEN;
	if (Df_stricmp(tok, "UPDATE") == 0)
		return BGP_UPDATE;
	if (Df_stricmp(tok, "KEEPALIVE") == 0)
		return BGP_KEEPALIVE;
	if (Df_stricmp(tok, "NOTIFICATION") == 0)
		return BGP_NOTIFICATION;
	if (Df_stricmp(tok, "ROUTE_REFRESH") == 0)
		return BGP_ROUTE_REFRESH;
	if (Df_stricmp(tok, "CLOSE") == 0)
		return BGP_CLOSE;

	Bgpgrep_Fatal("-type: Unknown BGP message type: %s", tok);
}

static BgpAttrCode ParseBgpAttr(void)
{
	const char *tok = BgpgrepC_ExpectAnyToken();

	if (Df_stricmp(tok, "ORIGIN") == 0) return BGP_ATTR_ORIGIN;
	if (Df_stricmp(tok, "AS_PATH") == 0) return BGP_ATTR_AS_PATH;
	if (Df_stricmp(tok, "NEXT_HOP") == 0) return BGP_ATTR_NEXT_HOP;
	if (Df_stricmp(tok, "MULTI_EXIT_DISC") == 0) return BGP_ATTR_MULTI_EXIT_DISC;
	if (Df_stricmp(tok, "LOCAL_PREFI") == 0) return BGP_ATTR_LOCAL_PREF;
	if (Df_stricmp(tok, "ATOMIC_AGGREGATE") == 0) return BGP_ATTR_ATOMIC_AGGREGATE;
	if (Df_stricmp(tok, "AGGREGATOR") == 0) return BGP_ATTR_AGGREGATOR;
	if (Df_stricmp(tok, "COMMUNITY") == 0) return BGP_ATTR_COMMUNITY;
	if (Df_stricmp(tok, "ORIGINATOR_ID") == 0) return BGP_ATTR_ORIGINATOR_ID;
	if (Df_stricmp(tok, "CLUSTER_LIST") == 0) return BGP_ATTR_CLUSTER_LIST;
	if (Df_stricmp(tok, "DPA") == 0) return BGP_ATTR_DPA;
	if (Df_stricmp(tok, "ADVERTISER") == 0) return BGP_ATTR_ADVERTISER;
	if (Df_stricmp(tok, "RCID_PATH_CLUSTER_ID") == 0) return BGP_ATTR_RCID_PATH_CLUSTER_ID;
	if (Df_stricmp(tok, "MP_REACH_NLRI") == 0) return BGP_ATTR_MP_REACH_NLRI;
	if (Df_stricmp(tok, "MP_UNREACH_NLRI") == 0) return BGP_ATTR_MP_UNREACH_NLRI;
	if (Df_stricmp(tok, "EXTENDED_COMMUNITY") == 0) return BGP_ATTR_EXTENDED_COMMUNITY;
	if (Df_stricmp(tok, "AS4_PATH") == 0) return BGP_ATTR_AS4_PATH;
	if (Df_stricmp(tok, "AS4_AGGREGATOR") == 0) return BGP_ATTR_AS4_AGGREGATOR;
	if (Df_stricmp(tok, "SAFI_SSA") == 0) return BGP_ATTR_SAFI_SSA;
	if (Df_stricmp(tok, "CONNECTOR") == 0) return BGP_ATTR_CONNECTOR;
	if (Df_stricmp(tok, "AS_PATHLIMIT") == 0) return BGP_ATTR_AS_PATHLIMIT;
	if (Df_stricmp(tok, "PMSI_TUNNEL") == 0) return BGP_ATTR_PMSI_TUNNEL;
	if (Df_stricmp(tok, "TUNNEL_ENCAPSULATION") == 0) return BGP_ATTR_TUNNEL_ENCAPSULATION;
	if (Df_stricmp(tok, "TRAFFIC_ENGINEERING") == 0) return BGP_ATTR_TRAFFIC_ENGINEERING;
	if (Df_stricmp(tok, "IPV6_ADDRESS_SPECIFIC_EXTENDED_COMMUNITY") == 0) return BGP_ATTR_IPV6_ADDRESS_SPECIFIC_EXTENDED_COMMUNITY;
	if (Df_stricmp(tok, "AIGP") == 0) return BGP_ATTR_AIGP;
	if (Df_stricmp(tok, "PE_DISTINGUISHER_LABELS") == 0) return BGP_ATTR_PE_DISTINGUISHER_LABELS;
	if (Df_stricmp(tok, "ENTROPY_LEVEL_CAPABILITY") == 0) return BGP_ATTR_ENTROPY_LEVEL_CAPABILITY;
	if (Df_stricmp(tok, "LS") == 0) return BGP_ATTR_LS;
	if (Df_stricmp(tok, "LARGE_COMMUNITY") == 0) return BGP_ATTR_LARGE_COMMUNITY;
	if (Df_stricmp(tok, "BGPSEC_PATH") == 0) return BGP_ATTR_BGPSEC_PATH;
	if (Df_stricmp(tok, "COMMUNITY_CONTAINER") == 0) return BGP_ATTR_COMMUNITY_CONTAINER;
	if (Df_stricmp(tok, "PREFIX_SID") == 0) return BGP_ATTR_PREFIX_SID;
	if (Df_stricmp(tok, "SET") == 0) return BGP_ATTR_SET;

	char *p;

	NumConvRet ret;
	unsigned code = Atou(tok, &p, /*base=*/0, &ret);
	if (ret != NCVENOERR || code > 0xff || *p != '\0')
		Bgpgrep_Fatal("-attr: Bad attribute '%s'", tok);

	return code;
}

static void AddPrefixListToTrie(Triepair      *dest,
                                const Pfxlist *list,
                                NetpfxType     type)
{
	memset(dest, 0, sizeof(*dest));

	for (const Pfxnode *n = list->head[type]; n; n = n->next[type]) {
		switch (n->pfx.afi) {
		case AFI_IP:
			if (!dest->v4) dest->v4 = BgpgrepC_NewTrie(AFI_IP);

			if (!Pat_Insert(&dest->v4->trie, PLAINPFX(&n->pfx)))
				Sys_OutOfMemory();

			break;
		case AFI_IP6:
			if (!dest->v6) dest->v6 = BgpgrepC_NewTrie(AFI_IP6);

			if (!Pat_Insert(&dest->v6->trie, PLAINPFX(&n->pfx)))
				Sys_OutOfMemory();

			break;
		default: UNREACHABLE; break;
		}
	}
}

static Expridx ParsePrefixOp(Bgpvmopc opc)
{
	Pfxlist list;

	Bgpvmbytec c[MAXLEAFLEN];
	size_t     nc = 0;
	Triepair   t;

	BgpgrepC_ParsePrefixList(&list);
	if (list.isEmpty)
		c[nc++] = BGP_VMOP(BGP_VMOP_LOADU, 0);  // empty list = always FAIL

	if (list.head[WITHDRAWN]) {
		AddPrefixListToTrie(&t, &list, WITHDRAWN);
		c[nc++] = t.v4 ? BGP_VMOP(BGP_VMOP_LOADK, t.v4->kidx) : BGP_VMOP_LOADN;
		c[nc++] = t.v6 ? BGP_VMOP(BGP_VMOP_LOADK, t.v6->kidx) : BGP_VMOP_LOADN;
		c[nc++] = BGP_VMOP(opc, BGP_VMOPA_ALL_WITHDRAWN);
	}
	if (list.head[ANNOUNCE]) {
		if (!list.areListsMatching) AddPrefixListToTrie(&t, &list, ANNOUNCE);
		if (list.head[WITHDRAWN])   c[nc++] = BGP_VMOP(BGP_VMOP_JNZ, 3);

		c[nc++] = t.v4 ? BGP_VMOP(BGP_VMOP_LOADK, t.v4->kidx) : BGP_VMOP_LOADN;
		c[nc++] = t.v6 ? BGP_VMOP(BGP_VMOP_LOADK, t.v6->kidx) : BGP_VMOP_LOADN;
		c[nc++] = BGP_VMOP(opc, BGP_VMOPA_ALL_NLRI);
	}

	Expridx idx = PushLeaf(c, nc);
	BgpgrepC_FreePrefixList(&list);

	return idx;
}

static Expridx GetTerm(void)
{
	Bgpvmbytec c[MAXLEAFLEN];
	size_t     n = 0;

	C.curterm = BgpgrepC_ExpectAnyToken();

	if (strcmp(C.curterm, "(") == 0) {
		Expridx e = BgpgrepC_ParseExpression();
		BgpgrepC_ExpectToken(")");
		return e;

	} else if (strcmp(C.curterm, "!") == 0 || strcmp(C.curterm, "-not") == 0) {
		return PushOp(0, OP_NOT, GetTerm());

	} else if (strcmp(C.curterm, "-type") == 0) {
		BgpType type = ParseBgpType();

		c[n++] = BGP_VMOP(BGP_VMOP_CHKT, type);
		return PushLeaf(c, n);
	} else if (strcmp(C.curterm, "-attr") == 0) {
		BgpAttrCode code = ParseBgpAttr();

		c[n++] = BGP_VMOP(BGP_VMOP_CHKA, code);
		return PushLeaf(c, n);

	} else if (strcmp(C.curterm, "-aspath") == 0) {
		Sint32 kidx = BgpgrepC_BakeAsRegexp();

		c[n++] = BGP_VMOP(BGP_VMOP_LOADK, kidx);
		c[n++] = BGP_VMOP_FASMTC;
		return PushLeaf(c, n);

	} else if (strcmp(C.curterm, "-peer") == 0) {
		Sint32 kidx = BgpgrepC_ParsePeerExpression();
		if (kidx >= 0) {
			// Non-empty list, compile to a call
			c[n++] = BGP_VMOP(BGP_VMOP_LOADK, kidx);
			c[n++] = BGP_VMOP(BGP_VMOP_CALL, C.peerMatchFn);
		} else {
			// Empty list, always fails
			c[n++] = BGP_VMOP(BGP_VMOP_LOADU, FALSE);
		}
		return PushLeaf(c, n);

	} else if (strcmp(C.curterm, "-loops") == 0) {
		c[n++] = BGP_VMOP(BGP_VMOP_CALL, C.loopsFn);
		return PushLeaf(c, n);

	} else if (strcmp(C.curterm, "-bogon-asn") == 0) {
		c[n++] = BGP_VMOP(BGP_VMOP_CALL, C.bogonAsnFn);
		return PushLeaf(c, n);

	} else if (strcmp(C.curterm, "-exact") == 0) {
		return ParsePrefixOp(BGP_VMOP_EXCT);

	} else if (strcmp(C.curterm, "-supernet") == 0) {
		return ParsePrefixOp(BGP_VMOP_SUPN);

	} else if (strcmp(C.curterm, "-subnet") == 0) {
		return ParsePrefixOp(BGP_VMOP_SUBN);

	} else if (strcmp(C.curterm, "-related") == 0) {
		return ParsePrefixOp(BGP_VMOP_RELT);

	} else if (strcmp(C.curterm, "-timestamp") == 0) {
		Sint32 kidx = BgpgrepC_ParseTimestampExpression();
		c[n++] = BGP_VMOP(BGP_VMOP_LOADK, kidx);
		c[n++] = BGP_VMOP(BGP_VMOP_CALL, C.timestampCmpFn);
		return PushLeaf(c, n);

	} else if (strcmp(C.curterm, "-communities") == 0) {
		Sint32 kidx = BgpgrepC_ParseCommunity(BGP_VMOPT_ASSUME_COMTCH);
		if (kidx >= 0) {
			// Non-empty list, emit COMTCH
			c[n++] = BGP_VMOP(BGP_VMOP_LOADK, kidx);
			c[n++] = BGP_VMOP_COMTCH;
		} else {
			// Empty community always fails
			c[n++] = BGP_VMOP(BGP_VMOP_LOADU, FALSE);
		}
		return PushLeaf(c, n);
	} else if (strcmp(C.curterm, "-all-communities") == 0) {
		Sint32 kidx = BgpgrepC_ParseCommunity(BGP_VMOPT_ASSUME_ACOMTC);
		if (kidx >= 0) {
			// Non-empt list, emit ACOMTC
			c[n++] = BGP_VMOP(BGP_VMOP_LOADK, kidx);
			c[n++] = BGP_VMOP_ACOMTC;
		} else {
			// Empty list always succeeds
			c[n++] = BGP_VMOP(BGP_VMOP_LOADU, TRUE);
		}
		return PushLeaf(c, n);
	} else {
		Bgpgrep_Fatal("Invalid expression term '%s'", C.curterm);
		return 0;
	}
}

static Expropc GetOp(void)
{
	const char *tok = BgpgrepC_GetToken();

	Expropc opc = OP_NONE;
	if (tok) {
		if (strcmp(tok, "-and") == 0 || strcmp(tok, "-a") == 0) {
			opc = OP_AND;
		} else if (strcmp(tok, "-or") == 0 || strcmp(tok, "-o") == 0) {
			opc = OP_OR;
		} else {
			// Implicitly assume AND with anything else that follows
			BgpgrepC_UngetToken();

			// Closing ) appear due to expression grouping
			// e.g. ( -type ORIGIN -or -type AGGREGATOR )
			//
			// In this case we should return OP_NONE to signal
			// expression end, propagating out of ParseExpression()
			if (strcmp(tok, ")") != 0) {
				C.wasImplicitAnd = TRUE;

				opc = OP_AND;
			}
		}
	}

	return opc;
}

static Expridx BgpgrepC_ParseExpressionRecurse(Expridx l, int prio)
{
	while (TRUE) {
		Expropc opc = GetOp();
		if (opc == OP_NONE)
			break;
		if ((int) opc < prio) {
			BgpgrepC_UngetToken();
			break;
		}

		Expridx r = GetTerm();
		while (TRUE) {
			Expropc nextOpc = GetOp();
			if (nextOpc == OP_NONE)
				break;

			BgpgrepC_UngetToken();
			if (nextOpc <= opc)
				break;

			r = BgpgrepC_ParseExpressionRecurse(r, nextOpc);
		}

		l = PushOp(l, opc, r);
	}

	return l;
}

static Expridx BgpgrepC_ParseExpression(void)
{
	Expridx l = GetTerm();
	return BgpgrepC_ParseExpressionRecurse(l, OP_NONE);
}

static Boolean IsNestedBlock(Expropc outer, Expropc inner)
{
	assert(inner == OP_AND || inner == OP_OR);

	return outer != OP_NONE && inner != outer;
}

static Expropc BgpgrepC_CompileRecurse(Expridx idx, Expropc opc)
{
	Boolean nestedBlock;

	const Exprop *op = &C.code[idx];
	switch (op->opc) {
	case OP_AND:
		nestedBlock = IsNestedBlock(opc, op->opc);
		if (nestedBlock)
			Bgp_VmEmit(&S.vm, BGP_VMOP_BLK);

		BgpgrepC_CompileRecurse(op->n.l, op->opc);
		if (C.code[op->n.l].opc != OP_AND) {
			Bgp_VmEmit(&S.vm, BGP_VMOP_NOT);
			Bgp_VmEmit(&S.vm, BGP_VMOP_CFAIL);
		}
		BgpgrepC_CompileRecurse(op->n.r, op->opc);
		if (C.code[op->n.r].opc != OP_AND) {
			Bgp_VmEmit(&S.vm, BGP_VMOP_NOT);
			Bgp_VmEmit(&S.vm, BGP_VMOP_CFAIL);
		}
		if (nestedBlock) {
			Bgp_VmEmit(&S.vm, BGP_VMOP(BGP_VMOP_LOADU, TRUE));
			Bgp_VmEmit(&S.vm, BGP_VMOP_CPASS);
			Bgp_VmEmit(&S.vm, BGP_VMOP_ENDBLK);
		}
		break;

	case OP_OR:
		nestedBlock = IsNestedBlock(opc, op->opc);
		if (nestedBlock)
			Bgp_VmEmit(&S.vm, BGP_VMOP_BLK);

		BgpgrepC_CompileRecurse(op->n.l, op->opc);
		if (C.code[op->n.l].opc != OP_OR)
			Bgp_VmEmit(&S.vm, BGP_VMOP_CPASS);

		BgpgrepC_CompileRecurse(op->n.r, op->opc);
		if (C.code[op->n.r].opc != OP_OR)
			Bgp_VmEmit(&S.vm, BGP_VMOP_CPASS);

		if (nestedBlock) {
			Bgp_VmEmit(&S.vm, BGP_VMOP(BGP_VMOP_LOADU, TRUE));
			Bgp_VmEmit(&S.vm, BGP_VMOP_CFAIL);
			Bgp_VmEmit(&S.vm, BGP_VMOP_ENDBLK);
		}
		break;

	case OP_NOT:
		BgpgrepC_CompileRecurse(op->n.r, OP_NOT);
		Bgp_VmEmit(&S.vm, BGP_VMOP_NOT);
		break;

	case OP_LEAF:
		for (unsigned i = 0; i < op->leaf.nc; i++)
			Bgp_VmEmit(&S.vm, op->leaf.c[i]);

		break;

	default: UNREACHABLE; break;
	}

	return op->opc;
}

static void BgpgrepC_Compile(Expridx startIdx)
{
	Expropc opc = BgpgrepC_CompileRecurse(startIdx, OP_NONE);

	// Compile the very last result
	switch (opc) {
	case OP_OR:
		Bgp_VmEmit(&S.vm, BGP_VMOP(BGP_VMOP_LOADU, TRUE));
		Bgp_VmEmit(&S.vm, BGP_VMOP_CFAIL);
		break;
	case OP_AND:
		Bgp_VmEmit(&S.vm, BGP_VMOP(BGP_VMOP_LOADU, TRUE));
		Bgp_VmEmit(&S.vm, BGP_VMOP_CPASS);
		break;

	case OP_NOT:
	case OP_LEAF:
		Bgp_VmEmit(&S.vm, BGP_VMOP_CPASS);
		Bgp_VmEmit(&S.vm, BGP_VMOP(BGP_VMOP_LOADU, TRUE));
		Bgp_VmEmit(&S.vm, BGP_VMOP_CFAIL);
		break;

	default: UNREACHABLE; break;
	}
}

static Boolean IsLoadNz(Bgpvmbytec bytec)
{
	switch (BGP_VMOPC(bytec)) {
	case BGP_VMOP_LOAD:
	case BGP_VMOP_LOADU: return BGP_VMOPARG(bytec) != 0;
	default:             return FALSE;
	}
}

static Boolean IsLoadZ(Bgpvmbytec bytec)
{
	switch (BGP_VMOPC(bytec)) {
	case BGP_VMOP_LOAD:
	case BGP_VMOP_LOADU: return BGP_VMOPARG(bytec) == 0;
	default:             return FALSE;
	}
}

static void BgpgrepC_Optimize(void)
{
	Uint32 i, j, n;

	// Perform trivial peephole optimization
	Uint32     wi[4];
	Bgpvmbytec w[4];
	Boolean    changed;

	i = 0;
	while (i < S.vm.progLen) {  // NOTE: don't care for END
		if (S.vm.prog[i] == BGP_VMOP_NOP) {
			// Skip initial NOPs
			i++;
			continue;
		}

		changed = FALSE;

		// Fill peephole window
		for (j = 0, n = 0; n < ARRAY_SIZE(w); j++) {
			if (i + j >= S.vm.progLen)  // NOTE: don't care for END
				break;
			if (S.vm.prog[i+j] == BGP_VMOP_NOP)
				continue;  // do not place any NOP inside window

			wi[n] = i+j;
			w[n]  = S.vm.prog[i+j];
			n++;
		}

		// Trivial redundant operation elimination
		for (j = 1; j < n; j++) {
			// NOT-NOT = NOP
			if (w[j] == BGP_VMOP_NOT && w[j-1] == BGP_VMOP_NOT) {
				w[j-1] = w[j] = BGP_VMOP_NOP;

				changed = TRUE;
				continue;
			}
			// LOADU 0-NOT = LOADU 1
			if (IsLoadZ(w[j-1]) && w[j] == BGP_VMOP_NOT) {
				w[j-1] = BGP_VMOP_NOP;
				w[j]   = BGP_VMOP(BGP_VMOP_LOADU, 1);

				changed = TRUE;
				continue;
			}
			// LOADU 1-NOT = LOADU 0
			if (IsLoadNz(w[j-1]) && w[j] == BGP_VMOP_NOT) {
				w[j-1] = BGP_VMOP_NOP;
				w[j]   = BGP_VMOP(BGP_VMOP_LOADU, 0);

				changed = TRUE;
				continue;
			}
		}

		if (n == 4) {
			// Simplify common CFAIL and CPASS chains introduced by AND/OR blocks
			static const Bgpvmbytec ncfncp[] = {
				BGP_VMOP_NOT,
				BGP_VMOP_CFAIL,
				BGP_VMOP(BGP_VMOP_LOADU, TRUE),
				BGP_VMOP_CPASS
			};
			static const Bgpvmbytec ncpncf[] = {
				BGP_VMOP_NOT,
				BGP_VMOP_CPASS,
				BGP_VMOP(BGP_VMOP_LOADU, TRUE),
				BGP_VMOP_CFAIL
			};

			if (memcmp(w, ncfncp, sizeof(ncfncp)) == 0) {
				// Move CPASS up
				w[0] = BGP_VMOP_NOP;
				w[1] = BGP_VMOP_CPASS;
				w[2] = BGP_VMOP(BGP_VMOP_LOADU, TRUE);
				w[3] = BGP_VMOP_CFAIL;

				changed = TRUE;
			} else if (memcmp(w, ncpncf, sizeof(ncpncf)) == 0) {
				// Move CFAIL up
				w[0] = BGP_VMOP_NOP;
				w[1] = BGP_VMOP_CFAIL;
				w[2] = BGP_VMOP(BGP_VMOP_LOADU, TRUE);
				w[3] = BGP_VMOP_CPASS;

				changed = TRUE;
			}
		}

		if (changed) {
			// Update VM bytecode
			for (j = 0; j < n; j++)
				S.vm.prog[wi[j]] = w[j];

			continue;  // another round of peephole optimization
		}

		// No optimization, slide window
		i++;
	}

	// Eliminate NOPs
	for (i = 0, j = 0; i <= S.vm.progLen; i++) {
		if (S.vm.prog[i] != BGP_VMOP_NOP)
			S.vm.prog[j++] = S.vm.prog[i];
	}
	S.vm.progLen = j-1;

	assert(S.vm.prog[S.vm.progLen] == BGP_VMOP_END);
}

Trielist *BgpgrepC_NewTrie(Afi afi)
{
	Trielist *t = (Trielist *) malloc(sizeof(*t));
	if (!t)
		Sys_OutOfMemory();

	memset(&t->trie, 0, sizeof(t->trie));
	t->trie.afi = afi;

	t->kidx = BGP_VMSETKA(&S.vm, Bgp_VmNewk(&S.vm), &t->trie);
	if (t->kidx == -1)
		Bgpgrep_Fatal("BGP filter variables limit hit");

	t->next    = S.trieList;
	S.trieList = t;
	return t;
}

void Bgpgrep_CompileVmProgram(int argc, char **argv)
{
	// Initial compiler setup
	C.argc           = argc;
	C.argidx         = 0;
	C.argv           = argv;
	C.ncode          = 0;
	C.wasImplicitAnd = FALSE;

	C.loopsFn        = BGP_VMSETFN(&S.vm,
	                               Bgp_VmNewFn(&S.vm),
	                               BgpgrepF_FindAsLoops);
	C.peerMatchFn    = BGP_VMSETFN(&S.vm,
	                               Bgp_VmNewFn(&S.vm),
	                               BgpgrepF_PeerAddrMatch);
	C.timestampCmpFn = BGP_VMSETFN(&S.vm,
	                               Bgp_VmNewFn(&S.vm),
	                               BgpgrepF_TimestampCompare);
	C.bogonAsnFn = BGP_VMSETFN(&S.vm,
	                           Bgp_VmNewFn(&S.vm),
	                           BgpgrepF_BogonAsn);

	assert(C.loopsFn >= 0);
	assert(C.peerMatchFn >= 0);
	assert(C.timestampCmpFn >= 0);
	assert(C.bogonAsnFn >= 0);

	// Actual compilation
	if (C.argc > 0) {
		Expridx r = BgpgrepC_ParseExpression();

		// Make sure we've consumed the whole expression
		if (!IsEndOfParse()) {
			const char *last = BgpgrepC_LastToken();
			const char *tok  = BgpgrepC_GetToken();

			if (tok)
				Bgpgrep_Fatal("Unexpected '%s' after '%s'", tok, last);
			else  // should never happen but still...
				Bgpgrep_Fatal("Unexpected match expression end after '%s'", last);
		}

		// Convert to bytecode
		BgpgrepC_Compile(r);
		BgpgrepC_Optimize();
	} else {
		// Trivial filter
		Bgp_VmEmit(&S.vm, BGP_VMOP(BGP_VMOP_LOADU, TRUE));
		Bgp_VmEmit(&S.vm, BGP_VMOP_CPASS);

		S.isTrivialFilter = TRUE;
	}
	if (S.dumpBytecode)
		Bgp_VmDumpProgram(&S.vm, STM_CONHN(STDERR), Stm_ConOps);
}
