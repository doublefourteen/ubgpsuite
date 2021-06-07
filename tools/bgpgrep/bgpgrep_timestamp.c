// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * \file bgpgrep_timestamp.c
 *
 * Timestamp expressions parsing and compilation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgpgrep_local.h"

#include "numlib.h"
#include "strlib.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define SECSPERMIN  (60)
#define SECSPERHOUR (60 * SECSPERMIN)

#define FATAL(msg)       Bgpgrep_Fatal("%s: " msg, BgpgrepC_CurTerm())
#define FATALF(fmt, ...) Bgpgrep_Fatal("%s: " fmt, BgpgrepC_CurTerm(), __VA_ARGS__)

static time_t UtcTime(struct tm *t)
{
#ifdef _WIN32
	return _mkgmtime(t);
#else
	return timegm(t);
#endif
}

static Uint32 ParseFrac(const char *s, const char *p, char **ep)
{
	char *endp;
	NumConvRet res;

	unsigned long long n = Atoull(p, &endp, 10, &res);
	if (res != NCVENOERR)
		FATALF("%s: Expecting fractional portion of a second after '.'", s);

	int    ndigs = endp - p;
	double frac  = pow(10, ndigs);

	if (ep)
		*ep = endp;

	return (Uint32) trunc((n / frac) * 100000.0);
}

static Boolean ParseRfc3339(const char *s, Timestampop *dest)
{
	NumConvRet res;
	char *p, *ep;
	int yyyy, mm, dd, h, min, secs, microsecs;
	int timeOff;
	Boolean expectTimeOfDay;

	struct tm t;
	memset(&t, 0, sizeof(t));

	p = (char *) s;

	yyyy = Atou(p, &ep, 10, &res);
	if (res != NCVENOERR || (*ep != '-' && *ep != ':'))
		return FALSE;  // Invalid RFC-3339 format

	h = min = secs = microsecs = 0;
	timeOff = 0;
	expectTimeOfDay = FALSE;

	if (*ep == '-') {
		// YYYY-MM-DD
		if (ep - p != 4)
			FATALF("%s: Expecting 4-digit year number", s);

		p = ++ep;

		mm = Atou(p, &ep, 10, &res);
		if (res != NCVENOERR || ep - p != 2)
			FATALF("%s: Expecting 2-digit month field", s);
		if (*ep != '-')
			FATALF("%s: Missing day of month field", s);

		p = ++ep;

		dd = Atou(p, &ep, 10, &res);
		if (res != NCVENOERR || ep - p != 2)
			FATALF("%s: Expecting 2-digit day of month field", s);

		// Rescale values for struct tm
		yyyy -= 1900;
		mm   -= 1;

		if (*ep == 'T' || *ep == 't') {
			++ep;
			expectTimeOfDay = TRUE;
		}

		p = ep;
	} else {
		// What we've parsed is not YYYY, but actually HH,
		// Reset parser and expect to get time of day
		p = (char *) s;
		expectTimeOfDay = TRUE;

		// Setup YYYY-MM-DD from current date, in UTC
		const time_t     now = time(NULL);
		const struct tm *cur = gmtime(&now);
		if (!cur)  // PARANOID
			FATAL("gmtime() failed: Could not retrieve current date");

		yyyy = cur->tm_year;
		mm   = cur->tm_mon;
		dd   = cur->tm_mday;
	}
	if (expectTimeOfDay) {
		// HOUR:MIN[:SECS[.FRAC]]
		h = Atou(p, &ep, 10, &res);
		if (res != NCVENOERR || *ep != ':' || ep - p != 2)
			FATALF("%s: Expected 2-digits for hour field", s);

		p = ++ep;

		min = Atou(p, &ep, 10, &res);
		if (res != NCVENOERR || ep - p != 2)
			FATALF("%s: Expected 2-digits for minutes field", s);

		if (*ep == ':') {
			// SECS[.FRAC]
			p = ++ep;

			secs = Atou(p, &ep, 10, &res);
			if (res != NCVENOERR || ep - p != 2)
				FATALF("%s: Expected 2-digits for seconds field", s);

			if (*ep == '.') {
				// Fraction of a second
				p = ++ep;

				microsecs = ParseFrac(s, p, &ep);
			}
		}
	}
	if (*ep == '+' || *ep == '-') {
		// Time offset +/- HH:MM
		int hoursOff, minOff;
		int sign = (*ep == '+') ? 1 : -1;

		p = ++ep;

		hoursOff = Atou(p, &ep, 10, &res);
		if (res != NCVENOERR || *ep != ':' || ep - p != 2 || hoursOff < 0 || hoursOff > 23)
			FATALF("%s: Expected valid 2-digit hours field for timezone offset", s);

		p = ++ep;

		minOff = Atou(p, &ep, 10, &res);
		if (res != NCVENOERR || ep - p != 2 || minOff < 0 || minOff > 59)
			FATALF("%s: Expected valid 2-digit minute field for timezone offset", s);

		timeOff  = (hoursOff*SECSPERHOUR + minOff*SECSPERMIN);
		timeOff *= sign;

	} else if (*ep == 'Z' || *ep == 'z')
		++ep;  // explicit UTC marker

	if (*ep != '\0')
		FATALF("%s: Bad timestamp format", s);

	if (h == 24 && min == 0 && secs == 0 && microsecs == 0) {
		// 24:00:00 -> 00:00:00
		// we adjust time offset to add one day to epoch
		h = 0;

		timeOff -= 24*SECSPERHOUR;
	}

	// Convert to UTC time_t
	t.tm_year = yyyy;
	t.tm_mon  = mm;
	t.tm_mday = dd;
	t.tm_hour = h;
	t.tm_min  = min;
	t.tm_sec  = secs;

	time_t epoch = UtcTime(&t);

	// Do not accept renormalized dates
	if (t.tm_year != yyyy || t.tm_mon != mm  || t.tm_mday != dd ||
	    t.tm_hour != h    || t.tm_min != min || t.tm_sec  != secs)
		FATALF("%s: Invalid timestamp", s);

	// Adjust for time offset
	dest->secs      = epoch - timeOff;  // assume POSIX time_t semantics
	dest->microsecs = microsecs;
	return TRUE;
}

