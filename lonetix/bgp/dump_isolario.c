// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/dump_isolario.c
 *
 * Isolario `bgpscanner`-like dump format.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"
#include "bgp/dump.h"
#include "sys/endian.h"
#include "sys/vt100.h"
#include "bufio.h"
#include "numlib.h"
#include "strlib.h"

#include <assert.h>
#include <string.h>

#define OPEN_MARKER          '>'
#define ANNOUNCE_MARKER      '+'
#define RIB_MARKER           '='
#define STATE_CHANGE_MARKER  '#'
#define WITHDRAW_MARKER      '-'
#define NOTIFY_MARKER        '!'
#define KEEPALIVE_MARKER     '@'
#define ROUTE_REFRESH_MARKER '^'
#define CLOSE_MARKER         '<'
#define UNKNOWN_MARKER       '?'

#define SEP_CHAR     '|'
#define SEP_CHAR_BAD '~'

#define CORRUPT_WARN "CORRUPTED"

// Aggregated information context for MRT dumps,
// useful for line trailing fields and for MP NEXT_HOP reconstruction
typedef struct Dumpfmtctx Dumpfmtctx;
struct Dumpfmtctx {
	Boolean8 hasPeer;
	Boolean8 hasTimestamp;
	Boolean8 isAsn32bit;
	Boolean8 isAddPath;
	Boolean8 isRibv2;
	Boolean8 withColors;

	const Mrthdr *hdr;  // NULL if not MRT

	Uint32 peerAs;      // host endian
	Uint32 pathId;      // host endian
	Uint32 timestamp;   // host endian
	Uint32 microsecs;   // host endian - non-zero if extended timestamp is available
	Ipadr  peerAddr;
};

/**
 * Binary Tree Indexed by PATH ID:
 *
 * e.g.
 * Root -> +---------------+
 *         | PATH ID: 0    |
 *         |  Prefix List  |
 *         |  +--------------------------------------------+
 *         |  | head --------------------------------+     |
 *         +--|                                      |     | Circular list, last
 *          / |           nextPrefix                 V     | element is the tree
 *         /  | | P[N] | <-- ... <-- | P[1] | <-- | P[0] | | node itself.
 *        /   +--------------------------------------------+
 *       /                \
 *      / children[0]      \ children[1]
 *    +--------------+    +--------------+
 *    | PATH ID: 1   |    | Path ID: 2   |
 *    |  Prefix List |    |  Prefix List |
 *    |   [...]      |    |   [...]      |
 *    +--------------+    +--------------+
 */
typedef struct Prefixtree Prefixtree;
struct Prefixtree {
	Uint32     pathId;  // host endian
	Afi        afi;
	RawPrefix *pfx;

	union {
		Prefixtree *nextPrefix; // for prefix list element
		Prefixtree *head;       // for tree node
	};

	Prefixtree *children[2];
};

#define MAXSEPS 10

// Empty separators buffer for state-change/unknown BGP4MP subtypes
static const char SEPS_BUF[MAXSEPS] = {
	SEP_CHAR, SEP_CHAR, SEP_CHAR, SEP_CHAR,
	SEP_CHAR, SEP_CHAR, SEP_CHAR, SEP_CHAR,
	SEP_CHAR, SEP_CHAR
};

static const char *Bgp_CapabilityToString(BgpCapCode code)
{
	switch (code) {
	case BGP_CAP_RESERVED:                         return "RESERVED";
	case BGP_CAP_MULTIPROTOCOL:                    return "MULTIPROTOCOL";
	case BGP_CAP_ROUTE_REFRESH:                    return "ROUTE_REFRESH";
	case BGP_CAP_COOPERATIVE_ROUTE_FILTERING:      return "COOPERATIVE_ROUTE_FILTERING";
	case BGP_CAP_MULTIPLE_ROUTES_DEST:             return "MULTIPLE_ROUTES_DEST";
	case BGP_CAP_EXTENDED_NEXT_HOP:                return "EXTENDED_NEXT_HOP";
	case BGP_CAP_EXTENDED_MESSAGE:                 return "EXTENDED_MESSAGE";
	case BGP_CAP_BGPSEC:                           return "BGPSEC";
	case BGP_CAP_MULTIPLE_LABELS:                  return "MULTIPLE_LABELS";
	case BGP_CAP_BGP_ROLE:                         return "BGP_ROLE";
	case BGP_CAP_GRACEFUL_RESTART:                 return "GRACEFUL_RESTART";
	case BGP_CAP_ASN32BIT:                         return "ASN32BIT";
	case BGP_CAP_DYNAMIC_CISCO:                    return "DYNAMIC_CISCO";
	case BGP_CAP_DYNAMIC:                          return "DYNAMIC";
	case BGP_CAP_MULTISESSION:                     return "MULTISESSION";
	case BGP_CAP_ADD_PATH:                         return "ADD_PATH";
	case BGP_CAP_ENHANCED_ROUTE_REFRESH:           return "ENHANCED_ROUTE_REFRESH";
	case BGP_CAP_LONG_LIVED_GRACEFUL_RESTART:      return "LONG_LIVED_GRACEFUL_RESTART";
	case BGP_CAP_CP_ORF:                           return "CP_ORF";
	case BGP_CAP_FQDN:                             return "FQDN";
	
	case BGP_CAP_ROUTE_REFRESH_CISCO:              return "ROUTE_REFRESH_CISCO";
	case BGP_CAP_ORF_CISCO:                        return "ORF_CISCO";
	case BGP_CAP_MULTISESSION_CISCO:               return "MULTISESSION_CISCO";
	default:                                       return NULL;
	}
}

