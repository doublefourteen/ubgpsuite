// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file      bufio.h
 *
 * Buffered stream writing utilities.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BUFIO_H_
#define DF_BUFIO_H_

#include "stm.h"

#include <stdarg.h>

/// `Stmbuf` buffer size in bytes
#define STM_BUFSIZ 8192uLL

/**
 * Buffered output helper structure.
 *
 * A small `struct` holding an output buffer to help
 * and reduce calls to a stream's `Write()` operation.
 */
typedef struct {
	Sint64        total;            ///< Total bytes flushed to output
	Uint32        len;              ///< Bytes currently buffered
	char          buf[STM_BUFSIZ];  ///< Output buffer
	void         *streamp;          ///< Output stream pointer
	const StmOps *ops;              ///< Output stream operations
} Stmbuf;

/**
 * Flush the buffer to output stream.
 *
 * \return On success returns the **total** bytes written to output
 *         stream since last call to `Bufio_Init()`,
 *         that is the value stored inside `sb->total` field after the flush
 *         operations. Otherwise returns -1.
 *
 * \note Partial flushes are possible on partial writes, in which case
 *       some amount of data will remain buffered in `sb` and may be
 *       flushed later on; `sb->total` and `sb->len` will still be updated
 *       consistently.
 */
Sint64 Bufio_Flush(Stmbuf *sb);

/**
 * Initialize the buffer for writing to `streamp` using the
 * `ops` stream operations.
 *
 * \param [out] sb      Buffer to be initialized, must not be `NULL`
 * \param [in]  streamp Output stream pointer
 * \param [in]  ops     Output stream operations, must not be `NULL`
 *                      and must provide a `Write()` operation
 */
FORCE_INLINE void Bufio_Init(Stmbuf       *sb,
                             void         *streamp,
                             const StmOps *ops)
{
	sb->total   = 0;
	sb->len     = 0;
	sb->streamp = streamp;
	sb->ops     = ops;
}

/**
 * Write a value to buffer, formatted as string.
 *
 * \param [in,out] sb  Buffer to write to, must not be `NULL`
 * \param [in]     val Value to be stringified and written to `sb`
 *
 * \return Number of bytes written to buffer on success,
 *         -1 on error.
 *
 * @{
 *   \fn Sint64 Bufio_Putu(Stmbuf *, unsigned long long)
 *   \fn Sint64 Bufio_Putx(Stmbuf *, unsigned long long)
 *   \fn Sint64 Bufio_Puti(Stmbuf *, long long)
 *   \fn Sint64 Bufio_Putf(Stmbuf *, double)
 * @}
 */
Sint64 Bufio_Putu(Stmbuf *sb, unsigned long long val);
Sint64 Bufio_Putx(Stmbuf *sb, unsigned long long val);
Sint64 Bufio_Puti(Stmbuf *sb, long long val);
Sint64 Bufio_Putf(Stmbuf *sb, double val);

/**
 * Write a single character to `sb`.
 *
 * \return Number of bytes written to `sb` on success (equals to 1),
 *         -1 on error.
 *
 * \note `\0` may be written and buffered like any other `char`.
 */
FORCE_INLINE Sint64 Bufio_Putc(Stmbuf *sb, char c)
{
	if (sb->len == sizeof(sb->buf) && Bufio_Flush(sb) == -1)
		return -1;

	sb->buf[sb->len++] = c;
	return 1;
}

/**
 * \def Bufio_Putsn
 *
 * Write a fixed amount of characters from a string to buffer.
 *
 * \param [in,out] sb     Buffer to write to, must not be `NULL`
 * \param [in]     s      String to pick the characters from
 * \param [in]     nbytes Bytes count to be written to `sb`
 *
 * \return Number of bytes written to `sb` on success (equal to
 *         `nbytes`), -1 on error.
 */
Sint64 _Bufio_Putsn(Stmbuf *, const char *, size_t);

#ifdef __GNUC__
// Optimize to call Bufio_Putc() if 'nbytes' is statically known to be 1
// NOTE: Avoids needless EOLN overhead on Unix
#define Bufio_Putsn(sb, s, nbytes) ( \
	(__builtin_constant_p(nbytes) && (nbytes) == 1) ? \
	    Bufio_Putc(sb, (s)[0]) :                      \
	    _Bufio_Putsn(sb, s, nbytes)                   \
)
#else
#define Bufio_Putsn(sb, s, nbytes) _Bufio_Putsn(sb, s, nbytes)
#endif

/**
 * Write string to buffer.
 *
 * \return Number of bytes written to `sb` on success (equal
 *         to string length), -1 on error.
 */
FORCE_INLINE Sint64 Bufio_Puts(Stmbuf *sb, const char *s)
{
	EXTERNC size_t strlen(const char *); // avoids #include

	return Bufio_Putsn(sb, s, strlen(s));
}

/**
 * `printf()`-like formatted text print to buffer.
 *
 * Write formatted string to buffer, like regular `fprintf()`.
 *
 * \return Number of bytes written to `sb` on success, -1 on error.
 *
 * @{
 *   \fn Sint64 Bufio_Printf(Stmbuf *, const char *, ...)
 *   \fn Sint64 Bufio_Vprintf(Stmbuf *, const char *, va_list)
 * @}
 */
CHECK_PRINTF(2, 3) Sint64 Bufio_Printf(Stmbuf *, const char *, ...);
CHECK_PRINTF(2, 0) Sint64 Bufio_Vprintf(Stmbuf *, const char *, va_list);

#endif
