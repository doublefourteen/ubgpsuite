// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/parameters.c
 *
 * Deals with BGP OPEN parameters.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"

Judgement Bgp_StartMsgParms(Bgpparmiter *it, Bgpmsg *msg)
{
	const Bgpopen *open = Bgp_GetMsgOpen(msg);
	if (!open)
		return NG;  // error already set

	Bgp_StartParms(it, BGP_OPENPARMS(open));
	return OK;  // error already cleared
}

void Bgp_StartParms(Bgpparmiter *it, const Bgpparmseg *p)
{
	it->ptr  = (Uint8 *) p->parms;
	it->base = it->ptr;
	it->lim  = it->base + p->len;
}

Bgpparm *Bgp_NextParm(Bgpparmiter *it)
{
	if (it->ptr >= it->lim) {
		Bgp_SetErrStat(BGPENOERR);
		return NULL;
	}

	Bgpparm *p  = (Bgpparm *) it->ptr;
	size_t left = it->lim - it->ptr;
	if (left < 2uLL || left < 2uLL + p->len) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	it->ptr += 2 + p->len;
	Bgp_SetErrStat(BGPENOERR);
	return p;
}

Judgement Bgp_StartMsgCaps(Bgpcapiter *it, Bgpmsg *msg)
{
	if (Bgp_StartMsgParms(&it->pi, msg) != OK)
		return NG;  // error already set

	// Set starting point so Bgp_NextCap() scans for next parameter
	it->base = it->lim = it->ptr = it->pi.ptr;
	return OK;
}

void Bgp_StartCaps(Bgpcapiter *it, const Bgpparmseg *parms)
{
	Bgp_StartParms(&it->pi, parms);
	// Set starting point so Bgp_NextCap() scans for next parameter
	it->base = it->lim = it->ptr = it->pi.ptr;
}

Bgpcap *Bgp_NextCap(Bgpcapiter *it)
{
	if (it->ptr >= it->lim) {
		// Try to find another CAPABILITY parameter
		Bgpparm *p;

		while ((p = Bgp_NextParm(&it->pi)) != NULL) {
			if (p->code == BGP_PARM_CAPABILITY)
				break;
		}
		if (!p) {
			// Scanned all parameters, nothing found
			Bgp_SetErrStat(BGPENOERR);
			return NULL;
		}

		// Setup for reading new capabilities from `p`
		it->base = (Uint8 *) p + 2;
		it->lim  = it->base + p->len;
		it->ptr  = it->base;
	}

	// Return current capability and move over
	size_t  left = it->lim - it->ptr;
	Bgpcap *cap  = (Bgpcap *) it->ptr;
	if (left < 2uLL || left < 2uLL + cap->len) {
		Bgp_SetErrStat(BGPETRUNCMSG);
		return NULL;
	}

	it->ptr += 2 + cap->len;
	Bgp_SetErrStat(BGPENOERR);
	return cap;
}