static const char *Bgp_KnownCommunityToString(BgpCommCode code)
{
	switch (code) {
	case BGP_COMMUNITY_PLANNED_SHUT:               return "PLANNED_SHUT";
	case BGP_COMMUNITY_ACCEPT_OWN:                 return "ACCEPT_OWN";
	case BGP_COMMUNITY_ROUTE_FILTER_TRANSLATED_V4: return "ROUTE_FILTER_TRANSLATED_V4";
	case BGP_COMMUNITY_ROUTE_FILTER_V4:            return "ROUTE_FILTER_V4";
	case BGP_COMMUNITY_ROUTE_FILTER_TRANSLATED_V6: return "ROUTE_FILTER_TRANSLATED_V6";
	case BGP_COMMUNITY_ROUTE_FILTER_V6:            return "ROUTE_FILTER_V6";
	case BGP_COMMUNITY_LLGR_STALE:                 return "LLGR_STALE";
	case BGP_COMMUNITY_NO_LLGR:                    return "NO_LLGR";
	case BGP_COMMUNITY_ACCEPT_OWN_NEXTHOP:         return "ACCEPT_OWN_NEXTHOP";
	case BGP_COMMUNITY_STANDBY_PE:                 return "STANDBY_PE";
	case BGP_COMMUNITY_BLACKHOLE:                  return "BLACKHOLE";
	case BGP_COMMUNITY_NO_EXPORT:                  return "NO_EXPORT";
	case BGP_COMMUNITY_NO_ADVERTISE:               return "NO_ADVERTISE";
	case BGP_COMMUNITY_NO_EXPORT_SUBCONFED:        return "NO_EXPORT_SUBCONFED";
	case BGP_COMMUNITY_NO_PEER:                    return "NO_PEER";
	default:                                       return NULL;
	}
}

static void DumpUnknown(Stmbuf *sb, BgpType type)
{
	Bufio_Putc(sb, UNKNOWN_MARKER);
	Bufio_Putc(sb, SEP_CHAR);
	Bufio_Putx(sb, type);
	Bufio_Putsn(sb, SEPS_BUF, 8);
}

static Sint32 DumpCaps(Stmbuf *sb, Bgpcapiter *caps, const Dumpfmtctx *ctx)
{
	const char *s;
	Bgpcap     *cap;
	char        buf[16];

	USED(ctx->withColors);  // TODO

	Sint32 ncaps = 0;
	while ((cap = Bgp_NextCap(caps)) != NULL) {
		s = Bgp_CapabilityToString(cap->code);
		if (!s) {
			Utoa(cap->code, buf);
			s = buf;
		}

		if (ncaps > 0)
			Bufio_Putc(sb, ' ');

		Bufio_Puts(sb, s);
		ncaps++;
	}

	return ncaps;
}

