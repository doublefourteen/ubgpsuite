// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file argv.c
 *
 * Portable command line argument parsing implementation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "argv.h"

#include "sys/con.h"
#include "utf/utf.h"

#include <assert.h>
#include <stdlib.h>  // for getenv() on __GNUC__
#include <string.h>

const char *com_progName   = NULL;
const char *com_synopsis   = NULL;
const char *com_shortDescr = NULL;
const char *com_longDescr  = NULL;

#define OPT_ISLONLY(flag)    ((flag)->opt == '-')
#define OPT_HASLONGNAM(flag) ((flag)->longopt != NULL)
#define OPT_ARGNAME(flag)    (((flag)->argName) ? (flag)->argName : "arg")

static void PrintHelpMessage(const char *prog, const Optflag *options)
{
	if (!prog)
		return;  // quiet parsing

	Sys_Printf(STDOUT, "Usage: %s", prog);
	if (com_synopsis)
		Sys_Printf(STDOUT, " %s", com_synopsis);

	Sys_Print(STDOUT, "\n");
	if (com_shortDescr)
		Sys_Printf(STDOUT, "%s\n", com_shortDescr);

	if (com_longDescr)
		Sys_Printf(STDOUT, "\n%s\n\n", com_longDescr);

	char buf[MAXUTF + 1];
	size_t n;

	for (const Optflag *flag = options; flag->opt != '\0'; flag++) {
		Sys_Print(STDOUT, "  ");

		// Single char option name
		if (OPT_ISLONLY(flag)) {
			// Long option only, leave 2 chars for alignment purposes
			assert(flag->longopt);
			Sys_Print(STDOUT, "  ");

		} else {
			// Write down single-char option first
			n = runetochar(buf, flag->opt);
			buf[n] = '\0';

			Sys_Printf(STDOUT, "-%s", buf);
		}

		// Long name and (optional) argument
		if (flag->longopt) {
			// Write comma if necessary, or leave 2 spaces for alignment
			Sys_Print(STDOUT, OPT_ISLONLY(flag) ? "  " : ", ");
			Sys_Printf(STDOUT, "--%s", flag->longopt);

			// Append argument with a leading = sign
			switch (flag->hasArg) {
			default:       assert(FALSE); break;
			case ARG_NONE: break;
			case ARG_OPT:  Sys_Printf(STDOUT, "[=%s]", OPT_ARGNAME(flag)); break;
			case ARG_REQ:  Sys_Printf(STDOUT, "=%s", OPT_ARGNAME(flag));   break;
			}
		} else {
			// Output argument, short options have no leading =
			switch (flag->hasArg) {
			default:       assert(FALSE); break;
			case ARG_NONE: break;
			case ARG_OPT:  Sys_Printf(STDOUT, " [%s]", OPT_ARGNAME(flag)); break;
			case ARG_REQ:  Sys_Printf(STDOUT, " <%s>", OPT_ARGNAME(flag)); break;
			}
		}
		if (flag->descr)
			Sys_Printf(STDOUT, "\t%s", flag->descr);

		Sys_Print(STDOUT, "\n");
	}

	// Append help options
	Sys_Print(STDOUT, "  -h, --help\tPrint this help message\n");
	Sys_Print(STDOUT, "  -?\tEquivalent to -h\n");
}

// If `prog` is not NULL, output an excess argument error (only called for long options).
static void PrintExcessArgError(const char    *prog,
                                const Optflag *flag,
                                const char    *arg)
{
	if (!prog)
		return;  // quiet parse

	assert(flag->longopt);

	Sys_Printf(STDERR, "%s: Option --%s takes no argument: --%s=%s", prog, flag->longopt, flag->longopt, arg);
}

static void PrintMissingArgError(const char *prog, const Optflag *flag, Boolean longName)
{
	if (!prog)
		return;  // quiet parse

	char buf[MAXUTF+1];
	const char *nam = NULL;
	const char *pfx = (longName) ? "--" : "-";

	if (longName)
		nam = flag->longopt;

	else {
		size_t n = runetochar(buf, flag->opt);
		buf[n] = '\0';

		nam = buf;
	}

	Sys_Printf(STDERR, "%s: Option", prog);
	if (nam)
		Sys_Printf(STDERR, " %s%s", pfx, nam);

	Sys_Print(STDERR, " requires mandatory argument");
	if (flag->argName)
		Sys_Printf(STDERR, " <%s>", flag->argName);

	Sys_Printf(STDERR, ": %s%s\n", pfx, nam);
}

CHECK_PRINTF(2, 0) static void PrintBadCmdLineError(const char *prog,
                                                    const char *fmt,
                                                    ...)
{
	if (!prog)
		return;  // quiet parsing

	va_list va;

	va_start(va, fmt);
	Sys_Printf(STDERR, "%s: ", prog);
	Sys_VPrintf(STDERR, fmt, va);
	Sys_Print(STDERR, "\n");
	va_end(va);
}

