// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_prefixlist.c
 *
 * Prefix matching expressions compilation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "sys/fs.h"
#include "sys/sys.h"
#include "lexer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static Pfxnode *AddPrefixToList(Pfxlist      *dest,
                                const Prefix *pfx,
                                Boolean       isAnnounce,
                                Boolean       isWithdrawn)
{
	Pfxnode *n = (Pfxnode *) calloc(1, sizeof(*n));
	if (!n)
		Sys_OutOfMemory();

	memcpy(&n->pfx, pfx, sizeof(n->pfx));
	if (isAnnounce) {
		n->next[ANNOUNCE]    = dest->head[ANNOUNCE];
		dest->head[ANNOUNCE] = n;

		n->refCount++;
	}
	if (isWithdrawn) {
		n->next[WITHDRAWN]    = dest->head[WITHDRAWN];
		dest->head[WITHDRAWN] = n;

		n->refCount++;
	}

	dest->areListsMatching &= (isAnnounce && isWithdrawn);
	dest->isEmpty           = FALSE;
	return n;
}

static Pfxnode *AddPrefixStringToList(Pfxlist *dest, const char *s)
{
	Prefix pfx;

	Boolean isAnnounce  = TRUE;
	Boolean isWithdrawn = TRUE;
	if (*s == '+') {
		// announce only
		s++;
		isWithdrawn = FALSE;
	} else if (*s == '-') {
		// withdrawn only
		s++;
		isAnnounce = FALSE;
	}
	if (Bgp_StringToPrefix(s, &pfx) != OK)
		Bgpgrep_Fatal("%s: Bad prefix '%s'", BgpgrepC_CurTerm(), s);

	return AddPrefixToList(dest, &pfx, isAnnounce, isWithdrawn);
}

static Judgement ParsePrefixListFile(Pfxlist *dest, const char *filename)
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
		Bgpgrep_Fatal("%s: Short read from file '%s'", BgpgrepC_CurTerm(), filename);

	Sys_Fclose(fd);

	// Setup Lex and start reading tokens in form: [+-]network[/len]
	unsigned flags = L_ALLOWIPADDR | L_PLAINIPADDRONLY | L_COLORED;
	if (S.noColor)
		flags &= ~L_COLORED;

	memset(&lex, 0, sizeof(lex));
	BeginLexerSession(&lex, filename, /*line=*/1);
	SetLexerTextn(&lex, text, fsiz);
	SetLexerFlags(&lex, flags);
	SetLexerErrorFunc(&lex, LEX_QUIT, LEX_WARN, NULL);

	char buf[MAXTOKLEN + 1 + MAXTOKLEN + 1];  // wild upperbound

	while (Lex_ReadToken(&lex, &tok)) {
		Boolean isAnnounce  = TRUE;
		Boolean isWithdrawn = TRUE;
		Prefix  pfx;

		if (tok.type == TT_PUNCT) {
			// Match limited to ANNOUNCE/WITHDRAWN
			switch (tok.subtype) {
			case P_ADD: isWithdrawn = FALSE; break;
			case P_SUB: isAnnounce  = FALSE; break;
			default:
				LexerError(&lex, "Read '%s' while expecting '+' or '-'", tok.text);
				break;
			}

			Lex_MatchAnyToken(&lex, &tok);
		}

		if (tok.type != TT_NUMBER || (tok.subtype & TT_IPADDR) == 0)
			LexerError(&lex, "Read '%s' while expecting internet address value", tok.text);

		strcpy(buf, tok.text);
		if (Lex_CheckTokenType(&lex, &tok, TT_PUNCT, P_DIV)) {
			strcat(buf, tok.text);

			Lex_MatchTokenType(&lex, &tok, TT_NUMBER, TT_INT);
			strcat(buf, tok.text);
		}

		if (Bgp_StringToPrefix(buf, &pfx) != OK)
			LexerError(&lex, "Bad network prefix '%s'", buf);

		AddPrefixToList(dest, &pfx, isAnnounce, isWithdrawn);
	}

	free(text);
	return OK;
}

void BgpgrepC_ParsePrefixList(Pfxlist *dest)
{
	memset(dest, 0, sizeof(*dest));
	dest->areListsMatching = TRUE;
	dest->isEmpty          = TRUE;

	const char *tok = BgpgrepC_ExpectAnyToken();
	if (strcmp(tok, "()") == 0)
		return;  // so the user doesn't have to split cmd line like \( \)

	if (strcmp(tok, "(") == 0) {
		// Explicit inline prefix list, directly inside command line
		while (TRUE) {
			tok = BgpgrepC_ExpectAnyToken();
			if (strcmp(tok, ")") == 0)
				break;

			AddPrefixStringToList(dest, tok);
		}
	} else {
		// Anything else is interpreted as either:
		// - A file argument, containing a space-separated prefix list
		// - A single prefix, if the file is not found
		//   (shorthand for "( <pfx> )" )

		if (ParsePrefixListFile(dest, tok) == OK)
			return;

		AddPrefixStringToList(dest, tok);
	}
}

void BgpgrepC_FreePrefixList(Pfxlist *list)
{
	for (int i = 0; i < NUM_NETPFX_TYPES; i++) {
		Pfxnode *n = list->head[i];
		while (n) {
			Pfxnode *t = n;
			n          = n->next[i];

			if (--t->refCount == 0)
				free(t);
		}
	}
}
