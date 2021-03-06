// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file strncatz.c
 *
 * Implements `Df_strncatz()`.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

// THIS CODE IS DERIVED FROM OpenBSD strlcat.c
// ORIGINAL LICENSE TERMS FOLLOW:

/* $OpenBSD: strlcat.c,v 1.2 1999/06/17 16:28:58 millert Exp $ */

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// ORIGINAL LICENSE TERMS END ***

#include "strlib.h"

#include <string.h>

size_t Df_strncatz(char *dst, const char *src, size_t n)
{
	char *d = dst;
	const char *s = src;

	size_t left = n;
	size_t dlen;

	// Find the end of dst and adjust bytes left but don't go past end
	while (left-- != 0 && *d != '\0') d++;

	dlen = d - dst;
	left = n - dlen;

	if (left == 0)
		return dlen + strlen(s);

	while (*s != '\0') {
		if (left != 1) {
			*d++ = *s;
			left--;
		}

		s++;
	}

	*d = '\0';

	return dlen + (s - src); // count does not include NUL
}
