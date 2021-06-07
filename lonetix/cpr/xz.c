// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file cpr/xz.c
 *
 * Interfaces with `liblzma` and implements LZMA compressor/decompressor.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "cpr/xz.h"

#include <assert.h>
#include <limits.h>
#include <lzma.h>
#include <stdlib.h>
#include <string.h>

// some equivalency we assume to be true
STATIC_ASSERT((lzma_check) XZCHK_NONE == LZMA_CHECK_NONE,
              "Incorrect XZCHK_NONE definition");
STATIC_ASSERT((lzma_check) XZCHK_CRC32 == LZMA_CHECK_CRC32,
              "Incorrect XZCHK_CRC32 definition");
STATIC_ASSERT((lzma_check) XZCHK_CRC64 == LZMA_CHECK_CRC64,
              "Incorrect XZCHK_CRC64 definition");
STATIC_ASSERT((lzma_check) XZCHK_SHA256 == LZMA_CHECK_SHA256,
              "Incorrect XZCHK_SHA256 definition");

typedef struct XzStmObj XzStmObj;
struct XzStmObj {
	lzma_stream   xz;
	void         *streamp;
	const StmOps *ops;
	unsigned      bufsiz;
	lzma_action   action;
	Boolean8      encoding;
	Uint8         buf[FLEX_ARRAY];  // `bufsiz` bytes
};

#define XZ_BUFSIZ (32 * 1024)

#define XZ_EIO        -2
#define XZ_EBADSTREAM -1

static Sint64 Xz_FlushData(XzStmHn hn)
{
	size_t nbytes = hn->bufsiz - hn->xz.avail_out;
	Sint64 n      = hn->ops->Write(hn->streamp, hn->buf, nbytes);
	if (n < 0)
		return -1;

	size_t left = nbytes - n;
	memmove(hn->buf, hn->buf + n, left);

	hn->xz.next_out  = hn->buf + left;
	hn->xz.avail_out = hn->bufsiz - left;
	return n;
}

static Sint64 Xz_StmRead(void *streamp, void *buf, size_t nbytes)
{
	return Xz_Read((XzStmHn) streamp, buf, nbytes);
}

static Sint64 Xz_StmWrite(void *streamp, const void *buf, size_t nbytes)
{
	return Xz_Write((XzStmHn) streamp, buf, nbytes);
}

static Sint64 Xz_StmTell(void *streamp)
{
	XzStmHn hn = (XzStmHn) streamp;
	return hn->encoding ? hn->xz.total_out : hn->xz.total_in;
}

static Judgement Xz_StmFinish(void *streamp)
{
	return Xz_Finish((XzStmHn) streamp);
}

static void Xz_StmClose(void *streamp)
{
	Xz_Close((XzStmHn) streamp);
}

static const StmOps xz_stmOps = {
	Xz_StmRead,
	Xz_StmWrite,
	NULL,
	Xz_StmTell,
	Xz_StmFinish,
	Xz_StmClose
};
static const StmOps xz_ncStmOps = {
	Xz_StmRead,
	Xz_StmWrite,
	NULL,
	Xz_StmTell,
	Xz_StmFinish,
	NULL
};

const StmOps *const Xz_StmOps   = &xz_stmOps;
const StmOps *const Xz_NcStmOps = &xz_ncStmOps;

static THREAD_LOCAL XzRet xz_errStat = LZMA_OK;

static void Xz_SetErrStat(XzRet ret)
{
	xz_errStat = ret;
}

XzRet Xz_GetErrStat(void)
{
	return xz_errStat;
}

const char *Xz_ErrorString(XzRet ret)
{
	assert(ret != LZMA_NO_CHECK);
	assert(ret != LZMA_GET_CHECK);

	switch (ret) {
	case XZ_EIO:                 return "I/O error";
	case XZ_EBADSTREAM:          return "Bad stream operation";

	case LZMA_OK:                return "Success";
	case LZMA_UNSUPPORTED_CHECK: return "Cannot calculate the integrity check";
	case LZMA_MEM_ERROR:         return "Memory allocation failure";
	case LZMA_MEMLIMIT_ERROR:    return "Memory usage limit was reached";
	case LZMA_FORMAT_ERROR:      return "Unrecognized file format";
	case LZMA_OPTIONS_ERROR:     return "Invalid or unsupported options";
	case LZMA_DATA_ERROR:        return "Data is corrupt";
	case LZMA_BUF_ERROR:         return "No progress is possible";
	case LZMA_PROG_ERROR:        return "Programming error";
	default:                     return "Unknown error";
	}
}

XzStmHn Xz_OpenCompress(void            *streamp,
                        const StmOps    *ops,
                        const XzEncOpts *opts)
{
	const XzEncOpts default_opts = {
		6,
		FALSE,
		XZCHK_CRC32,
		XZ_BUFSIZ
	};

	if (!opts)
		opts = &default_opts;

	Uint32 compression = MIN(opts->compress, 9);

	Uint32 presets = 0;
	if (opts->extreme)
		presets |= LZMA_PRESET_EXTREME;

	size_t bufsiz = MIN(opts->bufsiz, INT_MAX);
	if (bufsiz == 0)
		bufsiz = XZ_BUFSIZ;

	XzStmObj *hn = (XzStmObj *) malloc(offsetof(XzStmObj, buf[bufsiz]));
	if (!hn) {
		Xz_SetErrStat(LZMA_MEM_ERROR);
		return NULL;
	}

	memset(&hn->xz, 0, sizeof(hn->xz));
	hn->streamp  = streamp;
	hn->ops      = ops;
	hn->bufsiz   = bufsiz;
	hn->action   = LZMA_RUN;  // ...actually ignored for encoding buffers
	hn->encoding = TRUE;

	lzma_ret err = lzma_easy_encoder(
		&hn->xz,
		compression | presets,
		(lzma_check) opts->chk
	);

	if (err != LZMA_OK) {
		Xz_SetErrStat(err);
		free(hn);
		return NULL;
	}

	hn->xz.next_out  = hn->buf;
	hn->xz.avail_out = hn->bufsiz;
	Xz_SetErrStat(LZMA_OK);
	return hn;
}

