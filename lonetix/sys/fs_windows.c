// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/fs_windows.c
 *
 * System specific filesystem utilities over Windows.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/fs.h"
#include "sys/err_local.h"

#include <errno.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#error "Sorry, not fully implemented yet!"

typedef union {
	wchar_t wstr[_MAX_PATH + 1];
	char    str[(_MAX_PATH + 1) * sizeof(wchar_t)];
} Pathbuf;

static THREAD_LOCAL Pathbuf pathbuf;

static wchar_t *Sys_Utf8PathToW(const char *path)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, dir, -1, pathbuf.wstr, ARRAY_SIZE(pathbuf.wstr));
	if (n < 0) {
		Sys_SetErrStat(GetLastError(), "MultiByteToWideChar() failed");
		return NULL;
	}

	return pathbuf.wstr;
}

static char *Sys_WPathToUtf8(const wchar_t *path)
{
	int n = WideCharToMultiByte(CP_UTF16, 0, path, -1, pathbuf.str, sizeof(pathbuf.str));
	if (n < 0) {
		Sys_GetErrStat(GetLastError(), "WideCharToMultiByte() failed");
		return NULL;
	}

	return pathbuf.str;
}

Judgement Sys_SetEof(Fildes fd)
{
	if (!SetEndOfFile(fd)) {
		Sys_SetErrStat(GetLastError(), "SetEndOfFile() failed");
		return NG;
	}

	return OK;
}

void Sys_Fclose(Fildes fd)
{
	CloseHandle(fd);
}

typedef struct FileList dfFileList;
struct FileList {
	FileList *next;
	unsigned    len;
	char        name[1];  // dynamically sized len+1
};

char **Sys_ListFiles(const char *dir, unsigned *nfiles, const char *pat)
{
	if (!pat)
		pat = "";

	int flag;
	if (strcmp(pat, "/") == 0) {
		pat = "";
		flag = _A_SUBDIR;
	} else {
		flag = 0;
	}

	char *search = (char *) alloca(strlen(dir) + 1 + 1 + strlen(pat) + 1);
	if (sprintf(search, "%s\\*%s", dir, pat) < 0) {
		Sys_SetErrStat(errno, "sprintf() failed");
		return NULL;
	}

	wchar_t *wsearch = Sys_Utf8PathToW(search);
	if (!wsearch)
		return NULL;  // error already set

	FileList *entries = NULL;
	unsigned  count   = 0;
	size_t    nchars  = 0;

	errno = 0;

	struct _wfinddata_t finddata;
	intptr_t findhn = _wfindfirst(wsearch, &finddata);
	if (findhn != -1) {
		// Scan directory
		do {
			if (finddata.attrib & flag) {
				// remember this file
				char  *path = Sys_WPathToUtf8(finddata.name);
				size_t n    = strlen(path);

				count++;
				nchars += n + 1;

				FileList *e = (FileList *) alloca(sizeof(*e) + n);
				e->next = entries;
				e->len  = n;
				memcpy(e->name, path, n + 1);

				entries = e;
			}
		} while (_wfindnext(findhn, &finddata) == 0);
	}
	if (errno != 0) {
		Sys_SetErrStat(errno, "_wfindfirst()/_wfindnext()");
		goto fail;
	}
	_findclose(findhn);

	char **files = (char **) malloc(sizeof(*files) * (count + 1) + nchars);
	if (!files) {
		Sys_OutOfMemory();
		return NULL;
	}

	char *namep = (char *) (files + count + 1);
	for (unsigned i = 0; i < count; i++) {
		files[i] = namep;

		size_t n = entries->len + 1;
		memcpy(namep, entries->name, n);
		namep += n;

		entries = entries->next;
	}
	files[count] = NULL;

	if (nfiles)
		*nfiles = count;

	return files;

fail:
	_findclose(findhn);
	return NULL;
}

