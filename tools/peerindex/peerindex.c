// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file peerindex.c
 *
 * `peerindex` main entry point and general command line parsing.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "peerindex_local.h"

#include "bgp/bytebuf.h"
#include "cpr/flate.h"
#include "cpr/bzip2.h"
#include "cpr/xz.h"
#include "sys/endian.h"
#include "sys/fs.h"
#include "sys/con.h"
#include "sys/sys.h"
#include "argv.h"
#include "bufio.h"
#include "strlib.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define BYTEBUFSIZ (256 * 1024)

typedef enum {
	ONLY_REFS_FLAG,
	OUTPUT_FLAG,

	NUM_FLAGS
} PeerindexOpt;

static Optflag options[] = {
	[ONLY_REFS_FLAG] = {
		'r', "only-refs", NULL, "Only dump peers referenced by RIBs", ARG_NONE
	},
	[OUTPUT_FLAG] = {
		'o', NULL, "file", "Write output to file", ARG_REQ
	},

	[NUM_FLAGS] = { '\0' }
};

static PeerindexState S;
static BGP_FIXBYTEBUF(BYTEBUFSIZ) bgp_msgBuf = { BYTEBUFSIZ };

static void Peerindex_SetupCommandLine(char *argv0)
{
	Sys_StripPath(argv0, NULL);
	Sys_StripFileExtension(argv0, NULL);

	com_progName   = argv0;
	com_synopsis   = "[OPTIONS...] [FILES...]";
	com_shortDescr = "MRT TABLE_DUMPV2 Peer Index Table inspection tool";
	com_longDescr  =
		"Reads MRT dumps in various formats, and inspects the\n"
		"MRT TABLE_DUMPV2 peer index table, and prints it to stdout.\n"
		"If - is found inside FILES, then input is read from stdin.\n"
		"If no FILES arguments are provided, then input\n"
		"is implicitly expected from stdin.\n"
		"Any diagnostic message is logged to stderr.";
}

static void Peerindex_Fatal(const char *fmt, ...)
{
	va_list va;

	Sys_Print(STDERR, com_progName);
	Sys_Print(STDERR, ": ERROR: ");

	va_start(va, fmt);
	Sys_VPrintf(STDERR, fmt, va);
	va_end(va);

	Sys_Print(STDERR, "\n");
	exit(EXIT_FAILURE);
}

static void Peerindex_Warning(const char *fmt, ...)
{
	va_list va;

	Sys_Print(STDERR, com_progName);
	Sys_Print(STDERR, ": ");
	if (S.filename) {
		Sys_Print(STDERR, S.filename);
		Sys_Print(STDERR, ": ");
	}

	Sys_Print(STDERR, "WARNING: ");

	va_start(va, fmt);
	Sys_VPrintf(STDERR, fmt, va);
	va_end(va);

	Sys_Print(STDERR, "\n");
}

void Peerindex_DropRecord(const char *fmt, ...)
{
	va_list va;

	Sys_Print(STDERR, com_progName);
	Sys_Print(STDERR, ": ");

	assert(S.filename);
	Sys_Print(STDERR, S.filename);
	Sys_Print(STDERR, ": ");

	va_start(va, fmt);
	Sys_VPrintf(STDERR, fmt, va);
	va_end(va);

	Sys_Print(STDERR, "\n");
	S.nerrors++;

	// If we're dropping a record we have to kill S.rec
	Bgp_ClearMrt(&S.rec);

	// ...but don't drop PEER_INDEX_TABLE

	longjmp(S.dropRecordFrame, 1);
}

void Peerindex_DropFile(const char *fmt, ...)
{
	va_list va;

	Sys_Print(STDERR, com_progName);
	Sys_Print(STDERR, ": ");

	assert(S.filename);
	Sys_Print(STDERR, S.filename);
	Sys_Print(STDERR, ": ");

	va_start(va, fmt);
	Sys_VPrintf(STDERR, fmt, va);
	va_end(va);

	Sys_Print(STDERR, "\n");
	S.nerrors++;

	// Must clear PEER_INDEX_TABLE also, along with S.rec...
	Bgp_ClearMrt(&S.rec);
	Bgp_ClearMrt(&S.peerIndex);
	S.hasPeerIndex = FALSE;

	// Drop any opened file and jump out
	if (S.infOps) {
		if (S.infOps->Close) S.infOps->Close(S.inf);

		S.inf    = NULL;
		S.infOps = NULL;
	}
	S.filename = NULL;

	longjmp(S.dropFileFrame, 1);
}

