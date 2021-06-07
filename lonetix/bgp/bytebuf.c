// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/bytebuf.c
 *
 * Trivial BGP memory allocator for basic BGP workloads.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bytebuf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

STATIC_ASSERT(BGP_MEMBUF_ALIGN >= 4, "bytebuf.c assumes Uint32 header");

#define USEDBIT        BIT(0)
#define MAXBUFCHUNKSIZ 0xffffffffuLL

#define BLKSIZ(ptr)  ((*(Uint32 *) (ptr)) & ~USEDBIT)
#define ISUSED(ptr)  (((*(Uint32 *) (ptr)) & USEDBIT) != 0)
#define SETUSED(ptr) ((void) ((*(Uint32 *) (ptr)) |= USEDBIT))
#define CLRUSED(ptr) ((void) ((*(Uint32 *) (ptr)) &= ~USEDBIT))

static Boolean Mem_IsInBuffer(Bgpbytebuf *buf, void *ptr)
{
	return (Uint8 *) ptr >= buf->base &&
	       (Uint8 *) ptr <  buf->base + buf->size;
}

static Uint8 *Mem_FindPrevChunk(Bgpbytebuf *buf, void *chunk)
{
	assert(Mem_IsInBuffer(buf, chunk));

	Uint8 *p = buf->base;
	while (p < (Uint8 *) chunk) {
		size_t siz = BLKSIZ(p);

		if (p + siz == (Uint8 *) chunk)
			return p;

		p += siz;
	}

	return NULL;
}

static void Mem_BgpFree(void *allocator, void *ptr)
{
	Bgpbytebuf *buf = (Bgpbytebuf *) allocator;

	// Regular free() for out of buffer allocations
	if (!Mem_IsInBuffer(buf, ptr)) {
		free(ptr);
		return;
	}

	// Get pointer to chunk
	Uint8 *p = (Uint8 *) ptr - 4;
	assert(ISUSED(p));
	// Get buffer limit
	Uint8 *lim = buf->base + buf->pos;

	Uint32 siz = BLKSIZ(p);
	CLRUSED(p);  // toggle off USEDBIT

	// Find successor if any
	Uint8 *next = p + siz;
	if (next < lim && !ISUSED(next)) {
		// Merge forward
		siz += BLKSIZ(next);

		*(Uint32 *) p = siz;
	}

	// Find predecessor, if any
	Uint8 *prev = Mem_FindPrevChunk(buf, p);
	if (prev && !ISUSED(prev)) {
		// Merge backwards
		siz += BLKSIZ(prev);
		p    = prev;

		*(Uint32 *) p = siz;
	}

	// Move position backwards when freeing last block
	if (p + siz == lim)
		buf->pos -= siz;
}

static void *Mem_BgpDoRealloc(Bgpbytebuf *buf, void *oldp, size_t nbytes)
{
	// Use plain realloc() if we're not managing a buffered pointer
	if (!Mem_IsInBuffer(buf, oldp))
		return realloc(oldp, nbytes);

	assert(IS_PTR_ALIGNED(oldp, BGP_MEMBUF_ALIGN));

	Uint8 *ptr =  (Uint8 *) oldp - 4;
	assert(ISUSED(ptr));

	Uint32 oldSiz = BLKSIZ(ptr);
	assert(buf->pos >= oldSiz);

	size_t siz = 4 + ALIGN(nbytes, BGP_MEMBUF_ALIGN);
	if (oldSiz >= siz) {
		// Shrink operation, free up the trailing part if we're the last chunk
		if (ptr + oldSiz == buf->base + buf->pos) {
			*(Uint32 *) ptr = siz | USEDBIT;
			buf->pos       -= (oldSiz - siz);
		}

		return oldp;
	}

	// May only grow a chunk if this is the last one and we don't overflow
	if (ptr + oldSiz !=  buf->base + buf->pos)
		return NULL;

	size_t newPos = buf->pos + (siz - oldSiz);
	if (newPos > buf->size)
		return NULL;

	// Ok to grow the chunk
	*(Uint32 *) ptr = siz | USEDBIT;
	buf->pos        = newPos;
	return oldp;
}

static void *Mem_BgpDoAlloc(Bgpbytebuf *buf, size_t nbytes)
{
	// Use plain malloc() for large allocations or when out of buffer space
	size_t siz = 4 + ALIGN(nbytes, BGP_MEMBUF_ALIGN);
	if (buf->pos + siz > buf->size || siz > MAXBUFCHUNKSIZ)
		return malloc(nbytes);

	// Return the next chunk available
	Uint32 *ptr = (Uint32 *) (buf->base + buf->pos);
	buf->pos   += siz;
	assert((siz & USEDBIT) == 0);

	*ptr++ = siz | USEDBIT;
	return ptr;
}

static void *Mem_BgpAlloc(void *allocator, size_t nbytes, void *oldp)
{
	Bgpbytebuf *buf = (Bgpbytebuf *) allocator;

	// Handle common allocations with no `oldp`
	if (!oldp)
		return Mem_BgpDoAlloc(buf, nbytes);

	// Attempt memory reuse
	void *ptr = Mem_BgpDoRealloc(buf, oldp, nbytes);
	if (ptr)
		return ptr;

	// Fallback to simple allocation+memcpy()
	ptr = Mem_BgpDoAlloc(buf, nbytes);
	if (ptr) {
		memcpy(ptr, oldp, nbytes);
		Mem_BgpFree(buf, oldp);
	}

	return ptr;
}

static const MemOps mem_bgpBufTable = {
	Mem_BgpAlloc,
	Mem_BgpFree
};

const MemOps *const Mem_BgpBufOps = &mem_bgpBufTable;
