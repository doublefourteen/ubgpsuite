// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file cpr/bzip2.c
 *
 * Interfaces with `libbzip2` and implements BZ2 compressor/decompressor.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "cpr/bzip2.h"

#include <bzlib.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct Bzip2StmObj Bzip2StmObj;
struct Bzip2StmObj {
	bz_stream     bz2;
	void         *streamp;
	const StmOps *ops;
	unsigned      bufsiz;
	Boolean8      compressing;
	Boolean8      eof;
	Boolean8      small;
	Uint8         verbosity;
	char          buf[FLEX_ARRAY];  // `bufsiz' bytes
};

#define BZIP2_EBADSTREAM 0xffff

#define BZIP2_BUFSIZ (32 * 1024)

#define MAKESINT64(lo32, hi32) \
	((Sint64) ((Uint64) (lo32) | (((Uint64) hi32) << 32)))

static Sint64 Bzip2_FlushData(Bzip2StmHn hn)
{
	size_t nbytes = hn->bufsiz - hn->bz2.avail_out;

	Sint64 n = hn->ops->Write(hn->streamp, hn->buf, nbytes);
	if (n <= 0)
		return -1;

	size_t left = nbytes - n;
	memmove(hn->buf, hn->buf + n, left);

	hn->bz2.next_out  = hn->buf + left;
	hn->bz2.avail_out = hn->bufsiz - left;
	return n;
}

static Sint64 Bzip2_StmRead(void *streamp, void *buf, size_t nbytes)
{
	return Bzip2_Read((Bzip2StmHn) streamp, buf, nbytes);
}

static Sint64 Bzip2_StmWrite(void *streamp, const void *buf, size_t nbytes)
{
	return Bzip2_Write((Bzip2StmHn) streamp, buf, nbytes);
}

static Sint64 Bzip2_StmTell(void *streamp)
{
	Bzip2StmHn hn = (Bzip2StmHn) streamp;

	return hn->compressing ?
		MAKESINT64(hn->bz2.total_out_lo32, hn->bz2.total_out_hi32) :
		MAKESINT64(hn->bz2.total_in_lo32, hn->bz2.total_in_hi32);
}

static Judgement Bzip2_StmFinish(void *streamp)
{
	return Bzip2_Finish((Bzip2StmHn) streamp);
}

static void Bzip2_StmClose(void *streamp)
{
	Bzip2_Close((Bzip2StmHn) streamp);
}

static const StmOps bzip2_stmOps = {
	Bzip2_StmRead,
	Bzip2_StmWrite,
	NULL,
	Bzip2_StmTell,
	Bzip2_StmFinish,
	Bzip2_StmClose
};
static const StmOps bzip2_ncStmOps = {
	Bzip2_StmRead,
	Bzip2_StmWrite,
	NULL,
	Bzip2_StmTell,
	Bzip2_StmFinish,
	NULL
};

const StmOps *const Bzip2_StmOps   = &bzip2_stmOps;
const StmOps *const Bzip2_NcStmOps = &bzip2_ncStmOps;

static THREAD_LOCAL Bzip2Ret bzip2_errStat = 0;

static void Bzip2_SetErrStat(Bzip2Ret ret)
{
	bzip2_errStat = ret;
}

static Boolean Bzip2_FillBuf(Bzip2StmHn hn)
{
	Sint64 n = hn->ops->Read(hn->streamp, hn->buf, hn->bufsiz);

	if (n <= 0) {
		if (n == 0)
			hn->eof = TRUE;
		else
			Bzip2_SetErrStat(BZ_IO_ERROR);

		return FALSE;
	}

	hn->bz2.next_in  = hn->buf;
	hn->bz2.avail_in = n;
	return TRUE;
}