static Judgement DumpAttributes(Stmbuf           *sb,
                                const Bgpattrseg *tpa,
                                Bgpattrtab        table,
                                const Dumpfmtctx *ctx)
{
	Aspathiter  ases;
	Nexthopiter nh;
	Asn         asn;
	Ipadr      *addr;
	char        buf[128];

	// AS_PATH
	if (Bgp_StartRealAsPath(&ases, tpa, ctx->isAsn32bit, table) != OK)
		return NG;

	static const Asseg emptySeqSeg = { AS_SEQUENCE, 0 };

	Boolean      firstAsn = TRUE;
	const Asseg *lastSeg  = &emptySeqSeg;
	while ((asn = Bgp_NextAsPath(&ases)) != -1) {
		if (lastSeg != BGP_CURASSEG(&ases)) {
			// Segment changed, we might need to close a SET
			if (lastSeg->type == AS_SET) {
				// Close pending AS_SET
				Bufio_Putc(sb, '}');
				firstAsn = FALSE;  // an empty set still counts as an AS
			}

			// Update last segment type
			lastSeg = BGP_CURASSEG(&ases);
			if (lastSeg->type == AS_SET) {
				// Open a new AS_SET
				if (!firstAsn)
					Bufio_Putc(sb, ' ');

				Bufio_Putc(sb, '{');
				firstAsn = TRUE;
			}
		}

		// Separate consecutive ASN
		if (!firstAsn)
			Bufio_Putc(sb, (lastSeg->type == AS_SET) ? ',' : ' ');

		// Write current ASN
		Bufio_Putu(sb, beswap32(ASN(asn)));
		firstAsn = FALSE;
	}
	if (Bgp_GetErrStat(NULL))
		return NG;

	// Close last AS_SET, if necessary
	if (lastSeg->type == AS_SET)
		Bufio_Putc(sb, '}');

	// NEXT_HOP
	Bufio_Putc(sb, SEP_CHAR);

	if (ctx->isRibv2)
		Bgp_StartAllRibv2NextHops(&nh, ctx->hdr, tpa, table);
	else
		Bgp_StartAllNextHops(&nh, tpa, table);

	if (Bgp_GetErrStat(NULL))
		return NG;

	Boolean firstNextHop = TRUE;
	while ((addr = Bgp_NextNextHop(&nh)) != NULL) {
		if (!firstNextHop)
			Bufio_Putc(sb, ' ');

		char *ep = Ip_AdrToString(addr, buf);
		Bufio_Putsn(sb, buf, ep - buf);
	}
	if (Bgp_GetErrStat(NULL))
		return NG;

	// ORIGIN
	Bufio_Putc(sb, SEP_CHAR);

	Bgpattr *origin = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_ORIGIN, table);
	if (Bgp_GetErrStat(NULL))
		return NG;

	if (origin) {
		if (!BGP_CHKORIGINSIZ(origin))
			return Bgp_SetErrStat(BGPETRUNCATTR); // TODO BGPEBADORIGIN

		switch (BGP_ORIGIN(origin)) {
		case BGP_ORIGIN_IGP:        Bufio_Putc(sb, 'i'); break;
		case BGP_ORIGIN_EGP:        Bufio_Putc(sb, 'e'); break;
		case BGP_ORIGIN_INCOMPLETE: Bufio_Putc(sb, '?'); break;

		default:
			return Bgp_SetErrStat(BGPETRUNCATTR);  // TODO BGPEBADORIGIN
		}
	}

	// ATOMIC_AGGREGATE
	Bufio_Putc(sb, SEP_CHAR);

	Bgpattr *atomicAggr = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_ATOMIC_AGGREGATE, table);
	if (Bgp_GetErrStat(NULL))
		return NG;

	if (atomicAggr) {
		if (!BGP_CHKATOMICAGGRSIZ(atomicAggr))
			return Bgp_SetErrStat(BGPETRUNCATTR);  // TODO BGPEBADATOMICAGGR

		Bufio_Puts(sb, "AT");
	}

	// AGGREGATOR
	Bufio_Putc(sb, SEP_CHAR);

	Boolean  isAggr32bit = ctx->isAsn32bit;
	Bgpattr *aggr        = Bgp_GetRealAggregator(tpa, &isAggr32bit, table);
	if (Bgp_GetErrStat(NULL))
		return NG;

	if (aggr) {
		if (!BGP_CHKAGGRSIZ(aggr, isAggr32bit))
			return Bgp_SetErrStat(BGPETRUNCATTR);  // TODO BGPEBADAGGR

		const Bgpaggr *aggrp = (const Bgpaggr *) BGP_ATTRPTR(aggr);
		if (isAggr32bit) {
			char *ep = Ipv4_AdrToString(&aggrp->a32.addr, buf);

			Bufio_Putu(sb, beswap32(aggrp->a32.asn));
			Bufio_Putc(sb, ' ');
			Bufio_Putsn(sb, buf, ep -  buf);
		} else {
			char *ep = Ipv4_AdrToString(&aggrp->a16.addr, buf);

			Bufio_Putu(sb, beswap16(aggrp->a16.asn));
			Bufio_Putc(sb, ' ');
			Bufio_Putsn(sb, buf, ep - buf);
		}
	}

	// Communities
	// => COMMUNITY
	Bufio_Putc(sb, SEP_CHAR);

	Bgpcommiter communities;
	Bgpattr *community = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_COMMUNITY, table);
	if (Bgp_GetErrStat(NULL))
		return NG;

	unsigned ncommunities = 0;
	if (community) {
		Bgpcomm *comm;

		if (Bgp_StartCommunity(&communities, community) != OK)
			return NG;

		while ((comm = Bgp_NextCommunity(&communities)) != NULL) {
			if (ncommunities > 0)
				Bufio_Putc(sb, ' ');

			const char *knownString = Bgp_KnownCommunityToString(comm->code);
			if (knownString)
				Bufio_Puts(sb, knownString);

			else {
				char *ep = Utoa(beswap16(comm->hi), buf);
				*ep++    = ':';
				ep       = Utoa(beswap16(comm->lo), ep);
				Bufio_Putsn(sb, buf, ep - buf);
			}
			ncommunities++;
		}
		if (Bgp_GetErrStat(NULL))
			return NG;  // iteration failed
	}

	// => TODO EXTENDED_COMMUNITY

	// => LARGE_COMMUNITY
	Bgpattr *largeCommunity = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_LARGE_COMMUNITY, table);
	if (Bgp_GetErrStat(NULL))
		return NG;

	if (largeCommunity) {
		Bgplgcomm *comm;

		if (Bgp_StartCommunity(&communities, largeCommunity) != OK)
			return NG;

		while ((comm = Bgp_NextLargeCommunity(&communities)) != NULL) {
			if (ncommunities > 0)
				Bufio_Putc(sb, ' ');

			char *ep = Utoa(beswap32(comm->global), buf);
			*ep++    = ':';
			ep       = Utoa(beswap32(comm->local1), ep);
			*ep++    = ':';
			ep       = Utoa(beswap32(comm->local2), ep);

			Bufio_Putsn(sb, buf, ep - buf);
			ncommunities++;
		}
		if (Bgp_GetErrStat(NULL))
			return NG;
	}

	return OK;
}

