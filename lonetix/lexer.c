// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file lexer.c
 *
 * Implements C-compatible text lexer.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "lexer.h"

#include "sys/con.h"
#include "sys/sys.h"
#include "sys/vt100.h"
#include "utf/utf.h"
#include "argv.h"
#include "numlib.h"
#include "strlib.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Readability macro, during parsing `subtype` represents the token length,
 * it is then updated to reflect the actual token subtype after tokenization
 * is complete.
 */
#define TOKLEN(tok) ((tok)->subtype)

// Default punctuation list
static const Punctuation p_punctuations[] = {
	// Binary operators
	{ ">>=", P_RSHIFT_ASSIGN },
	{ "<<=", P_LSHIFT_ASSIGN },
	// Elipsis
	{ "...", P_PARMS },
	// Define merge operator
	{ "##", P_PRECOMPMERGE },  // pre-compiler
	// Logic operators
	{ "&&", P_LOGIC_AND },     // pre-compiler
	{ "||", P_LOGIC_OR },      // pre-compiler
	{ ">=", P_LOGIC_GEQ },     // pre-compiler
	{ "<=", P_LOGIC_LEQ },     // pre-compiler
	{ "==", P_LOGIC_EQ },      // pre-compiler
	{ "!=", P_LOGIC_UNEQ },    // pre-compiler
	// Arithmatic operators
	{ "*=", P_MUL_ASSIGN },
	{ "/=", P_DIV_ASSIGN },
	{ "%=", P_MOD_ASSIGN },
	{ "+=", P_ADD_ASSIGN },
	{ "-=", P_SUB_ASSIGN },
	{ "++", P_INC },
	{ "--", P_DEC },
	// Binary operators
	{ "&=", P_BIN_AND_ASSIGN },
	{ "|=", P_BIN_OR_ASSIGN },
	{ "^=", P_BIN_XOR_ASSIGN },
	{ ">>", P_RSHIFT },  // pre-compiler
	{ "<<", P_LSHIFT },  // pre-compiler
	// Reference operators
	{ "->", P_POINTERREF },
	// Arithmatic operators
	{ "*", P_MUL },  // pre-compiler
	{ "/", P_DIV },  // pre-compiler
	{ "%", P_MOD },  // pre-compiler
	{ "+", P_ADD },  // pre-compiler
	{ "-", P_SUB },  // pre-compiler
	{ "=", P_ASSIGN },
	// Binary operators
	{ "&", P_BIN_AND },  // pre-compiler
	{ "|", P_BIN_OR },   // pre-compiler
	{ "^", P_BIN_XOR },  // pre-compiler
	{ "~", P_BIN_NOT },  // pre-compiler
	// Logic operators
	{ "!", P_LOGIC_NOT },      // pre-compiler
	{ ">", P_LOGIC_GREATER },  // pre-compiler
	{ "<", P_LOGIC_LESS },     // pre-compiler
	// Reference operator
	{ ".", P_REF },
	// Seperators
	{ ",", P_COMMA },      // pre-compiler
	{ ";", P_SEMICOLON },
	// Label indication
	{ ":", P_COLON },  // pre-compiler
	// Ternary operator
	{ "?", P_QUESTIONMARK },  // pre-compiler
	// Embracements
	{ "(", P_PARENOPEN },       // pre-compiler
	{ ")", P_PARENCLOSE },      // pre-compiler
	{ "{", P_BRACEOPEN },       // pre-compiler
	{ "}", P_BRACECLOSE },      // pre-compiler
	{ "[", P_SQBRACKETOPEN },
	{ "]", P_SQBRACKETCLOSE },
	// Escaping
	{ "\\",P_BACKSLASH },
	// Precompiler operator
	{ "#", P_PRECOMP },  // pre-compiler
	{ "$", P_DOLLAR },
	// End of list
	{ NULL, 0 }
};

static NORETURN void Lex_Quit(Lex *p, const char *msg, void *obj)
{
	USED(p);
	USED(obj);

	Sys_Print(STDERR, msg);
	Sys_Print(STDERR, "\n");
	if (com_progName) {
		Sys_Print(STDERR, com_progName);
		Sys_Print(STDERR, ": ");
	}
	Sys_Print(STDERR, "Terminating on unrecoverable parsing error.\n");

	exit(EXIT_FAILURE);
}

static void Lex_Warn(Lex *p, const char *msg, void *obj)
{
	USED(p);
	USED(obj);

	Sys_Print(STDERR, msg);
}

// Advance `p` to next rune, handles parsing errors, end of parsing and
// makes no assumption over what's next in `p->pos`
static Rune NextRune(Lex *p)
{
	if (p->hasError)
		return (p->nr = '\0');  // refuse any subsequent read requests

	size_t n = p->lim - p->pos;
	if (n == 0)
		return (p->nr = '\0');

	if (*(Uint8 *) p->pos < RUNE_SELF) {
		// fast path for ASCII
		char c = *p->pos;
		if (c != '\0') p->pos++;

		return (p->nr = c);
	}

	// Slow path for UTF8
	if (!fullrune(p->pos, n)) {
		LexerError(p, "Incomplete UTF-8 code sequence");
		return (p->nr = '\0');
	}

	p->pos += chartorune(&p->nr, p->pos);
	return p->nr;
}

// Assuming that:
// - no error occurred
// - next rune is ASCII
// - current position is not end of parse
//
// Advance `p` accordingly using an optimized code path
static char NextChar(Lex *p)
{
	assert(!p->hasError);
	assert( p->pos < p->lim && *p->pos != '\0');
	assert(*(Uint8 *) p->pos < RUNE_SELF);

	return (p->nr = *p->pos++);
}

