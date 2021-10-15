// SPDX-License-Identifier: LGPL-3.0-or-later

#include "sys/sys_local.h"
#include "sys/fs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Fildes Sys_Fopen(const char *path, FopenMode mode, unsigned flags)
{
	Fildes fd;

	errno = 0;

	// Open file
	switch (mode) {
	case FM_READ:
		fd = open(path, O_RDONLY);
		break;

	case FM_WRITE:
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		break;

	case FM_APPEND:
		fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
		break;

	case FM_EXCL:
		fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
		break;

	case FM_TEMP: {
#ifdef __linux__
		// Linux supports O_TMPFILE for safer temporary file creation
		fd = open(path, O_WRONLY | O_EXCL | O_TMPFILE, 0600);
		if (fd >= 0)
			break;  // success

		if (errno == ENOENT || errno == ENOTDIR || errno == EISDIR)
			break;  // file not found - don't fallback
		if (errno != EOPNOTSUPP)
			break;  // error - don't fallback

		// Fallback to regular mkstemp()
#endif
		size_t n  = strlen(path);
		char *buf = (char *) alloca(n + 1 + 6 + 1);
		if (sprintf(buf, "%s/XXXXXX", path) < 0) {
			Sys_SetErrStat(errno, "sprintf() failed");
			return FILDES_BAD;
		}

		fd = mkstemp(buf);
		break;
	}

	default:
		Sys_SetErrStat(EINVAL, "Bad value for 'mode'");
		return FILDES_BAD;
	}

	if (errno == ENOENT || errno == ENOTDIR || errno == EISDIR)
		errno = 0;  // file not found - not an error

	Sys_SetErrStat(errno, "open()/mkstemp()");

	// Apply hints
	if (fd >= 0) {
		if (flags & FH_SEQ)
			posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
		if (flags & FH_RAND)
			posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
		if (flags & FH_NOREUSE)
			posix_fadvise(fd, 0, 0, POSIX_FADV_NOREUSE);
	}
	return fd;
}

Sint64 Sys_Fread(Fildes fd, void *buf, size_t nbytes)
{
	Sint64 n;

	errno = 0;

	n = read(fd, buf, nbytes);
	Sys_SetErrStat(errno, "read()");
	return n;
}

Sint64 Sys_Fwrite(Fildes fd, const void *buf, size_t nbytes)
{
	Sint64 n;

	errno = 0;

	n = write(fd, buf, nbytes);
	Sys_SetErrStat(errno, "write() failed");
	return n;
}

Sint64 Sys_Ftell(Fildes fd)
{
	Sint64 pos;

	errno = 0;

	pos = lseek(fd, 0, SEEK_CUR);
	if (pos < 0 && errno != ESPIPE)
		errno = 0;  // lseek() unsupported, but not a system error

	Sys_SetErrStat(errno, "lseek()");
	return pos;
}

Sint64 Sys_Fseek(Fildes fd, Sint64 off, SeekMode whence)
{
	Sint64 pos;
	int mode;

	switch (whence) {
	case SK_SET: mode = SEEK_SET; break;
	case SK_CUR: mode = SEEK_CUR; break;
	case SK_END: mode = SEEK_END; break;
	default:
		Sys_SetErrStat(EINVAL, "Bad value for 'whence'");
		return -1;
	}

	errno = 0;

	pos = lseek(fd, off, mode);
	if (pos < 0 && errno == ESPIPE)
		errno = 0;  // lseek() unsupported, but not a system error

	Sys_SetErrStat(errno, "lseek()");
	return pos;
}

Sint64 Sys_FileSize(Fildes fd)
{
	Sint64 size = -1;
	struct stat buf;

	errno = 0;

	if (fstat(fd, &buf) == 0)
		size = buf.st_size;

	Sys_SetErrStat(errno, "fstat()");
	return size;
}

Judgement Sys_SetEof(Fildes fd)
{
	Sint64 pos = Sys_Ftell(fd);
	if (pos < 0)
		return NG;  // Sys_Ftell() already sets error

	ftruncate(fd, pos);
	return Sys_SetErrStat(errno, "ftruncate()");
}

Judgement Sys_Fsync(Fildes fd, Boolean fullSync)
{
	errno = 0;

	if (fullSync)
		fsync(fd);
	else
		fdatasync(fd);

	return Sys_SetErrStat(errno, "fsync()/fdatasync()");
}

