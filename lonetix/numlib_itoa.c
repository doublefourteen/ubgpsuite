// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file numlib_itoa.c
 *
 * Integer to ASCII conversion.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "numlib.h"

#include <string.h>

// XXX(micia): benchmark memcpy()-less version, something with rep stos
//             amount of data copied over is very small, so it would probably pay off
//             leaving memcpy() out

char *Utoa(unsigned long long x, char *dest)
{
	char buf[DIGS(x) + 1];
	size_t n;

	char *p = buf + sizeof(buf);

	*--p = '\0';
	do {
		*--p  = '0' + (x % 10);
		x    /= 10;
	} while (x != 0);

	n = (buf + sizeof(buf)) - p;
	memcpy(dest, p, n);
	return dest + n - 1;
}

char *Itoa(long long x, char *dest)
{
	char buf[1 + DIGS(x) + 1];
	unsigned long long ax;  // NOTE keep this unsigned!
	size_t n;

	char *p = buf + sizeof(buf);

	ax = ABS(x);  // also handles LLONG_MIN, ax being unsigned
	              // NOTE: this is still a signed overflow, which is technically
	              //       undefined in standard C

	*--p = '\0';
	do {
		*--p  = '0' + (ax % 10);
		ax   /= 10;
	} while (ax != 0);

	if (x < 0)
		*--p = '-';

	n = (buf + sizeof(buf)) - p;
	memcpy(dest, p, n);
	return dest + n - 1;
}

char *Xtoa(unsigned long long x, char *dest)
{
	char buf[XDIGS(x) + 1];
	size_t n;

	char *p = buf + sizeof(buf);

	*--p = '\0';
	do {
		*--p   = "0123456789abcdef"[x & 0xf];
		x    >>= 4;
	} while (x != 0);

	n = (buf + sizeof(buf)) - p;
	memcpy(dest, p, n);
	return dest + n - 1;
}
