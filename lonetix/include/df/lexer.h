// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file lexer.h
 *
 * C-compliant non-allocating UTF-8 text lexer.
 *
 * \author Lorenzo Cogotti
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 */

#ifndef DF_LEXER_H_
#define DF_LEXER_H_

#include "utf/utfdef.h"

/// Maximum allowed token length inside text parsed by `Lex`.
#define MAXTOKLEN 256

/// String token type
#define TT_STRING  U16_C(1)
/// Literal token type
#define TT_LITERAL U16_C(2)
/// Numeric token type
#define TT_NUMBER  U16_C(3)
/// Token type for names or identifiers
#define TT_NAME    U16_C(4)
/// Punctuation token type
#define TT_PUNCT   U16_C(5)

/**
 * Token subtype flags for `TT_NUMBER`
 *
 * @{
 */

#define TT_INT         BIT(0)   ///< integer
#define TT_DEC         BIT(1)   ///< decimal number
#define TT_HEX         BIT(2)   ///< hexadecimal number
#define TT_OCT         BIT(3)   ///< octal number
#define TT_BIN         BIT(4)   ///< binary number
#define TT_LONG        BIT(5)   ///< long int
#define TT_LLONG       BIT(6)   ///< long long int
#define TT_UNSIGNED    BIT(7)   ///< unsigned int
#define TT_FLOAT       BIT(8)   ///< floating point number
#define TT_SINGLE_PREC BIT(9)   ///< float
#define TT_DOUBLE_PREC BIT(10)  ///< double
#define TT_EXT_PREC    BIT(11)  ///< long double
#define TT_INF         BIT(12)  ///< infinite 1.#INF
#define TT_INDEF       BIT(13)  ///< indefinite 1.#IND
#define TT_NAN         BIT(14)  ///< NaN
#define TT_IPADDR      BIT(15)  ///< ip address (address may still be ill-formed, e.g. `102948.22.999.1`)
#define TT_IPV4        BIT(16)  ///< ipv4 address format
#define TT_IPV6        BIT(17)  ///< ipv6 address format
#define TT_IPV6LIT     BIT(18)  ///< ipv6 address is expressed as literal (e.g. `[2001:db8:a::123]`)
#define TT_IPV6ZONE    BIT(19)  ///< ipv6 address contains a zone index/string (e.g. `fe80::1ff:fe23:4567:890a%3`)
#define TT_IPPORT      BIT(20)  ///< ip address includes a port

/** @} */

/**
 * Token flags
 *
 * @{
 */

/// Indicates `Tok` originally exceeded `MAXTOKLEN` and was consequently truncated.
#define TT_TRUNC BIT(15)

/** @} */

/// Lexer punctuation token descriptor (text -> token `subtype`).
typedef struct Punctuation Punctuation;
struct Punctuation {
	const char *p;   ///< NULL for last element in punctuation list.
	Uint32      id;  ///< Puntuation identifier (returned in `Tok->subtype`)
};

// punctuation ids
#define P_RSHIFT_ASSIGN 1
#define P_LSHIFT_ASSIGN 2
#define P_PARMS         3
#define P_PRECOMPMERGE  4

#define P_LOGIC_AND  5
#define P_LOGIC_OR   6
#define P_LOGIC_GEQ  7
#define P_LOGIC_LEQ  8
#define P_LOGIC_EQ   9
#define P_LOGIC_UNEQ 10

#define P_MUL_ASSIGN 11
#define P_DIV_ASSIGN 12
#define P_MOD_ASSIGN 13
#define P_ADD_ASSIGN 14
#define P_SUB_ASSIGN 15
#define P_INC        16
#define P_DEC        17

#define P_BIN_AND_ASSIGN 18
#define P_BIN_OR_ASSIGN  19
#define P_BIN_XOR_ASSIGN 20
#define P_RSHIFT         21
#define P_LSHIFT         22

#define P_POINTERREF 23
#define P_MUL        24
#define P_DIV        25
#define P_MOD        26
#define P_ADD        27
#define P_SUB        28
#define P_ASSIGN     29

#define P_BIN_AND 30
#define P_BIN_OR  31
#define P_BIN_XOR 32
#define P_BIN_NOT 33

#define P_LOGIC_NOT     34
#define P_LOGIC_GREATER 35
#define P_LOGIC_LESS    36