static Boolean ParseEpoch(const char *s, Timestampop *dest)
{
	char *p;
	NumConvRet res;

	dest->secs = Atoll(s, &p, 10, &res);
	if (res != NCVENOERR)
		return FALSE;

	if (*p == '.') {
		++p;

		dest->microsecs = ParseFrac(s, p, &p);
	} else
		dest->microsecs = 0;

	return *p == '\0';
}

Sint32 BgpgrepC_ParseTimestampExpression(void)
{
	const char *tok = BgpgrepC_ExpectAnyToken();

	Timestampop *stamp = (Timestampop *) Bgp_VmPermAlloc(&S.vm, sizeof(*stamp));
	if (!stamp)
		Bgpgrep_Fatal("Memory allocation failed, too many timestamp operations");

	// Optional operator
	stamp->opc = TIMESTAMP_EQ;
	if (strncmp(tok, "<=", 2) == 0) {
		stamp->opc = TIMESTAMP_LE;
		tok += 2;
	} else if (*tok == '<') {
		stamp->opc = TIMESTAMP_LT;
		tok++;
	} else if (*tok == '!') {
		stamp->opc = TIMESTAMP_NE;
		tok++;
	} else if (*tok == '=') {
		stamp->opc = TIMESTAMP_EQ;
		tok++;
	} else if (strncmp(tok, ">=", 2) == 0) {
		stamp->opc = TIMESTAMP_GE;
		tok += 2;
	} else if (*tok == '>') {
		stamp->opc = TIMESTAMP_GT;
		tok++;
	}

	// Timestamp value
	if (!ParseRfc3339(tok, stamp) && !ParseEpoch(tok, stamp))
		FATALF("Unrecognized timestamp format '%s'", tok);

	Sint32 kidx = BGP_VMSETKA(&S.vm, Bgp_VmNewk(&S.vm), stamp);
	if (kidx < 0)
		Bgpgrep_Fatal("Expression has too many timestamp operations");

	return kidx;
}
