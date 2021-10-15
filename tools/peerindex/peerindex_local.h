// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file peeridx_local.h
 *
 * `peerindex` private header.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_PEERINDEX_LOCAL_H_
#define DF_PEERINDEX_LOCAL_H_

#include "bgp/mrt.h"
#include "bufio.h"

#include <setjmp.h>

#define PEERREFSTABSIZ (0x10000uLL / 64)

typedef Uint64 PeerRefsTab[PEERREFSTABSIZ];

FORCE_INLINE void MARKPEERINDEX(PeerRefsTab tab, Uint16 idx)
{
	tab[idx >> 6] |= (1uLL << (idx & 0x3f));
}

FORCE_INLINE Boolean ISPEERINDEXREF(const PeerRefsTab tab, Uint16 idx)
{
	return (tab[idx >> 6] & (1uLL << (idx & 0x3f))) != 0;
}

typedef struct {
	// Output stream
	void         *outf;
	const StmOps *outfOps;

	// MRT input file stream
	const char   *filename;
	void         *inf;
	const StmOps *infOps;
	Stmrdbuf      infBuf;

	// Miscellaneous global flags and data
	Boolean8    hasPeerIndex;
	Uint8       peerIndexClearVal;
	Uint16      npeers;
	PeerRefsTab peerIndexRefs;
	Mrtrecord   peerIndex;
	Mrtrecord   rec;

	// Error tracking and management
	jmp_buf dropRecordFrame;  // used by `Peerindex_DropRecord()`
	jmp_buf dropFileFrame;    // used by `Peerindex_DropFile()`
	int     nerrors;          // for `exit()` status
} PeerindexState;

#endif
