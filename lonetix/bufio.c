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

static Judgement Bufio_FillRdBuf(Stmrdbuf *sb)
{
	assert(sb->pos == sb->availIn);

	Sint64 n = sb->ops->Read(sb->streamp, sb->buf, sizeof(sb->buf));
	if (n < 0) {
		sb->hasError = TRUE;
		return NG;
	}

	sb->isEof = (n == 0);

	sb->availIn  = n;
	sb->totalIn += n;
	sb->pos      = 0;
	return OK;
}

Sint64 Bufio_Read(Stmrdbuf *sb, void *buf, size_t nbytes)
{
	assert(sb->ops->Read);

	if (sb->hasError)
		return -1;  // refuse unless somebody clears error

	Uint8 *dest = (Uint8 *) buf;
	while (nbytes > 0) {
		if (sb->pos >= sb->availIn) {
			if (Bufio_FillRdBuf(sb) != OK) {
				if (dest > (Uint8 *) buf)
					break; // delay error to next read

				return -1;
			}
			if (sb->isEof)
				break;  // end of file
		}

		size_t size = MIN((size_t) (sb->availIn - sb->pos), nbytes);

		memcpy(dest, sb->buf + sb->pos, size);
		sb->pos += size;

		nbytes -= size;
		dest   += size;
	}

	return dest - (Uint8 *) buf;
}

void Bufio_Close(Stmrdbuf *sb)
{
	if (sb->ops->Close) sb->ops->Close(sb->streamp);
}

Sint64 Bufio_Flush(Stmwrbuf *sb)
{
	assert(sb->ops->Write);

	while (sb->availOut > 0) {
		Sint64 n = sb->ops->Write(sb->streamp, sb->buf, sb->availOut);
		if (n < 0)
			return -1;

		memmove(sb->buf, sb->buf + n, sb->availOut - n);
		sb->availOut -= n;
		sb->totalOut += n;
	}

	return sb->totalOut;
}

Sint64 _Bufio_Putsn(Stmwrbuf *sb, const char *s, size_t nbytes)
{
	if (sb->availOut + nbytes > sizeof(sb->buf) && Bufio_Flush(sb) == -1)
		return -1;
	if (nbytes > sizeof(sb->buf))
		return sb->ops->Write(sb->streamp, sb, nbytes);

	memcpy(sb->buf + sb->availOut, s, nbytes);
	sb->availOut += nbytes;
	return nbytes;
}

Sint64 Bufio_Putu(Stmwrbuf *sb, unsigned long long val)
{
	char buf[DIGS(val) + 1];

	char *eptr = Utoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Putx(Stmwrbuf *sb, unsigned long long val)
{
	char buf[XDIGS(val) + 1];

	char *eptr = Xtoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Puti(Stmwrbuf *sb, long long val)
{
	char buf[1 + DIGS(val) + 1];

	char *eptr = Itoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Putf(Stmwrbuf *sb, double val)
{
	char buf[DOUBLE_STRLEN + 1];

	char *eptr = Ftoa(val, buf);
	return Bufio_Putsn(sb, buf, eptr - buf);
}

Sint64 Bufio_Printf(Stmwrbuf *sb, const char *fmt, ...)
{
	va_list va;
	Sint64  n;

	va_start(va, fmt);
	n = Bufio_Vprintf(sb, fmt, va);
	va_end(va);

	return n;
}

Sint64 Bufio_Vprintf(Stmwrbuf *sb, const char *fmt, va_list va)
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

static Sint64 Bufio_StmRead(void *streamp, void *buf, size_t nbytes)
{
	return Bufio_Read((Stmrdbuf *) streamp, buf, nbytes);
}

static Sint64 Bufio_StmTell(void *streamp)
{
	Stmrdbuf *sb = (Stmrdbuf *) streamp;

	return (sb->hasError) ? -1 : sb->totalIn + (Sint64) sb->pos;
}

static void Bufio_StmClose(void *streamp)
{
	Bufio_Close((Stmrdbuf *) streamp);
}

static const StmOps _Stm_RdBufOps = {
	Bufio_StmRead,
	NULL,
	NULL,
	Bufio_StmTell,
	NULL,
	Bufio_StmClose
};
static const StmOps _Stm_NcRdBufOps = {
	Bufio_StmRead,
	NULL,
	NULL,
	Bufio_StmTell,
	NULL,
	NULL
};

const StmOps *const Stm_RdBufOps   = &_Stm_RdBufOps;
const StmOps *const Stm_NcRdBufOps = &_Stm_NcRdBufOps;
