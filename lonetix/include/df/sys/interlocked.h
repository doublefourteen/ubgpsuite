// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file      sys/interlocked.h
 *
 * Lock-free atomic operations on integers and pointers.
 *
 * \copyright The DoubleFourteen Code Forge (c) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_INTERLOCKED_H_
#define DF_SYS_INTERLOCKED_H_

#include "xpt.h"

/**
 * \fn void Smp_AtomicStore(long *p, long v)
 * \fn void Smp_AtomicStoreRx(long *p, long v)
 * \fn void Smp_AtomicStoreRel(long *p, long v)
 * \fn void Smp_AtomicStorePtr(void **p, void *v)
 * \fn void Smp_AtomicStorePtrRx(void **p, void *v)
 * \fn void Smp_AtomicStorePtrRel(void **p, void *v)
 * \fn void Smp_AtomicStore8(Sint8 *p, Sint8 v)
 * \fn void Smp_AtomicStore8Rx(Sint8 *p, Sint8 v)
 * \fn void Smp_AtomicStore8Rel(Sint8 *p, Sint8 v)
 * \fn void Smp_AtomicStore16(Sint16 *p, Sint16 v)
 * \fn void Smp_AtomicStore16Rx(Sint16 *p, Sint16 v)
 * \fn void Smp_AtomicStore16Rel(Sint16 *p, Sint16 v)
 * \fn void Smp_AtomicStore32(Sint32 *p, Sint32 v)
 * \fn void Smp_AtomicStore32Rx(Sint32 *p, Sint32 v)
 * \fn void Smp_AtomicStore32Rel(Sint32 *p, Sint32 v)
 * \fn void Smp_AtomicStore64(Sint64 *p, Sint64 v)
 * \fn void Smp_AtomicStore64Rx(Sint64 *p, Sint64 v)
 * \fn void Smp_AtomicStore64Rel(Sint64 *p, Sint64 v)
 *
 * Atomic store operation to a variable or pointer.
 *
 * \note Only a subset of the fixed-size variants may be available on
 *       specific architectures.
 */

#ifdef _MSC_VER

#include "interlocked_intrin_msvc.h"
#include "interlocked_ops_msvc.h"

#else

#if __GCC_ATOMIC_LONG_LOCK_FREE != 2
#error "interlocked.h requires lock-free atomics on long!"
#endif
#if __GCC_ATOMIC_POINTER_LOCK_FREE != 2
#error "interlocked.h requires lock-free atomics on void *!"
#endif

/******************************************************************************
 *                       ATOMICS ON FIXED SIZE INTEGERS                       *
 ******************************************************************************/

#if __GCC_ATOMIC_CHAR_LOCK_FREE == 2

#define INTERLOCKED_INT8

#define INTERLOCKED_TYPE Sint8
#define INTERLOCKED_SUFFIX 8
#include "interlocked_ops_gcc.h"
#undef INTERLOCKED_TYPE
#undef INTERLOCKED_SUFFIX

#endif /* INTERLOCKED_INT8 */

#if (__SIZEOF_SHORT__ == 2 && __GCC_ATOMIC_SHORT_LOCK_FREE == 2) ||  \
    (__SIZEOF_INT__ == 2 && __GCC_ATOMIC_INT_LOCK_FREE == 2)

#define INTERLOCKED_INT16

#define INTERLOCKED_TYPE Sint16
#define INTERLOCKED_SUFFIX 16
#include "interlocked_ops_gcc.h"
#undef INTERLOCKED_TYPE
#undef INTERLOCKED_SUFFIX

#endif  /* INTERLOCKED_INT16 */

#if (__SIZEOF_INT__ == 4 && __GCC_ATOMIC_INT_LOCK_FREE == 2) ||  \
    (__SIZEOF_LONG__ == 4)

#define INTERLOCKED_INT32

#define INTERLOCKED_TYPE Sint32
#define INTERLOCKED_SUFFIX 32
#include "interlocked_ops_gcc.h"
#undef INTERLOCKED_TYPE
#undef INTERLOCKED_SUFFIX

#endif /* INTERLOCKED_INT32 */

#if (__SIZEOF_LONG_LONG__ == 8 && __GCC_ATOMIC_LONG_LONG_LOCK_FREE == 2) ||  \
    (__SIZEOF_LONG__ == 8)

#define INTERLOCKED_INT64

#define INTERLOCKED_TYPE Sint64
#define INTERLOCKED_SUFFIX 64
#include "interlocked_ops_gcc.h"
#undef INTERLOCKED_TYPE
#undef INTERLOCKED_SUFFIX

#endif /* INTERLOCKED_INT64 */

/******************************************************************************
 *                         ATOMICS ON LONG INTEGERS                           *
 ******************************************************************************/

#define INTERLOCKED_TYPE long
#define INTERLOCKED_SUFFIX
#include "interlocked_ops_gcc.h"
#undef INTERLOCKED_TYPE
#undef INTERLOCKED_SUFFIX

/******************************************************************************
 *                            ATOMICS ON POINTERS                             *
 ******************************************************************************/

#define INTERLOCKED_TYPE void *
#define INTERLOCKED_SUFFIX Ptr
#define INTERLOCKED_NO_ARIT
#include "interlocked_ops_gcc.h"
#undef INTERLOCKED_TYPE
#undef INTERLOCKED_SUFFIX
#undef INTERLOCKED_NO_ARIT

#endif

#endif
