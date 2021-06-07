// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file strnicmp.c
 *
 * Implements `Df_strnicmp()`.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "strlib.h"

int Df_strnicmp(const char *a, const char *b, size_t n)
{
	int c1, c2;

	do {
		if (!n--)
			return 0;  // strings are equal until end point

		c1 = *a++;
		c2 = *b++;

		int d = c1 - c2;
		while (d != 0) {
			if (c1 <= 'Z' && c1 >= 'A') {
				d += ('a' - 'A');
				if (d == 0)
					break;
			}
			if (c2 <= 'Z' && c2 >= 'A') {
				d -= ('a' - 'A');
				if (d == 0)
					break;
			}
			return ((d >= 0) << 1) - 1;
		}
	} while (c1 != '\0');

	return 0;  // strings are equal
}
