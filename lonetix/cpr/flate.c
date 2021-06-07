// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file cpr/flate.c
 *
 * Interfaces with `zlib` and implements INFLATE/DEFLATE.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "cpr/flate.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct ZlibStmObj ZlibStmObj;
struct ZlibStmObj {
	z_stream      zs;
	void         *streamp;
	const StmOps *ops;
	unsigned      bufsiz;
	Boolean8      deflating;
	Uint8         buf[FLEX_ARRAY];  // `bufsiz` bytes
};

#define ZSTM_BUFSIZ (32 * 1024)

#define ZSTM_EIO        -1LL
#define ZSTM_EBADSTREAM -2LL

static Sint64 Zlib_FlushData(ZlibStmHn hn)
{
	size_t nbytes = hn->bufsiz - hn->zs.avail_out;

	Sint64 n = hn->ops->Write(hn->streamp, hn->buf, nbytes);
	if (n == -1)
		return -1;

	size_t left = nbytes - n;
	memmove(hn->buf, hn->buf + n, left);

	hn->zs.next_out  = hn->buf + left;
	hn->zs.avail_out = hn->bufsiz - left;
	return n;
}

static Sint64 Zlib_StmRead(void *streamp, void *buf, size_t nbytes)
{
	return Zlib_Read((ZlibStmHn) streamp, buf, nbytes);
}

static Sint64 Zlib_StmWrite(void *streamp, const void *buf, size_t nbytes)
{
	return Zlib_Write((ZlibStmHn) streamp, buf, nbytes);
}

static Sint64 Zlib_StmTell(void *streamp)
{
	ZlibStmHn hn = (ZlibStmHn) streamp;
	return hn->deflating ? hn->zs.total_out : hn->zs.total_in;
}

static Judgement Zlib_StmFinish(void *streamp)
{
	return Zlib_Finish((ZlibStmHn) streamp);
}

static void Zlib_StmClose(void *streamp)
{
	Zlib_Close((ZlibStmHn) streamp);
}

static const StmOps zlib_stmOps = {
	Zlib_StmRead,
	Zlib_StmWrite,
	NULL,
	Zlib_StmTell,
	Zlib_StmFinish,
	Zlib_StmClose
};
static const StmOps zlib_ncStmOps = {
	Zlib_StmRead,
	Zlib_StmWrite,
	NULL,
	Zlib_StmTell,
	Zlib_StmFinish,
	NULL
};

const StmOps *const Zlib_StmOps   = &zlib_stmOps;
const StmOps *const Zlib_NcStmOps = &zlib_ncStmOps;

static THREAD_LOCAL ZlibRet zlib_errStat = 0;

static void Zlib_SetErrStat(ZlibRet ret)
{
	if (ret == Z_ERRNO) {
		Sint64 err = errno;

		ret |= (Sint32) (err << 32);
	}
	zlib_errStat = ret;
}

ZlibRet Zlib_GetErrStat(void)
{
	return zlib_errStat;
}

const char *Zlib_ErrorString(ZlibRet ret)
{
	if (ret == ZSTM_EIO)
		return "I/O error";
	if (ret == ZSTM_EBADSTREAM)
		return "Bad stream operation";

	Sint32 zerrno = (Sint32) (ret &  0xffffffffu);
	Sint32 err    = (Sint32) (ret >> 32);

	switch (zerrno) {
	case Z_OK:            return "Success";
	case Z_ERRNO:         return strerror(err);
	case Z_STREAM_ERROR:  return "Stream error";
	case Z_DATA_ERROR:    return "Data error";
	case Z_MEM_ERROR:     return "Memory allocation failure";
	case Z_BUF_ERROR:     return "Buffer error";
	case Z_VERSION_ERROR: return "Zlib version error";
	default:              return "Unknown Zlib error";
	}
}

ZlibStmHn Zlib_InflateOpen(void              *streamp,
                           const StmOps      *ops,
                           const InflateOpts *opts)
{
	const InflateOpts default_opts = {
		15,
		ZFMT_RFC1952,
		ZSTM_BUFSIZ
	};

	if (!ops->Write) {
		Zlib_SetErrStat(ZSTM_EBADSTREAM);
		return NULL;
	}

	if (!opts)
		opts = &default_opts;

	int wbits = CLAMP(opts->win_bits, 8, 15);

	// Mangle window bits according to the required RFC
	switch (opts->format) {
	case ZFMT_RFC1951:
		wbits = -wbits;
		break;
	case ZFMT_RFC1950:
		break;

	case ZFMT_RFC1952:
	default:
		wbits += 16;
		break;
	}

	size_t bufsiz = MIN(opts->bufsiz, INT_MAX);  // safety to avoid short reads
	if (bufsiz == 0)
		bufsiz = ZSTM_BUFSIZ;

	ZlibStmObj *hn = (ZlibStmObj *) malloc(offsetof(ZlibStmObj, buf[bufsiz]));
	if (!hn) {
		Zlib_SetErrStat(Z_MEM_ERROR);
		return NULL;
	}

	memset(&hn->zs, 0, sizeof(hn->zs));

	int err = inflateInit2(&hn->zs, wbits);
	if (err != Z_OK) {
		Zlib_SetErrStat(err);
		free(hn);
		return NULL;
	}

	hn->streamp   = streamp;
	hn->ops       = ops;
	hn->bufsiz    = bufsiz;
	hn->deflating = FALSE;
	Zlib_SetErrStat(Z_OK);
	return hn;
}