void Sys_Fclose(Fildes fd)
{
	errno = 0;

	close(fd);
	Sys_SetErrStat(errno, "close()");
}

typedef struct FileList FileList;
struct FileList {
	FileList *next;
	size_t    len;
	char      name[1];  // dynamically sized, len+1
};

#ifdef _DIRENT_HAVE_D_TYPE
// May save some syscalls
#define ISUNK(ent) ((ent)->d_type == DT_UNKNOWN)
#define ISDIR(ent) ((ent)->d_type == DT_DIR)
#define ISLNK(ent) ((ent)->d_type == DT_LNK)
#else
// ...as if DT_UNKNOWN is always set
#define ISUNK(ent) (1)
#define ISDIR(ent) (0)
#define ISLNK(ent) (0)
#endif

char **Sys_ListFiles(const char *path, unsigned *nfiles, const char *pat)
{
	DIR *dir = opendir(path);
	if (!dir) {
		if (errno != ENOENT && errno != ENOTDIR)
			Sys_SetErrStat(errno, "opendir()");

		if (nfiles)
			*nfiles = 0;

		return NULL;
	}

	if (!pat)
		pat = "";

	Boolean dirOnly = (strcmp(pat, "/") == 0);
	size_t elen     = strlen(pat);

	// Scan directory

	FileList *entries = NULL;
	unsigned  count   = 0;
	size_t    nchars  = 0;

	struct stat st;
	struct dirent *ent;

	char **files;
	char  *namep;

	errno = 0;

	while ((ent = readdir(dir)) != NULL) {
		// Skip special entries
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			continue;

		size_t len = strlen(ent->d_name);

		if (dirOnly) {
			// Filter anything other than subdirectories
			Boolean isDir;
			if (ISUNK(ent) || ISLNK(ent)) {
				// Need a full stat() on entry
				if (fstatat(dirfd(dir), ent->d_name, &st, 0) != 0) {
					Sys_SetErrStat(errno, "fstatat()");
					goto fail;
				}

				isDir = S_ISDIR(st.st_mode);

			} else {
				// May take advantage of dirent's d_type
				isDir = ISDIR(ent);
			}

			if (!isDir)
				continue;

		} else {
			// Filter by extension pattern
			if (elen >= len)
				continue;

			if (strcmp(ent->d_name + len - elen, pat) != 0)
				continue;
		}

		// Update counters
		count++;
		nchars += len + 1;

		// ...and remember this entry
		FileList *e = (FileList *) alloca(sizeof(*e) + len);  // includes '\0'

		e->next = entries;
		e->len  = len;
		memcpy(e->name, ent->d_name, len + 1);

		entries = e;
	}
	if (errno != 0) {
		Sys_SetErrStat(errno, "readdir()");
		goto fail;
	}

	// Allocate result, single malloc(), +1 for NULL sentinel
	files = (char **) malloc(sizeof(*files) * (count + 1) + nchars);
	if (!files) {
		Sys_OutOfMemory();
		goto fail;
	}

	closedir(dir);  // safe, can't fail anymore

	// Collect files in buffer
	namep  = (char *) (files + count + 1);
	for (unsigned i = 0; i < count; i++) {
		files[i] = namep;

		size_t n = entries->len + 1;
		memcpy(namep, entries->name, n);
		namep += n;

		entries = entries->next;
	}
	files[count] = NULL;  // NULL-terminate the file list

	if (nfiles)
		*nfiles = count;

	return files;

fail:
	closedir(dir);

	if (nfiles)
		*nfiles = 0;

	return NULL;
}

Judgement Sys_Mkdir(const char *path)
{
	errno = 0;

	if (mkdir(path, 0755) != 0) {
		int err = errno;  // save errno - avoid overriding by stat()
		if (err == EEXIST) {
			// Make sure this is actually a directory
			struct stat buf;

			if (stat(path, &buf) == 0 && S_ISDIR(buf.st_mode))
				err = 0;  // all good
		}

		errno = err;
	}

	return Sys_SetErrStat(errno, "mkdir()");
}

Judgement Sys_Rename(const char *path, const char *new_path)
{
	errno = 0;

	rename(path, new_path);
	return Sys_SetErrStat(errno, "rename()");
}

Judgement Sys_Remove(const char *path)
{
	errno = 0;

	remove(path);

	// TODO check not found
	return Sys_SetErrStat(errno, "remove()");
}

