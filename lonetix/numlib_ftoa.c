// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file numlib_ftoa.c
 *
 * Float to ASCII conversion.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * This code is based on Florian Loitsch paper
 * **Printing Floating-Point Numbers Quickly and Accurately with Integers**,
 * algorithm used is Grisu2.
 *
 * \see [Printing Floating-Point Numbers Quickly and Accurately with Integers](https://www.cs.tufts.edu/~nr/cs257/archive/florian-loitsch/printf.pdf)
 */

#include "numlib.h"

#include "sys/endian.h"
#include "numlib_fltp.h"

#include <string.h>

#define fracmask  0x000fffffffffffffuLL
#define expmask   0x7ff0000000000000uLL
#define hiddenbit 0x0010000000000000uLL
#define signmask  0x8000000000000000uLL
#define expbias   (1023 + 52)

static const Uint64 tens[] = {
	10000000000000000000uLL, 1000000000000000000uLL, 100000000000000000uLL,
	10000000000000000uLL, 1000000000000000uLL, 100000000000000uLL,
	10000000000000uLL, 1000000000000uLL, 100000000000uLL,
	10000000000uLL, 1000000000uLL, 100000000uLL,
	10000000uLL, 1000000uLL, 100000uLL,
	10000uLL, 1000uLL, 100uLL,
	10uLL, 1uLL
};

static Uint64 get_dbits(double d)
{
	Doublebits dbl;
	dbl.f64 = d;
	return dbl.bits;
}

static Fp build_fp(double d)
{
	Uint64 bits = get_dbits(d);

	Fp fp;
	fp.frac = bits & fracmask;
	fp.exp = (bits & expmask) >> 52;

	if(fp.exp) {
		fp.frac += hiddenbit;
		fp.exp -= expbias;
	} else {
		fp.exp = -expbias + 1;
	}
	return fp;
}

static void normalize(Fp *fp)
{
	while ((fp->frac & hiddenbit) == 0) {
		fp->frac <<= 1;
		fp->exp--;
	}

	int shift = 64 - 52 - 1;
	fp->frac <<= shift;
	fp->exp -= shift;
}

static void get_normalized_boundaries(Fp *fp, Fp *lower, Fp *upper)
{
	upper->frac = (fp->frac << 1) + 1;
	upper->exp  = fp->exp - 1;

	while ((upper->frac & (hiddenbit << 1)) == 0) {
		upper->frac <<= 1;
		upper->exp--;
	}

	int u_shift = 64 - 52 - 2;

	upper->frac <<= u_shift;
	upper->exp = upper->exp - u_shift;


	int l_shift = fp->frac == hiddenbit ? 2 : 1;

	lower->frac = (fp->frac << l_shift) - 1;
	lower->exp = fp->exp - l_shift;


	lower->frac <<= lower->exp - upper->exp;
	lower->exp = upper->exp;
}

static Fp multiply(Fp *a, Fp *b)
{
	const Uint64 lomask = 0x00000000FFFFFFFFuLL;

	Uint64 ah_bl = (a->frac >> 32)    * (b->frac & lomask);
	Uint64 al_bh = (a->frac & lomask) * (b->frac >> 32);
	Uint64 al_bl = (a->frac & lomask) * (b->frac & lomask);
	Uint64 ah_bh = (a->frac >> 32)    * (b->frac >> 32);

	Uint64 tmp = (ah_bl & lomask) + (al_bh & lomask) + (al_bl >> 32); 
	// round up
	tmp += 1U << 31;

	Fp fp = {
		ah_bh + (ah_bl >> 32) + (al_bh >> 32) + (tmp >> 32),
		a->exp + b->exp + 64
	};

	return fp;
}

static void round_digit(char  *digits,
                        int    ndigits,
                        Uint64 delta,
                        Uint64 rem,
                        Uint64 kappa,
                        Uint64 frac)
{
	while ((rem < frac && delta - rem >= kappa) &&
	       (rem + kappa < frac || frac - rem > rem + kappa - frac)) {
		digits[ndigits - 1]--;
		rem += kappa;
	}
}

static int generate_digits(Fp* fp, Fp *upper, Fp* lower, char *digits, int *K)
{
	Uint64 wfrac = upper->frac - fp->frac;
	Uint64 delta = upper->frac - lower->frac;

	Fp one;
	one.frac = 1uLL << -upper->exp;
	one.exp  = upper->exp;

	Uint64 part1 = upper->frac >> -one.exp;
	Uint64 part2 = upper->frac & (one.frac - 1);

	int idx = 0, kappa = 10;

	// 1000000000
	for (const Uint64 *divp = tens + 10; kappa > 0; divp++) {
		Uint64   div   = *divp;
		unsigned digit = part1 / div;

		if (digit || idx)
			digits[idx++] = digit + '0';

		part1 -= digit * div;
		kappa--;

		Uint64 tmp = (part1 <<-one.exp) + part2;
		if (tmp <= delta) {
			*K += kappa;
			round_digit(digits, idx, delta, tmp, div << -one.exp, wfrac);

			return idx;
		}
	}

	// 10
	const Uint64 *unit = tens + 18;
	while (TRUE) {
		part2 *= 10;
		delta *= 10;
		kappa--;

		unsigned digit = part2 >> -one.exp;
		if (digit || idx)
			digits[idx++] = digit + '0';

		part2 &= one.frac - 1;
		if (part2 < delta) {
			*K += kappa;
			round_digit(digits, idx, delta, part2, one.frac, wfrac * *unit);
			break;
		}

		unit--;
	}

	return idx;
}

static int grisu2(double d, char *digits, int *K)
{
	Fp w = build_fp(d);

	Fp lower, upper;
	get_normalized_boundaries(&w, &lower, &upper);

	normalize(&w);

	int k;
	Fp cp = find_cachedpow10(upper.exp, &k);

	w     = multiply(&w,     &cp);
	upper = multiply(&upper, &cp);
	lower = multiply(&lower, &cp);

	lower.frac++;
	upper.frac--;

	*K = -k;

	return generate_digits(&w, &upper, &lower, digits, K);
}

static int emit_digits(char   *digits,
                       int     ndigits,
                       char   *dest,
                       int     K,
                       Boolean neg)
{
	int exp = ABS(K + ndigits - 1);

	// Write plain integer
	if (K >= 0 && (exp < (ndigits + 7))) {
		memcpy(dest, digits, ndigits);
		memset(dest + ndigits, '0', K);

		return ndigits + K;
	}

	// Write decimal w/o scientific notation
	if (K < 0 && (K > -7 || exp < 4)) {
		int offset = ndigits - ABS(K);
		if (offset <= 0) {
			// fp < 1.0 -> write leading zero
			offset = -offset;
			dest[0] = '0';
			dest[1] = '.';
			memset(dest + 2, '0', offset);
			memcpy(dest + offset + 2, digits, ndigits);

			return ndigits + 2 + offset;

		} else {
			// fp > 1.0
			memcpy(dest, digits, offset);
			dest[offset] = '.';
			memcpy(dest + offset + 1, digits + offset, ndigits - offset);

			return ndigits + 1;
		}
	}

	// write decimal w/ scientific notation
	ndigits = MIN(ndigits, 18 - neg);

	int idx = 0;
	dest[idx++] = digits[0];

	if (ndigits > 1) {
		dest[idx++] = '.';
		memcpy(dest + idx, digits + 1, ndigits - 1);
		idx += ndigits - 1;
	}

	dest[idx++] = 'e';

	char sign = K + ndigits - 1 < 0 ? '-' : '+';
	dest[idx++] = sign;

	int cent = 0;

	if (exp > 99) {
		cent = exp / 100;
		dest[idx++] = cent + '0';
		exp -= cent * 100;
	}
	if (exp > 9) {
		int dec = exp / 10;
		dest[idx++] = dec + '0';
		exp -= dec * 10;
	} else if (cent) {
		dest[idx++] = '0';
	}

	dest[idx++] = exp % 10 + '0';
	return idx;
}

static int filter_special(double fp, char *dest)
{
	if (fp == 0.0) {
		dest[0] = '0';
		return 1;
	}

	Uint64 bits = get_dbits(fp);

	Boolean nan = (bits & expmask) == expmask;
	if (!nan)
		return 0;

	if (bits & fracmask) {
		dest[0] = 'n'; dest[1] = 'a'; dest[2] = 'n';
	} else {
		dest[0] = 'i'; dest[1] = 'n'; dest[2] = 'f';
	}

	return 3;
}

char *Ftoa(double d, char *dest)
{
	char digits[18];

	int str_len = 0;
	Boolean neg = FALSE;

	if (get_dbits(d) & signmask) {
		dest[0] = '-';
		str_len++;
		neg = TRUE;
	}

	int spec = filter_special(d, dest + str_len);

	str_len += spec;
	if (spec == 0) {
		int K = 0;
		int ndigits = grisu2(d, digits, &K);

		str_len += emit_digits(digits, ndigits, dest + str_len, K, neg);
	}

	dest[str_len] = '\0';
	return dest + str_len;
}

