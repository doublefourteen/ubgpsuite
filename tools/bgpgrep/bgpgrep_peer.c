// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_peer.c
 *
 * Peer matching expressions compilation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "sys/fs.h"
#include "sys/endian.h"
#include "sys/sys.h"
#include "lexer.h"
#include "numlib.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	Peerlistop  *head;
	Peerlistop **lastp;
} Peermatch;

// Diagnostics used inside ParsePeerMatch()
#define WARN(fmt, ...) ((void) ((lexp) ? \
	LexerWarning(lexp, fmt, __VA_ARGS__) : \
	Bgpgrep_Warning("%s: " fmt, BgpgrepC_CurTerm(), __VA_ARGS__)))
#define FATAL(fmt, ...) ((void) ((lexp) ? \
	LexerError(lexp, fmt, __VA_ARGS__) : \
	Bgpgrep_Fatal("%s: " fmt, BgpgrepC_CurTerm(), __VA_ARGS__)))

static void ParsePeerMatch(Peermatch *dest, const char *tok, Lex *lexp)
{
	Peeraddropc opc;
	Ipadr       addr;
	Asn         asn;
	char *ep, *addrp, *asnp;

	unsigned long long n;
	NumConvRet res;
	Boolean notFlag;

	asn = ASN_ANY;

	// Peer matches are in the form: "[[!]ip address] [[!]ASN]"
	//
	// Quotes may be omitted when matching only [ip address] or [ASN]
	//
	// At least one between [ip address] and [ASN] must be specified for
	// each expression

	ep = strchr(tok, ' ');
	if (ep) {
		// Both peer address and ASN specified
		n = ep - tok;

		addrp = (char *) alloca(n + 1);
		memcpy(addrp, tok, n);
		addrp[n] = '\0';

		opc = PEER_ADDR_EQ;
		if (*addrp == '!') {
			opc = PEER_ADDR_NE;
			addrp++;
		}
		if (Ip_StringToAdr(addrp, &addr) != OK)
			FATAL("Got '%s' while expecting IP address", addrp);

		asnp = ep + 1;
		while (*asnp == ' ') asnp++;

		notFlag = FALSE;
		if (*asnp == '!') {
			notFlag = TRUE;
			asnp++;
		}

		n = Atoull(asnp, &ep, 10, &res);
		if (res != NCVENOERR || *ep != '\0' || n > 0xffffffffuLL)
			FATAL("Bad ASN '%s'", asnp);

		asn = ASN32BIT(beswap32(n));
		if (notFlag)
			asn = ASNNOT(asn);

	} else {
		notFlag = FALSE;
		if (*tok == '!') {
			notFlag = TRUE;
			tok++;
		}
		if (Ip_StringToAdr(tok, &addr) == OK) {
			opc = notFlag ? PEER_ADDR_NE : PEER_ADDR_EQ;

		} else {
			// attempt ASN
			n = Atoull(tok, &ep, 10, &res);
			if (res != NCVENOERR || *ep != '\0' || n > 0xffffffffuLL)
				FATAL("Got '%s' while expecting IP or ASN", tok);

			opc = PEER_ADDR_NOP;
			memset(&addr, 0, sizeof(addr));

			asn = ASN32BIT(beswap32(n));
			if (notFlag)
				asn = ASNNOT(asn);
		}
	}

	if (ISASTRANS(asn))
		WARN("'%s': AS_TRANS in peer match expression", tok);

	Peerlistop *p = Bgp_VmPermAlloc(&S.vm, sizeof(*p));
	if (!p)
		Bgpgrep_Fatal("Too many peer matches");

	// Store match
	p->opc  = opc;
	p->addr = addr;
	p->asn  = asn;

	// Append to match list
	p->next = *dest->lastp;

	*dest->lastp = p;
	dest->lastp  = &p->next;
}

static Judgement ParsePeerMatchFile(Peermatch *dest, const char *filename)
{
	Lex p;
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
	unsigned flags = L_NOSTRCAT | L_ALLOWIPADDR | L_PLAINIPADDRONLY | L_COLORED;
	if (S.noColor)
		flags &= ~L_COLORED;

	memset(&p, 0, sizeof(p));
	BeginLexerSession(&p, filename, /*line=*/1);
	SetLexerTextn(&p, text, fsiz);
	SetLexerFlags(&p, flags);
	SetLexerErrorFunc(&p, LEX_QUIT, LEX_WARN, NULL);
	char buf[1 + MAXTOKLEN + 1];

	while (Lex_ReadToken(&p, &tok)) {
		strcpy(buf, tok.text);
		if (tok.type == TT_PUNCT && tok.subtype == P_LOGIC_NOT) {
			// Special case for straight "[[!]ip address]/[[!]ASN" with no quotes
			if (tok.spacesBeforeToken == 0)
				LexerError(&p, "%s: '!' following previous peer match expression requires space", BgpgrepC_CurTerm());

			Lex_MatchAnyToken(&p, &tok);
			strcat(buf, tok.text);
		}

		ParsePeerMatch(dest, buf, &p);
	}

	free(text);
	return OK;
}

Sint32 BgpgrepC_ParsePeerExpression(void)
{
	const char *tok = BgpgrepC_ExpectAnyToken();
	if (strcmp(tok, "()") == 0)
		return -1;  // so the user doesn't have to split cmd line like \( \)

	Peermatch matches;

	memset(&matches, 0, sizeof(matches));
	matches.lastp = &matches.head;

	if (strcmp(tok, "(") == 0) {
		// Explicit inline peer match list
		while (TRUE) {
			tok = BgpgrepC_ExpectAnyToken();
			if (strcmp(tok, ")") == 0)
				break;

			ParsePeerMatch(&matches, tok, NULL);
		}
	} else {
		// Anything else is interpreted as either:
		// - A file argument, containing a space-separated prefix list
		// - A single match, if the file is not found
		//   (shorthand for "( <peer match> )" )

		if (ParsePeerMatchFile(&matches, tok) != OK)
			ParsePeerMatch(&matches, tok, NULL);
	}

	if (!matches.head)
		return -1;  // empty list

	Sint32 kidx = BGP_VMSETKA(&S.vm, Bgp_VmNewk(&S.vm), matches.head);
	if (kidx < 0)
		Bgpgrep_Fatal("Too many peer match expressions");

	return kidx;
}
