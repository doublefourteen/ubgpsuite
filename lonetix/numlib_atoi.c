// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file numlib_atoi.c
 *
 * Implements ASCII to integer conversion.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "numlib.h"

#include <assert.h>
#include <limits.h>

static int digitval(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 10;
	else
		return -1;
}

static unsigned long long ParseInt(const char *s,
                                   char      **endptr,
                                   unsigned    base,
                                   int        *signp,
                                   NumConvRet *outcome)
{
	int d;

	// Single optional + or -
	char c;
	int sign;

	const char *startptr = s;
	unsigned long long u = 0;

	sign = 1;
	if (base > 36) {
		*outcome = NCVEBADBASE;
		goto done;
	}

	c    = *s;
	if (c == '-' || c == '+') {
		sign -= (c == '-') << 1;
		s++;
	}

	// Determine base
	if (base == 0) {
		if (s[0] == '0') {
			if (s[1] == 'x' || s[1] == 'X') {
				s += 2;
				base = 16;
			} else if (s[1] == 'b' || s[1] == 'B') {
				s += 2;
				base = 2;
			} else {
				s++;
				base = 8;
			}
		} else {
			base = 10;
		}
	} else if (base == 16) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
			s += 2;
	} else if (base == 2) {
		if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B'))
			s += 2;
	}

	// Convert numeric value
	*outcome = NCVENOERR;  // assume successful conversion

	while ((d = digitval(*s)) >= 0 && d < (int) base) {
		unsigned long long v = u * base + d;
		if (u > v) {
			// Overflow
			v        = ULLONG_MAX;
			*outcome = NCVEOVERFLOW;
		}

		u = v;
		s++;
	}

	if (s == startptr)
		*outcome = NCVENOTHING;

done:
	if (endptr)
		*endptr = (char *) s;

	*signp = sign;
	return u;
}

unsigned long long Atoull(const char *s,
                          char      **endp,
                          unsigned    base,
                          NumConvRet *outcome)
{
	NumConvRet result;
	if (!outcome)
		outcome = &result;

	int sign;
	unsigned long long mag = ParseInt(s, endp, base, &sign, outcome);
	if (sign < 0)
		*outcome = NCVEUNDERFLOW;

	return sign * mag;
}

unsigned long Atoul(const char *s,
                    char      **endp,
                    unsigned    base,
                    NumConvRet *outcome)
{
	NumConvRet result;
	if (!outcome)
		outcome = &result;

	int sign;
	unsigned long long mag = ParseInt(s, endp, base, &sign, outcome);
	if (mag > ULONG_MAX)
		*outcome = NCVEOVERFLOW;
	if (sign < 0)
		*outcome = NCVEUNDERFLOW;

	return sign * mag;
}

unsigned Atou(const char *s, char **endp, unsigned base, NumConvRet *outcome)
{
	NumConvRet result;
	if (!outcome)
		outcome = &result;

	int sign;
	unsigned long long mag = ParseInt(s, endp, base, &sign, outcome);
	if (mag > UINT_MAX)
		*outcome = NCVEOVERFLOW;
	if (sign < 0)
		*outcome = NCVEUNDERFLOW;

	return sign * mag;
}

long long Atoll(const char *s, char **endp, unsigned base, NumConvRet *outcome)
{
	NumConvRet result;
	if (!outcome)
		outcome = &result;

	int sign;
	unsigned long long mag = ParseInt(s, endp, base, &sign, outcome);
	if (sign > 0 && mag > LLONG_MAX) {
		*outcome = NCVEOVERFLOW;
		return LLONG_MAX;
	}
	if (sign < 0 && mag > -((unsigned long long) LLONG_MIN)) {
		*outcome = NCVEUNDERFLOW;
		return LLONG_MIN;
	}
	return sign * mag;
}

long Atol(const char *s, char **endp, unsigned base, NumConvRet *outcome)
{
	NumConvRet result;
	if (!outcome)
		outcome = &result;

	int sign;
	unsigned long long mag = ParseInt(s, endp, base, &sign, outcome);
	if (sign > 0 && mag > LONG_MAX) {
		*outcome = NCVEOVERFLOW;
		return LONG_MAX;
	}
	if (sign < 0 && mag > -((unsigned long) LONG_MIN)) {
		*outcome = NCVEUNDERFLOW;
		return LONG_MIN;
	}
	return sign * mag;
}

int Atoi(const char *s, char **endp, unsigned base, NumConvRet *outcome)
{
	NumConvRet result;
	if (!outcome)
		outcome = &result;

	int sign;
	unsigned long long mag = ParseInt(s, endp, base, &sign, outcome);
	if (sign > 0 && mag > INT_MAX) {
		*outcome = NCVEOVERFLOW;
		return INT_MAX;
	}
	if (sign < 0 && mag > -((unsigned) INT_MIN)) {
		*outcome = NCVEUNDERFLOW;
		return INT_MIN;
	}
	return sign * mag;
}