NOINLINE static void Peerindex_HandleError(BgpRet err, Srcloc *loc, void *obj)
{
	USED(obj);

	// On out of memory we die with an appropriate backtrace
	if (err == BGPENOMEM) {
		loc->call_depth++;  // include this function itself

		_Sys_OutOfMemory(loc->filename, loc->func, loc->line, loc->call_depth);
	}
	// On I/O error we drop the entire file
	if (err == BGPEIO)
		Peerindex_DropFile("I/O error while reading MRT dump");  // TODO: better diagnostics

	// Hopefully we are not dealing with a programming error...
	assert(err != BGPEBADMRTTYPE);
	assert(BGP_ISMRTERR(err));

	if (err == BGPEBADPEERIDXCNT || err == BGPEBADRIBV2CNT) {
		// Only warrant a warning
		Peerindex_Warning("CORRUPT MRT RECORD: %s", Bgp_ErrorString(err));
		S.nerrors++;
		return;
	}

	// Anything else requires a full record drop
	Peerindex_DropRecord("SKIPPING MRT RECORD: %s", Bgp_ErrorString(err));
}

static void Peerindex_ApplyProgramOptions(void)
{
	if (options[ONLY_REFS_FLAG].flagged)
		S.peerIndexClearVal = 0;
	else
		S.peerIndexClearVal = 0xff;  // so we always print the full table

	if (options[OUTPUT_FLAG].flagged) {
		const char *filename = options[OUTPUT_FLAG].optarg;

		Fildes fd = Sys_Fopen(filename, FM_WRITE, /*hints=*/0);
		if (fd == FILDES_BAD)
			Peerindex_Fatal("Can't open output file \"%s\"", filename);

		S.outf    = STM_FILDES(fd);
		S.outfOps = Stm_FildesOps;
	} else {
		S.outf    = STM_CONHN(STDOUT);
		S.outfOps = Stm_ConOps;
	}
}

static void Peerindex_Init(void)
{
	S.rec.allocp = &bgp_msgBuf;
	S.rec.memOps = Mem_BgpBufOps;
}

static void Peerindex_OpenMrtDump(const char *filename)
{
	S.filename = filename;
	if (strcmp(S.filename, "-") == 0) {
		// Direct read from stdin, assume uncompressed
		Bufio_RdInit(&S.infBuf, STM_FILDES(CON_FILDES(STDIN)), Stm_NcFildesOps);

		S.inf    = &S.infBuf;
		S.infOps = Stm_NcRdBufOps;
		return;
	}

	Fildes fh = Sys_Fopen(filename, FM_READ, FH_SEQ);
	if (fh == FILDES_BAD)
		Peerindex_DropFile("Can't open file");

	const char *ext = Sys_GetFileExtension(filename);
	if (Df_stricmp(ext, ".bz2") == 0) {
		S.inf    = Bzip2_OpenDecompress(STM_FILDES(fh), Stm_FildesOps, /*opts=*/NULL);
		S.infOps = Bzip2_StmOps;

		Bzip2Ret err = Bzip2_GetErrStat();
		if (err)
			Peerindex_DropFile("Can't read Bz2 archive: %s", Bzip2_ErrorString(err));

	} else if (Df_stricmp(ext, ".gz") == 0 || Df_stricmp(ext, ".z") == 0) {
		S.inf    = Zlib_InflateOpen(STM_FILDES(fh), Stm_FildesOps, /*opts=*/NULL);
		S.infOps = Zlib_StmOps;

		ZlibRet err = Zlib_GetErrStat();
		if (err)
			Peerindex_DropFile("Can't read Zlib archive: %s", Zlib_ErrorString(err));

	} else if (Df_stricmp(ext, ".xz") == 0) {
		S.inf    = Xz_OpenDecompress(STM_FILDES(fh), Stm_FildesOps, /*opts=*/NULL);
		S.infOps = Xz_StmOps;

		XzRet err = Xz_GetErrStat();
		if (err)
			Peerindex_DropFile("Can't read LZMA archive: %s", Xz_ErrorString(err));

	} else {
		// Assume uncompressed file
		Bufio_RdInit(&S.infBuf, STM_FILDES(fh), Stm_FildesOps);

		S.inf    = &S.infBuf;
		S.infOps = Stm_RdBufOps;
	}
}

static void Peerindex_MarkPeerRefs(void)
{
	Mrtribiterv2 it;
	Mrtribentv2 *rib;

	Bgp_StartMrtRibEntriesv2(&it, &S.rec);
	while ((rib = Bgp_NextRibEntryv2(&it)) != NULL) {
		Uint16 idx = beswap16(rib->peerIndex);
		if (idx >= S.npeers) {
			Peerindex_Warning("CORRUPT MRT RECORD: Peer index '%u' is out of range", (unsigned) idx);
			S.nerrors++;
			continue;
		}

		MARKPEERINDEX(S.peerIndexRefs, idx);
	}
}