#define P_REF          37
#define P_COMMA        38
#define P_SEMICOLON    39
#define P_COLON        40
#define P_QUESTIONMARK 41

#define P_PARENOPEN      42
#define P_PARENCLOSE     43
#define P_BRACEOPEN      44
#define P_BRACECLOSE     45
#define P_SQBRACKETOPEN  46
#define P_SQBRACKETCLOSE 47
#define P_BACKSLASH      48

#define P_PRECOMP 49
#define P_DOLLAR  50

/**
 * \brief Token returned by `Lex`.
 *
 * Contains token text and information.
 */
typedef struct Tok Tok;
struct Tok {
	Uint16 type;
	Uint16 flags;
	Uint32 subtype;

	unsigned linesCrossed;
	unsigned spacesBeforeToken;
	unsigned line;

	long long intvalue;
	double    floatvalue;

	Tok *nextToken;

	char text[MAXTOKLEN];  // NOTE: last element to allow partial allocation
};

/// Disregard lexer errors
#define L_NOERR  BIT(0)
/// Disregard lexer warnings
#define L_NOWARN BIT(1)
/// Disregard both errors and warnings
#define L_QUIET (L_NOERR | L_NOWARN)
/// Use console colors when reporting errors and warnings
#define L_COLORED BIT(2)
/// Parse all tokens as strings, instead of breaking them using full-fledged C rules
#define L_STRONLY BIT(3)
/// Allow file paths within tokens
#define L_ALLOWPATHS BIT(4)
/// Do not allow escapes within strings
#define L_NOSTRESC BIT(5)
/// Do not concatenate consecutive strings
#define L_NOSTRCAT BIT(6)
/// Concatenate strings separated by a backslash+newline
#define L_ALLOWBACKSLASHSTRCAT BIT(7)
/// Allow multichar literals
#define L_ALLOWMULTICHARLIT    BIT(8)
/// Accepts IP addresses (parsed as `TT_NUMBER`)
#define L_ALLOWIPADDR          BIT(9)
/// IP addresses with port numbers, IPv6 literals or zone ids won't be accepted,
/// only meaningful if used with `L_ALLOWIPADDR`.
#define L_PLAINIPADDRONLY      BIT(10)
/// Allow special floating point exception tokens (0.#INF, 0.#IND).
#define L_ALLOWFLOATEXC        BIT(10)
/// Allow truncating tokens exceeding `MAXTOKLEN`.
#define L_ALLOWTRUNC           BIT(11)
/// Do not search base `#include` paths (used by PC library).
#define L_NOBASEINCLUDES       BIT(12)

/// Special callback, invokes immediate program termination after reporting a lexer message
#define LEX_QUIT  ((void (*)(Lex *, const char *, void *)) -1)
/// Special callback, makes the lexer ignore the the warning or error
/// (same behavior as `L_NOERR` and `L_NOWARN`, but as an explicit callback).
#define LEX_IGN   ((void (*)(Lex *, const char *, void *))  0)
/// Special callback, makes the lexer print an error or warning message to `stderr`,
/// doesn't terminate execution.
#define LEX_WARN  ((void (*)(Lex *, const char *, void *))  1)

/**
 * \brief A lexer, breaks text into single tokens, keeping track of the current position.
 *
 * \note This struct should be considered opaque.
 */
typedef struct Lex Lex;
struct Lex {
	char    *pos, *lim;
	unsigned line;
	Uint16   flags;
	Boolean8 hasError;
	Boolean8 hasBufferedToken;
	Rune     nr;

	const Punctuation *puncts;

	void  *obj;
	void (*Error)(Lex *, const char *, void *);
	void (*Warn)(Lex *, const char *, void *);

	Lex *nextLexer;

	Tok  buf;
	char name[MAXTOKLEN];
};

/// Register callbacks for lexer warning and error triggers.
FORCE_INLINE void SetLexerErrorFunc(Lex   *p,
                                    void (*errf)(Lex *, const char *, void *),
                                    void (*warnf)(Lex *, const char *, void *),
                                    void  *obj)
{
	p->Error = errf;
	p->Warn  = warnf;
	p->obj   = obj;
}

/**
 * \brief Set parsing session name and initial line number.
 *
 * \param [out] p    A lexer, must not be `NULL`
 * \param [in]  name Name for this parsing session
 * \param [in]  line Initial line number, 0 is implicitly changed to 1
 */
