// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bufio.c
 *
 * I/O stream buffering utilities implementation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/sys_local.h"  // for Sys_SetErrStat() - vsnprintf()
#include "bufio.h"
#include "numlib.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

Sint64 Bufio_Flush(Stmbuf *sb)
{
	assert(sb->ops->Write);

	while (sb->len > 0) {
		Sint64 n = sb->ops->Write(sb->streamp, sb->buf, sb->len);
		if (n < 0)
			return NG;

		memmove(sb->buf, sb->buf + n, sb->len - n);
		sb->len   -= n;
		sb->total += n;
	}

	return sb->total;
}

Sint64 _Bufio_Putsn(Stmbuf *sb, const char *s, size_t nbytes)
{
	if (sb->len + nbytes > sizeof(sb->buf) && Bufio_Flush(sb) == -1)
		return -1;
	if (nbytes > sizeof(sb->buf))
		return sb->ops->Write(sb->streamp, sb, nbytes);

	memcpy(sb->buf + sb->len, s, nbytes);
	sb->len += nbytes;
	return nbytes;
}

Sint64 Bufio_Putu(Stmbuf *sb, unsigned long long val)
{
	char buf[DIGS(val) + 1];

	char *eptr = Utoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Putx(Stmbuf *sb, unsigned long long val)
{
	char buf[XDIGS(val) + 1];

	char *eptr = Xtoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Puti(Stmbuf *sb, long long val)
{
	char buf[1 + DIGS(val) + 1];

	char *eptr = Itoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Putf(Stmbuf *sb, double val)
{
	char buf[DOUBLE_STRLEN + 1];

	char *eptr = Ftoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Printf(Stmbuf *sb, const char *fmt, ...)
{
	va_list va;
	Sint64  n;

	va_start(va, fmt);
	n = Bufio_Vprintf(sb, fmt, va);
	va_end(va);

	return n;
}

Sint64 Bufio_Vprintf(Stmbuf *sb, const char *fmt, va_list va)
{
	va_list vc;
	char   *buf;
	int     n1, n2;

	va_copy(vc, va);
	n1 = vsnprintf(NULL, 0, fmt, vc);
	va_end(vc);
	if (n1 < 0) {
		Sys_SetErrStat(errno, "vsnprintf() failed");
		return -1;
	}

	buf = (char *) alloca(n1 + 1);
	n2  = vsnprintf(buf, n1 + 1, fmt, va);
	if (n2 < 0) {
		Sys_SetErrStat(errno, "vsnprintf() failed");
		return -1;
	}

	assert(n1 == n2);

	return Bufio_Putsn(sb, buf, n2);
}

