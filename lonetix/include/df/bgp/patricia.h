// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/patricia.h
 *
 * PATRICIA trie implementation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGP_PATRICIA_H_
#define DF_BGP_PATRICIA_H_

#include "bgp/prefix.h"

/// Opaque type, trie memory chunk block.
typedef struct Patblock Patblock;
/// Opaque type, concrete trie node.
typedef union Patnode Patnode;

/// PATRICIA trie.
typedef struct {
	Afi       afi;                      ///< AFI type
	unsigned  nprefixes;                ///< Prefixes count stored inside trie

	Patnode  *head;                     ///< Trie root node
	Patblock *blocks;                   ///< PATRICIA memory blocks
	Patnode  *freeBins[IPV6_SIZE / 4];  ///< Fast free bins
} Patricia;

RawPrefix *Pat_Insert(Patricia *trie, const RawPrefix *pfx);
Boolean Pat_Remove(Patricia *trie, const RawPrefix *pfx);

RawPrefix *Pat_SearchExact(const Patricia *trie, const RawPrefix *pfx);

Boolean Pat_IsSubnetOf(const Patricia *trie, const RawPrefix *pfx);
Boolean Pat_IsSupernetOf(const Patricia *trie, const RawPrefix *pfx);
Boolean Pat_IsRelatedOf(const Patricia *trie, const RawPrefix *pfx);

/// Reset `trie` and free all memory.
void Pat_Clear(Patricia *trie);

#endif