static char PeekChar(Lex *p)
{
	assert(!p->hasError);

	return (p->pos < p->lim) ? *p->pos : '\0';
}

static Boolean PeekString(Lex *p, const char *s)
{
	assert(!p->hasError);
	assert(*s != '\0');

	size_t n1 = strlen(s);
	size_t n2 = p->lim - p->pos;
	if (n2 < n1)
		return FALSE;

	return strncmp(p->pos, s, n1) == 0;
}

static void UngetRune(Lex *p)
{
	if (p->nr == '\0')
		return;    // ignore on end of parse/error
	if (p->nr == RUNE_ERR) {
		p->pos--;  // only move back one byte on decoding error
		return;
	}

	p->pos -= runelen(p->nr);
}

static Boolean AppendDirty(Lex *p, Tok *tok)
{
	if (tok->flags & TT_TRUNC)
		return FALSE;  // refuse any further attempt to append

	size_t n = runelen(p->nr);
	if (TOKLEN(tok) + n < MAXTOKLEN) {  // NOTE: always leave 1 char for '\0'
		// All good, no overflow
		tok->subtype += runetochar(tok->text + TOKLEN(tok), p->nr);
		return TRUE;
	}

	// Truncation
	tok->flags |= TT_TRUNC;
	if ((p->flags & L_ALLOWTRUNC) == 0) {
		tok->text[TOKLEN(tok)] = '\0';  // force NUL termination

		LexerError(p, "'%s...': MAXTOKLEN limit exceeded", tok->text);
	}

	return FALSE;
}

static Boolean AppendDirtyString(Lex *p, Tok *tok, size_t n)
{
	if (tok->flags & TT_TRUNC)
		return FALSE;  // refuse any further attempt to append

	if (TOKLEN(tok) + n < MAXTOKLEN) {  // NOTE: always leave 1 char for '\0'
		// Success
		memcpy(tok->text + TOKLEN(tok), p->pos, n);
		tok->subtype += n;
		return TRUE;
	}

	// Truncation
	tok->flags |= TT_TRUNC;
	if ((p->flags & L_ALLOWTRUNC) == 0) {
		tok->text[TOKLEN(tok)] = '\0';  // force NUL termination

		LexerError(p, "'%s...': MAXTOKLEN limit exceeded", tok->text);
	}

	return FALSE;
}

static void SkipWhitespace(Lex *p)
{
	while (TRUE) {
		// Fast route to skip ASCII whitespaces
		Uint8 uc;
		while ((uc = PeekChar(p)) <= ' ') {
			if (uc == '\0')
				break;
			if (uc == '\n')
				p->line++;

			NextChar(p);
		}

		NextRune(p);
		if (isspacerune(p->nr))
			continue;  // ... rare occurrence of UTF8 space

		// Skip comments
		if (p->nr == '/') {
			// C++-Style comments
			char c = PeekChar(p);
			if (c == '/') {
				NextChar(p);

				while (NextRune(p) != '\n') {
					if (p->nr == '\0')
						return;
				}

				p->line++;
				continue;

			} else if (c == '*') {
				// C-Style comments
				NextChar(p);

				while (TRUE) {
					NextRune(p);
					if (p->nr == '\n') {
						p->line++;
						continue;
					}

					if (p->nr == '\0') {
						if (!p->hasError)
							LexerWarning(p, "Unterminated comment");

						return;
					}

					c = PeekChar(p);
					if (p->nr == '*' && c == '/')
						break;  // end of comment

					if (p->nr == '/' && c == '*')
						LexerWarning(p, "Nested comment");
				}

				NextChar(p);  // skip comment termination
				continue;
			}
		}

		UngetRune(p);
		break;  // not a comment, break the loop
	}
}

static Boolean ReadEscapeCharacter(Lex *p, Tok *tok)
{
	Rune r;

	// Determine the escape character
	char c = PeekChar(p);

	switch (c) {
	case '\\': r = '\\'; break;
	case 'n':  r = '\n'; break;
	case 'r':  r = '\r'; break;
	case 't':  r = '\t'; break;
	case 'v':  r = '\v'; break;
	case 'b':  r = '\b'; break;
	case 'f':  r = '\f'; break;
	case 'a':  r = '\a'; break;
	case '\'': r = '\''; break;
	case '\"': r = '\"'; break;
	case '\?': r = '\?'; break;
	case 'x':
		NextChar(p);

		r = 0;
		while (TRUE) {
			char c = PeekChar(p);
			if (c == '\0')
				break;

			if (c >= '0' && c <= '9')
				c = c - '0';
			else if (c >= 'A' && c <= 'Z')
				c = c - 'A' + 10;
			else if (c >= 'a' && c <= 'z')
				c = c - 'a' + 10;
			else
				break;

			NextChar(p);

			r = (r << 4) + c;
		}

		break;

	default:
		// NOTE: decimal ASCII code, NOT octal
		if (!Df_isdigit(c)) {
			if (c == '\0')
				LexerError(p, "Unexpected end of parse after backslash");
			else
				LexerError(p, "Unknown escape char");

			return FALSE;
		}

		r = 0;
		do {
			NextChar(p);

			r = r * 10 + (c - '0');

			c = PeekChar(p);
		} while (Df_isdigit(c));

		break;
	}

	if (r > 0xff) {
		LexerWarning(p, "Too large value in escape character");
		r = 0xff;
	}

	// Succesfully read escape character
	p->nr = r;
	AppendDirty(p, tok);
	return TRUE;
}