static Optflag *FindLongFlag(const char *p, char **optarg, const char *prog, Optflag *options)
{
	char *arg = strchr(p, '=');

	*optarg = arg;

	char *name;
	if (arg) {
		size_t n = arg - p;
		name     = (char *) alloca(n + 1);

		memcpy(name, p, n);
		name[n] = '\0';
	} else
		name = (char *) p;  // safe

	for (Optflag *flag = options; flag->opt != '\0'; flag++) {
		if (OPT_HASLONGNAM(flag) && strcmp(flag->longopt, name) == 0) {
			flag->flagged = TRUE;
			return flag;
		}
	}

	if (prog)
		Sys_Printf(STDERR, "%s: Unrecognized option: --%s\n", prog, name);

	return NULL;
}

static Optflag *FindFlag(Rune r, const char *prog, Optflag *flags)
{
	for (Optflag *flag = flags; flag->opt != '\0'; flag++) {
		// NOTE: options with long names are skipped (-- is end of argument list)
		if (flag->opt == r) {
			flag->flagged = TRUE;
			return flag;
		}
	}

	if (prog) {
		char buf[MAXUTF + 1];

		size_t n = runetochar(buf, r);
		buf[n] = '\0';

		Sys_Printf(STDERR, "%s: Unrecognized option: -%s\n", prog, buf);
	}
	return NULL;
}

#ifdef __GNUC__
static void ReorderArgv(int argc, char **argv, const Optflag *options)
{
	if (getenv("POSIXLY_CORRECT"))
		return;  // don't mess with argv is POSIX behavior is requested

	USED(argc); USED(argv); USED(options);
	// TODO
}

#endif

int Com_ArgParse(int argc, char **argv, Optflag *options, unsigned flags)
{
	Optflag *flag;

	char *p, *optarg;

	// Initial setup
	char *prog = NULL;  // FindFlag*() functions won't log on NULL `prog`
	if ((flags & ARG_QUIET) == 0) {
		// Extract program name, so parsing outputs meaningful messages
		prog = (char *) com_progName;
		if (!prog) {
			// Generate from argv[0]
			prog = argv[0];  // TODO: basename(argv[0]);
		}
	}

#ifdef __GNUC__
	/* GNU allows program options in any order with respect to program
	 * arguments, for example:
	 * ```c
	 *   program file -cv
	 * ```
	 * According to POSIX both `file` and `-cv` are program arguments.
	 * According to GNU `-cv` are options, `file` is a program argument.
	 *
	 * To do this GNU getopt() implicitly reorders `argv`.
	 */
	if ((flags & ARG_NOREORD) == 0)
		ReorderArgv(argc, argv, options);
#endif

	// Parse argument list
	int optind;
	for (optind = 1; optind < argc; optind++) {
		p = argv[optind];
		if (strcmp(p, "--") == 0) {
			optind++;
			break;  // explicit end of command list
		}

		if (p[0] == '-' && p[1] == '-') {
			// GNU style long option
			p += 2;
			if (strcmp(p, "help") == 0) {
				PrintHelpMessage(prog, options);
				return OPT_HELP;
			}

			flag = FindLongFlag(p, &optarg, prog, options);
			if (!flag)
				return OPT_UNKNOWN;

			if (!optarg && flag->hasArg)
				optarg = argv[++optind];  // fetch argument from `argv`

			if (optarg && flag->hasArg == ARG_NONE) {
				PrintExcessArgError(prog, flag, optarg);
				return OPT_EXCESSARG;
			}
			if (!optarg && flag->hasArg == ARG_REQ) {
				PrintMissingArgError(prog, flag, /*longName=*/TRUE);
				return OPT_ARGMISS;
			}

			flag->optarg = optarg;

		} else if (p[0] == '-' && p[1] != '\0') {
			// Unix-style single char option
			p++;

			do {
				Rune r;
				p += chartorune(&r, p);
				if (r == '-') {
					/* This can happen in events like: -a-c
					 * where a->hasArg == ARG_NONE
					 *
					 * There is no way to fix this ambiguity safely:
					 * * if -c is an argument to -a then it is OPT_EXCESSARG error
					 * * if -a -- -c problematic because it's counterintuitive
					 *   for the user
					 *   (one could get surprising -c program arguments)
					 * * if we take it as -a --(force as option) -c it would be
					 *   dangerous (sometimes -- is an option, sometimes an
					 *   end of option list)
					 *
					 *   NOTE: getopt() appears to do the last, and it's
					 *         horrific
					 */
					PrintBadCmdLineError(prog, "Ambiguous '-' in short options list: %s", argv[optind]);
					return OPT_BADARGV;
				}

				// Handle single char help requests
				if (r == '?' || r == 'h') {
					PrintHelpMessage(prog, options);
					return OPT_HELP;
				}

				flag = FindFlag(r, prog, options);
				if (!flag)
					return OPT_UNKNOWN;

				optarg = NULL;
				if (flag->hasArg)
					optarg = (*p != '\0') ? p : argv[++optind];

				if (!optarg && flag->hasArg == ARG_REQ) {
					PrintMissingArgError(prog, flag, /*longName=*/FALSE);
					return OPT_ARGMISS;
				}

				flag->optarg = optarg;

			} while (*p != '\0');
		}
#if 0 && defined(_WIN32)  // TODO
		else if (p[0] == '/' && p[1] != '\0') {
			// DOS style option
		}
#endif
		else {
			// End of argument list, break the loop
			break;
		}
	}

	return optind;
}
