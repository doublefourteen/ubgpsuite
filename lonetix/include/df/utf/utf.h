// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file utf/utf.h
 *
 * UTF-8 decoding and encoding functionality.
 *
 * \author Russ Cox
 * \author Rob Pike
 * \author Ken Thompson
 * \author Lorenzo Cogotti
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved.
 *
 * This API is derived by work authored by Russ Cox - namely the Unix port of the Plan 9
 * UTF-8 library, originally written by Rob Pike and Ken Thompson.
 *
 * Original license terms follow:
 * ```
 * Copyright © 2021 Plan 9 Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ```
 * The original libutf library is available at: https://9fans.github.io/plan9port/unix/libutf.tgz
 */

#ifndef DF_UTF_H_
#define DF_UTF_H_

#include "utf/utfdef.h"

/**
 * \brief Convert the first UTF-8 rune inside `\0` terminated string `str` to a `Rune` in `dest`.
 *
 * \return Number of bytes read from `str` for the returned `Rune`.
 *
 * \note Returned bytes are usually equivalent to `runelen()` over the returned `Rune`,
 *       but values may differ in case of a decoding error. In that case `¢hartorune()` returns `RUNE_ERR`,
 *       and returns 1. This allows the caller to skip one byte and move on with the decoding.
 */
size_t chartorune(Rune *dest, const char *str);
/// Inverse of `chartorune()`.
size_t runetochar(char *dest, Rune r);
/// Calculate the number of bytes necessary to encode `r`.
size_t runelen(Rune r);
/// Calculate the number of bytes necessary to encode the first `n` runes referenced by `r`.
size_t runenlen(const Rune *r, size_t n);
/// Test whether the first `n` bytes referenced by `src` form at least one `Rune`.
Boolean fullrune(const char *src, size_t n);

/// Convert `r` to lowercase.
Rune tolowerrune(Rune r);
/// Convert 'r` to uppercase.
Rune toupperrune(Rune r);
/// Convert `r` to titlecase.
Rune totitlerune(Rune r);
/// Test whether `r` is a lowercase UTF-8 rune.
Boolean islowerrune(Rune r);
/// Test whether `r` is an uppercase UTF-8 rune.
Boolean isupperrune(Rune r);
/// Test whether `r` represents an alphabetic UTF-8 rune.
Boolean isalpharune(Rune r);
/// Test wheter `r` is a title-case UTF-8 rune.
Boolean istitlerune(Rune r);
/// Test whether `r` represents a space UTF-8 rune.
Boolean isspacerune(Rune r);

/// Return the number of runes inside the `\0` terminated UTF-8 string `s`.
size_t utflen(const char *s);
/// Find the first occurrence of `r` inside the `\0' terminated UTF-8 string `s`, `NULL` if not found.
char  *utfrune(const char *s, Rune r);
/// Find the last occurrence of `r` inside the `\0` terminated UTF-8 string `s`, `NULL` if not found.
char  *utfrrune(const char *s, Rune r);
/// Find the first occurrence of the UTF-8 `\0` terminated UTF-8 string `needle` inside `haystack`, `NULL` if not found.
char  *utfutf(const char *haystack, const char *needle);

#endif
