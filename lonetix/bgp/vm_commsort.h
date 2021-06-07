// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm_commsort.h
 *
 * Generic basic sorting and binary searching over unsigned integer arrays.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * The following defines a bunch of static functions to sort
 * and search basic integer arrays.
 *
 * `#define` `UINT_TYPE` with an unsigned <= 4 bytes and `FNSUFFIX`
 * before inclusion.
 *
 * \note No guards, file `#include`d by bgp/vm_communities.c
 */

#define _CAT(X, Y)  X ## Y
#define _XCAT(X, Y) _CAT(X, Y)

#define _MANGLE(FN) _XCAT(FN, FNSUFFIX)


static Sint64 _MANGLE(BinarySearch) (const UINT_TYPE *arr,
                                     Uint32           n,
                                     UINT_TYPE        v)
{
	Uint32 len = n;
	Uint32 mid = n;
	Sint64 off = 0;
	while (mid > 0) {
		mid = len >> 1;
		if (arr[off+mid] <= v)
			off += mid;

		len -= mid;
	}

	return (off < n && arr[off] == v) ? off : -1;
}

static void _MANGLE(Radix) (int              off,
                            const UINT_TYPE *src,
                            Uint32           n,
                            UINT_TYPE       *dest)
{
	const Uint8 *sortKey;

	Uint32 index[256];
	Uint32 count[256] = { 0 };

	for (Uint32 i = 0; i < n; i++) {
		sortKey = ((const Uint8 *) &src[i]) + off;
		count[*sortKey]++;
	}

	index[0] = 0;
	for (Uint32 i = 1; i < 256; i++)
		index[i] = index[i-1] + count[i-1];

	for (Uint32 i = 0; i < n; i++) {
		sortKey = ((const Uint8 *) &src[i]) + off;
		dest[index[*sortKey]++] = src[i];
	}
}

static void _MANGLE(RadixSort) (UINT_TYPE *arr, Uint32 n)
{
	UINT_TYPE *scratch = (UINT_TYPE *) alloca(n * sizeof(*scratch));

	STATIC_ASSERT(sizeof(UINT_TYPE) % 2 == 0, "?!");

	if (EDN_NATIVE == EDN_LE) {
		for (unsigned i = 0; i < sizeof(UINT_TYPE); i += 2) {
			_MANGLE(Radix) (i + 0, arr,     n, scratch);
			_MANGLE(Radix) (i + 1, scratch, n, arr);
		}
	} else {
		for (unsigned i = sizeof(UINT_TYPE); i > 0; i -= 2) {
			_MANGLE(Radix) (i - 1, arr,     n, scratch);
			_MANGLE(Radix) (i - 2, scratch, n, arr);
		}
	}
}

static Uint32 _MANGLE(Uniq) (UINT_TYPE *arr, Uint32 n)
{
	Uint32 i, j;

	if (n == 0) return 0;

	for (i = 0, j = 1; j < n; j++) {
		if (arr[i] != arr[j])
			arr[++i] = arr[j];
	}
	return ++i;
}

#undef _MANGLE
#undef _XCAT
#undef _CAT
