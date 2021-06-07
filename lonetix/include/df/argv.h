// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file argv.h
 *
 * Command line argument parsing library.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_ARGV_H_
#define DF_ARGV_H_

#include "utf/utfdef.h"

#ifdef _WIN32
// DOS-style options are preferred
#define OPTCHAR '/'
#define OPTSEP  ':'
#else
// Unix-style options
#define OPTCHAR '-'
#define OPTSEP  '='
#endif

typedef enum {
	ARG_NONE, ///< `Optflag` takes no argument
	ARG_REQ,  ///< `Optflag` with mandatory argument
	ARG_OPT   ///< `Optflag` may take an optional argument
} Optarg;

typedef struct {
	Rune        opt;      ///< Set to `\0` to signal `Optflag` list end
	const char *longopt;  ///< Optional long name for this option
	const char *argName;  ///< Human readable descriptive argument name, ignored if `hasArg == ARG_NONE`
	const char *descr;    ///< Human readable description for option, displayed in help message
	Optarg      hasArg;   ///< Whether an argument to this `Optflag` is mandatory, optional or prohibited

	// Following fields are altered by `Com_ArgParse()` when flag is
	// encountered.
	// If option doesn't appear inside `argv`, then following fields are left
	// untouched (thus using them to provide default values is legal)

	Boolean flagged;  ///< Set to `TRUE` if option was found in command line
	char   *optarg;   ///< Set to a pointer inside `argv` when argument for this option is found
} Optflag;

extern const char *com_progName;    ///< If set to non-NULL string, it is used to provide a program name during `Com_ArgParse()`, instead of `argv[0]`
extern const char *com_synopsis;    ///< If set to non-NULL string, provides a short synopsis for `Com_ArgParse()` usage message
extern const char *com_shortDescr;  ///< If set to non-NULL string, provides a brief command summary for `Com_ArgParse()` usage message
extern const char *com_longDescr;   ///< If set to non-NULL string, provides a long description for `Com_ArgParse()` usage message

/// Do not emit error messages or help message automatically to `stderr`.
#define ARG_QUIET   BIT(0)
/// Do not perform GNU-like command flags reordering.
#define ARG_NOREORD BIT(1)

/// The provided `Optflag` list is ill-formed (e.g. option with `opt == '-' && longopt == NULL` was found)
#define OPT_BADLIST   -1
/// Missing required option argument
#define OPT_ARGMISS   -2
/// Excess argument inside long option that requires none (e.g. `--foo=bar`, but `foo->hasArg == ARG_NONE`)
#define OPT_EXCESSARG -3
/// Encountered unknown option
#define OPT_UNKNOWN   -4
/// Parsing terminated because help option was requested (-h, -?, --help or platform specific equivalents)
#define OPT_HELP      -5
/// Command line is ambiguous
#define OPT_BADARGV   -6

/**
 * Parse command line arguments, updating relevant option flags.
 *
 * \param [in]     argc    Argument count
 * \param [in]     argv    Argument vector`
 * \param [in,out] options Option flag list
 * \param [in]     flags   Parsing flags (`ARG_*` bit mask)
 *
 * \return Number of argument parsed or an appropriate error value:
 *         * On success the number of parsed arguments is returned (>= 0)
 *         * If the help option was requested then `OPT_HELP` is returned
 *         * If argument list is ill-formed, an error code is returned (`OPT_*`)
 */
int Com_ArgParse(int argc, char **argv, Optflag *options, unsigned flags);

#endif