static void DumpMrtInfoTrailer(Stmbuf *sb, const Dumpfmtctx *ctx)
{
	char buf[128];

	// Peer address, ASN
	if (ctx->hasPeer) {
		char *ep = Ip_AdrToString(&ctx->peerAddr, buf);

		Bufio_Putsn(sb, buf, ep - buf);
		Bufio_Putc(sb, ' ');
		Bufio_Putu(sb, ctx->peerAs);
		if (ctx->isAddPath) {
			Bufio_Putc(sb, ' ');
			Bufio_Putu(sb, ctx->pathId);
		}
	}

	Bufio_Putc(sb, SEP_CHAR);

	// MRT timestamp + microseconds if extended
	if (ctx->hasTimestamp) {
		Bufio_Putu(sb, ctx->timestamp);
		if (ctx->microsecs > 0) {
			Utoa(ctx->microsecs, buf);
			Df_strpadl(buf, '0', 9);

			Bufio_Putc(sb, '.');
			Bufio_Puts(sb, buf);
		}
	}

	Bufio_Putc(sb, SEP_CHAR);

	// ASN32BIT flag
	Bufio_Putc(sb, ctx->isAsn32bit ? '1' : '0');
	Bufio_Puts(sb, EOLN);
}

static Sint32 DumpRoutesFast(char              marker,
                             Stmbuf           *sb,
                             Bgpmpiter        *prefixes,
                             const Bgpattrseg *tpa,
                             Bgpattrtab        table,
                             const Dumpfmtctx *ctx)
{
	Prefix *pfx;
	char    buf[PFX_STRLEN + 1];

	assert(!ctx->isAddPath);

	Sint32 nprefixes = 0;
	while ((pfx = Bgp_NextMpPrefix(prefixes)) != NULL) {
		char *ep = Bgp_PrefixToString(pfx->afi, PLAINPFX(pfx), buf);
		if (nprefixes == 0) {
			Bufio_Putc(sb, marker);
			Bufio_Putc(sb, SEP_CHAR);
		} else
			Bufio_Putc(sb, ' ');

		Bufio_Putsn(sb, buf, ep - buf);
		nprefixes++;
	}
	if (Bgp_GetErrStat(NULL))
		return -1;
	if (nprefixes == 0)
		return 0;  // drop line, nothing to report

	Bufio_Putc(sb, SEP_CHAR);

	if (DumpAttributes(sb, tpa, table, ctx) != OK)
		return -1;

	Bufio_Putc(sb, SEP_CHAR);
	DumpMrtInfoTrailer(sb, ctx);
	return nprefixes;
}

// Add node 'n' to the prefix tree rooted at '*pr'.
static unsigned InsertRoute(Prefixtree **pr, Prefixtree *n)
{
	unsigned depth = 0;

	Prefixtree *i;
	while ((i = *pr) && n->pathId != i->pathId) {
		int idx = (n->pathId > i->pathId);

		pr = &i->children[idx];
		depth++;
	}
	if (i) {
		// Collision, PATH ID already seen,
		// copy childen pointers over
		n->nextPrefix = i->head;
		i->head = n;
	} else {
		// First prefix with this PATH ID, becomes list head
		n->head = n;
		n->children[0] = n->children[1] = NULL;

		*pr = n;
		depth++;
	}
	return depth;
}

static Sint32 DumpRouteWithPathId(char              marker,
                                  Stmbuf           *sb,
                                  const Prefixtree *n,
                                  const Bgpattrseg *tpa,
                                  Bgpattrtab        table,
                                  Dumpfmtctx       *ctx)
{
	char buf[PFX_STRLEN + 1];

	assert(ctx->isAddPath);
	ctx->pathId = n->pathId;

	Bufio_Putc(sb, marker);
	Bufio_Putc(sb, SEP_CHAR);

	Sint32 nprefixes = 0;

	Prefixtree *i = n->head;
	do {
		char *ep = Bgp_PrefixToString(i->afi, i->pfx, buf);
		if (nprefixes > 0)
			Bufio_Putc(sb, ' ');

		Bufio_Putsn(sb, buf, ep - buf);
		nprefixes++;

		i = i->nextPrefix;
	} while (i != n);

	Bufio_Putc(sb, SEP_CHAR);

	if (DumpAttributes(sb, tpa, table, ctx) != OK)
		return -1;

	Bufio_Putc(sb, SEP_CHAR);
	DumpMrtInfoTrailer(sb, ctx);
	return nprefixes;
}

