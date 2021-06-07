// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/fs.h
 *
 * Portable low-level filesystem layer
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * Achieves portable low-level, close to operating system, filesystem
 * functionality. Functionality includes:
 * - file I/O
 * - file usage hints
 * - file creation and removal
 * - directory listing, creation and removal
 * - path utilities.
 *
 * No library file buffering is attempted, nor any kind of text-based I/O.
 * Paths are UTF-8.
 */

#ifndef DF_SYS_FS_H_
#define DF_SYS_FS_H_

#include "sys/fsdef.h"  // platform-specific defs, including `Fildes`

/// File access mode for `Sys_Fopen()`.
typedef enum {
	FM_READ,    ///< Read-only access
	FM_WRITE,   ///< Write-only access
	FM_APPEND,  ///< File-append access
	FM_EXCL,    ///< Exclusive creation access
	FM_TEMP     ///< Temporary scratch file creation
} FopenMode;

/// Access pattern is sequential.
#define FH_SEQ     BIT(0)
/// Access pattern is random.
#define FH_RAND    BIT(1)
/// Avoid filesystem cache buffers with this file.
#define FH_UNBUF   BIT(2)
/// File data is accessed once and never reused.
#define FH_NOREUSE BIT(3)

/**
 * \brief Open a descriptor handle to a regular or special file.
 *
 * Every successfully opened file descriptor must be closed using
 * `Sys_Fclose()` when no longer necessary.
 *
 * \param [in] filename Path to file, must not be `NULL`
 * \param [in] mode     File access mode
 * \param [in] hints    File access hints (`FH_*` bit mask)
 *
 * \return Opened file descriptor on success, `FILDES_BAD` on failure.
 */
Fildes Sys_Fopen(const char *filename, FopenMode mode, unsigned hints);
/**
 * \brief Move file's cursor to the specified offset.
 *
 * \param [in] fd     Opened file descriptor
 * \param [in] offset Offset to move the cursor to, according to `whence`, in bytes
 * \param [in] whence Seek mode
 *
 * \return New cursor position on success, -1 on failure or non-seekable file.
 */
Sint64 Sys_Fseek(Fildes fd, Sint64 offset, SeekMode whence);
/// Retrieve current file cursor position, returns -1 on failure or non-seekable file.
Sint64 Sys_Ftell(Fildes fd);
/// Get current file size, in bytes, -1 on error.
Sint64 Sys_FileSize(Fildes fd);
/**
 * \brief Write raw bytes to file.
 *
 * \param [in] fd     Opened, writable, file descriptor
 * \param [in] buf    Containing at least `nbytes` bytes of data
 * \param [in] nbytes Bytes count in `buf` to be written to `fd`
 *
 * \return Number of bytes actually written on `fd`, possibly less
 *         than `nbytes` on short-write. -1 on failure.
 */
Sint64 Sys_Fwrite(Fildes fd, const void *buf, size_t nbytes);
/**
 * \brief Read raw bytes from file.
 *
 * \param [in]  fd     Opened, readable, file descriptor
 * \param [out] buf    Destination buffer for the read operation
 * \param [in]  nbytes Bytes count to read from `fd` inside `buf`
 *
 * \return Number of bytes actually read from `fd`, possibly less than `nbytes`,
 *         0 is returned on `EOF` condition. -1 on failure.
 */
Sint64 Sys_Fread(Fildes fd, void *buf, size_t nbytes);
/**
 * \brief Truncate or grow file size to its current cursor position.
 *
 * \return `OK` on success, and file is altered as follows,
 *         - if file size has been truncated excess data is lost;
 *         - if file has grown in size (file cursor was beyond the original size),
 *           new content is unspecified.
 *         On failure returns `NG` and file is unaltered.
 */
Judgement Sys_SetEof(Fildes fd);
/**
 * \brief Synchronize file data to disk, flushing buffers.
 *
 * Some systems require `fd` to be writable, whether `Sys_Fsync()` supports
 * a read-only descriptor is system specific.
 * Some systems allow optimized syncs that only guarantee read operations
 * consistency afterwards, this optimization is used on such systems when
 * `fullSync` is `FALSE`. otherwise `fullSync` is ignored (as if always `TRUE`).
 * A call to `Sys_Fsync()` with `fullSync` to `TRUE` causes the system
 * to sync disk data with `fd` in its entirety.
 *
 * \param [in] fd       Opened file descriptor
 * \param [in] fullSync Whether a full sync should occur
 *
 * \return `OK` on success, `NG` on failure or on systems where
 *         there is no mean to force file data sync with disk.
 */
Judgement Sys_Fsync(Fildes fd, Boolean fullSync);
/// Close opened file descriptor.
void Sys_Fclose(Fildes fd);

