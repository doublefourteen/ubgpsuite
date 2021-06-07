// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file utf/rune.c
 *
 * Implements `char`s to `Rune` conversion functions and back.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

/*
 * This work is derived from Plan 9 libutf, see utf/utf.h for details.
 * Original license terms follow:
 *
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE
 * ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * The original libutf library is available at: https://9fans.github.io/plan9port/unix/libutf.tgz
 */

#include "utf/utf.h"

#define BIT1 7
#define BITX 6
#define BIT2 5
#define BIT3 4
#define BIT4 3
#define BIT5 2

#define T1 (((1u << (BIT1+1)) - 1u) ^ 0xffu)  /* 0000 0000 */
#define TX (((1u << (BITX+1)) - 1u) ^ 0xffu)  /* 1000 0000 */
#define T2 (((1u << (BIT2+1)) - 1u) ^ 0xffu)  /* 1100 0000 */
#define T3 (((1u << (BIT3+1)) - 1u) ^ 0xffu)  /* 1110 0000 */
#define T4 (((1u << (BIT4+1)) - 1u) ^ 0xffu)  /* 1111 0000 */
#define T5 (((1u << (BIT5+1)) - 1u) ^ 0xffu)  /* 1111 1000 */

#define RUNE1 ((U32_C(1) << (BIT1 + 0*BITX)) - 1u)  /* 0000 0000 0000 0000 0111 1111 */
#define RUNE2 ((U32_C(1) << (BIT2 + 1*BITX)) - 1u)  /* 0000 0000 0000 0111 1111 1111 */
#define RUNE3 ((U32_C(1) << (BIT3 + 2*BITX)) - 1u)  /* 0000 0000 1111 1111 1111 1111 */
#define RUNE4 ((U32_C(1) << (BIT4 + 3*BITX)) - 1u)  /* 0011 1111 1111 1111 1111 1111 */

#define MASKX ((1u << BITX) - 1u)             /* 0011 1111 */
#define TESTX (MASKX ^ 0xFFu)                 /* 1100 0000 */

size_t chartorune(Rune *dest, const char *str)
{
	// One character sequence
	// 00000-0007F => T1
	Rune c = *(const Uint8 *) str;
	if (c < TX) {
		*dest = c;
		return 1;
	}

	// Two character sequence
	// 0080-07FF => T2 Tx
	Rune c1 = *(const Uint8 *) (str+1) ^ TX;
	if (c1 & TESTX)
		goto bad;

	if (c < T3) {
		if (c < T2)
			goto bad;

		Rune r = ((c << BITX) | c1) & RUNE2;
		if (r <= RUNE1)
			goto bad;

		*dest = r;
		return 2;
	}

	// Three character sequence
	// 0800-FFFF => T3 Tx Tx
	Rune c2 = *(const Uint8 *) (str+2) ^ TX;
	if (c2 & TESTX)
		goto bad;

	if (c < T4) {
		Rune r = ((((c << BITX) | c1) << BITX) | c2) & RUNE3;
		if (r <= RUNE2)
			goto bad;

		*dest = r;
		return 3;
	}

	// Four character sequence
	// 10000-10FFFF => T4 Tx Tx Tx
	Rune c3 = *(const Uint8*) (str+3) ^ TX;
	if (c3 & TESTX)
		goto bad;

	if (c < T5) {
		Rune r = ((((((c << BITX) | c1) << BITX) | c2) << BITX) | c3) & RUNE4;
		if (r <= RUNE3)
			goto bad;
		if (r > MAXRUNE)
			goto bad;

		*dest = r;
		return 4;
	}

	// Decoding error
bad:
	*dest = RUNE_ERR;
	return 1;
}

size_t runetochar(char *dest, Rune r)
{
	// One character sequence
	// 00000-0007F => 00-7F
	if (r <= RUNE1) {
		dest[0] = r;
		return 1;
	}

	// Two character sequence
	// 00080-007FF => T2 Tx
	if (r <= RUNE2) {
		dest[0] = T2 | (r >> 1*BITX);
		dest[1] = TX | (r & MASKX);
		return 2;
	}

	// Three character sequence
	// 00800-0FFFF => T3 Tx Tx
	if (r > MAXRUNE)
		r = RUNE_ERR;
	if (r <= RUNE3) {
		dest[0] = T3 |  (r >> 2*BITX);
		dest[1] = TX | ((r >> 1*BITX) & MASKX);
		dest[2] = TX |  (r & MASKX);
		return 3;
	}

	// Four character sequence
	// 010000-1FFFFF => T4 Tx Tx Tx
	dest[0] = T4 |  (r >> 3*BITX);
	dest[1] = TX | ((r >> 2*BITX) & MASKX);
	dest[2] = TX | ((r >> 1*BITX) & MASKX);
	dest[3] = TX |  (r & MASKX);
	return 4;
}

size_t runelen(Rune r)
{
	return 1 + (r > RUNE1) + (r > RUNE2) + (r > RUNE3 && r <= MAXRUNE);
}

size_t runenlen(const Rune *r, size_t n)
{
	size_t nb = 0;

	while (n--)
		nb += runelen(*r++);

	return nb;
}

Boolean fullrune(const char *str, size_t n)
{
	if (n == 0)
		return FALSE;

	Rune c = *(const Uint8 *) str;
	if (c < TX)
		return TRUE;
	if (c < T3)
		return n >= 2;
	if (c < T4)
		return n >= 3;

	return n >= 4;
}
