// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file stricmp.c
 *
 * Implements `Df_stricmp()`.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "strlib.h"

int Df_stricmp(const char *a, const char *b)
{
	int c1, c2;

	do {
		c1 = *a++;
		c2 = *b++;

		// Avoid converting case unless char differ
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