static char *ReadString(Lex *p, Tok *tok)
{
	// Leading quote
	Rune quote = p->nr;
	if (quote == '\"')
		tok->type = TT_STRING;
	else
		tok->type = TT_LITERAL;

	// Read string body
	while (TRUE) {
		NextRune(p);

		if (p->nr == '\\' && (p->flags & L_NOSTRESC) == 0) {
			// There is an escape character and escape characters are allowed
			if (!ReadEscapeCharacter(p, tok))
				return NULL;

		} else if (p->nr == quote) {
			// Got a trailing quote

			// If consecutive strings should not be concatenated
			if ((p->flags & L_NOSTRCAT) &&
				((p->flags & L_ALLOWBACKSLASHSTRCAT) == 0 || quote != '\"'))
				break;

			char    *tmp_p   = p->pos;
			unsigned tmpline = p->line;
			// Read white space between two possibly consecutive strings
			SkipWhitespace(p);

			if (p->flags & L_NOSTRCAT) {
				if (NextRune(p) != '\\') {  // also catches end of parse
					p->pos  = tmp_p;
					p->line = tmpline;
					break;
				}

				SkipWhitespace(p);
				if (NextRune(p) != quote) {
					LexerError(p, "Expecting string after ' terminated line");
					return NULL;
				}
			}

			// If there's no leading quote
			if (NextRune(p) != quote) {  // also catches end of parse
				p->pos  = tmp_p;
				p->line = tmpline;
				break;
			}
		} else {
			if (p->nr == '\0') {
				if (!p->hasError)
					LexerError(p, "Missing trailing quote");

				return NULL;
			}
			if (p->nr == '\n') {
				LexerError(p, "Newline inside string");
				return NULL;
			}

			AppendDirty(p, tok);
		}
	}

	tok->text[TOKLEN(tok)] = '\0';

	if (tok->type == TT_LITERAL) {
		if ((p->flags & L_ALLOWMULTICHARLIT) == 0 && TOKLEN(tok) != 1)
			LexerWarning(p, "Literal is not one character long");

		tok->subtype = tok->text[0];
	}

	return tok->text;
}

static Boolean IsNameChar(Rune r, unsigned flags)
{
	if (r < RUNE_SELF) {
		if (Df_isalnum(r) || r == '_')
			return TRUE;
		if (r == '/' || r == '\\' || r == ':' || r == '.')
			return (flags & L_ALLOWPATHS) != 0;

		return FALSE;
	}

	return !isspacerune(r);
}

static Boolean IsLetterChar(Rune r)
{
	return Df_isalpha(r) || r == '_' || r >= RUNE_SELF;
}

static void SkipBOM(Lex *p)
{
	if (!fullrune(p->pos, p->lim - p->pos))
		return;

	Rune r;

	size_t n = chartorune(&r, p->pos);
	if (r == BOM)
		p->pos += n;
}

static char *ReadName(Lex *p, Tok *tok)
{
	tok->type = TT_NAME;

	do {
		AppendDirty(p, tok);
		NextRune(p);
	} while (IsNameChar(p->nr, p->flags));

	UngetRune(p);

	tok->text[TOKLEN(tok)] = '\0';
	return tok->text;
}

