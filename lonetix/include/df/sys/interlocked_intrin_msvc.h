// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/interlocked_intrin_msvc.h
 *
 * MSVC-specific intrinsics for interlocked operations.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_INTERLOCKED_H_
#error "use interlocked.h, do not include interlocked_intrin_msvc.h directly"
#endif

#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedExchange)

#pragma intrinsic(_InterlockedCompareExchangePointer)
#pragma intrinsic(_InterlockedExchangePointer)

#pragma intrinsic(_InterlockedCompareExchange8)
#pragma intrinsic(_InterlockedExchangeAdd8)
#pragma intrinsic(_InterlockedExchange8)

#pragma intrinsic(_InterlockedCompareExchange16)
#pragma intrinsic(_InterlockedExchangeAdd16)
#pragma intrinsic(_InterlockedExchange16)
#pragma intrinsic(_InterlockedAnd16)
#pragma intrinsic(_InterlockedOr16)
#pragma intrinsic(_InterlockedXor16)

#if (defined(_M_IX86) && _M_IX86 >= 500) || defined(_M_AMD64) || defined(_M_IA64) || defined(_M_ARM)
#pragma intrinsic(_InterlockedCompareExchange64)
#pragma intrinsic(_InterlockedExchangeAdd64)
#pragma intrinsic(_InterlockedExchange64)
#endif

#ifdef _M_ARM

#pragma intrinsic(_InterlockedCompareExchange_nf)
#pragma intrinsic(_InterlockedCompareExchange_acq)
#pragma intrinsic(_InterlockedCompareExchange_rel)
#pragma intrinsic(_InterlockedCompareExchangePointer_nf)
#pragma intrinsic(_InterlockedCompareExchangePointer_acq)
#pragma intrinsic(_InterlockedCompareExchangePointer_rel)
#pragma intrinsic(_InterlockedCompareExchange8_nf)
#pragma intrinsic(_InterlockedCompareExchange8_acq)
#pragma intrinsic(_InterlockedCompareExchange8_rel)
#pragma intrinsic(_InterlockedCompareExchange16_nf)
#pragma intrinsic(_InterlockedCompareExchange16_acq)
#pragma intrinsic(_InterlockedCompareExchange16_rel)
#pragma intrinsic(_InterlockedCompareExchange64_nf)
#pragma intrinsic(_InterlockedCompareExchange64_acq)
#pragma intrinsic(_InterlockedCompareExchange64_rel)

#endif  /* _M_ARM */

#include <intrin.h>

#error "Sorry, not implemented yet"

