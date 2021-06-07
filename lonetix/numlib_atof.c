// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file numlib_atof.c
 *
 * Implements ASCII to float conversion.
 *
 * \copyright The Plan9 Authors
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * Algorithm based on Plan9 strtod(), licensed under LUCENT PUBLIC LICENSE:
 * Copyright (C) 2003, Lucent Technologies Inc. and others. All Rights Reserved.
 *
 * Original source code available at: https://9p.io/sources/plan9/
 */

#include "numlib.h"

#include <math.h>
#include <string.h>

/*
 * This routine will convert to arbitrary precision
 * floating point entirely in multi-precision fixed.
 * The answer is the closest floating point number to
 * the given decimal number. Exactly half way are
 * rounded ala IEEE rules.
 * Method is to scale input decimal between .500 and .999...
 * with external power of 2, then binary search for the
 * closest mantissa to this decimal number.
 * Nmant is is the required precision. (53 for ieee dp)
 * Nbits is the max number of bits/word. (must be <= 28)
 * Prec is calculated - the number of words of fixed mantissa.
 */

#define Nbits   28                            // bits safely represented in an unsigned long
#define Nmant   53                            // bits of precision required
#define Bias    1022
#define Prec    ((Nmant+Nbits+1) / Nbits)     // words of Nbits each to represent mantissa
#define Sigbit  (1uLL << (Prec*Nbits-Nmant))  // first significant bit of Prec-th word
#define Ndig    1500
#define One     (1uL << Nbits)
#define Half    (One >> 1)
#define Maxe    310

#define Fsign   BIT(0)  // found -
#define Fesign  BIT(1)  // found e-
#define Fdpoint BIT(2)  // found .

enum {
    S0	= 0,    // _		_S0	+S1	#S2	.S3
    S1,			// _+		#S2	.S3
    S2,			// _+#		#S2	.S4	eS5
    S3,			// _+.		#S4
    S4,			// _+#.#	#S4	eS5
    S5,			// _+#.#e	+S6	#S7
    S6,			// _+#.#e+	#S7
    S7,			// _+#.#e+#	#S7
};

typedef struct {
	int	        bp;
	int	        siz;
	const char *cmp;
} Tab;

static unsigned long umuldiv(unsigned long a, unsigned long b, unsigned long c)
{
	return ((unsigned long long) a * (unsigned long long) b) / c;
}

static void frnorm(unsigned long *f)
{
	int i, c;

	c = 0;
	for (i = Prec-1; i > 0; i--) {
		f[i] += c;
		c = f[i] >> Nbits;
		f[i] &= One-1;
	}
	f[0] += c;
}

static int fpcmp(char *a, unsigned long *f)
{
	unsigned long tf[Prec];
	int i, d, c;

	for (i = 0; i < Prec; i++)
		tf[i] = f[i];

	while (TRUE) {
		// tf *= 10
		for (i = 0; i < Prec; i++)
			tf[i] = tf[i] * 10;

		frnorm(tf);
		d = (tf[0] >> Nbits) + '0';
		tf[0] &= One-1;

		// Compare next digit
		c = *a;
		if (c == 0) {
			if ('0' < d)
				return -1;
			if (tf[0] != 0)
				goto cont;
			for (i = 1; i < Prec; i++) {
				if (tf[i] != 0)
					goto cont;
			}
			return 0;
		}
		if (c > d)
			return +1;
		if (c < d)
			return -1;
		a++;
	cont:;
	}
}

static void _divby(char *a, int *na, int b)
{
	int n, c;
	char *p;

	p = a;
	n = 0;
	while (n >> b == 0) {
		c = *a++;
		if (c == 0) {
			while (n) {
				c = n * 10;
				if (c>>b)
					break;
				n = c;
			}
			goto xx;
		}
		n = n*10 + c-'0';
		(*na)--;
	}
	while (TRUE) {
		c = n >> b;
		n -= c << b;
		*p++ = c + '0';
		c = *a++;
		if (c == 0)
			break;

		n = n*10 + c-'0';
	}
	(*na)++;
xx:
	while (n) {
		n = n * 10;
		c = n >> b;
		n -= c << b;
		*p++ = c + '0';
		(*na)++;
	}
	*p = '\0';
}

static void divby(char *a, int *na, int b)
{
	while (b > 9) {
		_divby(a, na, 9);
		a[*na] = 0;
		b -= 9;
	}
	if (b > 0)
		_divby(a, na, b);
}