static Sint32 DumpRoutes(char              marker,
                         Stmbuf           *sb,
                         Bgpmpiter        *prefixes,
                         const Bgpattrseg *tpa,
                         Bgpattrtab        table,
                         Dumpfmtctx       *ctx)
{
	Prefix     *pfx;
	Prefixtree *n;

	// Fast case when PATH ID is absent
	if (!ctx->isAddPath)
		return DumpRoutesFast(marker, sb, prefixes, tpa, table, ctx);

	// When PATH ID is available, we build a net tree grouped by PATH ID
	// directly on stack
	Sint32      nprefixes = 0;
	unsigned    maxDepth  = 0;
	Prefixtree *root      = NULL;
	while ((pfx = Bgp_NextMpPrefix(prefixes)) != NULL) {
		n = (Prefixtree *) alloca(sizeof(*n));

		n->pathId = beswap32(pfx->pathId);
		n->afi    = pfx->afi;
		n->pfx    = BGP_CURMPRAWPFX(prefixes);

		unsigned depth = InsertRoute(&root, n);
		if (depth > maxDepth)
			maxDepth = depth;
	}
	if (Bgp_GetErrStat(NULL))
		return -1;

	// Print every route inside tree
	Prefixtree **stackb = (Prefixtree **) alloca(maxDepth * sizeof(*stackb));
	unsigned     si     = 0;

	n = root;
	while (TRUE) {
		while (n) {
			stackb[si++] = n;

			n = n->children[0];
		}
		if (si == 0)
			break;

		n = stackb[--si];

		// Generate new line
		nprefixes += DumpRouteWithPathId(marker, sb, n, tpa, table, ctx);
		n = n->children[1];
	}

	return nprefixes;
}

static Judgement DumpBgp(Stmbuf        *sb,
                         BgpType        type,
                         const void    *data,
                         size_t         nbytes,
                         Bgpattrtab     table,
                         Dumpfmtctx    *ctx)
{
	Bgpcapiter caps;
	Bgpmpiter prefixes;
	Bgpwithdrawnseg *withdrawn;
	Bgpparmseg *parms;
	Bgpattrseg *tpa;
	Bgpattr *mpAttr;
	void    *nlri;
	size_t   nlriSize;

	switch (type) {
	case BGP_OPEN:
		Bufio_Putc(sb, OPEN_MARKER);

		parms = Bgp_GetParmsFromMemory(data, nbytes);
		if (!parms)
			goto corrupted;

		Bgp_StartCaps(&caps, parms);
		Bufio_Putc(sb, SEP_CHAR);
		DumpCaps(sb, &caps, ctx);
		Bufio_Putsn(sb, SEPS_BUF, 7);
		DumpMrtInfoTrailer(sb, ctx);
		break;

	case BGP_UPDATE:
		withdrawn = Bgp_GetWithdrawnFromMemory(data, nbytes);
		if (!withdrawn)
			goto corrupted;

		tpa = Bgp_GetAttributesFromMemory(data, nbytes);
		if (!tpa)
			goto corrupted;

		mpAttr = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_MP_UNREACH_NLRI, table);
		if (Bgp_GetErrStat(NULL))
			goto corrupted;

		Bgp_InitMpWithdrawn(&prefixes, withdrawn, mpAttr, ctx->isAddPath);
		if (DumpRoutes(WITHDRAW_MARKER, sb, &prefixes, tpa, table, ctx) == -1)
			goto corrupted;

		nlri = Bgp_GetNlriFromMemory(data, nbytes, &nlriSize);
		if (!nlri)
			goto corrupted;

		mpAttr = Bgp_GetUpdateAttribute(tpa, BGP_ATTR_MP_REACH_NLRI, table);
		if (Bgp_GetErrStat(NULL))
			goto corrupted;

		Bgp_InitMpNlri(&prefixes, nlri, nlriSize, mpAttr, ctx->isAddPath);
		if (DumpRoutes(ANNOUNCE_MARKER, sb, &prefixes, tpa, table, ctx) == -1)
			goto corrupted;

		break;

	case BGP_NOTIFICATION:
		Bufio_Putc(sb, NOTIFY_MARKER);
		Bufio_Putsn(sb, SEPS_BUF, 8);
		// TODO dump message
		DumpMrtInfoTrailer(sb, ctx);
		break;

	case BGP_KEEPALIVE:
		Bufio_Putc(sb, KEEPALIVE_MARKER);
		Bufio_Putsn(sb, SEPS_BUF, 8);
		DumpMrtInfoTrailer(sb, ctx);
		break;

	case BGP_ROUTE_REFRESH:
		Bufio_Putc(sb, ROUTE_REFRESH_MARKER);
		Bufio_Putsn(sb, SEPS_BUF, 8);
		DumpMrtInfoTrailer(sb, ctx);
		break;

	case BGP_CLOSE:
		Bufio_Putc(sb, CLOSE_MARKER);
		Bufio_Putsn(sb, SEPS_BUF, 8);
		DumpMrtInfoTrailer(sb, ctx);
		break;

	default:
		DumpUnknown(sb, type);
		DumpMrtInfoTrailer(sb, ctx);
		break;
	}

	return OK;

corrupted:
	Bufio_Putc(sb, SEP_CHAR_BAD);
	if (ctx->withColors)
		Bufio_Puts(sb, CORRUPT_WARN);
	else
		Bufio_Puts(sb, VTSGR(VTINV) CORRUPT_WARN VTSGR(VTNOINV));

	Bufio_Putc(sb, SEP_CHAR_BAD);
	return NG;
}

