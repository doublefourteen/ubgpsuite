// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep.c
 *
 * `bgpgrep` main entry point and general command line parsing.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "bgp/bgp.h"
#include "bgp/bytebuf.h"
#include "bgp/patricia.h"
#include "cpr/flate.h"
#include "cpr/bzip2.h"
#include "cpr/xz.h"
#include "sys/fs.h"
#include "sys/con.h"
#include "sys/sys.h"
#include "sys/vt100.h"
#include "argv.h"
#include "strlib.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define BYTEBUFSIZ (256 * 1024)

static BGP_FIXBYTEBUF(BYTEBUFSIZ) bgp_msgBuf = { BYTEBUFSIZ };

BgpgrepState S;

typedef enum {
	NO_COLOR_FLAG,
	DUMP_BYTECODE_FLAG,

	NUM_FLAGS
} BgpgrepOpt;

static Optflag options[] = {
	[NO_COLOR_FLAG] = {
		'-', "no-color", NULL, "Disable color output", ARG_NONE
	},
	[DUMP_BYTECODE_FLAG] = {
		'-', "dump-bytecode", NULL, "Dump BGP VM bytecode to stderr (debug)", ARG_NONE
	},

	[NUM_FLAGS] = { '\0' }
};

static void Bgpgrep_SetupCommandLine(char *argv0)
{
	Sys_StripPath(argv0, NULL);
	Sys_StripFileExtension(argv0, NULL);

	com_progName   = argv0;
	com_synopsis   = "[OPTIONS...] [FILES...]";
	com_shortDescr = "Filter and print BGP data within MRT dumps";
	com_longDescr  =
		"Reads MRT dumps in various formats, applying filtering rules\n"
		"to each BGP message therein, then outputs every passing message to stdout.\n"
		"If - is found inside FILES, then input is read from stdin.\n"
		"If no FILES arguments are provided, then input\n"
		"is implicitly expected from stdin.\n"
		"Any diagnostic message is logged to stderr.";
}

static void Bgpgrep_ApplyProgramOptions(void)
{
	S.noColor      = options[NO_COLOR_FLAG].flagged;
	S.dumpBytecode = options[DUMP_BYTECODE_FLAG].flagged;
}

static int CountFileArguments(int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++) {
		const char *arg = argv[i];
		if (arg[0] == '-' && arg[1] != '\0')
			break;
		if (strcmp(arg, "(") == 0 || strcmp(arg, "!") == 0)
			break;
	}

	return i;
}

void Bgpgrep_Fatal(const char *fmt, ...)
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

void Bgpgrep_Warning(const char *fmt, ...)
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

void Bgpgrep_DropMessage(const char *fmt, ...)
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

	Bgp_ClearMsg(&S.msg);
	S.nerrors++;

	longjmp_fast(S.dropMsgFrame);
}

void Bgpgrep_DropRecord(const char *fmt, ...)
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

	S.lenientBgpErrors = FALSE;  // reset for future files

	// If we're dropping a record we have to kill S.rec and S.msg
	Bgp_ClearMsg(&S.msg);
	Bgp_ClearMrt(&S.rec);

	// ...but don't drop PEER_INDEX_TABLE

	longjmp_fast(S.dropRecordFrame);
}

void Bgpgrep_DropFile(const char *fmt, ...)
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

	S.lenientBgpErrors = FALSE;  // reset for future files

	// Must clear PEER_INDEX_TABLE also, along with S.rec and S.msg...
	Bgp_ClearMsg(&S.msg);
	Bgp_ClearMrt(&S.rec);
	Bgp_ClearMrt(&S.peerIndex);
	S.hasPeerIndex = FALSE;

	// Finally close current file and jump
	if (S.infOps) {
		if (S.infOps->Close) S.infOps->Close(S.inf);

		S.inf    = NULL;
		S.infOps = NULL;
	}

	S.filename = NULL;
	longjmp_fast(S.dropFileFrame);
}

