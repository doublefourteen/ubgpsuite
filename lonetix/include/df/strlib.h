// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file strlib.h
 *
 * Additional ASCII string and char classification utility library.
 *
 * \copyright The DoubleFourteen Code Forge (c) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_STRLIB_H_
#define DF_STRLIB_H_

#include "xpt.h"

/// Test whether `c` is a digit `char`.
FORCE_INLINE Boolean Df_isdigit(char c)
{
	return c >= '0' && c <= '9';
}

/// Test whether `c` is an uppercase ASCII `char`.
FORCE_INLINE Boolean Df_isupper(char c)
{
	return c >= 'A' && c <= 'Z';
}

/// Test whether `c` is a lowercase ASCII `char`.
FORCE_INLINE Boolean Df_islower(char c)
{
	return c >= 'a' && c <= 'z';
}

/// Test whether `c` is an ASCII space `char`.
FORCE_INLINE Boolean Df_isspace(char c)
{
	return c == ' ' || (c >= '\t' && c <= '\n');
}

/// Test whether `c` is an alphabetic ASCII `char`.
FORCE_INLINE Boolean Df_isalpha(char c)
{
	return Df_islower(c) || Df_isupper(c);
}

/// Test whether `c` is an alphanumeric ASCII `char`.
FORCE_INLINE Boolean Df_isalnum(char c)
{
	return Df_isdigit(c) || Df_isalpha(c);
}

/**
 * \brief Convert `c` to uppercase ASCII `char`.
 *
 * \return Uppercase `c`, if `c` is lowecase, otherwise
 *         returns `c` itself.
 */
FORCE_INLINE char Df_toupper(char c)
{
	// Toggle off lowercase bit if c is lowercase
	return c & ~(Df_islower(c) << 5);
}

/**
 * \brief Convert `c` to lowercase ASCII `char`.
 *
 * \return Lowercase `c`, if `c` is uppercase, otherwise returns `c` itself.
 */
FORCE_INLINE char Df_tolower(char c)
{
	// Only toggle lowercase bit if c is uppercase
	return c | (Df_isupper(c) << 5);
}

/// Test whether `c` is an hexadecimal digit `char`.
FORCE_INLINE Boolean Df_ishexdigit(char c)
{
	char lc = Df_tolower(c);

	return Df_isdigit(c) || (lc >= 'a' && lc <= 'f');
}

/**
 * \brief Concatenate `dest` with `s`, writing at most `n` `char`s to `dest`
 *        (including `\0`).
 *
 * \return `strlen(dest) + strlen(s)` before concatenation, that is:
 *         the length of the concatenated string without truncation,
 *         truncation occurred if return value `>= n`.
 *
 * \note Function always terminates `dest` with `\0`.
 */
size_t Df_strncatz(char *dest, const char *s, size_t n);
/**
 * \brief Copy at most `n` `char`s from `s` to `dest` (including `\0`).
 *
 * \return The length of `s`, truncation occurred if return value `>= n`.
 *
 * \note Function always terminates `dest` with `\0`.
 */
size_t Df_strncpyz(char *dest, const char *s, size_t n);
/// Compare ASCII strings `a` and `b` ignoring case.
int Df_stricmp(const char *a, const char *b);
/// Compare at most `n` chars from ASCII strings `a` and `b`, ignoring case.
int Df_strnicmp(const char *a, const char *b, size_t n);

/// Convert ASCII string to lowercase, returns `s` itself.
FORCE_INLINE char *Df_strlwr(char *s)
{
	char *p = s;
	char  c;

	while ((c = *p) != '\0')
		*p++ = Df_tolower(c);

	return s;
}

/// Convert ASCII string to uppercase, returns `s` itself.
FORCE_INLINE char *Df_strupr(char *s)
{
	char *p = s;
	char  c;

	while ((c = *p) != '\0')
		*p++ = Df_toupper(c);

	return s;
}

/// Trim trailing whitespaces in-place, returns `s` itself.
INLINE char *Df_strtrimr(char *s)
{
	EXTERNC size_t strlen(const char *);

	size_t n = strlen(s);
	while (n > 0 && Df_isspace(s[n-1])) n--;

	s[n] = '\0';
	return s;
}

/// Trim leading whitespaces in-place, returns `s` itself.
INLINE char *Df_strtriml(char *s)
{
	char *p1 = s, *p2 = s;

	while (Df_isspace(*p1)) p1++;
	while ((*p2++ = *p1++) != '\0');

	return s;
}

/// Trim leading and trailing whitespaces in-place, returns `s` itself.
INLINE char *Df_strtrim(char *s)
{
	Df_strtrimr(s);
	return Df_strtriml(s);
}

/**
 * \brief Pad string to left with `c` up to `n` ASCII chars.
 *
 * \return Resulting string length.
 */
INLINE size_t Df_strpadl(char *s, char c, size_t n)
{
	EXTERNC size_t strlen(const char *);
	EXTERNC void  *memmove(void *, const void *, size_t);
	EXTERNC void  *memset(void *, int, size_t);

	size_t len = strlen(s);
	if (len < n) {
		memmove(s + n - len, s, len + 1);
		memset(s, c, n - len);
		len = n;
	}
	return len;
}

/**
 * \brief Pad string to right with `c` up to `n` ASCII chars.
 *
 * \return Resulting string length.
 */
INLINE size_t Df_strpadr(char *s, char c, size_t n)
{
	EXTERNC size_t strlen(const char *);

	size_t i = strlen(s);
	while (i < n) s[i++] = c;

	s[i] = '\0';
	return i;
}

#endif