static Boolean Bzip2_ResetDecompress(Bzip2StmHn hn)
{
	if (hn->eof)
		return FALSE;

	if (hn->bz2.avail_in == 0 && !Bzip2_FillBuf(hn))
		return FALSE;

	// Save current buffer state
	char *out = hn->bz2.next_out;
	size_t nout = hn->bz2.avail_out;

	char *unused = hn->bz2.next_in;
	size_t nunused = hn->bz2.avail_in;

	// Reset decompressor
	BZ2_bzDecompressEnd(&hn->bz2);

	int err = BZ2_bzDecompressInit(&hn->bz2, hn->verbosity, hn->small);

	// Restore state, so Bzip2_Read() can calculate
	// proper I/O stats
	hn->bz2.next_in   = unused;
	hn->bz2.avail_in  = nunused;
	hn->bz2.next_out  = out;
	hn->bz2.avail_out = nout;

	if (err != BZ_OK) {
		// Clear bz2.state so no trouble occurs on Bzip2_Close()
		hn->bz2.state = NULL;
		Bzip2_SetErrStat(err);
		return FALSE;
	}

	return TRUE;
}

Bzip2Ret Bzip2_GetErrStat(void)
{
	return bzip2_errStat;
}

const char *Bzip2_ErrorString(Bzip2Ret ret)
{
	switch (ret) {
	case BZIP2_EBADSTREAM:    return "Bad stream operation";
	case BZ_OK:               return "Success";
	case BZ_SEQUENCE_ERROR:   return "Sequence error";
	case BZ_PARAM_ERROR:      return "Invalid parameter";
	case BZ_MEM_ERROR:        return "Memory allocation failure";
	case BZ_DATA_ERROR:       return "Data integrity error";
	case BZ_DATA_ERROR_MAGIC: return "Stream magic number mismatch";
	case BZ_IO_ERROR:         return "I/O error";
	case BZ_UNEXPECTED_EOF:   return "Unexpected compressed stream end";
	case BZ_OUTBUFF_FULL:     return "Output buffer full";
	case BZ_CONFIG_ERROR:     return "Bzip2 library configuration error";
	default:                  return "Unknown BZ2 error";
	}
}

Bzip2StmHn Bzip2_OpenCompress(void               *streamp,
                              const StmOps       *ops,
                              const Bzip2CprOpts *opts)
{
	const Bzip2CprOpts defOpts = { 0, 0, 0, 0 };
	if (!opts)
		opts = &defOpts;

	if (!ops->Write) {
		Bzip2_SetErrStat(BZIP2_EBADSTREAM);
		return NULL;
	}

	int compression = CLAMP(opts->compression, 0, 9);
	if (compression == 0)
		compression = 9;  // default value

	int verbosity   = CLAMP(opts->verbose, 0, 4);
	int factor      = opts->factor;
	size_t bufsiz   = MIN(opts->bufsiz, INT_MAX);
	if (bufsiz == 0)
		bufsiz = BZIP2_BUFSIZ;

	Bzip2StmObj *hn = (Bzip2StmObj *) malloc(offsetof(Bzip2StmObj, buf) + bufsiz);
	if (!hn) {
		Bzip2_SetErrStat(BZ_MEM_ERROR);
		return NULL;
	}

	memset(&hn->bz2, 0, sizeof(hn->bz2));
	hn->streamp     = streamp;
	hn->ops         = ops;
	hn->bufsiz      = bufsiz;
	hn->compressing = TRUE;

	int err = BZ2_bzCompressInit(&hn->bz2, compression, verbosity, factor);
	if (err != BZ_OK) {
		Bzip2_SetErrStat(err);
		free(hn);
		return NULL;
	}

	hn->bz2.next_out  = hn->buf;
	hn->bz2.avail_out = hn->bufsiz;
	Bzip2_SetErrStat(BZ_OK);
	return hn;
}