// Common error management for BGP layer
NOINLINE static void Bgpgrep_HandleBgpError(BgpRet err, Srcloc *loc, void *obj)
{
	USED(obj);

	if (err == BGPEVMMSGERR)
		err = Bgp_GetErrStat(NULL);  // retrieve the actual BGP error

	// Hopefully we are not dealing with a programming error...
	assert(err != BGPEBADMRTTYPE);
	assert(err != BGPENOADDPATH);
	assert(err != BGPEBADTYPE && err != BGPEBADATTRTYPE);

	// On out of memory we die with an appropriate backtrace
	if (err == BGPENOMEM) {
		loc->call_depth++;  // include this function itself

		_Sys_OutOfMemory(loc->filename, loc->func, loc->line, loc->call_depth);
	}

	// BGP filter errors should never occur, but are theoretically
	// possible when filters are excessively complex,
	// we don't proceed any further since they may cause incomplete or
	// misleading output data
	if (BGP_ISVMERR(err))
		Bgpgrep_Fatal("Unexpected BGP filter error: %s", Bgp_ErrorString(err));

	// On I/O error we drop the entire file
	if (err == BGPEIO)
		Bgpgrep_DropFile("I/O error while reading MRT dump");  // TODO: better diagnostics

	// Deal with critical MRT errors that cause a record drop
	if (err == BGPETRUNCMRT || err == BGPETRUNCPEERV2 || err == BGPETRUNCRIBV2)
		Bgpgrep_DropRecord("SKIPPING MRT RECORD: %s", Bgp_ErrorString(err));

	if (BGP_ISMSGERR(err)) {
		if (S.lenientBgpErrors) {
			// Get away with just a warning
			Bgpgrep_Warning("CORRUPT BGP MESSAGE: %s", Bgp_ErrorString(err));
			S.nerrors++;
			return;
		}

		// Drop the affected entry
		Bgpgrep_DropMessage("SKIPPING BGP MESSAGE: %s", Bgp_ErrorString(err));
	}

	assert(BGP_ISMRTERR(err));

	if (err == BGPEBADPEERIDXCNT || err == BGPEBADRIBV2CNT) {
		// Only warrant a warning
		Bgpgrep_Warning("CORRUPT MRT RECORD: %s", Bgp_ErrorString(err));
		S.nerrors++;
		return;
	}

	// Anything else causes a BGP drop
	Bgpgrep_DropMessage("SKIPPING BGP MESSAGE: %s", Bgp_ErrorString(err));
}

static void Bgpgrep_Init(void)
{
	if (!S.noColor && Sys_IsVt100Console(STDOUT))
		S.outFmt = Bgp_IsolarioFmtWc;  // console supports colors
	else
		S.outFmt = Bgp_IsolarioFmt;

	Mrtrecord *rec = &S.rec;
	Bgpmsg    *msg = &S.msg;

	rec->allocp = msg->allocp = &bgp_msgBuf;
	rec->memOps = msg->memOps =  Mem_BgpBufOps;

	Bgp_InitVm(&S.vm, /*heapSize=*/0);
}

static void Bgpgrep_OpenMrtDump(const char *filename)
{
	S.filename = filename;
	if (strcmp(S.filename, "-") == 0) {
		// Direct read from stdin - assume uncompressed.
		S.inf    = STM_FILDES(CON_FILDES(STDIN));
		S.infOps = Stm_NcFildesOps;
		return;
	}

	Fildes fh = Sys_Fopen(S.filename, FM_READ, FH_SEQ);
	if (fh == FILDES_BAD)
		Bgpgrep_DropFile("Can't open file");

	const char *ext = Sys_GetFileExtension(S.filename);
	if (Df_stricmp(ext, ".bz2") == 0) {
		S.inf    = Bzip2_OpenDecompress(STM_FILDES(fh), Stm_FildesOps, /*opts=*/NULL);
		S.infOps = Bzip2_StmOps;

		Bzip2Ret err = Bzip2_GetErrStat();
		if (err)
			Bgpgrep_DropFile("Can't read Bz2 archive: %s", Bzip2_ErrorString(err));

	} else if (Df_stricmp(ext, ".gz") == 0 || Df_stricmp(ext, ".z") == 0) {
		S.inf    = Zlib_InflateOpen(STM_FILDES(fh), Stm_FildesOps, /*opts=*/NULL);
		S.infOps = Zlib_StmOps;

		ZlibRet err = Zlib_GetErrStat();
		if (err)
			Bgpgrep_DropFile("Can't read Zlib archive: %s", Zlib_ErrorString(err));

	} else if (Df_stricmp(ext, ".xz") == 0) {
		S.inf    = Xz_OpenDecompress(STM_FILDES(fh), Stm_FildesOps, /*opts=*/NULL);
		S.infOps = Xz_StmOps;

		XzRet err = Xz_GetErrStat();
		if (err)
			Bgpgrep_DropFile("Can't read LZMA archive: %s", Xz_ErrorString(err));

	} else {
		// Assume uncompressed file
		S.inf    = STM_FILDES(fh);
		S.infOps = Stm_FildesOps;
	}
}

