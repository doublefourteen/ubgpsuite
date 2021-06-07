// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file cpr/xz.h
 *
 * Compressors, LZMA/LZMA2 encoding and decoding implementation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_CPR_XZ_H_
#define DF_CPR_XZ_H_

#include "stm.h"

/// LZMA/LZMA2 stream handle.
typedef struct XzStmObj *XzStmHn;

/// LZMA/LZMA2 data integrity checksum algorithms.
typedef enum {
	XZCHK_NONE   = 0,  ///< Do not include any data checksum
	XZCHK_CRC32  = 1,  ///< Cyclic Redundancy Check, 32-bit
	XZCHK_CRC64  = 4,  ///< Cyclic Redundancy Check, 64-bit
	XZCHK_SHA256 = 10  ///< Secure Hash Algorithm, 256 bit
} Xzchk;

/// LZMA/LZMA2 compressor flags.
typedef struct {
	unsigned compress;  ///< Compression level, range [0-9]
	Boolean  extreme;   ///< Severely sacrifice speed for compression
	Xzchk    chk;       ///< Checksum algorithm to use
	size_t   bufsiz;    ///< Output buffer size in bytes
} XzEncOpts;

/// LZMA/LZMA2 decompression flags.
typedef struct {
	Uint64  memlimit;   ///< Decoder memory usage limit
	Boolean no_concat;  ///< Do not support concatenated xz streams
	Boolean no_chk;     ///< Disregard data checksum during decoding
	size_t  bufsiz;     ///< Input buffer size in bytes
} XzDecOpts;

/// LZMA operation status result.
typedef int XzRet;  // 0 == OK

/**
 * \brief Implementation of the `StmOps` interface for LZMA streams.
 *
 * Allows any API working with streams to function with LZMA streams.
 * `Xz_StmOps` implements the `Close()` method, while the non-closing
 * `Xz_NcStmOps` leaves `Close()` to `NULL`, preventing any
 * attempt to close the stream. Use this variant when such behavior is
 * desirable (e.g. streams similar to `stdout` or `stdin`).
 *
 * @{
 *   \var Xz_StmOps
 *   \var Xz_NcStmOps
 * @}
 */
extern const StmOps *const Xz_StmOps;
extern const StmOps *const Xz_NcStmOps;

/// Retrieve last operation's result status.
XzRet Xz_GetErrStat(void);
/// Convert `XzRet` to human readable string.
const char *Xz_ErrorString(XzRet res);

/**
 * \brief Open stream for compression.
 *
 * \param [in,out] streamp Output stream for compressed LZMA data
 * \param [in]     ops     Write operations interface for `streamp`, must not be `NULL` and provide `Write()`
 * \param [in]     opts    Compression options, may be `NULL` for defaults
 *
 * \return The LZMA compressor handle on success, `NULL` on failure.
 */
XzStmHn Xz_OpenCompress(void *streamp, const StmOps *ops, const XzEncOpts *opts);
/**
 * \brief Open an LZMA stream for decompressing.
 *
 * \param [in,out] streamp Input stream for LZMA compressed data
 * \param [in]     ops     Read operations interface for `streamp`, must not be `NULL` and provide `Read()`
 * \param [in]     opts    Decompression options, may be `NULL` for defaults
 *
 * \return The LZMA decompressor handle on success, `NULL` on failure.
 */
XzStmHn Xz_OpenDecompress(void *streamp, const StmOps *ops, const XzDecOpts *opts);

/**
 * \brief Decompress `nbytes` bytes from `hn` into `buf`.
 *
 * \return Number of bytes actually written to `buf`, at most `nbytes`,
 *         0 on end of stream, -1 on error.
 */
Sint64 Xz_Read(XzStmHn hn, void *buf, size_t nbytes);
/**
 * \brief Compresses `nbytes` bytes from `buf` to `hn`.
 *
 * \return Count of bytes actually compressed to `hn`, at most `nbytes`,
 *         -1 on error.
 *
 * \note Compression should be finalized with `Xz_Finish()` once all
 *       data is written.
 */
Sint64 Xz_Write(XzStmHn hn, const void *buf, size_t nbytes);

/**
 * \brief Finalize LZMA compression.
 *
 * \return `OK` on success, `NG` otherwise.
 */
Judgement Xz_Finish(XzStmHn hn);

/// Close LZMA stream.
void Xz_Close(XzStmHn hn);

#endif
