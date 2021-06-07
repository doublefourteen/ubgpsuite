// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_communities.c
 *
 * COMMUNITY matching expressions parsing.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "sys/endian.h"
#include "sys/fs.h"
#include "sys/sys.h"
#include "lexer.h"
#include "numlib.h"
#include "strlib.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define FATALF(fmt, ...) ((void) ((lexp) ? \
	LexerError(lexp, fmt, __VA_ARGS__) : \
	Bgpgrep_Fatal("%s: " fmt, BgpgrepC_CurTerm(), __VA_ARGS__)))

typedef struct {
	size_t cap, len;
	Bgpmatchcomm c[FLEX_ARRAY];
} Matchcommlist;

#define MINLISTSIZ  256
#define LISTGROWSIZ 128

static Boolean ParseWellKnownCommunity(Bgpmatchcomm *dest, const char *s)
{
	if (Df_stricmp(s, "PLANNED_SHUT") == 0)
		dest->c.code = BGP_COMMUNITY_PLANNED_SHUT;
	else if (Df_stricmp(s, "ACCEPT_OWN") == 0)
		dest->c.code = BGP_COMMUNITY_ACCEPT_OWN;
	else if (Df_stricmp(s, "ROUTE_FILTER_TRANSLATED_V4") == 0)
		dest->c.code = BGP_COMMUNITY_ROUTE_FILTER_TRANSLATED_V4;
	else if (Df_stricmp(s, "ROUTE_FILTER_V4") == 0)
		dest->c.code = BGP_COMMUNITY_ROUTE_FILTER_V4;
	else if (Df_stricmp(s, "ROUTE_FILTER_TRANSLATED_V6") == 0)
		dest->c.code = BGP_COMMUNITY_ROUTE_FILTER_TRANSLATED_V6;
	else if (Df_stricmp(s, "ROUTE_FILTER_V6") == 0)
		dest->c.code = BGP_COMMUNITY_ROUTE_FILTER_V6;
	else if (Df_stricmp(s, "LLGR_STALE") == 0)
		dest->c.code = BGP_COMMUNITY_LLGR_STALE;
	else if (Df_stricmp(s, "NO_LLGR") == 0)
		dest->c.code = BGP_COMMUNITY_NO_LLGR;
	else if (Df_stricmp(s, "ACCEPT_OWN_NEXTHOP") == 0)
		dest->c.code = BGP_COMMUNITY_ACCEPT_OWN_NEXTHOP;
	else if (Df_stricmp(s, "STANDBY_PE") == 0)
		dest->c.code = BGP_COMMUNITY_STANDBY_PE;
	else if (Df_stricmp(s, "BLACKHOLE") == 0)
		dest->c.code = BGP_COMMUNITY_BLACKHOLE;
	else if (Df_stricmp(s, "NO_EXPORT") == 0)
		dest->c.code = BGP_COMMUNITY_NO_EXPORT;
	else if (Df_stricmp(s, "NO_ADVERTISE") == 0)
		dest->c.code = BGP_COMMUNITY_NO_ADVERTISE;
	else if (Df_stricmp(s, "NO_EXPORT_SUBCONFED") == 0)
		dest->c.code = BGP_COMMUNITY_NO_EXPORT_SUBCONFED;
	else if (Df_stricmp(s, "NO_PEER") == 0)
		dest->c.code = BGP_COMMUNITY_NO_PEER;

	else return FALSE;

	return TRUE;
}

static Bgpmatchcomm *AppendMatch(Matchcommlist **dest)
{
	Matchcommlist *list = *dest;
	if (list->len == list->cap) {
		list->cap += LISTGROWSIZ;

		list = (Matchcommlist *) realloc(list, offsetof(Matchcommlist, c[list->cap]));
		if (!list)
			Sys_OutOfMemory();

		*dest = list;
	}

	return &list->c[list->len++];
}

