// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/con_unix.c
 *
 * Implements Unix console Input/Output.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/con.h"
#include "sys/fs.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int STM_TOFILDES(void *streamp)
{
	return (int) ((Sintptr) streamp);
}

static Sint64 Sys_OpConRead(void *streamp, void *buf, size_t nbytes)
{
	return Sys_Fread(STM_TOFILDES(streamp), buf, nbytes);
}

static Sint64 Sys_OpConWrite(void *streamp, const void *buf, size_t nbytes)
{
	return Sys_Fwrite(STM_TOFILDES(streamp), buf, nbytes);
}

static Judgement Sys_OpConFinish(void *streamp)
{
	USED(streamp);

	return OK;  // NOP
}

static const StmOps con_stmOps = {
	Sys_OpConRead,
	Sys_OpConWrite,
	NULL,
	NULL,
	Sys_OpConFinish,
	NULL
};

const StmOps *const Stm_ConOps = &con_stmOps;

void Sys_Print(ConHn hn, const char *s)
{
	(void) write(hn, s, strlen(s));
}

void Sys_VPrintf(ConHn hn, const char *fmt, va_list va)
{
	va_list vc;

	int   n1, n2;
	char *buf;

	va_copy(vc, va);
	n1 = vsnprintf(NULL, 0, fmt, vc);
	va_end(vc);
	if (n1 <= 0)
		return;

	buf = (char *) alloca(n1 + 1);
	n2  = vsnprintf(buf, n1 + 1, fmt, va);
	if (n2 <= 0)
		return;

	assert(n2 == n1);

	(void) write(hn, buf, n2);
}

void Sys_Printf(ConHn hn, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	Sys_VPrintf(hn, fmt, va);
	va_end(va);
}

size_t Sys_Read(ConHn hn, char *buf, size_t n)
{
	ssize_t nr = read(hn, buf, n);
	if (nr < 0)
		nr = 0;

	return nr;
}

size_t Sys_NbRead(ConHn hn, char *buf, size_t n)
{
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(hn, &rfds);

	tv.tv_sec  = 0;
	tv.tv_usec = 0;
	if (select(1 + hn, &rfds, NULL, NULL, &tv) != 1)
		return 0;

	ssize_t nr = read(hn, buf, n);
	if (nr < 0)
		nr = 0;

	return nr;
}

Boolean Sys_IsVt100Console(ConHn hn)
{
	static const char *const knownTerms[] = {
		"xterm",         "xterm-color",     "xterm-256color",
		"screen",        "screen-256color", "tmux",
		"tmux-256color", "rxvt-unicode",    "rxvt-unicode-256color",
		"linux",         "cygwin"
	};

	const char *term = getenv("TERM");
	if (!term)
		return FALSE;

	size_t i;
	for (i = 0; i < ARRAY_SIZE(knownTerms); i++) {
		if (strcmp(term, knownTerms[i]) == 0)
			break;
	}
	return i != ARRAY_SIZE(knownTerms) && isatty(hn);
}