static Sint64 Isolario_DumpMsg(const Bgphdr *hdr,
                               unsigned      flags,
                               void         *streamp,
                               const StmOps *ops,
                               Bgpattrtab    table)
{
	Stmbuf sb;

	size_t nbytes = beswap16(hdr->len);
	assert(nbytes >= BGP_HDRSIZ);
	nbytes -= BGP_HDRSIZ;

	Dumpfmtctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.isAsn32bit = BGP_ISASN32BIT(flags);
	ctx.isAddPath  = BGP_ISADDPATH(flags);

	Bufio_Init(&sb, streamp, ops);
	DumpBgp(&sb, hdr->type, hdr + 1, nbytes, table, &ctx);
	return Bufio_Flush(&sb);
}

static Sint64 Isolario_DumpMsgWc(const Bgphdr *hdr,
                                 unsigned      flags,
                                 void         *streamp,
                                 const StmOps *ops,
                                 Bgpattrtab    table)
{
	Stmbuf sb;

	size_t nbytes = beswap16(hdr->len);
	assert(nbytes >= BGP_HDRSIZ);
	nbytes -= BGP_HDRSIZ;

	Dumpfmtctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.isAsn32bit = BGP_ISASN32BIT(flags);
	ctx.isAddPath  = BGP_ISADDPATH(flags);
	ctx.withColors = TRUE;

	Bufio_Init(&sb, streamp, ops);
	DumpBgp(&sb, hdr->type, hdr + 1, nbytes, table, &ctx);

	return Bufio_Flush(&sb);
}

static Sint64 Isolario_DumpRibv2(const Mrthdr       *hdr,
                                 const Mrtpeerentv2 *peer,
                                 const Mrtribentv2  *rib,
                                 void               *streamp,
                                 const StmOps       *ops,
                                 Bgpattrtab          table)
{
	assert(hdr->type == MRT_TABLE_DUMPV2);
	assert(TABLE_DUMPV2_ISRIB(hdr->subtype));

	Stmbuf     sb;
	Dumpfmtctx ctx;
	char       buf[APPFX_STRLEN + 1];
	char      *ep;

	Bufio_Init(&sb, streamp, ops);

	memset(&ctx, 0, sizeof(ctx));
	ctx.hasPeer      = TRUE;
	ctx.hasTimestamp = TRUE;
	ctx.isAsn32bit   = TRUE;   // always the case for TABLE_DUMPV2
	ctx.isRibv2      = TRUE;
	ctx.hdr          = hdr;
	ctx.timestamp    = beswap32(rib->originatedTime);

	switch (peer->type & (MRT_PEER_ASN32BIT|MRT_PEER_IP6)) {
	case MRT_PEER_ASN32BIT | MRT_PEER_IP6:
		ctx.peerAs          = beswap32(peer->a32v6.asn);
		ctx.peerAddr.family = IP6;
		ctx.peerAddr.v6     = peer->a32v6.addr;
		break;
	case MRT_PEER_IP6:
		ctx.peerAs          = beswap16(peer->a16v6.asn);
		ctx.peerAddr.family = IP6;
		ctx.peerAddr.v6     = peer->a16v6.addr;
		break;
	case MRT_PEER_ASN32BIT:
		ctx.peerAs          = beswap32(peer->a32v4.asn);
		ctx.peerAddr.family = IP4;
		ctx.peerAddr.v4     = peer->a16v4.addr;
		break;
	default:
		ctx.peerAs          = beswap16(peer->a16v4.asn);
		ctx.peerAddr.family = IP4;
		ctx.peerAddr.v4     = peer->a16v4.addr;
		break;
	}

	const Mrtribhdrv2 *ribhdr = RIBV2_HDR(hdr);
	const Bgpattrseg  *tpa    = RIBV2_GETATTRIBS(hdr->subtype, rib);

	switch (hdr->subtype) {
	case TABLE_DUMPV2_RIB_IPV4_UNICAST_ADDPATH:
	case TABLE_DUMPV2_RIB_IPV4_MULTICAST_ADDPATH:
		ctx.isAddPath = TRUE;
		ctx.pathId    = beswap32(RIBV2_GETPATHID(rib));
		/*FALLTHROUGH*/
	case TABLE_DUMPV2_RIB_IPV4_UNICAST:
	case TABLE_DUMPV2_RIB_IPV4_MULTICAST:
		ep = Bgp_PrefixToString(AFI_IP, &ribhdr->nlri, buf);
		break;
	case TABLE_DUMPV2_RIB_IPV6_UNICAST_ADDPATH:
	case TABLE_DUMPV2_RIB_IPV6_MULTICAST_ADDPATH:
		ctx.isAddPath = TRUE;
		ctx.pathId    = beswap32(RIBV2_GETPATHID(rib));
		/*FALLTHROUGH*/
	case TABLE_DUMPV2_RIB_IPV6_UNICAST:
	case TABLE_DUMPV2_RIB_IPV6_MULTICAST:
		ep = Bgp_PrefixToString(AFI_IP6, &ribhdr->nlri, buf);
		break;
	case TABLE_DUMPV2_RIB_GENERIC_ADDPATH:
		ctx.isAddPath = TRUE;
		ctx.pathId    = beswap32(RIBV2_GETPATHID(rib));
		/*FALLTHROUGH*/
	case TABLE_DUMPV2_RIB_GENERIC:
		ep = Bgp_PrefixToString(ribhdr->gen.afi, &ribhdr->gen.nlri, buf);
		break;
	default:
		UNREACHABLE;
		break;
	}

	assert(ep != NULL);

	Bufio_Putc(&sb, RIB_MARKER);
	Bufio_Putc(&sb, SEP_CHAR);

	Bufio_Putsn(&sb, buf, ep - buf);
	Bufio_Putc(&sb, SEP_CHAR);

	DumpAttributes(&sb, tpa, table, &ctx);
	Bufio_Putc(&sb, SEP_CHAR);

	DumpMrtInfoTrailer(&sb, &ctx);
	return Bufio_Flush(&sb);
}

