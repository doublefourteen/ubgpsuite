// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file      sys/sys.h
 *
 * Miscellaneous system functionality.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_ERR_H_
#define DF_SYS_ERR_H_

#include "srcloc.h"

#include <stdarg.h>

#ifdef _MSC_VER
// for Sys_WipeMemory()
#include <intrin.h>
#pragma intrinsic(_ReadWriteBarrier)
#endif

/**
 * \typedef SysRet
 * \brief Platform-specific error code for system operations.
 */
typedef int SysRet;  // errno type

/**
 * \brief Error status structure.
 *
 * Stores latest operation result and current error handling callback.
 */
typedef struct {
	/// Latest operation result status.
	SysRet   code;
	/**
	 * \brief Error handler callback.
	 *
	 * \param [in] code   System error code
	 * \param [in] reason Human readable concise message providing context to error, never `NULL`
	 * \param [in] loc    Source location where error occurred, `NULL` if unavailable
	 * \param [in] obj    User defined additional data forwarded to callback unaltered
	 */
	void (*func)(SysRet code, const char *reason, Srcloc *loc, void *obj);
	/// Additional user data to be forwarded to `func`.
	void  *obj;
} SysErrStat;

/**
 * \brief Abort error handler callback.
 *
 * This error handler terminates execution abnormally,
 * attempting to log an error message as accurate as possible using system
 * specific facilities.
 *
 * If this handler is registered, execution shall terminate as soon as an
 * exceptional error condition is encountered in the system layer.
 */
#define SYS_ERR_ABORT ((void (*)(SysRet, const char *, Srcloc *, void *)) -1)
/**
 * \brief Ignoring error handler callback.
 *
 * If this handler is registered any system error is ignored.
 */
#define SYS_ERR_IGN   ((void (*)(SysRet, const char *, Srcloc *, void *))  0)
/**
 * \brief Terminating error handler callback.
 *
 * Terminates execution providing sensible error message, it differs
 * from `SYS_ERR_ABORT` in that no exceptional termination semantics is
 * implied, but rather an unsuccessful event that caused immediate exit
 * (no core dump or stack traces are left behind).
 */
#define SYS_ERR_QUIT  ((void (*)(SysRet, const char *, Srcloc *, void *))  1)

/**
 * \brief Install a system error handler callback.
 *
 * Initially installed handler is `SYS_ERR_IGN`,
 * thus any error is effectively ignored unless a new handler is installed.
 *
 * Once installed, the error callback is called immediately for every
 * exceptional error condition encountered inside system layer.
 *
 * Error handlers are thread-local, thus different threads may implement
 * appropriate error handling policy without interfering with each other.
 *
 * \param [in] func Error handler function
 * \param [in] obj  Custom user-provided object passed unaltered upon callback
 *
 * \see `SysErrStat` for callback documentation
 */
void Sys_SetErrFunc(void (*func)(SysRet, const char *, Srcloc *, void *), void *obj);

/**
 * Retrieve the system error status.
 *
 * \param [out] stat Storage for returned error status, may be `NULL`
 *
 * \return Last operation result status.
 */
SysRet Sys_GetErrStat(SysErrStat *stat);

/**
 * Trigger an out of memory error.
 *
 * @note Depending on the current error policy, execution may
 *       very well continue after call returns.
 */
 #define Sys_OutOfMemory() _Sys_OutOfMemory(__FILE__, __func__, __LINE__, 0)

// NOTE: implementation detail, should always be called through `Sys_OutOfMemory()`.
NOINLINE void _Sys_OutOfMemory(const char *,
                               const char *,
                               unsigned long long,
                               unsigned);

/**
 * \brief Zero-out `nbytes` bytes inside `data`, preventing compiler optimization.
 *
 * A compiler might optimize-out `memset()` or similar operations if `data`
 * is never read again.
 * This function ensures such operation is never optimized away, which
 * may be useful to zero-out sensitive data.
 */
FORCE_INLINE void Sys_WipeMemory(volatile void *data, size_t nbytes)
{
#if defined(_MSC_VER) || defined(__GNUC__)
	EXTERNC void *memset(void *, int, size_t);

	memset((void *) data, 0, nbytes);

	// Compiler fence
#ifdef _MSC_VER
	_ReadWriteBarrier();
#else
	__asm__ __volatile__ ("" : : : "memory");
#endif

#else
	// Slow but portable
	volatile Uint8 *p = (volatile Uint8 *) data;
	while (nbytes--)
		*p++ = 0x00;
#endif
}

/// Sleep *at least* for the specified amount of **milliseconds**.
void Sys_SleepMillis(Uint32 millis);

#endif
