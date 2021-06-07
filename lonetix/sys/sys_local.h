// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/sys_local.h
 *
 * Private functionality for system layer and error reporting.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_LOCAL_H_
#define DF_SYS_LOCAL_H_

#include "sys/sys.h"

/// Set system error code and message.
#define Sys_SetErrStat(code, msg) \
	_Sys_SetErrStat(code, msg, __FILE__, __func__, __LINE__, 0)

// NOTE: implementation detail, should only be called through `Sys_SetErrStat()`.
NOINLINE Judgement _Sys_SetErrStat(SysRet             code,
                                   const char        *msg,
                                   const char        *filename,
                                   const char        *func,
                                   unsigned long long line,
                                   unsigned           depth);

/// Abort execution with error.
#define Sys_FatalError(code, msg) \
	_Sys_FatalError(code, msg, __FILE__, __func__, __LINE__, 0)

NORETURN NOINLINE void _Sys_FatalError(SysRet             code,
                                       const char        *msg,
                                       const char        *filename,
                                       const char        *func,
                                       unsigned long long line,
                                       unsigned           depth);

#endif
