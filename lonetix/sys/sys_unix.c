// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/sys_unix.c
 *
 * General system specific functionality implementation over Unix.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/sys_local.h"

#include <time.h>

void Sys_SleepMillis(Uint32 millis)
{
	struct timespec ts;

	ts.tv_sec  = millis / 1000;
	ts.tv_nsec = (millis % 1000) * 1000000uLL;

	int res;
	do res = nanosleep(&ts, &ts); while (res != 0);  // res != 0 -> EINTR
}