void BeginLexerSession(Lex *p, const char *name, unsigned line);

/**
 * \brief Setup lexer to parse text, sized.
 *
 * \param [out] p    A lexer, must not be `NULL`
 * \param [in]  text Text to be parsed, must have at least `n` chars
 * \param [in]  n    Number of chars in `text`
 */
void SetLexerTextn(Lex *p, const char *text, size_t n);

/**
 * \brief Setup lexer to parse text.
 *
 * \param [out] p    A lexer, must not be `NULL`
 * \param [in]  text `NUL` terminated text to be parsed
 */
FORCE_INLINE void SetLexerText(Lex *p, const char *text)
{
	EXTERNC size_t strlen(const char *);

	SetLexerTextn(p, text, strlen(text));
}

/**
 * \brief Change lexer flags.
 *
 * \param [out] p     A lexer, must not be `NULL`
 * \param [in]  flags New flags for the lexer
 */
FORCE_INLINE void SetLexerFlags(Lex *p, unsigned flags)
{
	p->flags = flags;
}

/// Retrieve current lexer flags.
FORCE_INLINE unsigned GetLexerFlags(Lex *p)
{
	return p->flags;
}

/// Trigger an error over a lexer.
CHECK_PRINTF(2, 3) void LexerError(Lex *p, const char *fmt, ...);
/**
 * Trigger a warning over a lexer.
 */
CHECK_PRINTF(2, 3) void LexerWarning(Lex *p, const char *fmt, ...);

/// Test whether a lexer reached the end.
FORCE_INLINE Boolean IsLexerEndOfFile(Lex *p)
{
	return (p->pos >= p->lim || *p->pos == '\0') && !p->hasBufferedToken;
}

/// Test whether a lexer encountered an error.
FORCE_INLINE Boolean HasLexerError(Lex *p)
{
	return p->hasError;
}

/**
 * \brief Read and return next token.
 *
 * \param [in,out] p    A lexer, must not be `NULL`
 * \param [out]    dest Storage for the returned token, must not be `NULL`
 *
 * \return If a new token has been read, then `tok->text` is returned,
 *         `NULL` is returned if a parsing error has been encountered,
 *         or no more tokens are available.
 */
char *Lex_ReadToken(Lex *p, Tok *dest);
/**
 * \brief Read and return next token in the same line.
 *
 * This is a variant of `Lex_ReadToken()` useful to implement
 * a C Preprocessor, it avoids parsing spanning more than one line.
 * `\` followed by a newline is recognized and treated as a regular
 * space.
 */
char *Lex_ReadTokenOnLine(Lex *p, Tok *dest);
/**
 * \brief Expects an integral token, reading and returning its value.
 *
 * \param [in,out] p            A lexer, must not be `NULL`
 * \param [in]     optionalSign Allow an optional `+` or `-` sign before the
 *                              token, if set to `FALSE` only unsigned
 *                              integers are allowed.
 *
 * \return The token value, 0 on error, use `HasLexerError()` to distinguish
 *         between actual 0 and error value.
 */
long long Lex_ParseInt(Lex *p, Boolean optionalSign);
/**
 * \brief Expects a boolean token, reading and returning its value.
 *
 * \param [in,out] p            A lexer, must not be `NULL`
 * \param [in]     allowNumeric Convert numeric values to booleans, 0 for
 *                              `FALSE`, any other numeric value for `TRUE`
 *
 * \return The boolean value, `FALSE` on error or end of file,
 *         use `HasLexerError()` or `IsLexerEndOfFile()` to distinguish
 *         between actual `FALSE` and error value.
 */
Boolean Lex_ParseBool(Lex *p, Boolean allowNumeric);
/**
 * \brief Expects a floating point token, reading and returning its value.
 *
 * \param [in,out] p            A lexer, must not be `NULL`
 * \param [in]     optionalSign Allow an optional `+` or `-` sign before the
 *                              token, if set to `FALSE` only non-negative
 *                              values are allowed.
 *
 * \return The float value, 0 on error or end of file,
 *         use `HasLexerError()` or `IsLexerEndOfFile()` to distinguish
 *         between actual 0 and error value.
 */
double Lex_ParseFloat(Lex *p, Boolean optionalSign);

