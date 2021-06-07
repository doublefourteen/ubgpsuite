// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/fsdef.h
 *
 * Platform-specific filesystem types and definitions.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_SYS_FSDEF_H_
#define DF_SYS_FSDEF_H_

#include "xpt.h"

/**
 * \typedef Fildes
 * \brief Platform specific file handle (`HANDLE` on Windows, `int` elsewhere).
 *
 * \def FILDES_BAD
 * \brief Bad file descriptor.
 *
 * \def PATH_SEP
 * \brief Path separator character (`\` on Windows, `/` elsewhere).
 *
 * \def EOLN
 * \brief Text file newline sequence (`\r\n` on Windows, `\n` elsewhere).
 */

#ifdef _WIN32
typedef void *Fildes;

#define FILDES_BAD 0

#define PATH_SEP '\\'

#define EOLN "\r\n"

#else
typedef int Fildes;

#define FILDES_BAD -1

#define PATH_SEP '/'

#define EOLN "\n"

#endif

/// I/O stream seek modes.
typedef enum {
	SK_SET,  ///< Seek from beginning of stream
	SK_CUR,  ///< Seek from current position
	SK_END   ///< Seek from stream end
} SeekMode;

#endif
