// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file numlib.h
 *
 * Fast locale independent conversion from numerics (integer or floating point
 * types) to ASCII and back.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_NUMLIB_H_
#define DF_NUMLIB_H_

#include "xpt.h"

/**
 * \brief Integral compile-time costant representing an upper-bound estimate
 *        of the number of digit-characters necessary to hold the **decimal**
 *        representation of `typ`.
 *
 * Can be used as such:
 * ```c
 * char buf[1 + DIGS(int) + 1];  // sign + digits + '\0'
 *
 * Itoa(INT_MAX, buf);
 * ```
 *
 * Approximation is derived from the following formula, where `MAX_INT` is
 * the maximum integral value representable by `typ`:
 * ```
 * N = ceil(log10(MAX_INT)) ->
 * N = floor(log10(MAX_INT)) + 1 ->
 * (as base-256 logarithm)
 * N = floor( log256(MAX_INT) / log256(10) ) + 1 ->
 *
 * N <= floor( sizeof(typ) * 2.40824 ) + 1 ->
 *
 * N ~= 241 * sizeof(typ) / 100 + 1
 * ```
 *
 * \param typ An **integer** type or expression
 *
 * \author [John Bollinger](https://stackoverflow.com/a/43789115)
 */
#define DIGS(typ) ((241 * sizeof(typ)) / 100 + 1)
// #define DIGS(typ)  (sizeof(typ) * CHAR_BIT) gross upper-bound approx

/**
 * \brief Integral compile-time constant representing an upper-bound estimate
 *        of the number of digit-characters necessary to hold the **hexadecimal**
 *        representation of `typ`.
 *
 * \param typ An **integer** type or expression
 */
#define XDIGS(typ) (sizeof(typ) * 2)

/**
 * \brief Maximum number of characters returned by `Ftoa()`,
 *        **excluding terminating `\0` char**.
 *
 * Range of double (IEEE-754 `binary64`): `[1.7E-308 ... 1.7E308]`
 * - 1 char for sign
 * - 309 digits for integer part
 * - 1 char for mantissa dot
 * - 37 chars for mantissa
 */
#define DOUBLE_STRLEN (309 + 39)

STATIC_ASSERT(sizeof(double) <= 8, "DOUBLE_STRLEN might be inaccurate on this platform");
// should actually make sure we also have IEEE-754 floats...

/**
 * \brief Unsigned integer to ASCII conversion.
 *
 * Destination buffer is assumed to be large enough to hold the
 * result. A storage of `DIGS(x) + 1` chars is guaranteed
 * to be sufficient. Resulting string is always `NUL` terminated.
 *
 * \param [in]  x    Value to be converted
 * \param [out] dest Destination string, must not be `\0`
 *
 * \return Pointer to the trailing `\0` char inside `dest`, useful
 *         for further string concatenation.
 */
char *Utoa(unsigned long long x, char *dest);
/**
 * \brief Signed integer to ASCII conversion.
 *
 * Destination buffer is assumed to be large enough to hold the result.
 * A storage of `1 + DIGS(x) + 1` chars, accounting for the sign
 * character, is guaranteed to be sufficient.
 *
 * \param [in]  x    Value to be converted
 * \param [out] dest Destination string, must not be `NULL`
 *
 * \return Pointer to the trailing `\0` char in `dest`, useful for further
 *         string concatenation.
 */
char *Itoa(long long x, char *dest);
/**
 * \brief Unsigned integer to hexadecimal lowercase ASCII string.
 *
 * Destination buffer is assumed to be large enough to hold the result.
 * A storage of `XDIGS(x) + 1` chars is guaranteed to be
 * sufficient. Resulting string is always `\0` terminated.
 *
 * \param [in]  x    Value to be converted
 * \param [out] dest Destination string, must not be `NULL`
 *
 * \return Pointer to the trailing `\0` char in `dest`, useful for further
 *         string concatenation.
 *
 * \note No `0x` prefix is prepended to resulting string.
 */
char *Xtoa(unsigned long long x, char *dest);

/**
 * \brief Floating point number to scientific notation string.
 *
 * Destination string is assumed to be large enough to store the conversion
 * result, a buffer of size `DOUBLE_STRLEN + 1` is guaranteed to be large
 * enough for it. Result is always `\0` terminated.
 *
 * \param [in]  x    Floating point number to be converted
 * \param [out] dest Destination for result string, must not be `NULL`
 *
 * \return Pointer to the trailing `\0` char inside result, useful for
 *         further string concatenation.
 */
char *Ftoa(double x, char *dest);

/// Numeric conversion outcomes.
typedef enum {
	NCVENOERR = 0,  ///< Conversion successful
	NCVEBADBASE,    ///< The specified numeric base is out of range
	NCVENOTHING,    ///< No legal numeric data in input string
	NCVEOVERFLOW,   ///< Numeric input too large for target integer type
	NCVEUNDERFLOW   ///< Numeric input too small for target integer type
} NumConvRet;

/**
 * \brief ASCII to integer conversion.
 *
 * If `base` is 0 then the actual numeric base is guessed from input string
 * prefix according to an extended C convention:
 *
 * Numeric prefix | Base
 * ---------------|---------------------
 * __0__          | octal, base 8
 * __0x__         | hexadecimal, base 16
 * __0b__         | binary, base 2
 * __otherwise__  | decimal, base 10
 *
 * \param [in]  s       Input string to be converted, must not be `NULL`
 * \param [out] endp    Storage where end pointer is returned, may be `NULL`
 * \param [in]  base    Integer conversion base, a value between 0 and 36 inclusive
 * \param [out] outcome Storage where conversion outcome is returned, may be `NULL`
 *
 * \return Conversion result, 0 on error (use `outcome` to detect error).
 *
 * @{
 *   \fn unsigned long long Atoull(const char *, char **, unsigned, NumConvRet *)
 *   \fn unsigned long Atoul(const char *, char **, unsigned, NumConvRet *)
 *   \fn unsigned Atou(const char *, char **, unsigned, NumConvRet *)
 *   \fn long long Atoll(const char *, char **, unsigned, NumConvRet *)
 *   \fn long Atol(const char *, char **, unsigned, NumConvRet *)
 *   \fn int Atoi(const char *, char **, unsigned, NumConvRet *)
 * @}
 */
unsigned long long Atoull(const char *s, char **endp, unsigned base, NumConvRet *outcome);
unsigned long Atoul(const char *s, char **endp, unsigned base, NumConvRet *outcome);
unsigned Atou(const char *s, char **endp, unsigned base, NumConvRet *outcome);

long long Atoll(const char *s, char **endp, unsigned base, NumConvRet *outcome);
long Atol(const char *s, char **endp, unsigned base, NumConvRet *outcome);
int Atoi(const char *s, char **endp, unsigned base, NumConvRet *outcome);

/**
 * \brief ASCII to floating point conversion.
 *
 * \param [in]  s       Input string to be converted, must not be `NULL`
 * \param [out] endp    Storage where end pointer is returned, must not be `NULL`
 * \param [out] outcome Storage where conversion outcome is returned, may be `NULL`
 *
 * \return Conversion result, 0 on error (use `outcome` to detect error).
 */
double Atof(const char *s, char **endp, NumConvRet *outcome);

#endif