ZlibStmHn Zlib_DeflateOpen(void              *streamp,
                           const StmOps      *ops,
                           const DeflateOpts *opts)
{
	const DeflateOpts default_opts = {
		Z_DEFAULT_COMPRESSION,
		15,
		ZFMT_RFC1952,
		ZSTM_BUFSIZ
	};

	if (!ops->Read) {
		Zlib_SetErrStat(ZSTM_EBADSTREAM);
		return NULL;
	}

	if (!opts)
		opts = &default_opts;

	// Setup open options
	int compression = opts->compression;
	if (compression != Z_DEFAULT_COMPRESSION)
		compression = CLAMP(compression, 0, 9);

	int wbits = CLAMP(opts->win_bits, 8, 15);

	// Mangle window bits according to the required RFC
	switch (opts->format) {
	case ZFMT_RFC1951:
		wbits = -wbits;
		break;
	case ZFMT_RFC1950:
		break;

	case ZFMT_RFC1952:
	default:
		wbits += 16;
		break;
	}

	size_t bufsiz = MIN(opts->bufsiz, INT_MAX);  // safety to avoid short reads
	if (bufsiz == 0)
		bufsiz = ZSTM_BUFSIZ;

	ZlibStmObj *hn = (ZlibStmObj *) malloc(offsetof(ZlibStmObj, buf[bufsiz]));
	if (!hn) {
		Zlib_SetErrStat(Z_MEM_ERROR);
		return NULL;
	}

	memset(&hn->zs, 0, sizeof(hn->zs));
	hn->streamp   = streamp;
	hn->ops       = ops;
	hn->bufsiz    = bufsiz;
	hn->deflating = TRUE;

	int err = deflateInit2(
		&hn->zs,
		compression,
		Z_DEFLATED,
		wbits,
		8,
		Z_DEFAULT_STRATEGY
	);
	if (err != Z_OK) {
		Zlib_SetErrStat(err);
		free(hn);
		return NULL;
	}

	hn->zs.next_out  = hn->buf;
	hn->zs.avail_out = hn->bufsiz;
	Zlib_SetErrStat(Z_OK);
	return hn;
}

Sint64 Zlib_Read(ZlibStmHn hn, void *buf, size_t nbytes)
{
	if (hn->deflating) {
		Zlib_SetErrStat(ZSTM_EBADSTREAM);
		return -1;
	}

	ZlibRet ret = Z_OK;  // unless found otherwise

	hn->zs.next_out  = (Uint8 *) buf;
	hn->zs.avail_out = nbytes;
	while (hn->zs.avail_out > 0) {
		if (hn->zs.avail_in == 0) {
			// Fill buffer
			Sint64 n = hn->ops->Read(hn->streamp, hn->buf, hn->bufsiz);
			if (n <= 0) {
				if (n < 0) ret = ZSTM_EIO;

				break;
			}

			hn->zs.next_in  = hn->buf;
			hn->zs.avail_in = n;
		}

		int err = inflate(&hn->zs, Z_NO_FLUSH);
		if (err == Z_NEED_DICT)
			err = Z_DATA_ERROR;
		if (err != Z_OK && err != Z_STREAM_END) {
			ret = err;
			break;
		}
	}

	Zlib_SetErrStat(ret);
	return nbytes - hn->zs.avail_out;
}

Sint64 Zlib_Write(ZlibStmHn hn, const void *buf, size_t nbytes)
{
	if (!hn->deflating) {
		Zlib_SetErrStat(ZSTM_EBADSTREAM);
		return -1;
	}

	ZlibRet ret = Z_OK;

	hn->zs.next_in  = (Uint8 *) buf;
	hn->zs.avail_in = nbytes;
	while (hn->zs.avail_in > 0) {
		if (hn->zs.avail_out == 0) {
			Sint64 n = Zlib_FlushData(hn);
			if (n <= 0) {
				if (n < 0) ret = ZSTM_EIO;

				break;  // short-write
			}
		}

		int err = deflate(&hn->zs, Z_NO_FLUSH);
		if (err == Z_NEED_DICT)
			err = Z_DATA_ERROR;
		if (err != Z_OK) {
			ret = err;
			break;
		}
	}

	Zlib_SetErrStat(ret);
	return nbytes - hn->zs.avail_in;
}

Judgement Zlib_Finish(ZlibStmHn hn)
{
	if (!hn->deflating) {
		Zlib_SetErrStat(ZSTM_EBADSTREAM);
		return NG;
	}

	int err;
	do {
		err = deflate(&hn->zs, Z_FINISH);
		if (err != Z_STREAM_END && err != Z_BUF_ERROR && err != Z_OK) {
			Zlib_SetErrStat(err);
			return NG;
		}
		if (Zlib_FlushData(hn) == -1) {
			Zlib_SetErrStat(ZSTM_EIO);
			return NG;
		}
	} while (err != Z_STREAM_END);

	if (hn->zs.avail_out != hn->bufsiz) {
		Zlib_SetErrStat(Z_BUF_ERROR);
		return NG;
	}

	Zlib_SetErrStat(Z_OK);
	return OK;
}

void Zlib_Close(ZlibStmHn hn)
{
	// Close stream
	if (hn->ops->Close)
		hn->ops->Close(hn->streamp);

	// Finalize Z_stream
	if (hn->deflating)
		deflateEnd(&hn->zs);
	else
		inflateEnd(&hn->zs);

	free(hn);  // Free memory
}
