// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file utf/utflen.c
 *
 * Implements `utflen()`.
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

size_t utflen(const char *s)
{
	size_t n = 0;

	while (TRUE) {
		Rune r = *(const Uint8 *) s;

		if (r < RUNE_SELF) {
			if (r == '\0')
				break;

			s++;
		} else {
			s += chartorune(&r, s);
		}

		n++;
	}

	return n;
}