Bzip2StmHn Bzip2_OpenDecompress(void               *streamp,
                                const StmOps       *ops,
                                const Bzip2DecOpts *opts)
{
	const Bzip2DecOpts defOpts = { 0, 0, FALSE };
	if (!opts)
		opts = &defOpts;

	if (!ops->Read) {
		Bzip2_SetErrStat(BZIP2_EBADSTREAM);
		return NULL;
	}

	int small     = opts->low_mem;
	int verbosity = CLAMP(opts->verbose, 0, 4);
	size_t bufsiz = MIN(opts->bufsiz, INT_MAX);
	if (bufsiz == 0)
		bufsiz = BZIP2_BUFSIZ;

	Bzip2StmObj *hn = (Bzip2StmObj *) malloc(offsetof(Bzip2StmObj, buf[bufsiz]));
	if (!hn) {
		Bzip2_SetErrStat(BZ_MEM_ERROR);
		return NULL;
	}

	memset(&hn->bz2, 0, sizeof(hn->bz2));
	hn->streamp     = streamp;
	hn->ops         = ops;
	hn->bufsiz      = bufsiz;
	hn->compressing = FALSE;
	hn->eof         = FALSE;
	hn->small       = small;
	hn->verbosity   = verbosity;

	int err = BZ2_bzDecompressInit(&hn->bz2, verbosity, small);
	if (err != BZ_OK) {
		Bzip2_SetErrStat(err);
		free(hn);
		return NULL;
	}

	Bzip2_SetErrStat(BZ_OK);
	return hn;
}

Sint64 Bzip2_Read(Bzip2StmHn hn, void *buf, size_t nbytes)
{
	if (hn->compressing) {
		Bzip2_SetErrStat(BZIP2_EBADSTREAM);
		return -1;
	}

	hn->bz2.next_out  = (char *) buf;
	hn->bz2.avail_out = nbytes;

	while (hn->bz2.avail_out > 0) {
		if (hn->bz2.avail_in == 0 && !Bzip2_FillBuf(hn))
			break;

		int err = BZ2_bzDecompress(&hn->bz2);
		if (err != BZ_OK) {
			if (err != BZ_STREAM_END) {
				Bzip2_SetErrStat(err);
				break;
			}
			if (!Bzip2_ResetDecompress(hn))
				break;  // done
		}
	}

	return nbytes - hn->bz2.avail_out;
}

Sint64 Bzip2_Write(Bzip2StmHn hn, const void *buf, size_t nbytes)
{
	if (!hn->compressing) {
		Bzip2_SetErrStat(BZIP2_EBADSTREAM);
		return -1;
	}

	Bzip2Ret ret = BZ_OK;

	hn->bz2.next_in  = (char *) buf;  // safe
	hn->bz2.avail_in = nbytes;
	while (hn->bz2.avail_in > 0) {
		if (hn->bz2.avail_out == 0) {
			Sint64 n = Bzip2_FlushData(hn);
			if (n <= 0) {
				if (n < 0) ret = BZ_IO_ERROR;

				break;
			}
		}

		int err = BZ2_bzCompress(&hn->bz2, BZ_RUN);
		if (err != BZ_RUN_OK) {
			ret = err;
			break;
		}
	}

	Bzip2_SetErrStat(ret);
	return nbytes - hn->bz2.avail_in;
}

Judgement Bzip2_Finish(Bzip2StmHn hn)
{
	if (!hn->compressing) {
		Bzip2_SetErrStat(BZIP2_EBADSTREAM);
		return NG;
	}

	int err;

	do {
		// Call BZ2_bzCompress() repeatedly with BZ_FINISH to consume all data
		err = BZ2_bzCompress(&hn->bz2, BZ_FINISH);
		if (err != BZ_STREAM_END && err != BZ_FINISH_OK) {
			Bzip2_SetErrStat(err);
			return NG;
		}
		if (Bzip2_FlushData(hn) == -1) {
			Bzip2_SetErrStat(BZ_IO_ERROR);
			return NG;
		}
	} while (err != BZ_STREAM_END);

	Bzip2_SetErrStat(BZ_OK);
	return OK;
}

void Bzip2_Close(Bzip2StmHn hn)
{
	if (hn->ops->Close)
		hn->ops->Close(hn->streamp);

	if (hn->compressing)
		BZ2_bzCompressEnd(&hn->bz2);
	else
		BZ2_bzDecompressEnd(&hn->bz2);

	free(hn);
}
