// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file utf/utfrrune.c
 *
 * Implements `utfrrune()`.
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

#include <string.h>

char *utfrrune(const char *s, Rune c)
{
	if (c < RUNE_SYNC)
		return strrchr(s, c);

	char *s1 = NULL;
	while (TRUE) {
		Rune c1 = *(const Uint8 *) s;
		if (c1 < RUNE_SELF) {  // one byte rune
			if (c1 == '\0')
				break;

			if (RUNE_SYNC != RUNE_SELF && c1 == c)  // exposition only, optimized away, see topmost if ()
				s1 = (char *) s;

			s++;
			continue;
		}

		Rune   r;
		size_t n = chartorune(&r, s);
		if (r == c)
			s1 = (char *) s;

		s += n;
	}
	return s1;
}
