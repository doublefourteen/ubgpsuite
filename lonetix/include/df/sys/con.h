// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/con.h
 *
 * System console interface.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * Provides basic functionality to print to system console and
 * test for supported capabilities.
 */

#ifndef DF_SYS_CON_H_
#define DF_SYS_CON_H_

#include "stm.h"

#include <stdarg.h>

/**
 * \typedef ConHn
 * \brief Platform console handle descriptor.
 *
 * \def STDIN
 * \brief Platform Standard Input (`stdin`) handle.
 * \def STDOUT
 * \brief Platform Standard Output (`stdout`) handle.
 * \def STDERR
 * \brief Platform Standard Error (`stderr`) handle.
 *
 * \fn Fildes CON_FILDES(ConHn)
 *
 * \brief Convert a `ConHn` to a `Fildes`, making the console handle usable with
 * regular platform file API.
 */
#ifdef _WIN32

typedef Uint32 /*DWORD*/ ConHn;

#define STDIN  ((ConHn) -10)  // STD_INPUT_HANDLE
#define STDOUT ((ConHn) -11)  // STD_OUTPUT_HANDLE
#define STDERR ((ConHn) -12)  // STD_ERROR_HANDLE

Fildes CON_FILDES(ConHn hn);

#else

typedef int ConHn;

#define STDIN  ((ConHn) 0)
#define STDOUT ((ConHn) 1)
#define STDERR ((ConHn) 2)

FORCE_INLINE Fildes CON_FILDES(ConHn hn)
{
	return (Fildes) hn;  // trivial on Unix
}

#endif

/**
 * \brief Convert a `ConHn` to a `streamp` pointer for `StmOps`.
 *
 * \see `Stm_ConOps`
 */
FORCE_INLINE void *STM_CONHN(ConHn hn)
{
	STATIC_ASSERT(sizeof(Sintptr) >= sizeof(void *), "ConHn ill formed on this platform");
	return (void *) ((Sintptr) hn);
}

/**
 * \brief `StmOps` interface operating on console output.
 *
 * There is no `Close()` function for consoles.
 */
extern const StmOps *const Stm_ConOps;

/**
 * Print string to console.
 *
 * \note No newline is appended.
 */
void Sys_Print(ConHn hn, const char *s);

/// Formatted print to console handle.
CHECK_PRINTF(2, 3) void Sys_Printf(ConHn hn, const char *fmt, ...);
/// `Sys_Printf()` variant using `va_list`.
CHECK_PRINTF(2, 0) void Sys_VPrintf(ConHn hn, const char *fmt, va_list va);
/**
 * \brief Read at most `nbytes` characters from `hn` to `buf`, input is *not* `\0` terminated.
 *
 * \return Count of `char`s written to `buf`.
 */
size_t Sys_Read(ConHn hn, char *buf, size_t nbytes);
/// Non-Blocking variant of `Sys_Read()`.
size_t Sys_NbRead(ConHn hn, char *buf, size_t nbytes);  // NON-BLOCKING

/// Test whether `hn` supports VT100 commands.
Boolean Sys_IsVt100Console(ConHn hn);

#endif
