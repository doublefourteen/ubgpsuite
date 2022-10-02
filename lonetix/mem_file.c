// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file mem_file.c
 *
 * Implements operations over `MemFile` and its `StmOps` interface.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/sys.h"
#include "mem_file.h"

#include <stdlib.h>
#include <string.h>

static Judgement Stm_MemFileGrow(MemFile *stm, size_t size)
{
	char *buf;

	size = ALIGN(size, stm->gran);

	if ((stm->flags & MEM_FILE_OWNBIT) == 0) {
		// Original buffer not owned by `file`, allocate anew
		buf = (char *) malloc(size);
		if (!buf) {
			Sys_OutOfMemory();
			return NG;
		}

		memcpy(buf, stm->buf, stm->nbytes);
	} else {
		// May just reallocate the old one
		buf = (char *) realloc(stm->buf, size);
		if (!buf) {
			Sys_OutOfMemory();
			return NG;
		}
	}

	stm->buf = buf;
	stm->cap = size;

	// Buffer is now owned, regardless of the previous state
	stm->flags |= MEM_FILE_OWNBIT;
	return OK;
}

static Sint64 Stm_OpMemFileRead(void *streamp, void *buf, size_t nbytes)
{
	return Stm_MemFileRead((MemFile *) streamp, buf, nbytes);
}

static Sint64 Stm_OpMemFileWrite(void *streamp, const void *buf, size_t nbytes)
{
	return Stm_MemFileWrite((MemFile *) streamp, buf, nbytes);
}

static Sint64 Stm_OpMemFileSeek(void *streamp, Sint64 off, SeekMode whence)
{
	return Stm_MemFileSeek((MemFile *) streamp, off, whence);
}

static Sint64 Stm_OpMemFileTell(void *streamp)
{
	return ((MemFile *) streamp)->pos;
}

static Judgement Stm_OpMemFileFinish(void *streamp)
{
	USED(streamp);

	return OK;  // NOP
}

static void Stm_OpMemFileClose(void *streamp)
{
	Stm_MemFileClose((MemFile *) streamp);
}

static const StmOps mem_stmOps = {
    Stm_OpMemFileRead,
    Stm_OpMemFileWrite,
    Stm_OpMemFileSeek,
    Stm_OpMemFileTell,
    Stm_OpMemFileFinish,
    Stm_OpMemFileClose
};
static const StmOps mem_ncStmOps = {
    Stm_OpMemFileRead,
    Stm_OpMemFileWrite,
    Stm_OpMemFileSeek,
    Stm_OpMemFileTell,
    Stm_OpMemFileFinish,
    NULL
};

const StmOps *const Stm_MemFileOps   = &mem_stmOps;
const StmOps *const Stm_NcMemFileOps = &mem_ncStmOps;

void Stm_InitMemFile(MemFile *stm, size_t gran, unsigned flags)
{
	memset(stm, 0, sizeof(*stm));
	stm->gran  = gran;

	flags     &= ~MEM_FILE_NOGROWBIT;
	stm->flags = flags | MEM_FILE_WRBIT;
}

void Stm_MemFileFromBuf(MemFile *stm,
                        void    *buf,
                        size_t   nbytes,
                        size_t   gran,
                        unsigned flags)
{
	memset(stm, 0, sizeof(*stm));
	stm->buf   = (char *) buf;
	stm->cap   = nbytes;
	stm->gran  = gran;
	stm->flags = flags | MEM_FILE_WRBIT;
}

void Stm_RoMemFileFromBuf(MemFile *stm, const void *buf, size_t nbytes)
{
	memset(stm, 0, sizeof(*stm));
	stm->buf    = (char *) buf;  // safe, MEM_FILE_RDBIT
	stm->nbytes = nbytes;
	stm->flags  = MEM_FILE_RDBIT | MEM_FILE_NOGROWBIT;
}

Sint64 Stm_MemFileRead(MemFile *stm, void *buf, size_t nbytes)
{
	if ((stm->flags & MEM_FILE_RDBIT) == 0)
		return -1;

	size_t nleft = stm->nbytes - stm->pos;
	if (nbytes > nleft)
		nbytes = nleft;

	memcpy(buf, &stm->buf[stm->pos], nbytes);
	stm->pos += nbytes;
	return nbytes;
}

Sint64 Stm_MemFileWrite(MemFile *stm, const void *buf, size_t nbytes)
{
	if ((stm->flags & MEM_FILE_WRBIT) == 0)
		return -1;

	size_t navail = stm->cap - stm->pos;
	size_t nreq   = nbytes + 1;  // for trailing '\0'
	if (navail < nreq) {
		if ((stm->flags & MEM_FILE_NOGROWBIT) == 0) {
			// grow buffer
			if (Stm_MemFileGrow(stm, stm->pos + nreq) != OK)
				return -1;

		} else
			nbytes = navail;  // short write
	}

	memcpy(&stm->buf[stm->pos], buf, nbytes);
	stm->pos    += nbytes;
	stm->nbytes += nbytes;
	if (stm->pos == stm->nbytes)
		stm->buf[stm->pos] = '\0';  // always NUL-terminate

	return nbytes;
}

Sint64 Stm_MemFileSeek(MemFile *stm, Sint64 off, SeekMode whence)
{
	switch (whence) {
	case SK_SET:
		break;

	case SK_CUR:
		off += stm->pos;
		break;

	case SK_END:
		off += stm->nbytes;
		break;

	default: return -1;
	}

	// Make sure cursor isn't set out of bounds
	stm->pos = CLAMP(off, 0, (Sint64) stm->nbytes);
	return stm->pos;
}

void Stm_MemFileClose(MemFile *stm)
{
	if (stm->flags & MEM_FILE_OWNBIT)
		free(stm->buf);
}