XzStmHn Xz_OpenDecompress(void            *streamp,
                          const StmOps    *ops,
                          const XzDecOpts *opts)
{
	const XzDecOpts default_opts = {
		U64_C(0xffffffffffffffff),
		FALSE,
		FALSE,
		XZ_BUFSIZ
	};

	if (!opts)
		opts = &default_opts;

	Uint32 mask = LZMA_CONCATENATED;
	if (opts->no_concat)
		mask &= ~LZMA_CONCATENATED;

#ifdef LZMA_IGNORE_CHECK
	if (opts->no_chk)
		mask |= LZMA_IGNORE_CHECK;
#endif

	size_t bufsiz = MIN(opts->bufsiz, INT_MAX);
	if (bufsiz == 0)
		bufsiz = XZ_BUFSIZ;

	XzStmObj *hn = (XzStmObj *) malloc(offsetof(XzStmObj, buf[bufsiz]));
	if (!hn) {
		Xz_SetErrStat(LZMA_MEM_ERROR);
		return NULL;
	}

	memset(&hn->xz, 0, sizeof(hn->xz));
	hn->streamp  = streamp;
	hn->ops      = ops;
	hn->action   = LZMA_RUN;  // used to force LZMA_FINISH on EOF
	hn->bufsiz   = bufsiz;
	hn->encoding = FALSE;

	lzma_ret err = lzma_auto_decoder(&hn->xz, opts->memlimit, mask);
	if (err != LZMA_OK) {
		Xz_SetErrStat(err);
		free(hn);
		return NULL;
	}

	Xz_SetErrStat(LZMA_OK);
	return hn;
}

Sint64 Xz_Read(XzStmHn hn, void *buf, size_t nbytes)
{
	if (hn->encoding) {
		Xz_SetErrStat(XZ_EBADSTREAM);
		return -1;
	}

	XzRet ret = LZMA_OK;

	hn->xz.next_out  = (Uint8 *) buf;
	hn->xz.avail_out = nbytes;
	while (hn->xz.avail_out > 0) {
		if (hn->xz.avail_in == 0) {
			Sint64 n = hn->ops->Read(hn->streamp, hn->buf, hn->bufsiz);
			if (n <= 0) {
				if (n < 0) {
					ret = XZ_EIO;
					break;
				}

				hn->action = LZMA_FINISH;
			}

			hn->xz.next_in  = hn->buf;
			hn->xz.avail_in = n;
		}

		lzma_ret err = lzma_code(&hn->xz, hn->action);
		if (err == LZMA_STREAM_END)
			break;  // NOTE: shouldn't happen for a stream to end before EOF...
		if (err != LZMA_OK) {
			ret = err;
			break;
		}
	}

	Xz_SetErrStat(ret);
	return nbytes - hn->xz.avail_out;
}

Sint64 Xz_Write(XzStmHn hn, const void *buf, size_t nbytes)
{
	if (!hn->encoding) {
		Xz_SetErrStat(XZ_EBADSTREAM);
		return -1;
	}

	XzRet ret = LZMA_OK;

	hn->xz.next_in  = (Uint8 *) buf;
	hn->xz.avail_in = nbytes;
	while (hn->xz.avail_in > 0) {
		if (hn->xz.avail_out == 0) {
			Sint64 n = Xz_FlushData(hn);
			if (n < -1) ret = XZ_EIO;

			break;  // short-write
		}

		// Disregard `hn->action` on write, we will flush upon Xz_Finish()
		lzma_ret err = lzma_code(&hn->xz, LZMA_RUN);
		if (err != LZMA_OK) {
			ret = err;
			break;
		}
	}

	Xz_SetErrStat(ret);
	return nbytes - hn->xz.avail_in;
}

Judgement Xz_Finish(XzStmHn hn)
{
	if (!hn->encoding) {
		Xz_SetErrStat(XZ_EBADSTREAM);
		return NG;
	}

	lzma_ret err;
	do {
		// Flush LZMA to disk
		err = lzma_code(&hn->xz, LZMA_FINISH);
		if (err != LZMA_STREAM_END && err != LZMA_OK) {
			Xz_SetErrStat(err);
			return NG;
		}
		if (Xz_FlushData(hn) == -1) {
			Xz_SetErrStat(XZ_EIO);
			return NG;
		}
	} while (err != LZMA_STREAM_END);

	if (hn->xz.avail_out != hn->bufsiz) {
		Xz_SetErrStat(LZMA_BUF_ERROR);
		return NG;
	}

	return OK;
}

void Xz_Close(XzStmHn hn)
{
	if (hn->ops->Close)
		hn->ops->Close(hn->streamp);

	lzma_end(&hn->xz);
	free(hn);
}
