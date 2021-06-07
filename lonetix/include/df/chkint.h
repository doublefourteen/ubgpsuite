// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file chkint.h
 *
 * Overflow checked integer arithmetics.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * This API was modeled after D Language `core.checkedint` module,
 * available under the [Boost License 1.0](http://www.boost.org/LICENSE_1_0.txt),
 * and originally written by Walter Bright.
 *
 * \see [How Should You Write a Fast Integer Overflow Check?](https://blog.regehr.org/archives/1139)
 * \see [D Language Phobos documentation](https://dlang.org/phobos/core_checkedint.html)
 */

#ifndef DF_CHKINT_H_
#define DF_CHKINT_H_

#include "xpt.h"

#include <limits.h>

// Define this to force portable C implementation of the checked int API
// #define DF_C_ONLY_CHKINT

FORCE_INLINE long long Chk_NoAddll(long long lhs,
                                   long long rhs,
                                   Boolean  *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	long long res;
	*ovrflw |= __builtin_saddll_overflow(lhs, rhs, &res);
	return res;
#else
	long long res = (unsigned long long) lhs + (unsigned long long) rhs;

	*ovrflw |= ((lhs <  0 && rhs <  0 && res >= 0) ||
	            (lhs >= 0 && rhs >= 0 && res <  0));
	return res;
#endif
}

FORCE_INLINE long Chk_NoAddl(long lhs, long rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	long res;

	*ovrflw |= __builtin_saddl_overflow(lhs, rhs, &res);
	return res;
#elif LONG_MAX == LLONG_MAX
	return Chk_NoAddll(lhs, rhs, ovrflw);
#else
	long long res = (long long) lhs + (long long) rhs;

	*ovrflw |= (res < LONG_MIN || res > LONG_MAX);
	return (int) res;
#endif
}

FORCE_INLINE int Chk_NoAdd(int lhs, int rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	int res;

	*ovrflw |= __builtin_sadd_overflow(lhs, rhs, &res);
	return res;
#elif INT_MAX == LONG_MAX
	return Chk_NoAddl(lhs, rhs, ovrflw);
#else
	long res = (long) lhs + (long) rhs;

	*ovrflw |= (res < INT_MIN || res > INT_MAX);
	return (int) res;
#endif
}

FORCE_INLINE unsigned long long Chk_NoAddull(unsigned long long lhs,
                                             unsigned long long rhs,
                                             Boolean           *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned long long res;

	*ovrflw |= __builtin_uaddll_overflow(lhs, rhs, &res);
	return res;
#else
	unsigned long long res = lhs + rhs;

	*ovrflw |= (res < lhs || res < rhs);
	return res;
#endif
}

FORCE_INLINE unsigned long Chk_NoAddul(unsigned long lhs,
                                       unsigned long rhs,
                                       Boolean      *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned long res;

	*ovrflw |= __builtin_uaddl_overflow(lhs, rhs, &res);
	return res;
#else
	unsigned long res = lhs + rhs;

	*ovrflw |= (res < lhs || res < rhs);
	return res;
#endif
}

FORCE_INLINE unsigned Chk_NoAddu(unsigned lhs, unsigned rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned res;

	*ovrflw |= __builtin_uadd_overflow(lhs, rhs, &res);
	return res;
#else
	unsigned res = lhs + rhs;

	*ovrflw |= (res < lhs || res < rhs);
	return res;
#endif
}

FORCE_INLINE long long Chk_NoNegll(long long rhs, Boolean *ovrflw)
{
	*ovrflw |= (rhs == LLONG_MIN);
	return -rhs;
}

FORCE_INLINE long Chk_NoNegl(long rhs, Boolean *ovrflw)
{
	*ovrflw |= (rhs == LONG_MIN);
	return -rhs;
}

FORCE_INLINE int Chk_NoNeg(int rhs, Boolean *ovrflw)
{
	*ovrflw |= (rhs == INT_MIN);
	return -rhs;
}

FORCE_INLINE long long Chk_NoSubll(long long lhs,
                                   long long rhs,
                                   Boolean  *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	long long res;
	*ovrflw |= __builtin_ssubll_overflow(lhs, rhs, &res);
	return res;
#else
	long long res = (unsigned long long) lhs - (unsigned long long) rhs;

	*ovrflw |= ((lhs <  0 && rhs >= 0 && res >= 0) ||
	            (lhs >= 0 && rhs <  0 && (res <  0 || rhs == LLONG_MIN)));
	return res;
#endif
}

FORCE_INLINE long Chk_NoSubl(long lhs, long rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	long res;

	*ovrflw |= __builtin_ssubl_overflow(lhs, rhs, &res);
	return res;
#elif LONG_MAX == LLONG_MAX
	return Chk_NoSubll(lhs, rhs, ovrflw);
#else
	long long res = (long long) lhs - (long long) rhs;

	*ovrflw |= (res < LONG_MIN || res > LONG_MAX);
	return res;
#endif
}

FORCE_INLINE int Chk_NoSub(int lhs, int rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	int res;

	*ovrflw |= __builtin_ssub_overflow(lhs, rhs, &res);
	return res;
#elif INT_MAX == LONG_MAX
	return Chk_NoSubl(lhs, rhs, &rhs);
#else
	long res = (long) lhs - (long) rhs;

	*ovrflw |= (res < INT_MIN || res > INT_MAX);
	return res;
#endif
}

FORCE_INLINE unsigned long long Chk_NoSubull(unsigned long long lhs,
                                             unsigned long long rhs,
                                             Boolean           *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned long long _res;

	*ovrflw |= __builtin_usubll_overflow(lhs, rhs, &res);
	return res;
#else
	*ovrflw |= (lhs < rhs);
	return lhs - rhs;
#endif
}

FORCE_INLINE unsigned long Chk_NoSubul(unsigned long lhs,
                                       unsigned long rhs,
                                       Boolean      *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned long res;

	*ovrflw |= __builtin_usubl_overflow(lhs, rhs, &res);
	return res;
#else
	*ovrflw |= (lhs < rhs);
	return lhs - rhs;
#endif
}

FORCE_INLINE unsigned Chk_NoSubu(unsigned lhs, unsigned rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned res;

	*ovrflw |= __builtin_usub_overflow(lhs, rhs, &res);
	return res;
#else
	*ovrflw |= (lhs < rhs);
	return lhs - rhs;
#endif
}

FORCE_INLINE long long Chk_NoMulll(long long lhs,
                                   long long rhs,
                                   Boolean  *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	long long res;
	*ovrflw |= __builtin_smulll_overflow(lhs, rhs, &res);
	return res;
#else
	long long res = (unsigned long long) lhs * (unsigned long long) rhs;

	*ovrflw |= ((lhs & (~1LL)) != 0 &&
	            (res == rhs ? res : (res / lhs) != rhs));
	return res;
#endif
}

FORCE_INLINE long Chk_NoMull(long lhs, long rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	long res;
	*ovrflw |= __builtin_smull_overflow(lhs, rhs, &res);
	return res;
#elif LONG_MAX == LLONG_MAX
	return Chk_NoMulll(lhs, rhs, ovrflw);
#else
	long long res = (long long) lhs * (long long) rhs;

	*ovrflw |= (res < LONG_MIN || res > LONG_MAX);
	return res;
#endif
}

FORCE_INLINE int Chk_NoMul(int lhs, int rhs, Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	int res;

	*ovrflw |= __builtin_smul_overflow(lhs, rhs, &res);
	return res;
#elif INT_MAX == LONG_MAX
	return Chk_NoMull(lhs, rhs, ovrflw);
#else
	long res = (long) lhs * (long) rhs;

	*ovrflw |= (res < INT_MIN || res > INT_MAX);
	return res;
#endif
}

FORCE_INLINE unsigned long long Chk_NoMulull(unsigned long long lhs,
                                             unsigned long long rhs,
                                             Boolean           *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned long long res = 0;

	*ovrflw |= __builtin_umulll_overflow(lhs, rhs, &res);
	return res;
#else
	unsigned long long res = lhs * rhs;

	*ovrflw |= ((lhs | rhs) >> 32 && lhs && res / lhs != rhs);
	return res;
#endif
}

FORCE_INLINE unsigned long Chk_NoMulul(unsigned long lhs,
                                       unsigned long rhs,
                                       Boolean      *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned long res;

	*ovrflw |= __builtin_umull_overflow(lhs, rhs, &res);
	return res;
#elif ULONG_MAX == ULLONG_MAX
	return Chk_NoMulull(lhs, rhs, ovrflw);
#else
	unsigned long long res = (unsigned long long) lhs *
	                         (unsigned long long) rhs;

	*ovrflw |= ((res >> (sizeof(unsigned long)*CHAR_BIT)) != 0);
	return res;
#endif
}

FORCE_INLINE unsigned Chk_NoMulu(unsigned lhs,
                                 unsigned rhs,
                                 Boolean *ovrflw)
{
#if defined(__GNUC__) && !defined(DF_C_ONLY_CHKINT)
	unsigned res;

	*ovrflw |= __builtin_umul_overflow(lhs, rhs, &res);
	return res;
#elif UINT_MAX == ULONG_MAX
	return Chk_NoMulul(lhs, rhs, ovrflw);
#else
	unsigned long res = (unsigned long) lhs * (unsigned long) rhs;

	*ovrflw |= ((res >> (sizeof(unsigned)*CHAR_BIT)) != 0);
	return res;
#endif
}

#endif