static const Tab tab1[] = {
	{  1,  0, "" },
	{  3,  1, "7" },
	{  6,  2, "63" },
	{  9,  3, "511" },
	{ 13,  4, "8191" },
	{ 16,  5, "65535" },
	{ 19,  6, "524287" },
	{ 23,  7, "8388607" },
	{ 26,  8, "67108863" },
	{ 27,  9, "134217727" }
};

static void divascii(char *a, int *na, int *dp, int *bp)
{
	int b, d;
	const Tab *t;

	d = *dp;
	if (d >= (int) ARRAY_SIZE(tab1))
		d = ARRAY_SIZE(tab1)-1;

	t = tab1 + d;
	b = t->bp;
	if (memcmp(a, t->cmp, t->siz) > 0)
		d--;

	*dp -= d;
	*bp += b;
	divby(a, na, b);
}

static void mulby(char *a, char *p, char *q, int b)
{
	int n, c;

	n = 0;
	*p = 0;
	while (TRUE) {
		q--;
		if (q < a)
			break;

		c = *q - '0';
		c = (c << b) + n;
		n = c/10;
		c -= n*10;
		p--;
		*p = c + '0';
	}
	while (n) {
		c = n;
		n = c/10;
		c -= n*10;
		p--;
		*p = c + '0';
	}
}

static const Tab tab2[] = {
	{  1,  1, "" },				            // dp = 0-0
	{  3,  3, "125" },
	{  6,  5, "15625" },
	{  9,  7, "1953125" },
	{ 13, 10, "1220703125" },
	{ 16, 12, "152587890625" },
	{ 19, 14, "19073486328125" },
	{ 23, 17, "11920928955078125" },
	{ 26, 19, "1490116119384765625" },
	{ 27, 19, "7450580596923828125" }		// dp 8-9
};

static void mulascii(char *a, int *na, int *dp, int *bp)
{
	char *p;
	int d, b;
	const Tab *t;

	d = -*dp;
	if (d >= (int) ARRAY_SIZE(tab2))
		d = ARRAY_SIZE(tab2) - 1;

	t = tab2 + d;
	b = t->bp;
	if (memcmp(a, t->cmp, t->siz) < 0)
		d--;

	p = a + *na;
	*bp -= b;
	*dp += d;
	*na += d;
	mulby(a, p+d, p, b);
}

static Boolean xcmp(const char *a, const char *b)
{
	int c1, c2;

	while ((c1 = *b++) != '\0') {
		c2 = *a++;

		// Make c2 lowercase
		c2 |= ((c2 >= 'A' && c2 <= 'Z') << 5);
		if (c1 != c2)
			return FALSE;
	}
	return TRUE;
}

