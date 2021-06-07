// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file mem.c
 *
 * `MemOps` interface using `malloc()`, `realloc()` and `free()`.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "mem.h"

#include <stdlib.h>

static void *Mem_Alloc(void *allocp, size_t nbytes, void *oldp)
{
	USED(allocp);

	return realloc(oldp, nbytes);
}

static void Mem_Free(void *allocp, void *ptr)
{
	USED(allocp);

	free(ptr);
}

static const MemOps mem_stdTable = {
    Mem_Alloc,
    Mem_Free
};

const MemOps *const Mem_StdOps = &mem_stdTable;
