// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file      srcloc.h
 *
 * Source location information type.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SRCLOC_H_
#define DF_SRCLOC_H_

#include "xpt.h"

/**
 * Source location information, useful to pack toghether
 * information from `__FILE__`, `__LINE__`, and `__func__` for error reporting
 * purposes.
 */
typedef struct {
	const char        *filename;    ///< Filename the error was raised from
	unsigned long long line;        ///< Line that triggered the error
	const char        *func;        ///< Function that raised the error
	unsigned           call_depth;  ///< Call depth, number of functions skipped for backtrace
} Srcloc;

/**
 * Declare a `Srcloc` variable named `var` containing location
 * info about next, current, or previous source line.
 *
 * \param var Identifier name to assign to the newly created variable
 *
 * @{
 *   @def SRCLOC_NEXT_LINE
 *   @def SRCLOC_THIS_LINE
 *   @def SRCLOC_PREV_LINE
 * @}
 */
#define SRCLOC_NEXT_LINE(var) Srcloc var = { __FILE__, __LINE__ + 1, __func__, 0 }
#define SRCLOC_THIS_LINE(var) Srcloc var = { __FILE__, __LINE__, __func__, 0 }
#define SRCLOC_PREV_LINE(var) Srcloc var = { __FILE__, __LINE__ - 1, __func__, 0 }

#endif
