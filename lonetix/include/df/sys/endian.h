// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/endian.h
 *
 * Architecture specific byteswap utilities.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_ENDIAN_H_
#define DF_SYS_ENDIAN_H_

#include "xpt.h"

#ifdef _MSC_VER
#include <stdlib.h>  // _byteswap_*()
#endif

/// Helper `union` to access bit representation of `float`.
typedef union {
	float  f32;
	Uint32 bits;
} Floatbits;

/// Helper `union` to access bit representation of `double`.
typedef union {
	double f64;
	Uint64 bits;
} Doublebits;

STATIC_ASSERT(sizeof(Floatbits)  == sizeof(Uint32), "float vs Uint32 size mismatch");
STATIC_ASSERT(sizeof(Doublebits) == sizeof(Uint64), "double vs Uint64 size mismatch");

/// Swap bytes inside 16-bits word.
FORCE_INLINE Uint16 bswap16(Uint16 x)
{
#ifdef _MSC_VER
	return _byteswap_ushort(x);
#elif defined(__GNUC__)
	return __builtin_bswap16(x);
#else
	return BSWAP16(x);
#endif
}

/// Swap bytes inside 32-bits dword.
FORCE_INLINE Uint32 bswap32(Uint32 x)
{
#ifdef _MSC_VER
	return _byteswap_ulong(x);
#elif defined(__GNUC__)
	return __builtin_bswap32(x);
#else
	return BSWAP32(x);
#endif
}

/// Swap bytes inside 64-bits quadword.
FORCE_INLINE Uint64 bswap64(Uint64 x)
{
#ifdef _MSC_VER
	return _byteswap_uint64(x);
#elif defined(__GNUC__)
	return __builtin_bswap64(x);
#else
	return BSWAP64(x);
#endif
}

/// `bswap16()` if target isn't little-endian.
FORCE_INLINE Uint16 leswap16(Uint16 x)
{
#if EDN_NATIVE == EDN_LE
	return x;
#else
	return bswap16(x);
#endif
}

/// `bswap16()` if target isn't big-endian.
FORCE_INLINE Uint16 beswap16(Uint16 x)
{
#if EDN_NATIVE == EDN_BE
	return x;
#else
	return bswap16(x);
#endif
}

/// `bswap32()` if target isn't little-endian.
FORCE_INLINE Uint32 leswap32(Uint32 x)
{
#if EDN_NATIVE == EDN_LE
	return x;
#else
	return bswap32(x);
#endif
}

/// `bswap32()` if target isn't big-endian.
FORCE_INLINE Uint32 beswap32(Uint32 x)
{
#if EDN_NATIVE == EDN_BE
	return x;
#else
	return bswap32(x);
#endif
}

/// `bswap64()` if target isn't little-endian.
FORCE_INLINE Uint64 leswap64(Uint64 x)
{
#if EDN_NATIVE == EDN_LE
	return x;
#else
	return bswap64(x);
#endif
}

/// `bswap64()` if target isn't big-endian.
FORCE_INLINE Uint64 beswap64(Uint64 x)
{
#if EDN_NATIVE == EDN_BE
	return x;
#else
	return bswap64(x);
#endif
}

#endif
