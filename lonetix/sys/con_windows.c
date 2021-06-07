// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/con_windows.c
 *
 * Implement Windows console Input/Output.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/con.h"

#include <windows.h>

#include <stdio.h>

#error "Sorry, not fully implemented yet!"

Fildes CON_FILDES(ConHn hn)
{
	return GetStdHandle(hn);
}

void Sys_VPrintf(ConHn hn, const char *fmt, va_list va)
{
	HANDLE con = GetStdHandle(hn);
	if (!con)
		return;  // No console associated yet

	// FIXME implement
}

void Sys_Printf(ConHn hn, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	Sys_VPrintf(hn, fmt, va);
	va_end(va);
}