static char *ReadNumber(Lex *p, Tok *tok)
{
	tok->type = TT_NUMBER;

	char base = PeekChar(p);

	unsigned dot, len;
	unsigned subtype;

	if (p->nr == '0' && base != '.') {
		if (base == 'x' || base == 'X') {
			// Hexadecimal number
			AppendDirty(p, tok); NextChar(p);
			AppendDirty(p, tok);

			while (Df_ishexdigit(PeekChar(p))) {
				NextChar(p);
				AppendDirty(p, tok);
			}

			subtype = TT_HEX | TT_INT;
		} else if (base == 'b' || base == 'B') {
			// Binary number
			AppendDirty(p, tok); NextChar(p);
			AppendDirty(p, tok);

			while (TRUE) {
				char c = PeekChar(p);
				if (c != '0' && c != '1')
					break;

				NextChar(p);
				AppendDirty(p, tok);
			}

			subtype = TT_BIN | TT_INT;
		} else {
			// Octal number
			AppendDirty(p, tok); NextChar(p);

			while (TRUE) {
				char c = PeekChar(p);
				if (c < '0' || c > '7')
					break;

				NextChar(p);
				AppendDirty(p, tok);
			}

			subtype = TT_OCT | TT_INT;
		}
	} else {
		// Decimal integer or floating point number
		dot = 0;

		char c;
		while (TRUE) {
			if (p->nr == '.')
				dot++;

			AppendDirty(p, tok);
			c = PeekChar(p);
			if (!Df_isdigit(c) && c != '.')
				break;  // next character is not a digit

			NextChar(p);  // consume one more character
		}
		if (c == 'e' && dot == 0)
			dot++;  // scientific notation without a decimal point

		if (dot > 1) {
			LexerError(p, "More than one dot in number");
			return NULL;
		}

		if (dot == 1) {
			// FIXME(micia): How should we deal with stuff like 1.0e<EOF>

			// Floating point number
			subtype = TT_DEC | TT_FLOAT;

			// Check for floating point exponent
			if (c == 'e') {
				NextChar(p);
				AppendDirty(p, tok);

				c = PeekChar(p);
				if (c == '-' || c == '+') {
					NextChar(p);
					AppendDirty(p, tok);

					c = PeekChar(p);
				}
				while (TRUE) {
					if (!Df_isdigit(c))
						break;

					NextChar(p);
					AppendDirty(p, tok);

					c = PeekChar(p);
				}
			} else if (c == '#') {
				// Check for floating point exception infinite 1.#INF or indefinite 1.#IND or NaN

				NextChar(p);

				len = 4;
				if (PeekString(p, "INF"))
					subtype |= TT_INF;
				else if (PeekString(p, "IND"))
					subtype |= TT_INDEF;
				else if (PeekString(p, "NAN"))
					subtype |= TT_NAN;
				else if (PeekString(p, "QNAN")) {
					subtype |= TT_NAN;
					len++;
				} else if (PeekString(p, "SNAN")) {
					subtype |= TT_NAN;
					len++;
				}

				AppendDirtyString(p, tok, len);
				p->pos += len;

				while (Df_isdigit(PeekChar(p))) {
					NextChar(p);
					AppendDirty(p, tok);
				}
				
				if ((p->flags & L_ALLOWFLOATEXC) == 0) {
					LexerError(p, "Parsed '%*s'", (int) TOKLEN(tok), tok->text);
					return NULL;
				}
			}
		} else
			subtype = TT_DEC | TT_INT;
	}

	// Check for format suffixes
	char suffix = PeekChar(p);
	if (subtype & TT_FLOAT) {
		if (suffix == 'f' || suffix == 'F') {
			// Single-precision: float
			subtype |= TT_SINGLE_PREC;
			NextChar(p);
		} else if (suffix == 'l' || suffix == 'L') {
			// Extended-precision: long double
			subtype |= TT_EXT_PREC;
			NextChar(p);
		} else {
			// Default is double-precision: double
			subtype |= TT_DOUBLE_PREC;
		}
	} else if (subtype & TT_INT) {
		// Default: signed long
		for (unsigned i = 0; i < 2; i++) {
			if (suffix == 'l' || suffix == 'L') {
				if (subtype & TT_LONG)
					break;  // duplicate long suffix

				subtype |= TT_LONG;  // long integer
				NextChar(p);

				suffix = PeekChar(p);
				if (suffix == 'l' || suffix == 'L') {
					subtype |= TT_LLONG;  // long long integer
					NextChar(p);
				}

			} else if (suffix == 'u' || suffix == 'U') {
				if (subtype & TT_UNSIGNED)
					break;  // duplicate unsigned suffix

				subtype |= TT_UNSIGNED;  // unsigned integer
				NextChar(p);

			} else
				break;

			suffix = PeekChar(p);
		}
	}

	tok->text[TOKLEN(tok)] = '\0';

	tok->subtype = subtype;
	if (subtype & TT_INT) {
		tok->intvalue   = Atoll(tok->text, NULL, 0, NULL);
		tok->floatvalue = tok->intvalue;
	} else if (subtype & TT_FLOAT) {
		tok->floatvalue = Atof(tok->text, NULL, NULL);
		tok->intvalue   = (long long) tok->floatvalue;
	}

	return tok->text;
}

static char *ReadIpv4(Lex *p, Tok *tok)
{
	tok->type = TT_NUMBER;

	unsigned subtype = TT_IPADDR | TT_IPV4;

	int  dots = 0, mulDots = 0;
	char lastCh = '\0';

	AppendDirty(p, tok);
	while (TRUE) {
		char c = PeekChar(p);
		if (!Df_isdigit(c) && c != '.')
			break;

		if (c == '.') {
			if (lastCh == '.')
				mulDots++;  // don't report error immediately, so bad token may be skipped

			dots++;
		}

		NextChar(p);
		AppendDirty(p, tok);

		lastCh = c;
	}

	if (mulDots > 0) {
		LexerError(p, "Multiple consecutive dots inside IPv4 address");
		return NULL;
	}
	if (dots != 3) {
		LexerError(p, "IPv4 address should have 3 dots");
		return NULL;
	}

	if ((p->flags & L_PLAINIPADDRONLY) == 0 && PeekChar(p) == ':') {
		// Port number available
		AppendDirty(p, tok);
		NextChar(p);

		if (!Df_isdigit(PeekChar(p)))
			LexerError(p, "Missing port number after ':' inside IPv4 address");

		do {
			NextChar(p);
			AppendDirty(p, tok);
		} while (Df_isdigit(PeekChar(p)));

		subtype |= TT_IPPORT;
	}

	tok->text[TOKLEN(tok)] = '\0';

	tok->subtype = subtype;
	return tok->text;
}

static Boolean IsIpv6ZoneChar(Rune r)
{
	if (r < RUNE_SELF)
		return Df_isalnum(r) || r == '-' || r == '_' || r == '.' || r == '+' || r == '*';

	return !isspacerune(r);
}