static Sint64 Isolario_DumpRib(const Mrthdr    *hdr,
                               const Mrtribent *rib,
                               void            *streamp,
                               const StmOps    *ops,
                               Bgpattrtab       table)
{
	assert(hdr->type == MRT_TABLE_DUMP);

	Stmbuf     sb;
	Dumpfmtctx ctx;
	RawPrefix  pfx;
	char       buf[APPFX_STRLEN + 1];
	char      *ep;

	Bufio_Init(&sb, streamp, ops);

	memset(&ctx, 0, sizeof(ctx));
	ctx.hasPeer      = TRUE;
	ctx.hasTimestamp = TRUE;
	ctx.hdr          = hdr;

	const Bgpattrseg *tpa = RIB_GETATTRIBS(hdr->subtype, rib);

	switch (hdr->subtype) {
	case AFI_IP:
		pfx.width = rib->v4.prefixLen;
		pfx.v4    = rib->v4.prefix;
		assert(pfx.width < IPV4_WIDTH);

		ctx.timestamp = beswap32(rib->v4.originatedTime);

		ctx.peerAddr.family = IP4;
		ctx.peerAddr.v4     = rib->v4.peerAddr;
		ctx.peerAs          = beswap16(rib->v4.peerAs);
		break;
	case AFI_IP6:
		pfx.width = rib->v6.prefixLen;
		pfx.v6    = rib->v6.prefix;
		assert(pfx.width < IPV6_WIDTH);

		ctx.timestamp = beswap32(rib->v6.originatedTime);

		ctx.peerAddr.family = IP6;
		ctx.peerAddr.v6     = rib->v6.peerAddr;
		ctx.peerAs          = beswap16(rib->v6.peerAs);
		break;
	default:
		UNREACHABLE;
		break;
	}

	ep = Bgp_PrefixToString(hdr->subtype, &pfx, buf);

	Bufio_Putc(&sb, RIB_MARKER);
	Bufio_Putc(&sb, SEP_CHAR);

	Bufio_Putsn(&sb, buf, ep - buf);
	Bufio_Putc(&sb, SEP_CHAR);

	DumpAttributes(&sb, tpa, table, &ctx);
	Bufio_Putc(&sb, SEP_CHAR);

	DumpMrtInfoTrailer(&sb, &ctx);
	return Bufio_Flush(&sb);
}

static Sint64 Isolario_DumpBgp4mp(const Mrthdr    *hdr,
                                  void            *streamp,
                                  const StmOps    *ops,
                                  Bgpattrtab       table)
{
	assert(hdr->type == MRT_BGP4MP || hdr->type == MRT_BGP4MP_ET);

	Stmbuf     sb;
	Dumpfmtctx ctx;
	size_t     offset;  // offset to BGP4MP payload
	size_t     len;

	const Bgp4mphdr *bgp4mp = BGP4MP_HDR(hdr);

	Bufio_Init(&sb, streamp, ops);

	len = beswap32(hdr->len);

	memset(&ctx, 0, sizeof(ctx));
	ctx.hasPeer      = TRUE;
	ctx.hasTimestamp = TRUE;
	ctx.isAsn32bit   = BGP4MP_ISASN32BIT(hdr->subtype);
	ctx.isAddPath    = BGP4MP_ISADDPATH(hdr->subtype);
	ctx.timestamp    = beswap32(hdr->timestamp);
	if (hdr->type == MRT_BGP4MP_ET)
		ctx.microsecs = beswap32(((const Mrthdrex *) hdr)->microsecs);

	ctx.hdr = hdr;

	if (ctx.isAsn32bit) {
		ctx.peerAs = beswap32(bgp4mp->a32.peerAs);
		if (bgp4mp->a32.afi == AFI_IP) {
			ctx.peerAddr.family = IP4;
			ctx.peerAddr.v4     = bgp4mp->a32v4.peerAddr;

			offset = sizeof(bgp4mp->a32v4);
		} else {
			assert(bgp4mp->a32.afi == AFI_IP6);

			ctx.peerAddr.family = IP6;
			ctx.peerAddr.v6     = bgp4mp->a32v6.peerAddr;

			offset = sizeof(bgp4mp->a32v6);
		}
	} else {
		ctx.peerAs = beswap16(bgp4mp->a16.peerAs);
		if (bgp4mp->a16.afi == AFI_IP) {
			ctx.peerAddr.family = IP4;
			ctx.peerAddr.v4     = bgp4mp->a16v4.peerAddr;

			offset = sizeof(bgp4mp->a16v4);
		} else {
			assert(bgp4mp->a16.afi == AFI_IP6);

			ctx.peerAddr.family = IP6;
			ctx.peerAddr.v6     = bgp4mp->a16v6.peerAddr;

			offset = sizeof(bgp4mp->a16v6);
		}
	}

	assert(len >= offset);

	// Write contents
	if (BGP4MP_ISSTATECHANGE(hdr->subtype)) {
		// Dump state change
		BgpFsmState chng[2];
		memcpy(chng, (Uint8 *) bgp4mp + offset, sizeof(chng));

		Bufio_Putc(&sb, STATE_CHANGE_MARKER);
		Bufio_Putc(&sb, SEP_CHAR);
		Bufio_Putu(&sb, beswap16(chng[0]));
		Bufio_Putc(&sb, '-');
		Bufio_Putu(&sb, beswap16(chng[1]));
		Bufio_Putsn(&sb, SEPS_BUF, 7);
		DumpMrtInfoTrailer(&sb, &ctx);

	} else if (BGP4MP_ISMESSAGE(hdr->subtype)) {
		// Wraps BGP message, wrapped message includes header and marker
		const Bgphdr *msg    = (const Bgphdr *) ((Uint8 *) bgp4mp + offset);
		size_t        nbytes = len - offset;

		nbytes  = Bgp_CheckMsgHdr(msg, nbytes, /*allowExtendedSize=*/TRUE);
		nbytes -= BGP_HDRSIZ;

		DumpBgp(&sb, msg->type, msg + 1, nbytes, table, &ctx);

	} else {
		// Deprecated/Unknown type
		Bufio_Putc(&sb, UNKNOWN_MARKER);
		Bufio_Putsn(&sb, SEPS_BUF, 9);
		DumpMrtInfoTrailer(&sb, &ctx);
	}

	return Bufio_Flush(&sb);
}

