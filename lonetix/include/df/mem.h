// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file mem.h
 *
 * Common allocator interface.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_MEM_H_
#define DF_MEM_H_

#include "xpt.h"

/**
 * \brief Memory allocator interface.
 *
 * This is a common interface `struct` used by functions or types
 * that accomodate for custom memory allocation policies.
 *
 * \note The behavior described here for the `Alloc` and `Free` functions is
 *       a sensible contract that should be respected by most allocators.
 *       Still, special purpose allocators may tweak it to fit very
 *       specific situations, e.g. an allocator optimized for a specific use may
 *       ignore all calls to `Free`, or may interpret a `Free` of chunks never
 *       returned by `Alloc` as hints to grow its available memory pool,
 *       a sensitive allocator may choose to only return zeroed memory on
 *       new chunks and zero them out as soon as they're `Free`d.
 *       Such allocators should be restricted to special circumstances, while
 *       the behavior described here should provide the general rule.
 */
typedef struct {
	/**
	 * \brief Allocate or reallocate a memory chunk.
	 *
	 * The `Alloc` function takes an optional `allocp`, which may represent
	 * the allocator state. Some allocators don't require any state or
	 * provide a global one, in which case `allocp` may be `NULL`.
	 * The returned chunk is required to be at least `size` bytes large.
	 * The `oldp` argument is `NULL` for new allocations, but may also be
	 * a pointer to an existing chunk previously returned by the allocator,
	 * which is the case for shrink or grow requests. The allocator may
	 * enlarge or shrink the chunk referenced by `oldp` to reduce
	 * fragmentation.
	 * On shrink and grow requests the data up to the minimum value between
	 * `size` and the old chunk size is preserved 
	 *
	 * \param [in,out] allocp Allocator state pointer, `NULL` if allocator has no state
	 * \param [in]     size   Requested memory chunk size, in bytes
	 * \param [in,out] oldp   Old memory chunk pointer, `NULL` if a new allocation is
	 *                        requested
	 *
	 * \return One of the following:
	 *         * a pointer to a possibly uninitialized memory chunk on successful new
	 *           allocation,
	 *         * a pointer to the resized memory block, which may reside
	 *           in a different location rather than `oldp`, in the event of
	 *           a successful shrink or grow request,
	 *         * `NULL` on allocation failure.
	 */
	void *(*Alloc)(void *allocp, size_t size, void *oldp);
	/**
	 * \brief Free a memory chunk.
	 *
	 * The `Free` function takes an optional `allocp`, which may
	 * represent the allocator state, with the same semantics as `Alloc`,
	 * and a pointer previously returned by `Alloc`, and marks the chunk
	 * referenced by it as free for future allocations.
	 * Any allocator should silently ignore requests to free the `NULL`
	 * pointer.
	 *
	 * \param [in,out] allocp Allocator state pointer, `NULL` if allocator has no
	 *                        state
	 * \param [in]     p      Pointer to a memory chunk previously returned by `Alloc`,
	 *                        if `NULL` calling this function is a NOP.
	 */
	void  (*Free)(void *allocp, void *p);
} MemOps;

/// Plain `MemOps` using regular `realloc()` and `free()`, use `NULL` for `allocp`.
extern const MemOps *const Mem_StdOps;

#endif