static void Bgpgrep_ProcessMrtDump(const char *filename)
{
	// NOTE: This call is responsible to set:
	// S.filename, S.inf, S.infOps.
	// These must be cleared either here or within a file drop.
	Bgpgrep_OpenMrtDump(filename);

	setjmp_fast(S.dropRecordFrame);  // NOTE: The ONLY place where this is set
	setjmp_fast(S.dropMsgFrame);     // NOTE: May be set again by specific BgpgrepD_*()
	while (Bgp_ReadMrt(&S.rec, S.inf, S.infOps) == OK) {
		const Mrthdr *hdr = MRT_HDR(&S.rec);

		switch (hdr->type) {
		case MRT_TABLE_DUMPV2: BgpgrepD_TableDumpv2(); break;
		case MRT_TABLE_DUMP:   BgpgrepD_TableDump();   break;
		case MRT_BGP:          BgpgrepD_Zebra();       break;
		default:
			if (MRT_ISBGP4MP(hdr->type))
				BgpgrepD_Bgp4mp();

			break;
		}

		Bgp_ClearMrt(&S.rec);
	}

	// Don't need PEER_INDEX_TABLE anymore
	Bgp_ClearMrt(&S.peerIndex);
	S.hasPeerIndex = FALSE;

	// Close input
	if (S.infOps->Close) S.infOps->Close(S.inf);

	S.inf      = NULL;
	S.infOps   = NULL;
	S.filename = NULL;
}

static int Bgpgrep_CleanupAndExit(void)
{
	Bgp_ClearVm(&S.vm);

	Trielist *t = S.trieList;
	while (t) {
		Trielist *tn = t->next;

		Pat_Clear(&t->trie);
		free(t);

		t = tn;
	}
	return (S.nerrors > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
#ifndef NDEBUG
	Sys_SetErrFunc(SYS_ERR_ABORT, NULL);
#else
	Sys_SetErrFunc(SYS_ERR_QUIT, NULL);
#endif
	Bgp_SetErrFunc(Bgpgrep_HandleBgpError, NULL);

	// Initial program setup
	Bgpgrep_SetupCommandLine(argv[0]);

	int optind = Com_ArgParse(argc, argv, options, /*flags=*/ARG_NOREORD);
	if (optind == OPT_HELP)
		return EXIT_SUCCESS;  // only called to get help...
	if (optind < 0)
		return EXIT_FAILURE;  // can't parse command line

	Bgpgrep_ApplyProgramOptions();

	Bgpgrep_Init();  // initialize according to command line

	// Done with options
	argc -= optind;
	argv += optind;

	// Extrapolate files arguments from argv
	char **files  = argv;
	int    nfiles = CountFileArguments(argc, argv);

	// Move to filtering rules and compile them
	argv += nfiles;
	argc -= nfiles;
	Bgpgrep_CompileVmProgram(argc, argv);

	if (nfiles == 0) {
		// If no FILES are provided, read from stdin
		static const char *const stdinFile[] = { "-" };

		files  = (char **) stdinFile;
		nfiles = ARRAY_SIZE(stdinFile);
	}

	// Files processing
	volatile int i = 0;

	setjmp_fast(S.dropFileFrame);  // NOTE: The ONLY place where this is set
	while (i < nfiles)
		Bgpgrep_ProcessMrtDump(files[i++]);

	return Bgpgrep_CleanupAndExit();
}