/**
 * \brief Read a one dimensional matrix (vector of length `n`) from `p` into `dest`.
 *
 * Matrix format is:
 * ```
 * (x y z w ...)
 * ```
 *
 * \return `TRUE` on success, `FALSE` on error.
 */
Boolean Lex_ParseMatrix1(Lex *p, float *dest, size_t n);
/**
 * \brief `Lex_ParseMatrix1()` variant for two dimensional matrixes.
 *
 * Matrix format is:
 * ```
 * ((x0 y0 z0 w0 ...) (x1 y1 z1 w1 ...) ...)
 * ```
 */
Boolean Lex_ParseMatrix2(Lex *p, float *dest, size_t n, size_t m);
/// `Lex_ParseMatrix1()` variant for tridimensional matrixes.
Boolean Lex_ParseMatrix3(Lex *p, float *dest, size_t n, size_t m, size_t u);

/// Discard any buffered token and any in text token up to a new line.
void Lex_SkipLine(Lex *p);
/**
 * \brief Skip every token until `tok` is encountered.
 *
 * \param [in,out] p   A lexer, must not be `NULL`
 * \param [in]     tok Token to look for, must not be `NULL`
 *
 * \return `tok` on success, `NULL` on error or end of file.
 */
char *Lex_SkipUntil(Lex *p, const char *tok);
/**
 * \brief Expect and skip section enclosed within braces.
 *
 * Braced sections are enclosed by punctuation tokens of id `P_BRACEOPEN` and
 * `P_BRACECLOSE`.
 *
 * \param [in,out] p               A lexer, must not be `NULL`
 * \param [in]     parseFirstBrace Whether the function should expect the next
 *                                 token to be the first brace of the section
 *                                 (`TRUE`) or it should assume the first brace
 *                                 has already been parsed (`FALSE`).
 *
 * \return `TRUE` if section was skipped successfully, `FALSE` on error
 *         (either unbalanced braces or unexpected token).
 */
Boolean Lex_SkipBracedSection(Lex *p, Boolean parseFirstBrace);

/**
 * \brief Expect a token, matching and returning it, raises error on mismatch.
 *
 * \param [in,out] p   A lexer, must not be `NULL`
 * \param [in]     tok Token to be expected, must not be `NULL`
 *
 * \return On success `tok` is returned, on error `NULL`.
 */
char *Lex_MatchToken(Lex *p, const char *tok);
/**
 * \brief Expect any token, raises an error if none is found.
 *
 * \param [in,out] p    A lexer, must not be `NULL`
 * \param [out]    dest Storage for returned token, must not be `NULL`
 *
 * \return On success `tok->text` is returned, on error `NULL`.
 */
char *Lex_MatchAnyToken(Lex *p, Tok *dest);
/**
 * \brief Expect a token of a specific `type` and `subtype`, raise error on mismatch.
 *
 * \param [in,out] p       A lexer, must not be `NULL`
 * \param [out]    dest    Storage for returned token, must not be `NULL`
 * \param [in]     type    Token type to be expected
 * \param [in]     subtype Subtype mask for the expected token
 *
 * \return On success `tok->text`, `NULL` otherwise.
 */
char *Lex_MatchTokenType(Lex *p, Tok *dest, int type, unsigned subtype);

/**
 * Check whether next token matches `tok`.
 *
 * If token matches it is read from `p` and returned, as in `Lex_ReadToken()`,
 * otherwise `p` is left unaltered (except for parsing errors).
 */
char *Lex_CheckToken(Lex *p, const char *tok);
/// Similar to `Lex_CheckToken()`, but matches by token `type` and `subtype`.
char *Lex_CheckTokenType(Lex *p, Tok *dest, int type, unsigned subtype);

/**
 * \brief Peek next token from `p` and test whether it matches with `tok`.
 *
 * In no case next token is consumed from `p`, lexer is left unaltered
 * (except for parsing errors).
 */
char *Lex_PeekToken(Lex *p, const char *tok);
/// Similar to `Lex_PeekToken()`, but matches by token `type` and `subtype`.
char *Lex_PeekTokenType(Lex *p, Tok *dest, int type, unsigned subtype);

/**
 * \brief Place a token back into the lexer.
 *
 * Only one token may be placed back into the lexer at a time,
 * it will be returned back on the next call to `Lex_ReadToken()`.
 */
void Lex_UngetToken(Lex *p, const Tok *tok);

#endif
