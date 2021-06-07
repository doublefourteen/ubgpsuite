// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file mem_file.h
 *
 * Manage raw memory byte buffers as an I/O stream file-like resource.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * Memory file resources are regular raw-bytes buffer that may be read,
 * written and seeked like regular files, implementing the
 * `StmOps` interface. Memory buffer may be managed into a variety of ways
 * giving the API some flexibility, the policy is determined by a set of flags,
 * specified upon initialization (and possibly changed on the fly).
 * When writing to a memory file the buffer will tipically be
 * `realloc()`ated as needed, the `granularity` field provides a hint
 * to reduce excessive reallocations (buffer shall be `realloc()`ated to
 * multiples of such value). Buffer reallocation may be opted out altoghether
 * by toggling the `MEM_FILE_NOGROWBIT` flag, which turns any attempt
 * of writing more bytes than currently available into a short-write.
 * Memory buffers may also be initialized in read-only or write-only mode,
 * enabling the API to reliably manage `const` qualified buffers or
 * write-only memory zones.
 */

#ifndef DF_MEM_FILE_H_
#define DF_MEM_FILE_H_

#include "stm.h"

/// Default `MemFile` reallocation granularity.
#define MEM_FILE_GRAN   (16u * 1024)


/// Allow write operations.
#define MEM_FILE_WRBIT     BIT(0)
/// Allow read operations.
#define MEM_FILE_RDBIT     BIT(1)
/**
 * \brief `MemFile` whose `OWN` flag is set own the memory buffer,
 *        `free()`ing it upon close and `realloc()`ating it as necessary.
 */
#define MEM_FILE_OWNBIT    BIT(2)
/**
 * \brief `MemFile` with `NOGROW` flag won't reallocate
 *        their buffer, consequently short-writes are not treated as errors.
 */
#define MEM_FILE_NOGROWBIT BIT(3)

/**
 * \brief A file-like memory buffer.
 *
 * Provides functionality to operate on a memory chunk in a stream-like fashion,
 */
typedef struct {
	char    *buf;     ///< memory buffer `MemFile` operates on
	size_t   pos;     ///< current position inside `buf`
	size_t   nbytes;  ///< `buf` length, in bytes
	size_t   cap;     ///< `buf` actual capacity, in bytes
	size_t   gran;    ///< reallocation granularity in bytes, must be a power of 2
	unsigned flags;   ///< operating mode flags
} MemFile;

/// Stream operations on `MemFile`, including `Close()`.
extern const StmOps *const Stm_MemFileOps;
/// Non `Close()`-ing stream operations on `MemFile`.
extern const StmOps *const Stm_NcMemFileOps;

/**
 * \brief Perform basic initialization of a `MemFile`, with an empty
 *        initial buffer.
 *
 * The resulting memory file resource shall be initialized according to
 * function arguments and its buffer shall be `NULL`.
 *
 * \param [out] stm  Pointer to a memory file, must not be `NULL`
 * \param [in]  gran Reallocation granularity in bytes, must be a power of 2
 * \param [in] flags Operating mode flags for the newly initialized memory file
 */
void Stm_InitMemFile(MemFile *stm, size_t gran, unsigned flags);

/**
 * \brief Initialize `MemFile` from an existing buffer.
 *
 * \param [out] stm    Pointer to a memory file, must not be `NULL`
 * \param [in]  buf    Read-only buffer to be used, must hold at least `nbytes` bytes
 * \param [in]  nbytes `buf` size in bytes
 * \param [in]  gran   Memory file reallocation granularity, in bytes, must be a power of 2
 * \param [in]  flags  Memory file resource operating mode flags
 */
void Stm_MemFileFromBuf(MemFile *stm, void *buf, size_t nbytes, size_t gran, unsigned flags);

/**
 * \brief Initialize a `MemFile` for read over an existing memory buffer.
 *
 * Resulting memory file resource is implicitly initialized as read-only,
 * its buffer won't grow, won't be reallocated, nor it shall be `free()`-d upon
 * close.
 *
 * \param [out] stm    Pointer to a memory file, must not be `NULL`
 * \param [in]  buf    Read-only buffer to be used, must hold at least `nbytes` bytes
 * \param [in]  nbytes `buf` size in bytes
 *
 * \note No check is made to ensure the provided buffer is NUL-terminated,
 *       the resulting `MemFile` is initialized with the provided buffer
 *       as-is. If such characteristic is important, it is the caller's
 *       responsibility to ensure that.
 */
void Stm_RoMemFileFromBuf(MemFile *, const void *, size_t);

/**
 * \brief Take ownership of the buffer managed by a `MemFile`.
 *
 * \return A pointer to the managed buffer, whose ownership is transferred
 *         to the caller, thus the caller shall be responsible for its
 *         deallocation (if necessary).
 *
 * \note It is not always necessary to call `free()` on the returned buffer,
 *       since memory files may operate even on static or stack-allocated
 *       buffers (see `Stm_MemFileFromBuf()` for example).
 */
FORCE_INLINE char *Stm_TakeMemFileBuf(MemFile *stm)
{
	char *buf = stm->buf;

	stm->flags &= ~MEM_FILE_OWNBIT;
	stm->buf    = NULL;
	stm->nbytes = stm->cap = 0;
	return buf;
}

/**
 * \brief Read `nbytes` bytes from `stm` into `buf`.
 *
 * \return Number of bytes actually read from `stm`, -1 on error.
 */
Sint64 Stm_MemFileRead(MemFile *stm, void *buf, size_t nbytes);
/**
 * \brief Write `nbytes` bytes from `buf` into `stm`.
 *
 * \return Number of bytes actually written to `stm`, -1 on error.
 */
Sint64 Stm_MemFileWrite(MemFile *stm, const void *buf, size_t nbytes);
/**
 * \brief Set `stm` file pointer, offsetting it of `pos` bytes from `whence` 
 *
 * \return New file cursor position, -1 on error.
 */
Sint64 Stm_MemFileSeek(MemFile *stm, Sint64 pos, SeekMode whence);
/// Close `stm`, `free()`-ing buffer if necessary.
void Stm_MemFileClose(MemFile *stm);

#endif