static char *ReadIpv6(Lex *p, Tok *tok)
{
	tok->type = TT_NUMBER;

	unsigned subtype = TT_IPADDR | TT_IPV6;

	int colons = 0, dots = 0, mulDots = 0;
	Boolean isLiteral = FALSE;
	char c, lastCh = p->nr;

	if (p->nr == ':')
		colons++;
	if (p->nr == '[')
		isLiteral = TRUE;

	assert(!isLiteral || (p->flags & L_PLAINIPADDRONLY) == 0);

	AppendDirty(p, tok);
	while (TRUE) {
		c = PeekChar(p);
		if (!Df_ishexdigit(c) && c != '.' && c != ':')
			break;

		if (c == '.') {
			if (colons == 0)
				break;
			if (lastCh == '.')
				mulDots++;  // report error after reading the full token

			dots++;
		}
		if (c == ':') {
			if (dots > 0)
				break;

			colons++;
		}

		if (dots > 0 && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
			break;

		NextChar(p);
		AppendDirty(p, tok);

		lastCh = c;
	}

	// Perform basic format checks (still they don't ensure correct address)
	if (colons < 2) {
		LexerError(p, "IPv6 address should have at least 2 colons");
		return NULL;
	}
	if (dots > 0) {
		if (mulDots > 0) {
			LexerError(p, "IPv4 mapped IPv6 addess has multiple consecutive dots");
			return NULL;
		}
		if (dots != 3) {
			LexerError(p, "IPv4-mapped IPv6 address should have 3 dots");
			return NULL;
		}
		if (colons > 6) {
			LexerError(p, "IPv4-mapped IPv6 address should have at most 6 colons");
			return NULL;
		}
	} else if (colons > 7) {
		LexerError(p, "IPv6 address should have at most 7 colons");
		return NULL;
	}

	if ((p->flags & L_PLAINIPADDRONLY) == 0 && c == '%') {
		// Address contains a zone id
		NextChar(p);
		AppendDirty(p, tok);

		Rune r = NextRune(p);
		if (!IsIpv6ZoneChar(r)) {
			UngetRune(p);

			LexerError(p, "Missing IPv6 address zone id after '%%'");
			return NULL;
		}
	
		do AppendDirty(p, tok); while (IsIpv6ZoneChar(NextRune(p)));

		UngetRune(p);

		subtype |= TT_IPV6ZONE;
	}
	if (isLiteral) {
		// Ensure proper IPv6 literal termination
		if (PeekChar(p) != ']') {
			LexerError(p, "IPv6 address literal lacks closing ']'");
			return NULL;
		}

		NextChar(p);
		AppendDirty(p, tok);

		subtype |= TT_IPV6LIT;

		if (PeekChar(p) == ':') {
			// Has a specific port number
			NextChar(p);
			AppendDirty(p, tok);

			if (!Df_isdigit(PeekChar(p))) {
				LexerError(p, "IPv6 address literal lacks port number after ':'");
				return NULL;
			}

			do {
				NextChar(p);
				AppendDirty(p, tok);
			} while (Df_isdigit(PeekChar(p)));

			subtype |= TT_IPPORT;
		}
	}

	tok->text[TOKLEN(tok)] = '\0';

	tok->subtype = subtype;
	return tok->text;
}

static char *ReadPunct(Lex *p, Tok *tok)
{
	const Punctuation *puncts = p->puncts;
	if (!puncts)
		puncts = p_punctuations;

	for (size_t i = 0; p_punctuations[i].p; i++) {
		const Punctuation *punc = &p_punctuations[i];

		size_t len = strlen(punc->p);

		// Check for this punctuation in the script
		if (PeekString(p, punc->p)) {
			// Punctuation matches, copy into the token
			tok->type = TT_PUNCT;
			AppendDirtyString(p, tok, len);
			tok->text[len] = '\0';

			tok->subtype = punc->id;

			p->pos += len;
			return tok->text;
		}
	}

	return NULL;
}

void BeginLexerSession(Lex *p, const char *name, unsigned startLine)
{
	Df_strncpyz(p->name, name, sizeof(p->name));
	p->line = MAX(startLine, 1);
}

void SetLexerTextn(Lex *p, const char *text, size_t n)
{
	p->pos = (char *) text;
	p->lim = p->pos + n;

	p->hasError = p->hasBufferedToken = FALSE;

	SkipBOM(p);
}

static Boolean PeekIpv4(Lex *p)
{
	// To unambiguously determine that a token is an IPv4 address
	// we must encounter at least 2 dots within the first 8 chars
	// (this is necessary to avoid confusing a double for an IPv4)
	size_t peekLen = 7;  // p->nr tested separately

	if (!Df_isdigit(p->nr))
		return FALSE;

	if (peekLen > (size_t) (p->lim - p->pos))
		peekLen = p->lim - p->pos;

	int dots = 0;
	for (size_t i = 0; i < peekLen; i++) {
		if (!Df_isdigit(p->pos[i]) && p->pos[i] != '.')
			break;

		if (p->pos[i] == '.' && ++dots == 2)
			return TRUE;
	}

	return FALSE;
}

static Boolean PeekIpv6(Lex *p)
{
	// In order to unambiguously determine that a token is in fact an
	// IPv6 address, we have to encounter 2 colons within the first 10 chars
	// (this is needed to avoid confusing `(a) ? 100:200` for an IPv6 address)
	//
	// NOTE: C++ syntax may kills us anyway, `:: function()` would be
	//       misinterpreted like `IPv6` `function`...

	size_t peekLen = 9;  // p->nr is tested separately
	int    colons  = 0;

	if ((p->flags & L_PLAINIPADDRONLY) == 0 && p->nr == '[')
		peekLen++;  // could be: [address]:port
	else if (p->nr == ':') {
		colons++;
		peekLen = 5;
	} else if (!Df_ishexdigit(p->nr))
		return FALSE;

	if (peekLen > (size_t) (p->lim - p->pos))
		peekLen = p->lim - p->pos;

	for (size_t i = 0; i < peekLen; i++) {
		if (!Df_ishexdigit(p->pos[i]) && p->pos[i] != ':')
			return FALSE;

		if (p->pos[i] == ':' && ++colons == 2)
				return TRUE;
	}
	return FALSE;
}

char *Lex_ReadToken(Lex *p, Tok *tok)
{
	if (p->hasError)
		return NULL;

	if (p->hasBufferedToken) {
		// Return previously stored token
		memcpy(tok, &p->buf, sizeof(*tok));
		p->hasBufferedToken = FALSE;
		return tok->text;
	}

	char    *tmp_p   = p->pos;
	unsigned tmpline = p->line;

	// Advance to next token position
	SkipWhitespace(p);

	// Clear token
	tok->type              = 0;
	tok->flags             = 0;
	tok->subtype           = 0;  // used as length during parsing
	tok->linesCrossed      = p->line - tmpline;
	tok->spacesBeforeToken = p->pos - tmp_p;
	tok->line              = p->line;

	tok->intvalue   = 0;
	tok->floatvalue = 0.0;
	tok->text[0]    = '\0';

	NextRune(p);
	if (p->nr == '\0')
		return NULL;

	if (p->nr == '\"' || p->nr == '\'')
		return ReadString(p, tok);

	if (IsLetterChar(p->nr) || (p->flags & L_STRONLY) != 0)
		return ReadName(p, tok);

	if (p->flags & L_ALLOWIPADDR) {
		if (PeekIpv4(p))
			return ReadIpv4(p, tok);
		if (PeekIpv6(p))
			return ReadIpv6(p, tok);
	}

	if (Df_isdigit(p->nr) || (p->nr == '.' && Df_isdigit(PeekChar(p))))
		return ReadNumber(p, tok);

	UngetRune(p);

	if (!ReadPunct(p, tok)) {
		LexerError(p, "Syntax error");
		return NULL;
	}

	return tok->text;
}

long long Lex_ParseInt(Lex *p, Boolean optionalSign)
{
	Tok tok;

	long long signum = 1LL;
	if (optionalSign && Lex_CheckTokenType(p, &tok, TT_PUNCT, 0)) {
		switch (tok.subtype) {
		case P_ADD:                          break;
		case P_SUB: signum = -1LL;           break;
		default:    Lex_UngetToken(p, &tok); break;
		}
	}
	if (!Lex_MatchTokenType(p, &tok, TT_NUMBER, TT_INT))
		return 0LL;

	return signum * tok.intvalue;
}

Boolean Lex_ParseBool(Lex *p, Boolean allowNumeric)
{
	Tok tok;
	if (allowNumeric && Lex_CheckTokenType(p, &tok, TT_NUMBER, TT_INT))
		return tok.intvalue != 0;
	if (!Lex_MatchAnyToken(p, &tok))
		return FALSE;

	if (strcmp(tok.text, "true") == 0)
		return TRUE;
	if (strcmp(tok.text, "false") == 0)
		return FALSE;

	LexerError(p, "Read '%s' while expecting boolean value", tok.text);
	return FALSE;
}

double Lex_ParseFloat(Lex *p, Boolean optionalSign)
{
	Tok tok;

	double signum = 1.0;
	if (optionalSign && Lex_CheckTokenType(p, &tok, TT_PUNCT, 0)) {
		switch (tok.subtype) {
		case P_ADD:                          break;
		case P_SUB: signum = -1.0;           break;
		default:    Lex_UngetToken(p, &tok); break;
		}
	}
	if (!Lex_MatchTokenType(p, &tok, TT_NUMBER, TT_FLOAT))
		return 0.0;

	return signum * tok.floatvalue;
}

void Lex_UngetToken(Lex *p, const Tok *tok)
{
	if (p->hasError)
		return;
	if (p->hasBufferedToken)
		LexerError(p, "%s: BUG: Function called twice", __func__);

	// NOTE: memcpy() only the correct buffer size
	//       a Tok may be partially allocated!
	size_t n = strlen(tok->text);
	memcpy(&p->buf, tok, offsetof(Tok, text[n+1]));
	p->hasBufferedToken = TRUE;
}

#define WARN_FMT "%s:%u: WARNING: %s\n"
#define COLOR_WARN_FMT \
	VTSGR(VTBLD) "%s" VTSGR(VTNOBLD) ":" VTSGR(VTBLD) "%u" VTSGR(VTNOBLD) ": " \
	VTSGR(VTYEL) VTSGR(VTBLD) "WARNING" VTSGR(VTFGDFLT) VTSGR(VTNOBLD) ": " \
	"%s\n"

#define ERR_FMT "%s:%u: ERROR: %s\n"
#define COLOR_ERR_FMT \
	VTSGR(VTBLD) "%s" VTSGR(VTNOBLD) ":" VTSGR(VTBLD) "%u" VTSGR(VTNOBLD) ": " \
	VTSGR(VTRED) VTSGR(VTBLD) "ERROR" VTSGR(VTFGDFLT) VTSGR(VTNOBLD) ": " \
	"%s\n"

static void EmitMessage(Lex *p,
	void (*emitf)(Lex *, const char *, void *),
	const char *fmt,
	...)
{
	va_list va;
	char *buf;
	int n1, n2;

	if (emitf == LEX_QUIT)
		emitf = Lex_Quit;
	if (emitf == LEX_WARN)
		emitf = Lex_Warn;

	va_start(va, fmt);
	n1 = vsnprintf(NULL, 0, fmt, va);
	va_end(va);
	if (n1 < 0) {
		buf = "<Lexer: Can't emit message: vsnprintf() failed!>\n";
		goto do_emit;
	}

	buf = (char *) alloca(n1 + 1);
	va_start(va, fmt);
	n2 = vsnprintf(buf, n1 + 1, fmt, va);
	va_end(va);
	if (n2 < 0) {
		buf = "<Lexer: Can't emit message: vsnprintf() failed!>\n";
		goto do_emit;
	}

	assert(n1 == n2);

do_emit:
	emitf(p, buf, p->obj);
}

void LexerError(Lex *p, const char *fmt, ...)
{
	if (p->flags & L_NOERR)
		return;

	p->hasError = TRUE;  // *ONLY* place where error flag is set!

	void (*errf)(Lex *, const char *, void *);
	va_list va;
	const char *msgt;
	char *buf;
	int n1, n2;

	errf = p->Error;
	if (errf == LEX_IGN)
		return;

	va_start(va, fmt);
	n1 = vsnprintf(NULL, 0, fmt, va);
	va_end(va);
	if (n1 < 0) {
		buf = "<Lexer: Can't format error: vsnprintf() failed!>\n";
		goto emit;
	}

	buf = (char *) alloca(n1 + 1);
	va_start(va, fmt);
	n2 = vsnprintf(buf, n1 + 1, fmt, va);
	va_end(va);
	if (n2 < 0) {
		buf = "<Lexer: Can't format error: vsnprintf() failed!>\n";
		goto emit;
	}

	assert(n1 == n2);

emit:
	msgt = (p->flags & L_COLORED) ? COLOR_ERR_FMT : ERR_FMT;
	EmitMessage(p, errf, msgt, p->name, p->line, buf);
}

void LexerWarning(Lex *p, const char *fmt, ...)
{
	if (p->flags & L_NOWARN)
		return;

	void (*warnf)(Lex *, const char *, void *);
	va_list va;
	const char *msgt;
	char *buf;
	int n1, n2;

	warnf = p->Warn;
	if (warnf == LEX_IGN)
		return;

	va_start(va, fmt);
	n1 = vsnprintf(NULL, 0, fmt, va);
	va_end(va);
	if (n1 < 0) {
		buf = "<Lexer: Can't format warning: vsnprintf() failed!>";
		goto emit;
	}

	buf = (char *) alloca(n1 + 1);
	va_start(va, fmt);
	n2 = vsnprintf(buf, n1 + 1, fmt, va);
	va_end(va);
	if (n2 < 0) {
		buf = "<Lexer: Can't format warning: vsnprintf() failed!>";
		goto emit;
	}

	assert(n1 == n2);

emit:
	msgt = (p->flags & L_COLORED) ? COLOR_WARN_FMT : WARN_FMT;
	EmitMessage(p, warnf, msgt, p->name, p->line, buf);
}

char *Lex_MatchToken(Lex *p, const char *match)
{
	Tok tok;

	char *t = Lex_ReadToken(p, &tok);
	if (t && strcmp(t, match) == 0)
		return (char *) match;

	if (p->hasError)
		return NULL;

	if (t)
		LexerError(p, "Read unexpected token '%s' while expecting '%s'", t, match);
	else
		LexerError(p, "Couldn't read expected token '%s'", match);
	
	return NULL;
}

char *Lex_MatchTokenType(Lex *p, Tok *tok, int type , unsigned subtype)
{
	char *t = Lex_ReadToken(p, tok);
	if (t && tok->type == type && (tok->subtype & subtype) == subtype)
		return t;  // success

	if (p->hasError)
		return NULL;

	// Make some effort to provide meaningful diagnostic

	char ty[64], sub[256];
	char *sp;

	switch (type) {
	case TT_STRING:  strcpy(ty, "string");      break;
	case TT_LITERAL: strcpy(ty, "literal");     break;
	case TT_NUMBER:  strcpy(ty, "numeric");     break;
	case TT_NAME:    strcpy(ty, "name");        break;
	case TT_PUNCT:   strcpy(ty, "punctuation"); break;
	default:
		sp    = strcpy(ty, "<TYPE: 0x");    sp += 9;
		sp    = Xtoa((unsigned) type, sp);
		*sp++ = '>';
		*sp   = '\0';
		break;
	}
	if (!t) {
		LexerError(p, "Reached end while expecting %s token", ty);
		return NULL;
	}

	// Report type mismatch
	if (tok->type != type) {
		LexerError(p, "Read '%s' while expecting %s token", tok->text, ty);
		return NULL;
	}

	// Report subtype mismatch
	if (type == TT_NUMBER) {
		sub[0] = '\0';

		if (subtype & TT_DEC)
			strcat(sub, "decimal ");
		if (subtype & TT_HEX) {
			assert((subtype & (TT_OCT|TT_BIN|TT_DEC|TT_FLOAT)) == 0);
			strcat(sub, "hex ");
		}
		if (subtype & TT_OCT) {
			assert((subtype & (TT_HEX|TT_BIN|TT_DEC|TT_FLOAT)) == 0);
			strcat(sub, "octal ");
		}
		if (subtype & TT_BIN) {
			assert((subtype & (TT_HEX|TT_OCT|TT_DEC|TT_FLOAT)) == 0);
			strcat(sub, "binary ");
		}
		if (subtype & TT_UNSIGNED) {
			assert((subtype & (TT_FLOAT|TT_IPADDR)) == 0);
			strcat(sub, "unsigned ");
		}
		if (subtype & TT_LONG) {
			assert((subtype & (TT_FLOAT|TT_IPADDR)) == 0);
			strcat(sub, "long ");
		}
		if (subtype & TT_LLONG) {
			assert(subtype & TT_LONG);
			strcat(sub, "long ");
		}
		if (subtype & TT_FLOAT) {
			assert((subtype & (TT_INT|TT_IPADDR)) == 0);
			strcat(sub, "float ");
		}
		if (subtype & TT_INT) {
			assert((subtype & (TT_FLOAT|TT_IPADDR)) == 0);
			strcat(sub, "integer ");
		}
		if (subtype & TT_IPV4) {
			assert((subtype & (TT_FLOAT|TT_INT)) == 0);
			assert((subtype & TT_IPV6) == 0);
			strcat(sub, "IPv4 ");
		}
		if (subtype & TT_IPV6) {
			assert((subtype & (TT_FLOAT|TT_INT)) == 0);
			assert((subtype & TT_IPV4) == 0);
			strcat(sub, "IPv6 ");
		}
		if (subtype & TT_IPADDR) {
			assert((subtype & (TT_FLOAT|TT_INT)) == 0);
			strcat(sub, "internet address ");
		}

		Df_strtrimr(sub);
		if (sub[0] != '\0') {
			LexerError(p, "Read '%s' while expecting %s value", tok->text, sub);
			return NULL;
		}
	} else if (type == TT_PUNCT) {
		const Punctuation *puncts = p->puncts;
		if (!puncts)
			puncts = p_punctuations;

		for (size_t i = 0; puncts[i].p; i++) {
			if (puncts[i].id == subtype) {
				LexerError(p, "Read '%s' while expecting %s", tok->text, puncts[i].p);
				return NULL;
			}
		}
	}

	LexerError(p, "'%s': Wrong %s token", tok->text, ty);
	return NULL;
}

char *Lex_MatchAnyToken(Lex *p, Tok *tok)
{
	if (Lex_ReadToken(p, tok))
		return tok->text;

	if (!p->hasError)
		LexerError(p, "Couldn't read expected token");

	return NULL;
}

Boolean Lex_ParseMatrix1(Lex *p, float *dest, size_t n)
{
	Tok tok;

	Lex_MatchTokenType(p, &tok, TT_PUNCT, P_PARENOPEN);
	for (size_t i = 0; i < n; i++)
		dest[i] = Lex_ParseFloat(p, /*optionalSign=*/TRUE);

	Lex_MatchTokenType(p, &tok, TT_PUNCT, P_PARENCLOSE);
	return !p->hasError;
}

Boolean Lex_ParseMatrix2(Lex *p, float *dest, size_t n, size_t m)
{
	Tok tok;

	Lex_MatchTokenType(p, &tok, TT_PUNCT, P_PARENOPEN);
	for (size_t i = 0; i < m; i++)
		Lex_ParseMatrix1(p, &dest[i * n], n);

	Lex_MatchTokenType(p, &tok, TT_PUNCT, P_PARENCLOSE);
	return !p->hasError;
}

Boolean Lex_ParseMatrix3(Lex *p, float *dest, size_t n, size_t m, size_t u)
{
	Tok tok;

	Lex_MatchTokenType(p, &tok, TT_PUNCT, P_PARENOPEN);
	for (size_t i = 0; i < u; i++)
		Lex_ParseMatrix2(p, &dest[i * n*m], n, m);

	Lex_MatchTokenType(p, &tok, TT_PUNCT, P_PARENCLOSE);
	return !p->hasError;
}

#define LEX_SAVE(p) \
	char    *_last_pos_     = p->pos;              \
	unsigned _last_line_    = p->line;             \
	Boolean  _was_buffered_ = p->hasBufferedToken; \
	Boolean  _had_error_    = p->hasError

#define LEX_RESTORE(p) do { \
	p->pos              = _last_pos_;     \
	p->line             = _last_line_;    \
	p->hasBufferedToken = _was_buffered_; \
	p->hasError         = _had_error_;    \
} while (0)

