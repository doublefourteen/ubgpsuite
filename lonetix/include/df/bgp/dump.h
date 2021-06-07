// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/dump.h
 *
 * BGP message dump utilities.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGP_DUMP_H_
#define DF_BGP_DUMP_H_

#include "mrt.h"

/**
 * \brief BGP message dump formatter.
 *
 * \note Calling the dump functions directly should be considered low-level,
 *       under normal circumstances use: `Bgp_DumpMsg()`, `Bgp_DumpMrtUpdate()`,
 *       `Bgp_DumpRib()`, and `Bgp_DumpRibv2()`.
 */
typedef struct {
	Sint64 (*DumpMsg)(const Bgphdr *, unsigned,
	                  void *, const StmOps *,
	                  Bgpattrtab);

	Sint64 (*DumpRibv2)(const Mrthdr *,
	                    const Mrtpeerentv2 *, const Mrtribentv2 *,
	                    void *, const StmOps *,
	                    Bgpattrtab);

	Sint64 (*DumpRib)(const Mrthdr *,
	                  const Mrtribent *,
	                  void *, const StmOps *,
	                  Bgpattrtab);

	Sint64 (*DumpBgp4mp)(const Mrthdr *,
	                     void *, const StmOps *,
	                     Bgpattrtab);

	Sint64 (*DumpZebra)(const Mrthdr *,
	                    void *, const StmOps *,
	                    Bgpattrtab);
} BgpDumpfmt;

// Standard dump formatters
extern const BgpDumpfmt *const Bgp_IsolarioFmt;    ///< Isolario `bgpscanner` like output format
extern const BgpDumpfmt *const Bgp_IsolarioFmtWc;  ///< Isolario `bgpscanner` like output format, with colors

// extern const BgpDumpfmt *const Bgp_BgpdumpFmt;   ///< `bgpdump` style output format
// extern const BgpDumpfmt *const Bgp_RawFmt;       ///< output message raw bytes
// extern const BgpDumpfmt *const Bgp_HexFmt;       ///< perform BGP message hexadecimal dump
// extern const BgpDumpfmt *const Bgp_CFmt;         ///< outputs BGP message as a C-style array

FORCE_INLINE Sint64 Bgp_DumpMsg(Bgpmsg *msg,
                                void *streamp, const StmOps *ops,
                                const BgpDumpfmt *fmt)
{
	extern Judgement _Bgp_SetErrStat(BgpRet,
	                                 const char *,
	                                 const char *,
	                                 unsigned long long,
	                                 unsigned);

	Sint64 res = 0;
	if (ops->Write && fmt->DumpMsg)
		res = fmt->DumpMsg(BGP_HDR(msg), msg->flags, streamp, ops, msg->table);
	else
		_Bgp_SetErrStat(BGPENOERR, NULL, NULL, 0, 0);

	return res;
}

Sint64 Bgp_DumpMrtUpdate(const Mrthdr *hdr,
                         void *streamp, const StmOps *ops,
                         const BgpDumpfmt *fmt);

Sint64 Bgp_DumpMrtRibv2(const Mrthdr *hdr,
                        const Mrtpeerentv2 *peer, const Mrtribentv2 *ent,
                        void *streamp, const StmOps *ops,
                        const BgpDumpfmt *fmt);

Sint64 Bgp_DumpMrtRib(const Mrthdr *hdr,
                      const Mrtribent *ent,
                      void *streamp, const StmOps *ops,
                      const BgpDumpfmt *fmt);

#endif
