// SPDX-License-Identifier: LGPL-3.0-or-later

#include "sys/fs.h"

#include "strlib.h"

#include <assert.h>
#include <string.h>

static Boolean IsSep(char c)
{
	return c == '/' || c == '\\' || c == ':';
}

char *Sys_GetAbsoluteFileExtension(const char *path)
{
	char *p = (char *) path;
	char *sep = p, *extp = p;

	while (*p != '\0') {
		if (*p == '.' && extp == sep && p > sep + 1)
			extp = p;
		if (IsSep(*p))
			extp = sep = p;

		p++;
	}

	if (extp == sep || *(extp+1) == '\0')
		extp = p;  // no extensions in path

	return extp;
}

char *Sys_GetFileExtension(const char *path)
{
	char *p = (char *) path;
	char *sep = p, *extp = p;

	while (*p != '\0') {
		if (*p == '.' && p > sep + 1)
			extp = p;
		if (IsSep(*p))
			sep = extp = p;

		p++;
	}

	if (extp == sep || *(extp+1) == '\0')
		extp = p;  // no extensions in path

	return extp;
}

char *Sys_SetFileExtension(char *path, const char *ext)
{
	assert(*ext == '.' && *(ext+1) != '\0');

	char *extp = Sys_GetFileExtension(path);
	return strcpy(extp, ext);
}

size_t Sys_StripFileExtension(char *path, const char *ext)
{
	if (!ext)
		ext = "";

	assert(*ext == '\0' || (*ext == '.' && *(ext+1) != '\0'));

	char *extp = Sys_GetFileExtension(path);
	if (*ext == '\0' || Df_stricmp(ext, extp) == 0)
		*extp = '\0';

	return extp - path;
}

char *Sys_DefaultFileExtension(char *path, const char *ext)
{
	assert(*ext == '.' && *(ext+1) != '\0');

	char *extp = Sys_GetFileExtension(path);
	if (*extp == '\0')
		strcpy(extp, ext);

	return extp;
}

static size_t Sys_StripPathFast(char *path)
{
	size_t len = strlen(path);
	size_t i   = len;
	while (i-- > 0) {
		if (IsSep(path[i])) {
			size_t start = i + 1;
			size_t n     = len - start;

			memmove(path, path + start, n + 1);
			return n;
		}
	}

	return len;
}

size_t Sys_StripPath(char *path, const char *basePath)
{
	if (!basePath || *basePath == '\0')
		return Sys_StripPathFast(path);  // fast case, no basePath

	int c1, c2;

	size_t i = 0, j = 0;
	while (TRUE) {
		c1 = path[i];
		c2 = basePath[j];

		if (c1 == '\\' || c1 == ':' || c1 == '\0')
			c1 = '/';
		if (c2 == '\\' || c2 == ':' || c2 == '\0')
			c2 = '/';

		if (c1 != c2)
			return i + strlen(path + i);

		while (IsSep(path[i])) i++;
		while (IsSep(basePath[j])) j++;

		if (basePath[j] == '\0')
			break;

		i++;
		j++;
	}

	size_t n = strlen(path + i);
	memmove(path, path + i, n + 1);
	return n;

}

size_t Sys_PathDepth(const char *path)
{
	size_t n = 0;

	const char *p = path;
	while (IsSep(*p))
		p++;

	while (TRUE) {
		// Skip anything that isn't a separator
		while (*p != '\0' && !IsSep(*p))
			p++;

		if (*p == '\0')
			break;

		n++;

		// Skip any consecutive separator
		while (IsSep(*p))
			p++;
	}

	return n;
}

size_t Sys_ConvertPath(char *path)
{
	// Strip leading slashes
	char *start = path;
	while (*start != '\0') {
		if (IsSep(*start))
			break;

		start++;
	}

	// Convert path separators to '/'
	char *end = start;
	while (*end != '\0') {
		if (*end == '\\' || *end == ':')
			*end = '/';

		end++;
	}

	// Move everything to the beginning of the string
	size_t n = end - start;
	memmove(path, start, n + 1);
	return n;
}

size_t Sys_ReplaceSeps(char *path)
{
	Boolean lastWasSep = FALSE;

	char *p = path;
	while (*p != '\0') {
		if (IsSep(*p)) {
			if (!lastWasSep) {
				*p = PATH_SEP;
				lastWasSep = TRUE;
			} else {
				memmove(p, p + 1, strlen(p));  // NOTE: no +1
			}
		} else {
			lastWasSep = FALSE;
		}

		p++;
	}

	return p - path;
}

int Sys_PathCompare(const char *p1, const char *p2)
{
	int c1, c2;

	do {
		c1 = *p1++;
		c2 = *p2++;

		if (IsSep(c1)) {
			c1 = '/';

			while (IsSep(*p1)) p1++;
		}
		if (IsSep(c2)) {
			c2 = '/';

			while (IsSep(*p2)) p2++;
		}
	} while (c1 == c2 && c1 != '\0');

	return c1 - c2;
}
