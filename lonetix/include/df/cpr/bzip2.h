// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file cpr/bzip2.h
 *
 * Burrows–Wheeler bzip2 compression streaming support library.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_CPR_BZIP2_H_
#define DF_CPR_BZIP2_H_

#include "stm.h"

/// Bzip2 stream handle.
typedef struct Bzip2StmObj *Bzip2StmHn;

/**
 * \brief BZip2 Compression options.
 *
 * \see `Bz2_InitCompress()`
 */
typedef struct {
	/**
	 * \brief Compression level, from 1 to 9 inclusive.
	 *
	 * Higher values imply better compression, at the price of speed loss
	 * and memory consumption.
	 * Out of range values are silently clamped to allowed range.
	 */
	int compression;
	/// Compressor load factor.
	int factor;
	/// Compressor buffer size in bytes, 0 to use suggested value.
	size_t bufsiz;
	/**
	 * \brief Debugging message verbosity.
	 *
	 * Values higher than 0 make the stream log debug messages to standard
	 * error, debugging level goes from 0 to 4 inclusive.
	 * Larger values are silently truncated to the maximum allowed.
	 */
	unsigned verbose;
} Bzip2CprOpts;

/**
 * \brief Decompression options.
 *
 * \see `Bz2_InitDecompress()`
 */
typedef struct {
	/// Decompressor buffer size in bytes, 0 to use suggested value.
	size_t bufsiz;
	/// Conserve memory during decompression, in spite of speed.
	Boolean low_mem;
	/**
	 * \brief Debugging message verbosity.
	 *
	 * Values higher than 0 make the stream log debug messages to standard
	 * error, debugging level goes from 0 to 4 inclusive.
	 * Larger values are implicitly truncated to the maximum allowed.
	 */
	unsigned verbose;
} Bzip2DecOpts;

/// BZip2 result status, returned by `Bzip2_GetErrStat()`.
typedef int Bzip2Ret;  // 0 == OK

/// Implementation of `StmOps` for BZip2 compression/decompression.
extern const StmOps *const Bzip2_StmOps;
/// Non-closing variant of `Bzip2_StmOps`.
extern const StmOps *const Bzip2_NcStmOps;

/// Return last BZip2 operation result.
Bzip2Ret Bzip2_GetErrStat(void);
/// Convert `Bzip2Ret` value to human readable string.
const char *Bzip2_ErrorString(Bzip2Ret ret);

/**
 * \brief Open stream for compression.
 *
 * \param [in,out] streamp Output stream for compressed data
 * \param [in]     ops     Write operations interface for `streamp`, must not be `NULL` and provide `Write()`
 * \param [in]     opts    Compression options, may be `NULL` for defaults
 *
 * \return The BZip2 compressor handle on success, `NULL` on failure.
 */
Bzip2StmHn Bzip2_OpenCompress(void *streamp, const StmOps *ops, const Bzip2CprOpts *opts);
/**
 * \brief Open a stream for decompressing.
 *
 * \param [in,out] streamp Input stream for BZip2 compressed data
 * \param [in]     ops     Read operations interface for `streamp`, must not be `NULL` and provide `Read()`
 * \param [in]     opts    Decompression options, may be `NULL` for defaults
 *
 * \return The BZip2 decompressor handle on success, `NULL` on failure.
 */
Bzip2StmHn Bzip2_OpenDecompress(void *streamp, const StmOps *ops, const Bzip2DecOpts *opts);

/**
 * \brief Decompress `nbytes` bytes from `hn` to `buf`.
 *
 * \return Number of actual bytes written to `buf`, 0 on end of stream,
 *         -1 on error. 
 */
Sint64 Bzip2_Read(Bzip2StmHn hn, void *buf, size_t nbytes);
/**
 * \brief Compress `nbytes` bytes from `buf` to `hn`.
 *
 * \return Number of bytes actually written to `hn`, which may be less
 *         than `nbytes`, -1 on error.
 *
 * \note Compression should be finalized with `Bzip2_Finish()` once all
 *       data is written.
 */
Sint64 Bzip2_Write(Bzip2StmHn hn, const void *buf, size_t nbytes);

/**
 * \brief Flush Bzip2 encoder.
 *
 * Should be called before closing a BZip2 encoder.
 *
 * \param [in,out] hn Stream to be finalized, must not be `NULL`
 *
 * \return `OK` on success, `NG` on failure.
 */
Judgement Bzip2_Finish(Bzip2StmHn hn);

/// Close a Bzip2 stream.
void Bzip2_Close(Bzip2StmHn hn);

#endif
