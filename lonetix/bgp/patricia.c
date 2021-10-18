// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/patricia.c
 *
 * Implements PATRICIA trie utilities.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/patricia.h"
#include "sys/sys.h"  // for Sys_OutOfMemory()
#include "smallbytecopy.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define PAT_BLOCK_ALIGN_SHIFT  2
#define PAT_BLOCK_ALIGN       (1uLL << PAT_BLOCK_ALIGN_SHIFT)
#define PAT_BLOCK_SIZE         2048

struct Patblock {
	Patblock *nextBlock;
	Uint32    freeOfs;
	ALIGNED(PAT_BLOCK_ALIGN, Uint8 buf[PAT_BLOCK_SIZE]);
};

/**
 * \brief Trie node, contains node and prefix info.
 *
 * A node may be either:
 * - Used - node is in use and part of the trie.
 * - Unused - node is inside free node list and may be reclaimed on further
 *   node insertion.
 */
union Patnode {
	// Following struct is significant when node is used.
	struct {
		Patnode *parent;      // NOTE: LSB used to mark glue nodes
		Patnode *children[2];

		// Prefix part follows...
		Uint8    width;
		Uint8    bytes[FLEX_ARRAY];
	};
	// Following field is significant when node is unused.
	Patnode *nextFree;
};

static Patnode *Pat_NodeForPrefix(const RawPrefix *pfx)
{
	return (Patnode *) ((Uint8 *) pfx - offsetof(Patnode, width));
}

static Boolean Pat_IsNodeGlue(const Patnode *n)
{
	return (((Uintptr) n->parent) & 1uLL) != 0;
}

static Patnode *Pat_GetNodeParent(const Patnode *n)
{
	return (Patnode *) ((Uintptr) n->parent & ~1uLL);
}

static void Pat_SetNodeParent(Patnode *n, Patnode *parent)
{
	n->parent = (Patnode *) ((Uintptr) parent | (((Uintptr) n->parent) & 1uLL));
}

static void Pat_SetNodeGlue(Patnode *n)
{
	n->parent = (Patnode *) ((Uintptr) n->parent | 1uLL);
}

static void Pat_ResetNodeGlue(Patnode *n)
{
	n->parent = (Patnode *) ((Uintptr) n->parent & ~1uLL);
}

static Patnode *Pat_AllocNode(Patricia *trie, Uint8 width)
{
	Patnode  *n;
	Patblock *block;
	size_t    siz, len;
	unsigned  idx;

	// Calculate block size and lookup inside free cache
	len = PFXLEN(width);
	assert(len <= IPV6_SIZE);

	idx = len >> PAT_BLOCK_ALIGN_SHIFT;
	siz = ALIGN(offsetof(Patnode, bytes[len]), PAT_BLOCK_ALIGN);
	n   = trie->freeBins[idx];
	if (n) {
		// ...free list cache hit
		trie->freeBins[idx] = n->nextFree;

		goto return_node;
	}

	// Need to allocate a new node
	block = trie->blocks;
	if (!block || block->freeOfs + siz > PAT_BLOCK_SIZE) {
		// Must allocate a new block altoghether
		block = (Patblock *) malloc(sizeof(*block));
		if (!block) {
			Sys_OutOfMemory();
			return NULL;  // too bad...
		}

		block->freeOfs   = 0;
		block->nextBlock = trie->blocks;
		trie->blocks     = block;
	}

	n = (Patnode *) (block->buf + block->freeOfs);
	block->freeOfs += siz;

return_node:
	n->parent      = NULL;
	n->children[0] = n->children[1] = NULL;

	n->width = width;
	return n;
}

static void Pat_FreeNode(Patricia *trie, Patnode *n)
{
	// Place inside free cache bins
	unsigned idx = PFXLEN(n->width) >> PAT_BLOCK_ALIGN_SHIFT;

	n->nextFree         = trie->freeBins[idx];
	trie->freeBins[idx] = n;
}

