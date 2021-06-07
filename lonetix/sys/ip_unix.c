// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file sys/ip_unix.c
 *
 * System specific Internet Protocol functionality implementation over Unix.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "sys/ip.h"

#include <arpa/inet.h>

Judgement Ipv4_StringToAdr(const char *s, Ipv4adr *dest)
{
	if (inet_pton(AF_INET, s, dest->bytes) != 1)
		return NG;

	return OK;
}

Judgement Ipv6_StringToAdr(const char *s, Ipv6adr *dest)
{
	if (inet_pton(AF_INET6, s, dest->bytes) != 1)
		return NG;

	return OK;
}
