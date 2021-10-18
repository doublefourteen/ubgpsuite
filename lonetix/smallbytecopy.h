// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file smallbytecopy.h
 *
 * Optimized routines for tiny buffer copy (<= 64 bytes).
 *
 * Whenever possible, avoid these routines and use regular
 * memcpy()/memmove().
 * These routines are recommended only when:
 * - you are absolutely sure of the maximum size of your data.
 * - the compiler cannot possibly estimate it statically,
 *   otherwise the compiler could do a much better job at
 *   optimizing the copy.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SMALLBYTECOPY_H_
#define DF_SMALLBYTECOPY_H_

#include "xpt.h"

#if defined(__i386__) || defined(__x86_64__)
#include <assert.h>

// Optimize copy and don't pay attention to alignment,
// ugly but fast and inline-able compared to plain memcpy()...

#define _bytecopy1(d, s)  ((void) (*(Uint8 *)  (d) = *(Uint8 *)  (s)))
#define _bytecopy2(d, s)  ((void) (*(Uint16 *) (d) = *(Uint16 *) (s)))
#define _bytecopy4(d, s)  ((void) (*(Uint32 *) (d) = *(Uint32 *) (s)))
#define _bytecopy8(d, s)  ((void) (*(Uint64 *) (d) = *(Uint64 *) (s)))

INLINE void _smallbytecopy4(void *__restrict dest, const void *__restrict src, size_t n)
{
	Uint8 *d = (Uint8 *) dest, *s = (Uint8 *) src;

	assert(n <= 4);

	switch (n) {
	case 4:  _bytecopy4(d, s);
	         break;
	case 3:  _bytecopy2(d + 1, s + 1);
	         /*FALLTHROUGH*/
	case 1:  _bytecopy1(d, s);
	         break;
	case 2:  _bytecopy2(d, s);
	         break;
	case 0:  break;
	default: UNREACHABLE; break;
	}
}

INLINE void _smallbytecopy8(void *__restrict dest, const void *__restrict src, size_t n)
{
	Uint8 *d = (Uint8 *) dest, *s = (Uint8 *) src;

	assert(n <= 8);

	switch (n) {
	case 8:  _bytecopy8(d, s);
	         break;
	case 7:  _bytecopy4(d + 3, s + 3);
	         /*FALLTHROUGH*/
	case 3:  _bytecopy2(d + 1, s + 1);
	         /*FALLTHROUGH*/
	case 1:  _bytecopy1(d, s);
	         break;
	case 6:  _bytecopy4(d + 2, s + 2);
	         /*FALLTHROUGH*/
	case 2:  _bytecopy2(d, s);
	         break;
	case 5:  _bytecopy1(d + 4, s + 4);
	         /*FALLTHROUGH*/
	case 4:  _bytecopy4(d, s);
	         break;
	case 0:  break;
	default: UNREACHABLE; break;
	}
}

INLINE void _smallbytecopy16(void *__restrict dest, const void *__restrict src, size_t n)
{
	Uint8 *d = (Uint8 *) dest, *s = (Uint8 *) src;

	assert(n <= 16);

	if (n > 8) {
		_bytecopy8(d, s);
		d += 8, s += 8, n -= 8;
	}
	_smallbytecopy8(d, s, n);
}

INLINE void _smallbytecopy32(void *__restrict dest, const void *__restrict src, size_t n)
{
	Uint8 *d = (Uint8 *) dest, *s = (Uint8 *) src;

	assert(n <= 32);

	if (n > 16) {
		_bytecopy8(d, s);
		_bytecopy8(d + 8, s + 8);
		d += 16, s += 16, n -= 16;
	}
	_smallbytecopy16(d, s, n);
}

INLINE void _smallbytecopy64(void *__restrict dest, const void *__restrict src, size_t n)
{
	Uint8 *d = (Uint8 *) dest, *s = (Uint8 *) src;

	assert(n <= 64);

	if (n > 32) {
		_bytecopy8(d, s);
		_bytecopy8(d + 8,  s + 8);
		_bytecopy8(d + 16, s + 16);
		_bytecopy8(d + 24, s + 24);
		d += 32, s += 32, n -= 32;
	}
	_smallbytecopy32(d, s, n);
}

#undef _bytecopy1
#undef _bytecopy2
#undef _bytecopy4
#undef _bytecopy8

#else
#include <string.h>

#define _smallbytecopy4(d, s, n)  ((void) memcpy(d, s, n))
#define _smallbytecopy8(d, s, n)  ((void) memcpy(d, s, n))
#define _smallbytecopy16(d, s, n) ((void) memcpy(d, s, n))
#define _smallbytecopy32(d, s, n) ((void) memcpy(d, s, n))
#define _smallbytecopy64(d, s, n) ((void) memcpy(d, s, n))

#endif

#endif