static Sint64 Isolario_DumpZebra(const Mrthdr   *hdr,
                                 void           *streamp,
                                 const StmOps   *ops,
                                 Bgpattrtab      table)
{
	assert(hdr->type == MRT_BGP);

	Stmbuf     sb;
	Dumpfmtctx ctx;

	const Zebrahdr *zebra = ZEBRA_HDR(hdr);

	Bufio_Init(&sb, streamp, ops);

	memset(&ctx, 0, sizeof(ctx));
	ctx.hasPeer         = TRUE;
	ctx.hasTimestamp    = TRUE;
	ctx.timestamp       = beswap32(hdr->timestamp);
	ctx.hdr             = hdr;
	ctx.peerAddr.family = IP4;
	ctx.peerAddr.v4     = zebra->peerAddr;

	if (hdr->subtype == ZEBRA_STATE_CHANGE) {
		const Zebrastatchng *chng = (const Zebrastatchng *) zebra;

		Bufio_Putc(&sb, STATE_CHANGE_MARKER);
		Bufio_Putc(&sb, SEP_CHAR);
		Bufio_Putu(&sb, beswap16(chng->oldState));
		Bufio_Putc(&sb, '-');
		Bufio_Putu(&sb, beswap16(chng->newState));
		Bufio_Putsn(&sb, SEPS_BUF, 7);
		DumpMrtInfoTrailer(&sb, &ctx);

	} else if (ZEBRA_ISMESSAGE(hdr->subtype)) {
		const Zebramsghdr *zebramsg = (const Zebramsghdr *) zebra;
		size_t             len      = beswap32(hdr->len);
		size_t             offset   = offsetof(Zebramsghdr, msg);

		size_t nbytes = len - offset;

		BgpType type;
		switch (hdr->subtype) {
		case ZEBRA_UPDATE:    type = BGP_UPDATE;       break;
		case ZEBRA_OPEN:      type = BGP_OPEN;         break;
		case ZEBRA_NOTIFY:    type = BGP_NOTIFICATION; break;
		case ZEBRA_KEEPALIVE: type = BGP_KEEPALIVE;    break;
		default:
			UNREACHABLE;
			return -1;
		}

		DumpBgp(&sb, type, zebramsg->msg, nbytes, table, &ctx);

	} else {
		// Unknown type
		Bufio_Putc(&sb, UNKNOWN_MARKER);
		Bufio_Putsn(&sb, SEPS_BUF, 9);
		DumpMrtInfoTrailer(&sb, &ctx);
	}

	return Bufio_Flush(&sb);
}

static const BgpDumpfmt bgp_isolarioTable = {
	Isolario_DumpMsg,
	Isolario_DumpRibv2,
	Isolario_DumpRib,
	Isolario_DumpBgp4mp,
	Isolario_DumpZebra
};
static const BgpDumpfmt bgp_isolarioTableWc = {
	Isolario_DumpMsgWc,
	Isolario_DumpRibv2,
	Isolario_DumpRib,
	Isolario_DumpBgp4mp,
	Isolario_DumpZebra
};

const BgpDumpfmt *const Bgp_IsolarioFmt   = &bgp_isolarioTable;
const BgpDumpfmt *const Bgp_IsolarioFmtWc = &bgp_isolarioTableWc;
