// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/bgp_local.h
 *
 * Private BGP library header.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGP_LOCAL_H_
#define DF_BGP_LOCAL_H_

#include "bgp/mrt.h"

// Low level prefix operations

void Bgp_InitMpWithdrawn(Bgpmpiter *it, const Bgpwithdrawnseg *withdrawn, const Bgpattr *mpUnreach, Boolean isAddPath);
void Bgp_InitMpNlri(Bgpmpiter *it, const void *data, size_t nbytes, const Bgpattr *mpReach, Boolean isAddPath);

// Low level BGP operations

Uint16 Bgp_CheckMsgHdr(const void *data, size_t nbytes, Boolean allowExtendedSize);

Bgpparmseg *Bgp_GetParmsFromMemory(const void *data, size_t size);
Bgpwithdrawnseg *Bgp_GetWithdrawnFromMemory(const void *data, size_t size);
Bgpattrseg *Bgp_GetAttributesFromMemory(const void *data, size_t size);
void *Bgp_GetNlriFromMemory(const void *nlri, size_t size, size_t *nbytes);

// Extension in attribute.c special iteration on attributes

/// Non-caching variant of `Bgp_NextAttribute()`, doesn't update `it->table`.
Bgpattr *Bgp_NcNextAttribute(Bgpattriter *it);

#define Bgp_SetErrStat(code) \
	_Bgp_SetErrStat(code, __FILE__, __func__, __LINE__, 0)

NOINLINE Judgement _Bgp_SetErrStat(BgpRet             code,
                                   const char        *filename,
                                   const char        *func,
                                   unsigned long long line,
                                   unsigned           depth);

#endif