RawPrefix *Pat_Insert(Patricia *trie, const RawPrefix *pfx)
{
	Patnode *n;
	unsigned maxWidth;
	
	assert(trie->afi == AFI_IP || trie->afi == AFI_IP6);
	maxWidth = (trie->afi == AFI_IP6) ? IPV6_WIDTH : IPV4_WIDTH;

	assert(pfx->width <= maxWidth);

	n = trie->head;
	if (!n) {
		// First node ever, create trie head node
		n = Pat_AllocNode(trie, pfx->width);
		if (!n)
			return NULL;

		_smallbytecopy16(n->bytes, pfx->bytes, PFXLEN(pfx->width));

		// Place it in `trie`
		trie->head = n;
		trie->nprefixes++;
		return PLAINPFX(n);
	}

	while (n->width < pfx->width || Pat_IsNodeGlue(n)) {
		int bit = (n->width < maxWidth) &&
		          (pfx->bytes[n->width >> 3] & (0x80 >> (n->width & 0x07)));

		if (!n->children[bit])
			break;

		n = n->children[bit];
	}

	unsigned checkBit  = MIN(n->width, pfx->width);
	unsigned differBit = 0;

#if 1
	// unoptimized version
	unsigned r;
	for (unsigned i = 0, z = 0; z < checkBit; i++, z += 8) {
		r = (pfx->bytes[i] ^ n->bytes[i]);
		if (r == 0) {
			differBit = z + 8;
			continue;
		}

		unsigned j;
		for (j = 0; j < 8; j++)
			if (r & (0x80 >> j))
				break;

		differBit = z + j;
		break;
	}
#else
	/* TODO possible optimization:
	 * example 32 bit portion with different endianness:

	 LSB    (visit using LSB->MSB)        MSB (LE)
	 01000000 00000000 00000100 00000000
	 MSB    (visit using MSB->LSB)        LSB (BE)

	 leftmost bit is:
	 -> 32 - bsr32() (BE)
	 -> bsf32() - 1  (LE)
	 */

	for (unsigned i = 0, z = 0; z < checkBit; i++, z += 32) {
		Uint32 r = (pfx->u32[i] ^ n->u32[i]);

		if (r == 0) {
			differBit = z + 32;
			continue;
		}

		unsigned j = (EDN_NATIVE == EDN_BIG) ?  32 - bsr32(r) : bsf32(r) - 1; // clz(beswap32(r));

		differBit = z + j;
		break;
	}

#endif

	if (differBit > checkBit)
		differBit = checkBit;

	Patnode *parent = Pat_GetNodeParent(n);
	while (parent && parent->width >= differBit) {
		n      = parent;
		parent = Pat_GetNodeParent(n);
	}

	if (differBit == pfx->width && n->width == pfx->width) {
		if (Pat_IsNodeGlue(n)) {
			// Replace glue node
			Pat_ResetNodeGlue(n);
			_smallbytecopy16(n->bytes, pfx->bytes, PFXLEN(pfx->width));
		}

		trie->nprefixes++;
		return PLAINPFX(n);
	}

	// Must allocate new node
	Patnode *newNode = Pat_AllocNode(trie, pfx->width);
	if (!newNode)
		return NULL;  // out of memory

	_smallbytecopy16(newNode->bytes, pfx->bytes, PFXLEN(pfx->width));
	trie->nprefixes++;

	if (n->width == differBit) {
		Pat_SetNodeParent(newNode, n);

		int bit = (n->width < maxWidth) &&
		          (pfx->bytes[n->width >> 3] & (0x80 >> (n->width & 0x07)));

		n->children[bit] = newNode;
		return PLAINPFX(newNode);
	}

	if (pfx->width == differBit) {
		int bit = (pfx->width < maxWidth) &&
		          (n->bytes[pfx->width >> 3] & (0x80 >> (pfx->width & 0x07)));

		newNode->children[bit] = n;
		Pat_SetNodeParent(newNode, n);

		parent = Pat_GetNodeParent(n);
		if (!parent)
			trie->head = newNode;

		else if (parent->children[1] == n) {
			int bit = (parent->children[1] == n);
			parent->children[bit] = n;
		}

		Pat_SetNodeParent(n, newNode);
	} else {
		Patnode *glue = Pat_AllocNode(trie, differBit);
		if (!glue)
			return NULL;

		parent = Pat_GetNodeParent(n);

		glue->parent = parent;
		Pat_SetNodeGlue(glue);

		int bit = (differBit < maxWidth) &&
		          (pfx->bytes[differBit >> 3] & (0x80 >> (differBit & 0x07)));

		glue->children[bit]  = newNode;
		glue->children[!bit] = n;

		newNode->parent = glue;
		if (!parent)
			trie->head = glue;

		else {
			int bit = (parent->children[1] == n);
			parent->children[bit] = glue;
		}

		Pat_SetNodeParent(n, glue);
	}

	return PLAINPFX(newNode);
}

static Boolean Ip_CompWithMask(const Uint8 *a, const Uint8 *b, Uint8 mask)
{
	unsigned n = mask / 8;

	if (memcmp(a, b, n) == 0) {
		unsigned m = ~0u << (8 - (mask % 8));

		if ((mask & 0x7) == 0 || (a[n] & m) == (b[n] & m))
			return TRUE;
	}

	return FALSE;
}

RawPrefix *Pat_SearchExact(const Patricia *trie, const RawPrefix *pfx)
{
	const Patnode *n = trie->head;
	if (!n)
		return NULL;

	while (n->width < pfx->width) {
		int bit = (pfx->bytes[n->width >> 3] & (0x80 >> (n->width & 0x07))) != 0;

		n = n->children[bit];
		if (!n)
			return NULL;
	}
	
	if (n->width > pfx->width || Pat_IsNodeGlue(n))
		return NULL;
	
	if (Ip_CompWithMask(n->bytes, pfx->bytes, pfx->width))
		return PLAINPFX(n);
	
	return NULL;
}

