// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/interlocked_ops_gcc.h
 *
 * Generates interlocked functions from GCC intrinsics, based on some macros.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * This file should be `#include`d from `interlocked.h`, on GNUC compilers.
 * It generated interlocked primitives based on `INTERLOCKED_TYPE`,
 * `INTERLOCKED_SUFFIX` macros.
 * `#define`ing `INTERLOCKED_NO_ARIT` disables arithmetic functions
 * generation.
 * File may be `#include`d multiple times.
 */

#ifndef DF_SYS_INTERLOCKED_H_
#error "Use interlocked.h, do not include interlocked_gcc_ops.h directly"
#endif

#ifndef INTERLOCKED_TYPE
#error "Please define INTERLOCKED_TYPE for atomic operation target type"
#endif
#ifndef INTERLOCKED_SUFFIX
#error "Please define INTERLOCKED_SUFFIX for atomic operation suffix"
#endif

#define _PASTE(a, b) a ## b
#define _XPASTE(a, b) _PASTE(a, b)
#define _FNAME(wk, name, memory_order)  \
	_XPASTE(_XPASTE(Smp_ ## wk ## Atomic ## name, INTERLOCKED_SUFFIX), memory_order)
#define _FOP(op, memory_order, ...)  \
	__atomic_ ## op (__VA_ARGS__, __ATOMIC_ ## memory_order)
#define _FCAS(wkflag, succ_memory_order, fail_memory_order, ...)  \
	__atomic_compare_exchange_n(__VA_ARGS__, wkflag, __ATOMIC_ ## succ_memory_order, __ATOMIC_ ## fail_memory_order)

FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Load,) (INTERLOCKED_TYPE *_p)
{
	return _FOP(load_n, SEQ_CST, _p);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Load,Rx) (INTERLOCKED_TYPE *_p)
{
	return _FOP(load_n, RELAXED, _p);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Load,Acq) (INTERLOCKED_TYPE *_p)
{
	return _FOP(load_n, ACQUIRE, _p);
}

FORCE_INLINE void _FNAME(,Store,) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	_FOP(store_n, SEQ_CST, _p, _v);
}
FORCE_INLINE void _FNAME(,Store,Rx) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	_FOP(store_n, RELAXED, _p, _v);
}
FORCE_INLINE void _FNAME(,Store,Rel) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	_FOP(store_n, RELEASE, _p, _v);
}

#ifndef INTERLOCKED_NO_ARIT

FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Add,) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(add_fetch, ACQ_REL, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Add,Rx) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(add_fetch, RELAXED, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Add,Acq) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(add_fetch, ACQUIRE, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Add,Rel) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(add_fetch, RELEASE, _p, _v);
}

FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Sub,) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(sub_fetch, ACQ_REL, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Sub,Rx) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(sub_fetch, RELAXED, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Sub,Acq) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(sub_fetch, ACQUIRE, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Sub,Rel) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(sub_fetch, RELEASE, _p, _v);
}

FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Incr,) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_add, ACQ_REL, _p, 1);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Incr,Rx) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_add, RELAXED, _p, 1);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Incr,Acq) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_add, ACQUIRE, _p, 1);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Incr,Rel) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_add, RELEASE, _p, 1);
}

FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Decr,) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_sub, ACQ_REL, _p, 1);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Decr,Rx) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_sub, RELAXED, _p, 1);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Decr,Acq) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_sub, ACQUIRE, _p, 1);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Decr,Rel) (INTERLOCKED_TYPE *_p)
{
	return _FOP(fetch_sub, RELEASE, _p, 1);
}

#endif /* INTERLOCKED_NO_ARIT */

FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Xchng,) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(exchange_n, ACQ_REL, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Xchng,Rx) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(exchange_n, RELAXED, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Xchng,Acq) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(exchange_n, ACQUIRE, _p, _v);
}
FORCE_INLINE INTERLOCKED_TYPE _FNAME(,Xchng,Rel) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _v)
{
	return _FOP(exchange_n, RELEASE, _p, _v);
}

FORCE_INLINE Boolean _FNAME(,Cas,) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(FALSE, ACQ_REL, ACQUIRE, _p, &_e, _v);
}
FORCE_INLINE Boolean _FNAME(,Cas,Rx) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(FALSE, RELAXED, RELAXED, _p, &_e, _v);
}
FORCE_INLINE Boolean _FNAME(,Cas,Acq) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(FALSE, ACQUIRE, ACQUIRE, _p, &_e, _v);
}
FORCE_INLINE Boolean _FNAME(,Cas,Rel) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(FALSE, RELEASE, RELAXED, _p, &_e, _v);
}
FORCE_INLINE Boolean _FNAME(Wk,Cas,) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(TRUE, ACQ_REL, ACQUIRE, _p, &_e, _v);
}
FORCE_INLINE Boolean _FNAME(Wk,Cas,Rx) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(TRUE, RELAXED, RELAXED, _p, &_e, _v);
}
FORCE_INLINE Boolean _FNAME(Wk,Cas,Acq) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(TRUE, ACQUIRE, ACQUIRE, _p, &_e, _v);
}
FORCE_INLINE Boolean _FNAME(Wk,Xchng,Rel) (INTERLOCKED_TYPE *_p, INTERLOCKED_TYPE _e, INTERLOCKED_TYPE _v)
{
	return _FCAS(TRUE, RELEASE, RELAXED, _p, &_e, _v);
}

#undef _PASTE
#undef _XPASTE
#undef _FNAME
#undef _FOP
#undef _FCAS