static void ParseCommunity(Matchcommlist **dest, const char *s, Lex *lexp)
{
	char *p = (char *) s;

	unsigned long long n;
	NumConvRet res;

	Bgpmatchcomm *m = AppendMatch(dest);
	memset(m, 0, sizeof(*m));
	if (ParseWellKnownCommunity(m, s))
		return;  // got a well known community

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		// Hexadecimal representation
		n = Atoull(p, &p, 16, &res);
		if (res != NCVENOERR || n > 0xffffffffuLL || *p != '\0')
			FATALF("%s: Hexadecimal COMMUNITY code is out of range", s);

		m->c.code = beswap32(n);
		return;
	}

	// HIVALUE:LOVALUE (either one may be *)
	if (*p == '*') {
		m->maskHi = TRUE;
		p++;
	} else {
		n = Atoull(p, &p, 10, &res);
		if (res != NCVENOERR || n > 0xffffuLL)
			FATALF("%s: Expected numeric value or '*' for COMMUNITY high order bytes", s);

		m->c.hi = beswap16(n);
	}
	if (*p != ':')
		FATALF("%s: Expecting ':' after COMMUNITY high order bytes", s);

	p++;

	if (*p == '*') {
		m->maskLo = TRUE;
		p++;
	} else {
		n = Atoull(p, &p, 10, &res);
		if (res != NCVENOERR || n > 0xffffuLL)
			FATALF("%s: Expected numeric value or '*' for COMMUNITY low order bytes", s);

		m->c.lo = beswap16(n);
	}
	if (*p != '\0')
		FATALF("%s: Bad COMMUNITY, should be an hexadecimal code or 'hivalue:lowvalue'", s);

	if (m->maskHi && m->maskLo)
		FATALF("%s: At least one between high order or low order bytes must be defined", s);
}

static Judgement ParseCommunityFile(Matchcommlist **dest, const char *filename)
{
	Lex lex;
	Tok tok;

	// Map file in memory
	Fildes fd = Sys_Fopen(filename, FM_READ, FH_SEQ);
	if (fd == FILDES_BAD)
		return NG;

	Sint64 fsiz = Sys_FileSize(fd);  // NOTE: aborts on failure
	assert(fsiz >= 0);

	char *text = (char *) malloc(fsiz);
	if (!text)
		Sys_OutOfMemory();

	Sint64 n = Sys_Fread(fd, text, fsiz);
	if (n != fsiz)
		Bgpgrep_Fatal("Short read from file '%s'", filename);

	Sys_Fclose(fd);

	// Setup Lex and start reading strings
	memset(&lex, 0, sizeof(lex));
	BeginLexerSession(&lex, filename, /*line=*/1);
	SetLexerTextn(&lex, text, fsiz);
	if (!S.noColor)
		SetLexerFlags(&lex, L_COLORED);

	SetLexerErrorFunc(&lex, LEX_QUIT, LEX_WARN, NULL);

	char buf[MAXTOKLEN + 1 + MAXTOKLEN + 1];
	while (Lex_ReadToken(&lex, &tok)) {
		strcpy(buf, tok.text);
		Lex_MatchTokenType(&lex, &tok, TT_PUNCT, P_COLON);
		strcat(buf, tok.text);
		Lex_MatchAnyToken(&lex, &tok);
		strcat(buf, tok.text);

		ParseCommunity(dest, buf, &lex);
	}

	free(text);
	return OK;
}

Sint32 BgpgrepC_ParseCommunity(BgpVmOpt opt)
{
	const char *tok = BgpgrepC_ExpectAnyToken();
	if (strcmp(tok, "()") == 0)
		return -1;  // empty list always fails

	Matchcommlist *list = (Matchcommlist *) malloc(offsetof(Matchcommlist,
	                                                        c[MINLISTSIZ]));
	if (!list)
		Sys_OutOfMemory();

	list->cap = MINLISTSIZ;
	list->len = 0;

	if (strcmp(tok, "(") == 0) {
		// Explicit inline prefix list, directly inside command line
		while (TRUE) {
			tok = BgpgrepC_ExpectAnyToken();
			if (strcmp(tok, ")") == 0)
				break;

			ParseCommunity(&list, tok, /*lexp=*/NULL);
		}
	} else {
		// Anything else is interpreted as either:
		// - A file argument, containing a space-separated COMMUNITY match list
		// - A single match, if the file is not found
		//   (shorthand for "( <community> )" )

		if (ParseCommunityFile(&list, tok) != OK)
			ParseCommunity(&list, tok, /*lexp=*/NULL);
	}

	void *match = Bgp_VmCompileCommunityMatch(&S.vm, list->c, list->len, opt);
	if (!match)
		Bgpgrep_Fatal("%s: %s", BgpgrepC_CurTerm(), Bgp_ErrorString(S.vm.errCode));

	free(list);

	Sint32 kidx = BGP_VMSETKA(&S.vm, Bgp_VmNewk(&S.vm), match);
	if (kidx == -1)
		Bgpgrep_Fatal("Too many community match expressions");

	return kidx;
}
