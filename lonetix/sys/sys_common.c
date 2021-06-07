// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/sys_common.c
 *
 * Cross-system utilities for system layer and error management.
 *
 * \copyright The DoubleFourteen Code Forge (C)  All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/sys_local.h"

#include "sys/dbg.h"
#include "argv.h"
#include "numlib.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXTRACE 64

// Define this to always omit backtrace from Sys_Abort()
// #define NSTKTRACE

static THREAD_LOCAL SysErrStat sys_errs;

static void ReportError(SysRet        code,
                        const char   *reason,
                        const Srcloc *loc,
                        Boolean       includeFileLoc)
{
	const char *err = strerror(code);

	Sys_Print(STDERR, "\t");

	if (loc) {
		if (includeFileLoc && loc->filename && loc->line > 0) {
			char buf[64];

			Utoa(loc->line, buf);
			Sys_Print(STDERR, "Error occurred in file ");
			Sys_Print(STDERR, loc->filename);
			Sys_Print(STDERR, ":");
			Sys_Print(STDERR, buf);
			Sys_Print(STDERR, "\n\t");
		}
		if (loc->func) {
			Sys_Print(STDERR, "[");
			Sys_Print(STDERR, loc->func);
			Sys_Print(STDERR, "()]: ");
		}
	}

	Sys_Print(STDERR, reason);
	if (err) {
		Sys_Print(STDERR, ": ");
		Sys_Print(STDERR, err);
	}

	Sys_Print(STDERR, ".\n");
}

NORETURN NOINLINE static void Sys_Abort(SysRet      code,
                                        const char *reason,
                                        Srcloc     *loc,
                                        void       *obj)
{
	USED(obj);

	loc->call_depth++;  // include Sys_Abort() as well

	if (com_progName) {
		Sys_Print(STDERR, com_progName);
		Sys_Print(STDERR, ": ");
	}

	Sys_Print(STDERR, "FATAL ERROR - Execution aborted in response to an unrecoverable system error.\n");

	ReportError(code, reason, loc, /*includeFileLoc=*/TRUE);

#ifndef NSTKTRACE
	void *trace[MAXTRACE + 1];  // +1 for detecting overflow

	unsigned call_depth = loc->call_depth;
	unsigned ntrace     = Sys_GetBacktrace(trace, ARRAY_SIZE(trace));
	if (ntrace > call_depth) {
		ntrace -= call_depth;  // omit error propagation from trace

		Sys_Print(STDERR, "\nBacktrace:\n");

		Sys_DumpBacktrace(STDERR, &trace[call_depth], MIN(ntrace, MAXTRACE));
		if (ntrace > MAXTRACE)
			Sys_Print(STDERR, "... [additional frames omitted]\n");

	} else {
		Sys_Print(STDERR, "No backtrace information available.\n");
	}
#endif
#ifndef NDEBUG
	if (Sys_IsDebuggerPresent())
		raise(SIGTRAP);  // bail out to debugger
#endif

	abort();
}

NORETURN NOINLINE static void Sys_Quit(SysRet      code,
                                       const char *reason,
                                       Srcloc     *loc,
                                       void       *obj)
{
	USED(obj);

	loc->call_depth++;  // include Sys_Quit() as well

	if (com_progName) {
		Sys_Print(STDERR, com_progName);
		Sys_Print(STDERR, ": ");
	}

	Sys_Print(STDERR, "Terminate called in response to an unrecoverable system error\n");

	Boolean includeFileLoc = FALSE;
#ifndef NDEBUG
	includeFileLoc = TRUE;
#endif

	ReportError(code, reason, loc, includeFileLoc);

	_Exit(EXIT_FAILURE);
}

Judgement _Sys_SetErrStat(SysRet             code,
                          const char        *reason,
                          const char        *filename,
                          const char        *func,
                          unsigned long long line,
                          unsigned           depth)
{
	sys_errs.code = code;
	if (code == 0)
		return OK;

	void (*err_func)(SysRet, const char *, Srcloc *, void *);
	Srcloc loc;

	err_func = sys_errs.func;

	// Remap SYS_ERR_ABORT and SYS_ERR_QUIT to correct function
	if (err_func == SYS_ERR_ABORT)
		err_func = Sys_Abort;
	if (err_func == SYS_ERR_QUIT)
		err_func = Sys_Quit;

	if (err_func) {
		loc.filename   = filename;
		loc.func       = func;
		loc.line       = line;
		loc.call_depth = depth + 1;  // +1 for this function

		err_func(code, reason, &loc, sys_errs.obj);
	}

	return NG;
}

void _Sys_OutOfMemory(const char        *file,
                      const char        *func,
                      unsigned long long line,
                      unsigned           depth)
{
	depth++;  // include _Sys_OutOfMemory() as well

	_Sys_SetErrStat(ENOMEM, "Out of memory", file, func, line, depth);
}

void _Sys_FatalError(SysRet             code,
                     const char        *reason,
                     const char        *file,
                     const char        *func,
                     unsigned long long line,
                     unsigned           depth)
{
	depth++;  // include _Sys_FatalError() as well

	Srcloc loc;

	loc.filename   = file;
	loc.func       = func;
	loc.line       = line;
	loc.call_depth = depth;

	Sys_Abort(code, reason, &loc, NULL);
}

SysRet Sys_GetErrStat(SysErrStat *stat)
{
	if (stat)
		*stat = sys_errs;

	return sys_errs.code;
}

void Sys_SetErrFunc(void (*func)(SysRet, const char *, Srcloc *, void *),
                    void  *obj)
{
	sys_errs.func = func;
	sys_errs.obj = obj;
}