Boolean Pat_IsSubnetOf(const Patricia *trie, const RawPrefix *pfx)
{
	const Patnode *n = trie->head;

	while (n && n->width < pfx->width) {
		if (!Pat_IsNodeGlue(n))
			return Ip_CompWithMask(n->bytes, pfx->bytes, n->width);

		int bit = (pfx->bytes[n->width >> 3] & (0x80 >> (n->width & 0x07))) != 0;

		n = n->children[bit];
	}

	return n && !Pat_IsNodeGlue(n) &&
	       n->width <= pfx->width &&
	       Ip_CompWithMask(n->bytes, pfx->bytes, pfx->width);
}

Boolean Pat_IsSupernetOf(const Patricia *trie, const RawPrefix *pfx)
{
	Patnode *start = trie->head;
	while (start && start->width < pfx->width) {
		int bit = (pfx->bytes[start->width >> 3] & (0x80 >> (start->width & 0x07))) != 0;

		start = start->children[bit];
	}

	Patnode *node;

	Patnode  *stack[128+1];
	Patnode **sp   = stack;
	Patnode  *next = start;
	while ((node = next) != NULL) {
		if (!Pat_IsNodeGlue(node)) {
			if (Ip_CompWithMask(node->bytes, pfx->bytes, pfx->width))
				return TRUE;

			break;
		}

		if (next->children[0]) {
			if (next->children[1])
				*sp++ = next->children[1];

			next = next->children[0];
		} else if (next->children[1]) {
			next = next->children[1];
		} else if (sp != stack) {
			next = *(sp--);
		} else {
			next = NULL;
		}
	}

	return FALSE;
}

Boolean Pat_IsRelatedOf(const Patricia *trie, const RawPrefix *pfx)
{
	Patnode *start = trie->head;
	while (start && start->width < pfx->width) {
		if (!Pat_IsNodeGlue(start) && Ip_CompWithMask(start->bytes, pfx->bytes, start->width))
			return TRUE;

		int bit = (pfx->bytes[start->width >> 3] & (0x80 >> (start->width & 0x07))) != 0;

		start = start->children[bit];
	}

	Patnode *node;

	Patnode  *stack[128+1];
	Patnode **sp   = stack;
	Patnode  *next = start;
	while ((node = next) != NULL) {
		if (!Pat_IsNodeGlue(node) && Ip_CompWithMask(node->bytes, pfx->bytes, pfx->width))
			return TRUE;

		if (next->children[0]) {
			if (next->children[1])
				*sp++ = next->children[1];

			next = next->children[0];
		} else if (next->children[1]) {
			next = next->children[1];
		} else if (sp != stack) {
			next = *(sp--);
		} else {
			next = NULL;
		}
	}

	return FALSE;
}

Boolean Pat_Remove(Patricia *trie, const RawPrefix *pfx)
{
	RawPrefix *res = Pat_SearchExact(trie, pfx);
	if (!res)
		return FALSE;

	Patnode *n = Pat_NodeForPrefix(res);
	if (!n)
		return FALSE;

	trie->nprefixes--;
	if (n->children[0] && n->children[1]) {
		Pat_SetNodeGlue(n);
		return TRUE;
	}

	Patnode *parent, *pparent;
	Patnode *child;
	int      bit;

	parent = Pat_GetNodeParent(n);
	if (!n->children[0] && !n->children[1]) {
		Pat_FreeNode(trie, n);
		if (!parent) {
			trie->head = NULL;
			return TRUE;
		}

		bit = (parent->children[1] == n);
		parent->children[bit] = NULL;
		child = parent->children[!bit];

		if (!Pat_IsNodeGlue(parent))
			return TRUE;

		// If here, then parent is glue, we need to remove them both
		pparent = Pat_GetNodeParent(parent);
		if (!pparent) {
			trie->head = child;

		} else {
			bit = (pparent->children[1] == parent);
			pparent->children[bit] = child;
		}

		Pat_SetNodeParent(child, pparent);
		Pat_FreeNode(trie, parent);
		return TRUE;
	}

	bit   = (n->children[1] != NULL);
	child = n->children[bit];

	Pat_SetNodeParent(child, parent);
	Pat_FreeNode(trie, n);
	if (!parent) {
		trie->head = child;
		return TRUE;
	}

	bit = (parent->children[1] == n);
	parent->children[bit] = child;
	return TRUE;
}

void Pat_Clear(Patricia *trie)
{
	while (trie->blocks) {
		Patblock *t = trie->blocks;

		trie->blocks = t->nextBlock;
		free(t);
	}

	trie->afi       = 0;
	trie->nprefixes = 0;
	trie->head      = NULL;
	memset(trie->freeBins, 0, sizeof(trie->freeBins));
}
