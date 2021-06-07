// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/dump.c
 *
 * General BGP dump functions wrappers.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"
#include "bgp/dump.h"
#include "sys/endian.h"

#define CALLFMT(fn, ...) \
	((fn) ? (fn(__VA_ARGS__)) : ((Sint64) Bgp_SetErrStat(BGPENOERR)))

Sint64 Bgp_DumpMrtUpdate(const Mrthdr *hdr,
                         void *streamp, const StmOps *ops,
                         const BgpDumpfmt *fmt)
{
	Bgpattrtab table;

	if (!ops->Write) {
		Bgp_SetErrStat(BGPENOERR);
		return 0;
	}

	BGP_CLRATTRTAB(table);
	if (MRT_ISBGP4MP(hdr->type)) {
		return CALLFMT(fmt->DumpBgp4mp, hdr, streamp, ops, table);

	} else if (hdr->type == MRT_BGP) {
		return CALLFMT(fmt->DumpZebra, hdr, streamp, ops, table);

	} else {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return -1;
	}
}

Sint64 Bgp_DumpMrtRibv2(const Mrthdr *hdr,
                        const Mrtpeerentv2 *peer, const Mrtribentv2 *ent,
                        void *streamp, const StmOps *ops,
                        const BgpDumpfmt *fmt)
{
	Bgpattrtab table;

	if (!ops->Write) {
		Bgp_SetErrStat(BGPENOERR);
		return 0;
	}
	if (hdr->type != MRT_TABLE_DUMPV2 || !TABLE_DUMPV2_ISRIB(hdr->subtype)) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return -1;
	}

	BGP_CLRATTRTAB(table);
	return CALLFMT(fmt->DumpRibv2, hdr, peer, ent, streamp, ops, table);
}

Sint64 Bgp_DumpMrtRib(const Mrthdr *hdr,
                      const Mrtribent *ent,
                      void *streamp, const StmOps *ops,
                      const BgpDumpfmt *fmt)
{
	Bgpattrtab table;

	if (!ops->Write) {
		Bgp_SetErrStat(BGPENOERR);
		return 0;
	}
	if (hdr->type != MRT_TABLE_DUMP) {
		Bgp_SetErrStat(BGPEBADMRTTYPE);
		return -1;
	}

	BGP_CLRATTRTAB(table);
	return CALLFMT(fmt->DumpRib, hdr, ent, streamp, ops, table);
}