static void Peerindex_FlushPeerIndexTable(void)
{
	char buf[IPV6_STRLEN + 1];
	Stmwrbuf sb;

	Ipadr adr;

	Mrtpeeriterv2 it;
	Mrtpeerentv2 *peer;

	if (!S.hasPeerIndex)
		return;  // NOP

	Uint16 idx = 0;

	Bufio_WrInit(&sb, S.outf, S.outfOps);

	Bgp_StartMrtPeersv2(&it, &S.peerIndex);
	while ((peer = Bgp_NextMrtPeerv2(&it)) != NULL) {
		if (ISPEERINDEXREF(S.peerIndexRefs, idx)) {
			Asn asn    = MRT_GETPEERADDR(&adr, peer);
			char *eptr = Ip_AdrToString(&adr, buf);

			Bufio_Putsn(&sb, buf, eptr - buf);
			Bufio_Putc(&sb, ' ');
			Bufio_Putu(&sb, beswap32(ASN(asn)));
			Bufio_Putc(&sb, '|');
			Bufio_Putc(&sb, ISASN32BIT(asn) ? '1' : '0');
			Bufio_Putc(&sb, '\n');
		}

		idx++;
	}
	Bufio_Flush(&sb);
}

static void Peerindex_ProcessRecord(void)
{
	const Mrthdr *hdr = MRT_HDR(&S.rec);
	if (hdr->type != MRT_TABLE_DUMPV2)
		return;  // don't care for anything else

	if (hdr->subtype == TABLE_DUMPV2_PEER_INDEX_TABLE) {
		// Dump peers seen so far, if any
		Peerindex_FlushPeerIndexTable();
		// Clear previous PEER_INDEX_TABLE, if any
		Bgp_ClearMrt(&S.peerIndex);
		// Clear reference table...
		memset(S.peerIndexRefs, S.peerIndexClearVal, sizeof(S.peerIndexRefs));

		//...and update current PEER_INDEX_TABLE
		size_t npeers;

		MRT_MOVEREC(&S.peerIndex, &S.rec);
		Bgp_GetMrtPeerIndexPeers(&S.peerIndex, &npeers, /*nbytes=*/NULL);
		S.npeers       = npeers;
		S.hasPeerIndex = TRUE;
		return;
	}

	if (!TABLE_DUMPV2_ISRIB(hdr->subtype))
		return;  // irrelevant to us

	if (!S.hasPeerIndex)
		Peerindex_DropRecord("SKIPPING TABLE_DUMPV2 RECORD - No PEER_INDEX_TABLE found yet");

	Peerindex_MarkPeerRefs();
}

static void Peerindex_ProcessMrtDump(const char *filename)
{
	// NOTE: This call is responsible to set:
	// S.filename, S.inf, S.infOps.
	// These must be cleared either here or within a file drop.
	Peerindex_OpenMrtDump(filename);

	setjmp(S.dropRecordFrame);  // NOTE: The ONLY place where this is set
	while (Bgp_ReadMrt(&S.rec, S.inf, S.infOps) == OK) {
		Peerindex_ProcessRecord();

		Bgp_ClearMrt(&S.rec);
	}

	Peerindex_FlushPeerIndexTable();  // execute the very last dump

	// Don't need PEER_INDEX_TABLE anymore
	Bgp_ClearMrt(&S.peerIndex);
	S.hasPeerIndex = FALSE;

	// Finally close file and call it a day
	if (S.infOps->Close) S.infOps->Close(S.inf);

	S.inf      = NULL;
	S.infOps   = NULL;
	S.filename = NULL;
}

int main(int argc, char **argv)
{
#ifndef NDEBUG
	Sys_SetErrFunc(SYS_ERR_ABORT, NULL);
#else
	Sys_SetErrFunc(SYS_ERR_QUIT, NULL);
#endif
	Bgp_SetErrFunc(Peerindex_HandleError, NULL);

	Peerindex_SetupCommandLine(argv[0]);

	int optind = Com_ArgParse(argc, argv, options, /*flags=*/0);
	if (optind == OPT_HELP)
		return EXIT_SUCCESS;  // only called to get help...
	if (optind < 0)
		return EXIT_FAILURE;  // can't parse command line

	Peerindex_ApplyProgramOptions();

	Peerindex_Init();  // initialize according to command line

	// Done with options
	argc -= optind;
	argv += optind;

	// Take file list
	char **files  = argv;
	int    nfiles = argc;

	if (nfiles == 0) {
		// If no FILES are provided, read from stdin
		static const char *const stdinFile[] = { "-" };

		files  = (char **) stdinFile;
		nfiles = ARRAY_SIZE(stdinFile);
	}

	volatile int i = 0;

	setjmp(S.dropFileFrame);  // NOTE: The ONLY place where this is set
	while (i < nfiles)
		Peerindex_ProcessMrtDump(files[i++]);

	if (S.outfOps->Close) S.outfOps->Close(S.outf);

	return (S.nerrors > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
