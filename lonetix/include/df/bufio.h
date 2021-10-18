// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file      bufio.h
 *
 * Buffered stream input/output utilities.
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
	Sint64        totalOut;         ///< Total bytes flushed to output
	Uint32        availOut;         ///< Bytes currently buffered
	char          buf[STM_BUFSIZ];  ///< Output buffer
	void         *streamp;          ///< Output stream pointer
	const StmOps *ops;              ///< Output stream operations
} Stmwrbuf;

/**
 * Buffered input helper structure.
 *
 * A small `struct` holding an input buffer.
 * Reading variant of the `Stmwrbuf` structure.
 *
 * In contrast with `Stmwrbuf`, this struct may be used
 * as a stream with `Stm_RdBufOps` and `Stm_NcRdBufOps`.
 */
typedef struct {
	Sint64        totalIn;          ///< Total bytes read from input.
	Uint16        availIn;          ///< Bytes currently buffered.
	Uint16        pos;              ///< Offset inside buffer to next byte to be returned.
	Boolean8      hasError;         ///< Whether a read error was encountered
	Boolean8      isEof;            ///< Whether last Read() from input returned 0
	char          buf[STM_BUFSIZ];  ///< Input buffer
	void         *streamp;          ///< Input stream pointer
	const StmOps *ops;              ///< Input stream operations
} Stmrdbuf;

/// Clear `Stmrdbuf` error flag, if set.
FORCE_INLINE void BUFIO_CLRERROR(Stmrdbuf *sb)
{
	sb->hasError = FALSE;
}

/// Clear `Stmrdbuf` EOF flag, if set.
FORCE_INLINE void BUFIO_CLREOF(Stmrdbuf *sb)
{
	sb->isEof = FALSE;
}

/**
 * Initialize the buffer for reading from `streamp` using the
 * `ops` stream operations.
 *
 * \param [out] sb      Buffer to be initialized, must not be `NULL`
 * \param [in]  streamp Output stream pointer
 * \param [in]  ops     Output stream operations, must not be `NULL`
 *                      and must provide a `Read()` operation
 */
FORCE_INLINE void Bufio_RdInit(Stmrdbuf *sb, void *streamp, const StmOps *ops)
{
	sb->totalIn  = 0;
	sb->availIn  = 0;
	sb->hasError = FALSE;
	sb->isEof    = FALSE;
	sb->streamp  = streamp;
	sb->ops      = ops;
}

Sint64 Bufio_Read(Stmrdbuf *sb, void *buf, size_t nbytes);
void Bufio_Close(Stmrdbuf *sb);

/**
 * Flush the buffer to output stream.
 *
 * \return On success returns the **total** bytes written to output
 *         stream since last call to `Bufio_WrInit()`,
 *         that is the value stored inside `sb->totalOut` field after the flush
 *         operations. Otherwise returns -1.
 *
 * \note Partial flushes are possible on partial writes, in which case
 *       some amount of data will remain buffered in `sb` and may be
 *       flushed later on; `sb->totalOut` and `sb->len` will still be updated
 *       consistently.
 */
Sint64 Bufio_Flush(Stmwrbuf *sb);

/**
 * Initialize the buffer for writing to `streamp` using the
 * `ops` stream operations.
 *
 * \param [out] sb      Buffer to be initialized, must not be `NULL`
 * \param [in]  streamp Output stream pointer
 * \param [in]  ops     Output stream operations, must not be `NULL`
 *                      and must provide a `Write()` operation
 */
FORCE_INLINE void Bufio_WrInit(Stmwrbuf     *sb,
                               void         *streamp,
                               const StmOps *ops)
{
	sb->totalOut = 0;
	sb->availOut = 0;
	sb->streamp  = streamp;
	sb->ops      = ops;
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
 *   \fn Sint64 Bufio_Putu(Stmwrbuf *, unsigned long long)
 *   \fn Sint64 Bufio_Putx(Stmwrbuf *, unsigned long long)
 *   \fn Sint64 Bufio_Puti(Stmwrbuf *, long long)
 *   \fn Sint64 Bufio_Putf(Stmwrbuf *, double)
 * @}
 */
Sint64 Bufio_Putu(Stmwrbuf *sb, unsigned long long val);
Sint64 Bufio_Putx(Stmwrbuf *sb, unsigned long long val);
Sint64 Bufio_Puti(Stmwrbuf *sb, long long val);
Sint64 Bufio_Putf(Stmwrbuf *sb, double val);

/**
 * Write a single character to `sb`.
 *
 * \return Number of bytes written to `sb` on success (equals to 1),
 *         -1 on error.
 *
 * \note `\0` may be written and buffered like any other `char`.
 */
FORCE_INLINE Sint64 Bufio_Putc(Stmwrbuf *sb, char c)
{
	if (sb->availOut == sizeof(sb->buf) && Bufio_Flush(sb) == -1)
		return -1;

	sb->buf[sb->availOut++] = c;
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
Sint64 _Bufio_Putsn(Stmwrbuf *, const char *, size_t);
Sint64 _Bufio_SmallPutsn(Stmwrbuf *, const char *, size_t);

#ifdef __GNUC__
// Optimize to call Bufio_Putc() if 'nbytes' is statically known to be 1
// NOTE: Avoids needless EOLN overhead on Unix
#define Bufio_Putsn(sb, s, nbytes) (                       \
	(__builtin_constant_p(nbytes) && (nbytes) <= 64) ?     \
	(((nbytes) == 1) ? Bufio_Putc(sb, (s)[0])              \
	:                 _Bufio_SmallPutsn(sb, s, nbytes))    \
	:                 _Bufio_Putsn(sb, s, nbytes)          \
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
FORCE_INLINE Sint64 Bufio_Puts(Stmwrbuf *sb, const char *s)
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
 *   \fn Sint64 Bufio_Printf(Stmwrbuf *, const char *, ...)
 *   \fn Sint64 Bufio_Vprintf(Stmwrbuf *, const char *, va_list)
 * @}
 */
CHECK_PRINTF(2, 3) Sint64 Bufio_Printf(Stmwrbuf *, const char *, ...);
CHECK_PRINTF(2, 0) Sint64 Bufio_Vprintf(Stmwrbuf *, const char *, va_list);

extern const StmOps *const Stm_RdBufOps;
extern const StmOps *const Stm_NcRdBufOps;

#endif
