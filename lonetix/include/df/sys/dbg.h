// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/dbg.h
 *
 * Debugging utilities to retrieve stack trace and symbol names.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_DBG_H_
#define DF_SYS_DBG_H_

#include "sys/con.h"

/// Helper union to cast `void *` to function pointer - be sad if you use it.
typedef union {
	void  *sym;          ///< Function as a `void *`
	void (*func)(void);  ///< Function pointer.
} Funsym;

STATIC_ASSERT(sizeof(void *) == sizeof(void (*)(void)), "Ill-formed Funsym for target platform");

/// Test whether the current process is being executed under a debugger.
Boolean Sys_IsDebuggerPresent(void);
/**
 * \brief Get a symbol's name, if available.
 *
 * \param [in] sym Symbol whose name is needed.
 *
 * \return A pointer to a statically allocated thread-local buffer containing
 *         a `\0` terminated symbol name on success, `NULL` if no name could be
 *         retrieved.
 */
char *Sys_GetSymbolName(void *sym);
/**
 * \brief Dump at most `n` backtrace entries to `dest`.
 *
 * Returned backtrace does not include `Sys_GetBacktrace()`
 * itself, and has the caller as the topmost symbol.
 *
 * \return Number of entries actually returned to `dest` on success,
 *         0 when no backtrace information is available.
 */
size_t Sys_GetBacktrace(void **dest, size_t n);
/**
 * \brief Get the caller's caller.
 *
 * Useful for quick debugging:
 * ```c
 * printf("Called by: %s\n", Sys_GetSymbolName(Sys_GetCaller()));
 * ```
 */
void *Sys_GetCaller(void);
/// Dump backtrace to console (typically `STDERR`).
void Sys_DumpBacktrace(ConHn hn, void **trace, size_t n);

#endif
