// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/dbg_unix.c
 *
 * Implements debugging utilities for Unix platforms.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/dbg.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __GNUC__
#include <execinfo.h>
#endif

Boolean Sys_IsDebuggerPresent(void)
{
#ifdef __linux__
	// Read process status file
	char buf[4096];

	int fd = open("/proc/self/status", O_RDONLY);
	if (fd < 0)
		return FALSE;

	ssize_t n = read(fd, buf, sizeof(buf) - 1);
	close(fd);

	if (n < 0)
		return FALSE;

	buf[n] = '\0';

	const char *tracer = "TracerPid:\t";
	const char *pos    = strstr(buf, tracer);
	if (!pos)
		return FALSE;

	pos += strlen(tracer);

	pid_t pid = atol(pos);
	return pid > 0;

#else
	// TODO MacOS, BSD via sysctl()
	// http://developer.apple.com/qa/qa2004/qa1361.html
	// https://github.com/chromium/chromium/blob/master/base/debug/debugger_posix.cc
	return FALSE;
#endif
}

size_t Sys_GetBacktrace(void **dest, size_t n)
{
	USED(dest);
	USED(n);

#ifdef __GNUC__
	if (n > (size_t) (INT_MAX - 1))
		n = INT_MAX - 1;

	void **trace = (void **) alloca(sizeof(*trace) * (n + 1));

	int res = backtrace(trace, n + 1);
	if (res > 0) {
		// Exclude ourselves from the trace
		res--;

		memcpy(dest, trace + 1, res * sizeof(*dest));
		return res;
	}
#endif

	return 0;
}

void *Sys_GetCaller(void)
{
#ifdef __GNUC__
	void *trace[3];  // ourselves, caller, caller's caller

	int n = backtrace(trace, ARRAY_SIZE(trace));
	if (n == ARRAY_SIZE(trace))
		return trace[2];

#endif

	return NULL;
}

char *Sys_GetSymbolName(void *sym)
{
	USED(sym);

#ifdef __GNUC__
	static THREAD_LOCAL char buf[128];

	char **names = backtrace_symbols(&sym, 1);
	if (names) {
		size_t n = strlen(names[0]);
		if (n > sizeof(buf) - 1) {
			// Truncate symbol
			n = sizeof(buf) - 4;

			memcpy(buf, names[0], n);
			buf[n+0] = '.'; buf[n+1] = '.'; buf[n+2] = '.';
			buf[n+3] = '\0';

		} else {
			memcpy(buf, names[0], n + 1);
		}

		free(names);
		return buf;
	}
#endif

	return NULL;
}

void Sys_DumpBacktrace(ConHn hn, void **trace, size_t n)
{
	USED(hn);
	USED(trace);
	USED(n);

#ifdef __GNUC__
	backtrace_symbols_fd(trace, MIN(n, (size_t) INT_MAX), hn);
#endif
}