/**
 * \brief List directory contents.
 *
 *  `pat`   | Meaning
 * ---------|------------------------------------------
 * __NULL__ | Equivalent to `""`
 *    ""    | No filtering
 *    /     | Only return subdirectories
 *    .*    | Only return files whose extension matches
 *
 * \param [in]  path   Path to directory, must not be `NULL`
 * \param [out] nfiles Location to store returned file count, may be `NULL` if unimportant
 * \param [in]  pat    Optional pattern to filter directory files, may be `NULL`
 *
 * \return `malloc()`ed string list containing matching files,
 *         must be `free()`d by the caller when no longer necessary.
 *         A single `free()` on the returned list is sufficient to
 *         release it entirely.
 */
char **Sys_ListFiles(const char *path, unsigned *nfiles, const char *pat);

/**
 * \brief Create directory.
 *
 * \param [in] path Directory creation path, must not be `NULL`
 *
 * \return `OK` on success, and directory is created.
 *         Creating already existing directories is a success.
 *         `NG` on failure.
 */
Judgement Sys_Mkdir(const char *path);

/**
 * \brief Rename a file.
 *
 * Different systems may impose different restrictions on
 * rename operations, in particular when the operation crosses
 * different devices in the filesystem.
 * The only portable and safe way to rename a file (or, in this
 * specific scenario, *move* it) is copying it over `newPath`,
 * and remove `path` on success, but the basic `rename` operation is
 * usually safe, unexpensive and atomic under the same device.
 * `Sys_Rename()` works on directories as well.
 *
 * \param [in] path    Path to the file to be renamed, must not be `NULL`
 * \param [in] newPath New name for the file, destination directory must exist
 *
 * \return `OK` on success, `NG` otherwise.
 */
Judgement Sys_Rename(const char *path, const char *newPath);
/**
 * \brief Remove a file or empty directory.
 *
 * \param [in] path Path to file or directory to be removed, must not be `NULL`
 *
 * \return `OK` on success, `NG` otherwise.
 */
Judgement Sys_Remove(const char *path);

// Path utilities

/// Retrieve the absolute file extension (leftmost not leading dot in basename).
char *Sys_GetAbsoluteFileExtension(const char *path);
/// Retrieve the file extension (rightmost not leading dot in basename).
char *Sys_GetFileExtension(const char *path);
/**
 * \brief Set `path` file extension to `ext`.
 *
 * \param [in,out] path UTF-8 path, must not be `NULL`
 * \param [in]     ext  File extension, including dot, must not be `NULL`
 *
 * \return Pointer to the extension inside `path`.
 *
 * \note Assumes `path` is is large enough to hold the result.
 */
char *Sys_SetFileExtension(char *path, const char *ext);
/**
 * \brief Removes extension from `path` if it matches `ext`.
 *
 * \param [in,out] path UTF-8 path, must not be `NULL`
 * \param [in]     ext  File extension, including dot, leave to `""` or `NULL`
 *                      to remove any extension
 *
 * \return Length of the resulting path, in chars.
 */
size_t Sys_StripFileExtension(char *path, const char *ext);
/**
 * \brief If file in `path` has no extension yet, set it to `ext`.
 *
 * \param [in,out] path UTF-8 path, must not be `NULL`
 * \param [in]     ext  Default extension to be set, including dot, must not be `NULL`
 *
 * \return Pointer to the extension inside `path`
 *
 * \note Assums `path` is large enough to hold the result.
 */
char *Sys_DefaultFileExtension(char *path, const char *ext);
/**
 * Strip initial portion of `path` if it matches `basePath`.
 *
 * \param [in,out] path     UTF-8 path, must not be `NULL`
 * \param [in]     basePath Initial path to be removed, use `""` or `NULL` to strip the entire path and leave only file (Unix `basename()`)
 *
 * \return Length of the resulting path, in chars.
 */
size_t Sys_StripPath(char *path, const char *basePath);
/// Return `path` depth (number of path components).
size_t Sys_PathDepth(const char *path);

/**
 * \brief Strip leading slashes and change any path separator to `/`.
 *
 * \return Resulting path length, in chars.
 */
size_t Sys_ConvertPath(char *path);
/**
 * \brief Change any path separator to a single `PATH_SEP`.
 *
 * \return Resulting path length, in chars.
 */
size_t Sys_ReplaceSeps(char *path);

/// Case-sensitive UTF-8 path comparison, regardless of separators.
int Sys_PathCompare(const char *a, const char *b);
// XXX int Sys_PathCompareNoCase(const char *a, const char *b);
// XXX int Sys_PathCompareNoCaseAscii(const char *a, const char *b);

#endif
