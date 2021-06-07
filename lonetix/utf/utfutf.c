// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file utf/utfutf.c
 *
 * Implements `utfutf()`.
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

char *utfutf(const char *s1, const char *s2)
{
	Rune   f;
	size_t n1 = chartorune(&f, s2);
	if (f <= RUNE_SYNC)  // represents self
		return strstr(s1, s2);

	size_t n2 = strlen(s2);
	for (char *p = (char *) s1; (p = utfrune(p, f)) != NULL; p += n1) {
		if (strncmp(p, s2, n2) == 0)
			return p;
	}
	return NULL;
}
