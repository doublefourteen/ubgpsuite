// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file cpr/flate.h
 *
 * Compressor DEFLATE and inflate stream implementation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * \see [RFC 1950](https://tools.ietf.org/html/rfc1950)
 * \see [RFC 1951](https://tools.ietf.org/html/rfc1951)
 * \see [RFC 1952](https://tools.ietf.org/html/rfc1952)
 */

#ifndef DF_CPR_FLATE_H_
#define DF_CPR_FLATE_H_

#include "stm.h"

/// DEFLATE or inflate stream handle.
typedef struct ZlibStmObj *ZlibStmHn;

/// Supported DEFLATE data formats.
typedef enum {
	ZFMT_RFC1952,  ///< gzip compression, see [RFC 1952](https://tools.ietf.org/html/rfc1952)
	ZFMT_RFC1951,  ///< Original deflate format, see [RFC 1951](https://tools.ietf.org/html/rfc1951)
	ZFMT_RFC1950   ///< Zlib format, see [RFC 1950](https://tools.ietf.org/html/rfc1950)
} Zlibfmt;

/// Inflate (decompression) options.
typedef struct {
	unsigned win_bits;  ///< Compression window size in bits
	Zlibfmt  format;    ///< Input DEFLATE encoding format
	size_t   bufsiz;    ///< Input buffer size in bytes, 0 for default
} InflateOpts;

/// DEFLATE (compression) options.
typedef struct {
	int      compression;  ///< Compression, range `[0-9]` (0 = none, 9 = best)
	unsigned win_bits;     ///< Compression window size in bits
	Zlibfmt  format;       ///< Output DEFLATE format
	size_t   bufsiz;       ///< Output buffer size in bytes, leave to 0 for default
} DeflateOpts;

/// Zlib result status.
typedef Sint64 ZlibRet; // 0 == OK

/**
 * \brief Implementation of the `StmOps` interface for DEFLATE streams.
 *
 * Passing these interfaces to any API working with streams allows it to
 * operate on DEFLATE streams.
 * `Zlib_StmOps` implements the `Close()` method, while the non-closing
 * `Zlib_NcStmOps` leaves `Close()` to `NULL`, effectively preventing any
 * attempt to close such stream. Use this variant when this behavior is
 * desirable (e.g. streams similar to `stdout` or `stdin`).
 *
 * @{
 *   \var Zlib_StmOps
 *   \var Zlib_NcStmOps
 * @}
 */
extern const StmOps *const Zlib_StmOps;
extern const StmOps *const Zlib_NcStmOps;

/// Return last Zlib operation's return status.
ZlibRet Zlib_GetErrStat(void);
/// Convert `ZlibRet` result to human readable string.
const char *Zlib_ErrorString(ZlibRet res);

/**
 * \brief Start Zlib decompression over a stream.
 *
 * \param [in,out] streamp Compressed input source stream
 * \param [in]     ops     Read operations over `streamp`, must not be `NULL` and provide `Read()`
 * \param [in]     opts    Decompression options, `NULL` for defaults
 *
 * \return Opened Zlib handle on success, `NULL` on failure.
 */
ZlibStmHn Zlib_InflateOpen(void *streamp, const StmOps *ops, const InflateOpts *opts);
/**
 * \brief Start Zlib compression over a stream.
 *
 * \param [in,out] streamp Destination for compressed output
 * \param [in]     ops     Write operations over `ßtreamp`, must not be `NULL` and provide `Write()`
 * \param [in]     opts    Compression options, `NULL` for defaults.
 *
 * \return Opened Zlib handle on success, `NULL` on failure.
 */
ZlibStmHn Zlib_DeflateOpen(void *streamp, const StmOps *ops, const DeflateOpts *opts);

/**
 * \brief Decompress `nbytes` bytes from `hn` into `buf`.
 *
 * \return Number of bytes actually written to `buf`, at most `nbytes`,
 *         0 on end of stream, -1 on error.
 */
Sint64 Zlib_Read(ZlibStmHn hn, void *buf, size_t nbytes);
/**
 * \brief Compresses `nbytes` bytes from `buf` to `hn`.
 *
 * \return Count of bytes actually compressed to `hn`, at most `nbytes`,
 *         -1 on error.
 *
 * \note Compression should be finalized with `Zlib_Finish()` once all
 *       data is written.
 */
Sint64 Zlib_Write(ZlibStmHn hn, const void *buf, size_t nbytes);

/**
 * \brief Finalize DEFLATE compression.
 *
 * \return `OK` on success, `NG` otherwise.
 */
Judgement Zlib_Finish(ZlibStmHn hn);

/// Close Zlib stream handle.
void Zlib_Close(ZlibStmHn hn);

#endif
