// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_asmatch.c
 *
 * AS_PATH regular expressions compilation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "sys/con.h"
#include "sys/endian.h"
#include "lexer.h"
#include "strlib.h"

#include <stdlib.h>
#include <string.h>

#define MAXEXCERPTLEN 32

#define GROWSTEP 128

typedef struct {
	unsigned cap, len;
	Asn      expr[FLEX_ARRAY];
} Asnbuf;

// Return TRUE if `asn` may be followed by '+', '?' or '*' wildcards.
static Boolean WildcardAllowed(Asn asn)
{
	return asn != ASN_START && asn != ASN_END && asn != ASN_ALT && asn != ASN_NEWGRP;
}

static void AddAsnToList(Asnbuf **pbuf, Asn asn)
{
	Asnbuf *buf = *pbuf;

	if (!buf || buf->len == buf->cap) {
		size_t oldcap = buf ? buf->cap : 0;
		size_t newcap = oldcap + GROWSTEP;

		buf = (Asnbuf *) realloc(buf, offsetof(Asnbuf, expr[newcap]));
		if (!buf)
			Bgpgrep_Fatal("Memory allocation failure");

		buf->cap = newcap;
		buf->len = oldcap;

		*pbuf = buf;
	}

	buf->expr[buf->len++] = asn;
}

Sint32 BgpgrepC_BakeAsRegexp(void)
{
	// Generate expression name
	char expnam[1 + 16 + MAXEXCERPTLEN + 1 + 1], *ptr = expnam;

	const char *match = BgpgrepC_ExpectAnyToken();

	*ptr++ = '<';

	strcpy(ptr, "AS PATH regexp: "); ptr += 16;

	size_t n = Df_strncpyz(ptr, match, MAXEXCERPTLEN);
	if (n > MAXEXCERPTLEN) strcpy(ptr + MAXEXCERPTLEN-3, "...");

	ptr += n;

	*ptr++ = '>';
	*ptr   = '\0';

	// Setup lexer
	Lex p;
	Tok tok;

	memset(&p, 0, sizeof(p));
	if (!S.noColor)
		SetLexerFlags(&p, L_COLORED);

	BeginLexerSession(&p, expnam, /*line=*/1);
	SetLexerTextn(&p, match, n);
	SetLexerErrorFunc(&p, LEX_QUIT, LEX_WARN, NULL);

	// Compile expression
	Asnbuf *buf = NULL;

	int notcount = 0;
	int nparens = 0;
	Boolean seenAsn = FALSE, seenBol = FALSE, seenEol = FALSE;
	Boolean wUnmatchable = TRUE;
	Asn asn;

	while (Lex_ReadToken(&p, &tok)) {
		// Special handling for negation (don't produce any ASN)
		if (strcmp(tok.text, "!") == 0) {
			notcount++;
			continue;
		}

		// Any other case
		if (strcmp(tok.text, "(") == 0) {
			if (notcount > 0)
				LexerError(&p, "Illegal '(' after '!' ASN modifier");

			AddAsnToList(&buf, ASN_NEWGRP);
			seenBol = seenAsn = seenEol = FALSE;
			wUnmatchable = TRUE;
			nparens++;
		} else if (strcmp(tok.text, ")") == 0) {
			if (notcount > 0)
				LexerError(&p, "Dangling '!' at end of group");
			if (nparens == 0)
				LexerError(&p, "Stray ')' in match expression");

			AddAsnToList(&buf, ASN_ENDGRP);
			seenBol = seenAsn = seenEol = FALSE;
			wUnmatchable = TRUE;
			nparens--;
		} else if (strcmp(tok.text, "^") == 0) {
			if (notcount > 0)
				LexerError(&p, "Illegal '^' after '!' ASN modifier");
			if (seenBol)
				LexerWarning(&p, "Duplicate '^'");
			if (seenAsn && wUnmatchable) {
				LexerWarning(&p, "'^' following ASN term never matches");
				wUnmatchable = FALSE;
			}
			if (seenEol && wUnmatchable) {
				LexerWarning(&p, "'^' following '$' never matches");
				wUnmatchable = FALSE;
			}

			AddAsnToList(&buf, ASN_START);
			seenBol = TRUE;
		} else if (strcmp(tok.text, "$") == 0) {
			if (notcount > 0)
				LexerError(&p, "Illegal '$' after '!' ASN modifier");
			if (seenEol)
				LexerWarning(&p, "Duplicate '$'");

			AddAsnToList(&buf, ASN_END);
			seenEol = TRUE;
		} else if (strcmp(tok.text, "|") == 0) {
			if (notcount > 0)
				LexerError(&p, "Alternative not allowed after '!'");

			AddAsnToList(&buf, ASN_ALT);
			seenBol = seenAsn = seenEol = FALSE;
			wUnmatchable = TRUE;
		} else {
			if (strcmp(tok.text, ".") == 0) {
				// Any match, make sure ! wasn't used
				if (notcount > 0)
					LexerError(&p, "Illegal use of '!' combined with '.' operator");

				asn = ASN_ANY;

			} else {
				// Read actual numeric ASN
				Lex_UngetToken(&p, &tok);

				long long num = Lex_ParseInt(&p, /*optionalSign=*/FALSE);
				if (num > 0xffffffff)
					LexerError(&p, "%lld: ASN is out of range", num);

				asn = ASN32BIT(beswap32(num));
			}

			if (notcount & 1)
				asn = ASNNOT(asn);  // negate match

			if (seenEol && wUnmatchable) {
				LexerWarning(&p, "ASN term following '$' never matches");
				wUnmatchable = FALSE;
			}

			AddAsnToList(&buf, asn);
			notcount = 0;

			seenAsn = TRUE;
		}

		// Allow *, ? and + repetition operators
		if (WildcardAllowed(buf->expr[buf->len-1])) {
			if (Lex_CheckToken(&p, "*"))
				AddAsnToList(&buf, ASN_STAR);
			else if (Lex_CheckToken(&p, "+"))
				AddAsnToList(&buf, ASN_PLUS);
			else if (Lex_CheckToken(&p, "?"))
				AddAsnToList(&buf, ASN_QUEST);
		}
	}
	if (nparens != 0)
		LexerError(&p, "Missing ')' at end of match expression");
	if (notcount != 0)
		LexerError(&p, "Dangling '!' at end of match expression");

	void *regexp = Bgp_VmCompileAsMatch(&S.vm, buf->expr, buf->len);
	if (!regexp)
		Bgpgrep_Fatal("AS PATH Regexp compilation failed");

	Sint32 kidx = BGP_VMSETKA(&S.vm, Bgp_VmNewk(&S.vm), regexp);
	if (kidx == -1)
		Bgpgrep_Fatal("Maximum BGP VM variables limit hit: please try to simplify the filtering expression");

	free(buf);

	return kidx;
}
