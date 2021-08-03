// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_local.h
 *
 * `bgpgrep` private header.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGPGREP_LOCAL_H_
#define DF_BGPGREP_LOCAL_H_

#include "bgp/dump.h"
#include "bgp/mrt.h"
#include "bgp/patricia.h"
#include "bgp/vm.h"

#include <setjmp.h>

// =======================================
// Data structures used during compilation

typedef struct Trielist Trielist;
struct Trielist {
	Sint32    kidx;
	Patricia  trie;
	Trielist *next;
};

typedef enum {
    ANNOUNCE, WITHDRAWN, NUM_NETPFX_TYPES
} NetpfxType;

typedef struct Pfxnode Pfxnode;
struct Pfxnode {
	Prefix   pfx;
	Uint8    refCount;
	Pfxnode *next[NUM_NETPFX_TYPES];
};

typedef struct Pfxlist Pfxlist;
struct Pfxlist {
	Boolean8 areListsMatching;  // TRUE if head[ANNOUNCE] and head[WITHDRAWN] lists are equal
	Boolean8 isEmpty;           // TRUE if both lists are NULL
	Pfxnode *head[NUM_NETPFX_TYPES];
};

typedef struct Triepair Triepair;
struct Triepair {
	Trielist *v4, *v6;
};

typedef enum {
	PEER_ADDR_NOP,  // operate only on AS
	PEER_ADDR_EQ,   // test for equality
	PEER_ADDR_NE    // test for inequality
} Peeraddropc;

typedef struct Peerlistop Peerlistop;
struct Peerlistop {
	Peeraddropc opc;
	Asn         asn;
	Ipadr       addr;
	Peerlistop *next;
};

typedef enum {
	TIMESTAMP_LE,
	TIMESTAMP_LT,
	TIMESTAMP_NE,
	TIMESTAMP_EQ,
	TIMESTAMP_GT,
	TIMESTAMP_GE
} Timestampopc;

typedef struct Timestampop Timestampop;
struct Timestampop {
	Sint64       secs;
	Uint32       microsecs;
	Timestampopc opc;
};

// ==================================
// Non Local Jumps - error management

#if defined(__GNUC__) && !defined(__clang__)
// Faster alternative, less taxing than standard setjmp(), but comes with
// a few gotchas: https://gcc.gnu.org/onlinedocs/gcc/Nonlocal-Gotos.html
typedef Sintptr frame_buf[5];

#define setjmp_fast(frame)  __builtin_setjmp(frame)
#define longjmp_fast(frame) __builtin_longjmp(frame, 1)
#else
typedef jmp_buf frame_buf;

#define setjmp_fast(frame)  setjmp(frame)
#define longjmp_fast(frame) longjmp(frame, 1)
#endif

// ====================
// Global bgpgrep state

typedef struct BgpgrepState BgpgrepState;
struct BgpgrepState {
	// Output
	const BgpDumpfmt *outFmt;
	void             *outf;
	const StmOps     *outfOps;

	// MRT input file stream
	const char   *filename;  // current file being processed
	void         *inf;       // NOTE: may be NULL even in a file is open
	const StmOps *infOps;    // if NULL no file is open

	// Miscellaneous global flags and data
	Boolean8  noColor;
	Boolean8  hasPeerIndex;
	Boolean8  dumpBytecode;
	Boolean8  isTrivialFilter;   // TRUE when `vm` program = "return TRUE"
	Boolean8  lenientBgpErrors;  // If TRUE bgpgrep won't drop a message on BGP error
	                             // (allows dumping BGP data with corrupted markers)
	Mrtrecord peerIndex;
	Mrtrecord rec;
	Bgpmsg    msg;
	Bgpvm     vm;
	Trielist *trieList;

	// MRT extensions for Bgpvm (used by bgpgrep VM intrinsice: BgpgrepF_*)
	Ipadr  peerAddr;
	Asn    peerAs;

	Sint64 timestampSecs;
	Uint32 timestampMicrosecs;

	// Error tracking and management
	frame_buf dropMsgFrame;     // used to skip single RIB entry or BGP message
	frame_buf dropRecordFrame;  // used by `Bgpgrep_DropRecord()`
	frame_buf dropFileFrame;    // used by `Bgpgrep_DropFile()`
	int       nerrors;          // for `exit()` status
};

extern BgpgrepState S;

// ================
// Error management

CHECK_PRINTF(1, 2) void Bgpgrep_Warning(const char *, ...);
CHECK_PRINTF(1, 2) NORETURN void Bgpgrep_Fatal(const char *, ...);
CHECK_PRINTF(1, 2) NORETURN void Bgpgrep_DropMessage(const char *, ...);
CHECK_PRINTF(1, 2) NORETURN void Bgpgrep_DropRecord(const char *, ...);
CHECK_PRINTF(1, 2) NORETURN void Bgpgrep_DropFile(const char *, ...);

// ===============
// Rules compiler

// -> Entry point
void Bgpgrep_CompileVmProgram(int, char **);

// -> Called by `Bgpgrep_CompileVmProgram()` during compilation stage
char *BgpgrepC_GetToken(void);
char *BgpgrepC_CurTerm(void);
char *BgpgrepC_ExpectAnyToken(void);
char *BgpgrepC_ExpectToken(const char *);

Trielist *BgpgrepC_NewTrie(Afi);
Sint32 BgpgrepC_BakeAsRegexp(void);
void BgpgrepC_ParsePrefixList(Pfxlist *);
void BgpgrepC_FreePrefixList(Pfxlist *);

Sint32 BgpgrepC_ParsePeerExpression(void);
Sint32 BgpgrepC_ParseTimestampExpression(void);
Sint32 BgpgrepC_ParseCommunity(BgpVmOpt);

// -> Specific BGP VM intrinsics for bgpgrep operations (CALL targets)
void BgpgrepF_FindAsLoops(Bgpvm *);
void BgpgrepF_PeerAddrMatch(Bgpvm *);
void BgpgrepF_TimestampCompare(Bgpvm *);

// ==================
// MRT Dump functions

void BgpgrepD_Bgp4mp(void);
void BgpgrepD_Zebra(void);
void BgpgrepD_TableDumpv2(void);
void BgpgrepD_TableDump(void);

#endif
