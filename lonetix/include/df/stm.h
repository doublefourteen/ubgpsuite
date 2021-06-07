// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file stm.h
 *
 * General stream I/O definitions and utilities.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_STM_H_
#define DF_STM_H_

#include "sys/fsdef.h"  // Fildes and SeekMode definitions

/**
 * \brief I/O stream operations interface.
 *
 * Defines an interface to work with an abstract I/O stream.
 */
typedef struct {
	/**
	 * \brief Read bytes from the stream.
	 *
	 * Function pointer may be `NULL` on write-only streams.
	 *
	 * \param streamp Stream handle
	 * \param buf     Destination buffer
	 * \param nbytes_ Bytes count to be read from `streamp` to `buf`
	 *
	 * \return Number of bytes actually read from the stream,
	 *         which may be less than the requested number of bytes.
	 *         If no bytes are available (`EOF`) function returns 0.
	 *         On error `-1` is returned.
	 *
	 * \see For reference `Sys_Fread()`
	 */
	Sint64    (*Read)(void *streamp, void *buf, size_t nbytes);
	/**
	 * \brief Write bytes to the stream.
	 *
	 * Function pointer may be `NULL` on read-only streams.
	 *
	 * \param streamp Stream handle
	 * \param buf     buffer to be written, holds at least `nbytes` bytes
	 * \param nbytes  Bytes count to be written to `hn`
	 *
	 * \return Number of bytes actually written to `streamp`, which
	 *         may be less than `nbytes`, -1 on error.
	 *
	 * \see For reference `Sys_Fwrite()`
	 */
	Sint64    (*Write)(void *streamp, const void *buf, size_t nbytes);
	/**
	 * \brief Seek inside the stream.
	 *
	 * Function pointer may be `NULL` on non-seekable streams.
	 *
	 * \param streamp Stream handle
	 * \param off     Seek offset in bytes
	 * \param whence  Seek mode
	 *
	 * \return The final cursor position on success, -1 on error.
	 *
	 * \see For reference [Sys_Fseek()](@ref Sys_Fseek)
	 */
	Sint64 (*Seek)(void *streamp, Sint64 off, SeekMode whence);
	/**
	 * \brief Retrieve current position inside stream.
	 *
	 * Function pointer may be `NULL` on non-seekable streams.
	 *
	 * \param streamp Stream handle
	 *
	 * \return Current cursor position inside `streamp`, -1 on error.
	 *
	 * \see For reference `Sys_Ftell()`
	 */
	Sint64 (*Tell)(void *streamp);
	/**
	 * \brief Finalizes writes to stream.
	 *
	 * Function pointer may be `NULL` in read-only streams or when such
	 * operation is unavailable or meaningless.
	 *
	 * \param streamp Stream handle
	 *
	 * \return`OK` on success`NG` otherwise.
	 */
	Judgement (*Finish)(void *streamp);
	/**
	 * \brief Closes the stream and free its resources.
	 *
	 * Function pointer may be `NULL` if such operation is not necessary.
	 *
	 * \param streamp Stream handle to be closed
	 *
	 * \see For reference `Sys_Fclose()`
	 */
	void (*Close)(void *streamp);
} StmOps;

/**
 * \brief Implementation of `StmOps` over `Fildes`.
 *
 * May be used to enable any function accepting `StmOps` to
 * work with `Fildes`.
 * Complete ownership is provided by `Stm_FildesOps`, non-closing
 * access is provided by `Stm_NcFildesOps`.
 * `Finish()` function performs a full file sync to disk
 * (as in: `Sys_Fsync()` with a `TRUE` `fullSync` flag).
 *
 * @{
 *   \var Stm_FildesOps
 *   \var Stm_NcFildesOps
 * @}
 */
extern const StmOps *const Stm_FildesOps;
extern const StmOps *const Stm_NcFildesOps;

/**
 * \brief Obtain a stream pointer from a file descriptor.
 *
 * \param [in] fd File descriptor
 *
 * \return Pointer suitable to be used as the `streamp` parameter for
 *         a `StmOps` interface.
 *
 * \see `Stm_FildesOps`, `Stm_NcFildesOps`
 *
 * \note Returned pointer may very well be `NULL`.
 */
FORCE_INLINE void *STM_FILDES(Fildes fd)
{
	STATIC_ASSERT(sizeof(fd) <= sizeof(Sintptr), "STM_FILDES() ill formed on this platform");
	return ((void *) ((Sintptr) fd));
}

#endif