char *Lex_ReadTokenOnLine(Lex *p, Tok *tok)
{
	LEX_SAVE(p);

	if (Lex_ReadToken(p, tok) && tok->linesCrossed == 0)
		return tok->text;

	LEX_RESTORE(p);
	return NULL;
}

void Lex_SkipLine(Lex *p)
{
	Tok tok;

	while (TRUE) {
		LEX_SAVE(p);

		if (!Lex_ReadToken(p, &tok))
			break;
		if (tok.linesCrossed > 0) {
			LEX_RESTORE(p);
			break;
		}
	}
}

char *Lex_SkipUntil(Lex *p, const char *match)
{
	Tok tok;

	while (Lex_MatchAnyToken(p, &tok)) {
		if (strcmp(tok.text, match) == 0)
			return (char *) match;
	}
	return NULL;
}

Boolean Lex_SkipBracedSection(Lex *p, Boolean parseFirstBrace)
{
	Tok tok;
	if (parseFirstBrace && !Lex_MatchTokenType(p, &tok, TT_PUNCT, P_BRACEOPEN))
		return FALSE;

	unsigned depth = 1;
	do {
		if (!Lex_MatchAnyToken(p, &tok))
			return FALSE;

		if (tok.type == TT_PUNCT) {
			if (tok.subtype == P_BRACEOPEN)
				depth++;
			else if (tok.subtype == P_BRACECLOSE)
				depth--;
		}
	} while (depth > 0);

	return TRUE;
}

char *Lex_CheckToken(Lex *p, const char *match)
{
	Tok tok;

	LEX_SAVE(p);

	char *t = Lex_ReadToken(p, &tok);
	if (t && strcmp(t, match) == 0)
		return (char *) match;

	LEX_RESTORE(p);
	return NULL;
}

char *Lex_CheckTokenType(Lex *p, Tok *tok, int type, unsigned subtype)
{
	LEX_SAVE(p);

	char *t = Lex_ReadToken(p, tok);
	if (t && tok->type == type && (tok->subtype & subtype) == subtype)
		return t;

	LEX_RESTORE(p);
	return NULL;
}

char *Lex_PeekToken(Lex *p, const char *match)
{
	Tok   tok;
	char *t;

	LEX_SAVE(p);
	t = Lex_ReadToken(p, &tok);
	LEX_RESTORE(p);

	return (t && strcmp(t, match) == 0) ? (char *) match : NULL;
}

char *Lex_PeekTokenType(Lex *p, Tok *tok, int type, unsigned subtype)
{
	char *t;

	LEX_SAVE(p);
	t = Lex_ReadToken(p, tok);
	LEX_RESTORE(p);

	if (t && tok->type == type && (tok->subtype & subtype) == subtype)
		return t;

	return NULL;
}