double Atof(const char *s, char **endp, NumConvRet *outcome)
{
	NumConvRet result;
	unsigned long low[Prec], hig[Prec], mid[Prec];
	unsigned long num, den;
	const char *sp;
	double d;
	int ona, bp, c, i;
	char a[Ndig];

	unsigned flag = 0;  // Fsign, Fesign, Fdpoint
	int na        = 0;  // number of digits of a[]
	int dp        = 0;  // na of decimal point
	int ex        = 0;  // exponent

	int state = S0;

	if (!outcome)
		outcome = &result;

	*outcome = NCVENOERR; // assume conversion is successful

	for (sp = s;; sp++) {
		c = *sp;
		if (c >= '0' && c <= '9') {
			switch (state) {
			default:
				UNREACHABLE;
				break;

			case S0:
			case S1:
			case S2:
				state = S2;
				break;
			case S3:
			case S4:
				state = S4;
				break;

			case S5:
			case S6:
			case S7:
				state = S7;
				ex = ex*10 + (c-'0');
				continue;
			}

			if (na == 0 && c == '0') {
				dp--;
				continue;
			}
			if (na < Ndig-50)
				a[na++] = c;

			continue;
		}
		switch (c) {
		case '-':
			if (state == S0)
				flag |= Fsign;
			else
				flag |= Fesign;

			// FALLTHROUGH
		case '+':
			if (state == S0)
				state = S1;
			else if (state == S5)
				state = S6;
			else
				break;	// syntax

			continue;
		case '.':
			flag |= Fdpoint;
			dp = na;
			if (state == S0 || state == S1) {
				state = S3;
				continue;
			}
			if (state == S2) {
				state = S4;
				continue;
			}
			break;
		case 'e':
		case 'E':
			if (state == S2 || state == S4) {
				state = S5;
				continue;
			}
			break;

		default:
			break;
		}

		break;
	}

	// Clean up return char-pointer
	switch (state) {
	case S0:
		if (xcmp(sp, "nan")) {
			if (endp)
				*endp = (char *) sp + 3;

			goto retnan;
		}

		// FALLTHROUGH
	case S1:
		if (xcmp(sp, "infinity")) {
			if (endp)
				*endp = (char *) sp + 8;

			goto retinf;
		}
		if (xcmp(sp, "inf")) {
			if (endp)
				*endp = (char *) sp + 3;

			goto retinf;
		}

		// FALLTHROUGH
	case S3:
		if (endp)
			*endp = (char *) sp;

		*outcome = NCVENOTHING;
		goto ret0;  // no digits found

	case S6:
		sp--;        // back over +-

		// FALLTHROUGH
	case S5:
		sp--;        // back over e
		break;
	}
	if (endp)
		*endp = (char *) sp;

	if (flag & Fdpoint) {
		while (na > 0 && a[na-1] == '0')
			na--;
	}
	if (na == 0)
		goto ret0;  // zero

	a[na] = 0;
	if (!(flag & Fdpoint))
		dp = na;
	if (flag & Fesign)
		ex = -ex;

	dp += ex;
	if (dp < -Maxe-Nmant/3)  // actually -Nmant*log(2)/log(10), but Nmant/3 close enough
		goto ret0;      // underflow by exp
	else if (dp > +Maxe)
		goto retinf;    // overflow by exp

	// Normalize the decimal ascii number
	// to range .[5-9][0-9]* e0
	bp = 0;  // binary exponent
	while (dp > 0)
		divascii(a, &na, &dp, &bp);
	while (dp < 0 || a[0] < '5')
		mulascii(a, &na, &dp, &bp);

	a[na] = '\0';

	// Very small numbers are represented using
	// bp = -Bias+1.  adjust accordingly.
	if (bp < -Bias+1) {
		ona = na;
		divby(a, &na, -bp-Bias+1);
		if (na < ona) {
			memmove(a+ona-na, a, na);
			memset(a, '0', ona-na);
			na = ona;
		}
		a[na] = '\0';
		bp    = -Bias+1;
	}

	// Close approx by naive conversion
	num = 0;
	den = 1;
	for (i = 0; i < 9 && (c = a[i]) != '\0'; i++) {
		num = num*10 + (c - '0');
		den *= 10;
	}

	low[0] = umuldiv(num, One, den);
	hig[0] = umuldiv(num+1, One, den);
	for (i = 1; i < Prec; i++) {
		low[i] = 0;
		hig[i] = One-1;
	}

	// Binary search for closest mantissa
	while (TRUE) {
		// mid = (hig + low) / 2
		c = 0;
		for (i = 0; i < Prec; i++) {
			mid[i] = hig[i] + low[i];
			if (c)
				mid[i] += One;

			c = mid[i] & 1;
			mid[i] >>= 1;
		}
		frnorm(mid);

		// Compare
		c = fpcmp(a, mid);
		if (c > 0) {
			c = 1;
			for (i = 0; i < Prec; i++) {
				if (low[i] != mid[i]) {
					c = 0;
					low[i] = mid[i];
				}
			}
			if (c)
				break;  // between mid and hig

			continue;
		}
		if (c < 0) {
			for (i = 0; i < Prec; i++)
				hig[i] = mid[i];

			continue;
		}

		// Only hard part is if even/odd roundings wants to go up
		c = mid[Prec-1] & (Sigbit-1);
		if (c == Sigbit/2 && (mid[Prec-1] & Sigbit) == 0)
			mid[Prec-1] -= c;

		break;	// exactly mid
	}

	// Normal rounding applies
	c = mid[Prec-1] & (Sigbit-1);
	mid[Prec-1] -= c;
	if (c >= (int) Sigbit/2) {
		mid[Prec-1] += Sigbit;
		frnorm(mid);
	}

	d = 0;
	for (i = 0; i < Prec; i++)
		d = d*One + mid[i];
	if (flag & Fsign)
		d = -d;

	d = ldexp(d, bp - Prec*Nbits);
	return d;

ret0:
	return 0;

retnan:
	return NAN;

retinf:
	return (flag & Fsign) ? -INFINITY : INFINITY;
}

