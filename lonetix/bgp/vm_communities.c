// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm_communities.c
 *
 * BGP VM COMTCH, ACOMTC instructions and COMMUNITY index.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"

#include "bgp/vmintrin.h"
#include "sys/endian.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	BgpVmOpt opt;
	Uint32   hiOnlyCount;
	Uint32   loOnlyCount;
	Uint32   fullCount;
	Uint32   bitsetWords;
	Uint32  *bitset;
	Uint16  *hiOnly;  // parital match on community hi
	Uint16  *loOnly;  // partial match on community lo
	Uint32   full[];  // full matches on whole community codes
	// Uint32 bitset[];  <- order preserves alignment requirements
	// Uint16 hi[];
	// Uint16 lo[];
} Bgpcommidx;

FORCE_INLINE size_t BITSETWIDTH(const Bgpcommidx *idx)
{
	return idx->hiOnlyCount + idx->loOnlyCount + idx->fullCount;
}
FORCE_INLINE size_t BITSETLEN(size_t width)
{
	return (width >> 5) + ((width & 0x1f) != 0);
}
FORCE_INLINE size_t FULLBITIDX(const Bgpcommidx *idx, Uint32 i)
{
	USED(idx);
	return i;
}
FORCE_INLINE size_t HIBITIDX(const Bgpcommidx *idx, Uint32 i)
{
	return (size_t) idx->fullCount + i;
}
FORCE_INLINE size_t LOBITIDX(const Bgpcommidx *idx, Uint32 i)
{
	return (size_t) idx->fullCount + idx->hiOnlyCount + i;
}

FORCE_INLINE Boolean ISBITSET(const Uint32 *bitset, size_t idx)
{
	return (bitset[idx >> 5] & (1u << (idx & 0x1f))) != 0;
}

FORCE_INLINE void SETBIT(Uint32 *bitset, size_t idx)
{
	bitset[idx >> 5] |= (1u << (idx & 0x1f));
}

FORCE_INLINE void CLRBITSET(Uint32 *bitset, size_t len)
{
	memset(bitset, 0, len * sizeof(*bitset));
}


#ifdef __GNUC__
FORCE_INLINE unsigned FindFirstSet(Uint32 x)
{
	STATIC_ASSERT(sizeof(x) == sizeof(int), "__builtin_ffs() operates on int");
	return __builtin_ffs(x);
}
#else
FORCE_INLINE unsigned FindFirstSet(Uint32 x)
{
	if (x == 0) return 0;

	unsigned n = 0;

	if ((x & 0x0000ffffu) == 0) n += 16, x >>= 16;
	if ((x & 0x000000ffu) == 0) n +=  8, x >>=  8;
	if ((x & 0x0000000fu) == 0) n +=  4, x >>=  4;
	if ((x & 0x00000003u) == 0) n +=  2, x >>=  2;
	if ((x & 0x00000001u) == 0) n +=  1;

	return ++n;
}
#endif

static size_t FFZ(const Uint32 *bitset, size_t len)
{
	size_t i;

	assert(len > 0);

	len--;
	for (i = 0; i < len && bitset[i] == 0xffffffffu; i++);

	size_t n = i << 6;
	n += FindFirstSet(~bitset[i]) - 1;
	return n;
}

#define UINT_TYPE Uint16
#define FNSUFFIX  16
#include "bgp/vm_commsort.h"
#undef UINT_TYPE
#undef FNSUFFIX

#define UINT_TYPE Uint32
#define FNSUFFIX  32
#include "bgp/vm_commsort.h"
#undef UINT_TYPE
#undef FNSUFFIX

static Boolean MatchCommunity(const Bgpcomm *c, const Bgpcommidx *idx)
{
		return BinarySearch16(idx->hiOnly, idx->hiOnlyCount, c->hi) >= 0 ||
		       BinarySearch16(idx->loOnly, idx->loOnlyCount, c->lo) >= 0 ||
		       BinarySearch32(idx->full,   idx->fullCount, c->code) >= 0;
}

static void OptimizeComtch(Bgpcommidx *idx)
{
	// Remove every full match more specific than an existing partial match.

	// NOTE: Assumes arrays have been sorted and Uniq()d

	Uint32 i, j;
	Bgpcomm c;

	for (i = 0, j = 0; i < idx->fullCount; i++) {
		c.code = idx->full[i];
		if (BinarySearch16(idx->hiOnly, idx->hiOnlyCount, c.hi) >= 0 ||
		    BinarySearch16(idx->loOnly, idx->loOnlyCount, c.lo) >= 0)
			continue;

		idx->full[j++] = idx->full[i];
	}

	idx->fullCount = j;
}

static void OptimizeAcomtc(Bgpcommidx *idx)
{
	// Remove every partial match less specific than an existing full match

	// NOTE: Assumes arrays have been sorted and Uniq()d

	Uint32 i, j;

	// Mark redundant entries in bitset
	CLRBITSET(idx->bitset, idx->bitsetWords);
	for (i = 0; i < idx->fullCount; i++) {
		Sint64  pos;
		Bgpcomm c;

		c.code = idx->full[i];

		pos = BinarySearch16(idx->hiOnly, idx->hiOnlyCount, c.hi);
		if (pos >= 0)
			SETBIT(idx->bitset, HIBITIDX(idx, pos));

		pos = BinarySearch16(idx->loOnly, idx->loOnlyCount, c.lo);
		if (pos >= 0)
			SETBIT(idx->bitset, LOBITIDX(idx, pos));
	}

	// Remove redundant entries
	for (i = 0, j = 0; i < idx->hiOnlyCount; i++) {
		if (!ISBITSET(idx->bitset, HIBITIDX(idx, i)))
			idx->hiOnly[j++] = idx->hiOnly[i];
	}
	idx->hiOnlyCount = j;

	for (i = 0, j = 0; i < idx->loOnlyCount; i++) {
		if (!ISBITSET(idx->bitset, LOBITIDX(idx, i)))
			idx->loOnly[j++] = idx->loOnly[i];
	}
	idx->loOnlyCount = j;
}

static void CompactIndex(Bgpvm *vm, Bgpcommidx *idx, size_t idxSize)
{
	size_t offset    = offsetof(Bgpcommidx, full[idx->fullCount]);
	size_t bitsetSiz = idx->bitsetWords * sizeof(*idx->bitset);
	size_t hiSiz     = idx->hiOnlyCount * sizeof(*idx->hiOnly);
	size_t loSiz     = idx->loOnlyCount * sizeof(*idx->loOnly);

	Uint8 *ptr = (Uint8 *) idx + offset;

	idx->bitset = (Uint32 *) memmove(ptr, idx->bitset, bitsetSiz);
	ptr += bitsetSiz;
	idx->hiOnly = (Uint16 *) memmove(ptr, idx->hiOnly, hiSiz);
	ptr += hiSiz;
	idx->loOnly = (Uint16 *) memmove(ptr, idx->loOnly, loSiz);
	ptr += loSiz;

	size_t siz = ptr - (Uint8 *) idx;

	siz    = ALIGN(siz, ALIGNMENT);
	offset = idxSize - siz;

	vm->hLowMark -= offset;
}

void *Bgp_VmCompileCommunityMatch(Bgpvm             *vm,
                                  const Bgpmatchcomm *match,
                                  size_t              n,
                                  BgpVmOpt            opt)
{
	// NOTE: Bgp_VmPermAlloc() already clears VM error and asserts !vm->isRunning

	Sint32 nlow = 0, nhigh = 0, nfull = 0, nbitswords = 0;

	for (size_t i = 0; i < n; i++) {
		const Bgpmatchcomm *m = &match[i];
		if (m->maskLo && m->maskHi) {
			vm->errCode = BGPEVMBADCOMTCH;
			return NULL;
		}

		if (m->maskLo)
			nhigh++;
		else if (m->maskHi)
			nlow++;
		else
			nfull++;
	}

	Bgpcommidx *idx;

	size_t offBits = offsetof(Bgpcommidx, full[nfull]);
	if (opt != BGP_VMOPT_ASSUME_COMTCH)
		nbitswords = BITSETLEN(nlow + nhigh + nfull);  // must allocate bitset

	size_t offHigh = offBits + nbitswords * sizeof(*idx->bitset);
	size_t offLow  = offHigh + nhigh * sizeof(*idx->hiOnly);
	size_t nbytes  = offLow  + nlow * sizeof(*idx->loOnly);

	nbytes = ALIGN(nbytes, ALIGNMENT);

	idx = Bgp_VmPermAlloc(vm, nbytes);
	if (!idx)
		return NULL;

	idx->bitset = (Uint32 *) ((Uint8 *) idx + offBits);
	idx->hiOnly = (Uint16 *) ((Uint8 *) idx + offHigh);
	idx->loOnly = (Uint16 *) ((Uint8 *) idx + offLow);

	idx->opt         = opt;
	idx->bitsetWords = nbitswords;
	idx->hiOnlyCount = idx->loOnlyCount = idx->fullCount = 0;
	for (size_t i = 0; i < n; i++) {
		const Bgpmatchcomm *m = &match[i];
		if (m->maskLo)
			idx->hiOnly[idx->hiOnlyCount++] = m->c.hi;
		else if (m->maskHi)
			idx->loOnly[idx->loOnlyCount++] = m->c.lo;
		else
			idx->full[idx->fullCount++]     = m->c.code;
	}

	// Sort lookup arrays
	RadixSort16(idx->hiOnly, idx->hiOnlyCount);
	RadixSort16(idx->loOnly, idx->loOnlyCount);
	RadixSort32(idx->full,   idx->fullCount);

	// Optimize tables
	idx->hiOnlyCount = Uniq16(idx->hiOnly, idx->hiOnlyCount);
	idx->loOnlyCount = Uniq16(idx->loOnly, idx->loOnlyCount);
	idx->fullCount   = Uniq32(idx->full,   idx->fullCount);

	// Discard redundant entries
	switch (opt) {
	case BGP_VMOPT_ASSUME_COMTCH: OptimizeComtch(idx); break;
	case BGP_VMOPT_ASSUME_ACOMTC: OptimizeAcomtc(idx); break;

	default:
	case BGP_VMOPT_NONE:
		break;
	}

	// Free-up excess memory after optimization
	CompactIndex(vm, idx, nbytes);
	return idx;
}

static Bgpattr *Bgp_VmDoComSetup(Bgpvm *vm, Bgpcommiter *it, BgpAttrCode code)
{
	Bgpupdate *update = (Bgpupdate *) BGP_VMCHKMSGTYPE(vm, BGP_UPDATE);
	if (!update) {
		Bgp_VmStoreMsgTypeMatch(vm, /*isMatching=*/FALSE);
		return NULL;
	}

	Bgpattrseg *tpa = Bgp_GetUpdateAttributes(update);
	if (!tpa) {
		vm->errCode = BGPEVMMSGERR;
		return NULL;
	}

	Bgpattr *attr = Bgp_GetUpdateAttribute(tpa, code, vm->msg->table);
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return NULL;
	}
	if (attr)
		Bgp_StartCommunity(it, attr);

	return attr;
}

void Bgp_VmDoComtch(Bgpvm *vm)
{
	if (!BGP_VMCHKSTKSIZ(vm, 1))
		return;

	Bgpcommidx *idx = (Bgpcommidx *) BGP_VMPOPA(vm);
	if (!idx || idx->opt == BGP_VMOPT_ASSUME_ACOMTC) {
		vm->errCode = BGPEVMBADCOMTCH; // TODO: BGPEVMBADCOMIDX;
		return;
	}

	Boolean isMatching = FALSE;  // unless found otherwise

	Bgpcommiter it;
	Bgpattr    *attr = Bgp_VmDoComSetup(vm, &it, BGP_ATTR_COMMUNITY);
	if (vm->errCode)
		return;
	if (!attr)
		goto done;

	Bgpcomm *c;
	while ((c = Bgp_NextCommunity(&it)) != NULL) {
		if (MatchCommunity(c, idx)) {
			isMatching = TRUE;
			break;
		}
	}
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

done:
	BGP_VMPUSH(vm, isMatching);
}

static void ScMatchCommunityAndSetBit(const Bgpcomm *c, Bgpcommidx *idx)
{
	Sint64 pos = BinarySearch16(idx->hiOnly, idx->hiOnlyCount, c->hi);
	if (pos >= 0) {
		SETBIT(idx->bitset, HIBITIDX(idx, pos));
		return;
	}

	pos = BinarySearch16(idx->loOnly, idx->loOnlyCount, c->lo);
	if (pos >= 0) {
		SETBIT(idx->bitset, LOBITIDX(idx, pos));
		return;
	}

	pos = BinarySearch32(idx->full, idx->fullCount, c->code);
	if (pos >= 0)
		SETBIT(idx->bitset, FULLBITIDX(idx, pos));
}

static void MatchCommunityAndSetBit(const Bgpcomm *c, Bgpcommidx *idx)
{
	Sint64 pos = BinarySearch16(idx->hiOnly, idx->hiOnlyCount, c->hi);
	if (pos >= 0)
		SETBIT(idx->bitset, HIBITIDX(idx, pos));

	pos = BinarySearch16(idx->loOnly, idx->loOnlyCount, c->lo);
	if (pos >= 0)
		SETBIT(idx->bitset, LOBITIDX(idx, pos));

	pos = BinarySearch32(idx->full, idx->fullCount, c->code);
	if (pos >= 0)
		SETBIT(idx->bitset, FULLBITIDX(idx, pos));
}

static Boolean Bgp_VmDoAcomtcFast(Bgpcommidx *idx, Bgpcommiter *it)
{
	Bgpcomm *c;
	while ((c = Bgp_NextCommunity(it)) != NULL)
		ScMatchCommunityAndSetBit(c, idx);

	return FFZ(idx->bitset, idx->bitsetWords) == BITSETWIDTH(idx);
}

static Boolean Bgp_VmDoAcomtcSlow(Bgpcommidx *idx, Bgpcommiter *it)
{
	Bgpcomm *c;
	while ((c = Bgp_NextCommunity(it)) != NULL)
		MatchCommunityAndSetBit(c, idx);

	return FFZ(idx->bitset, idx->bitsetWords) == BITSETWIDTH(idx);
}

void Bgp_VmDoAcomtc(Bgpvm *vm)
{
	if (!BGP_VMCHKSTKSIZ(vm, 1))
		return;

	Bgpcommidx *idx = (Bgpcommidx *) BGP_VMPOPA(vm);
	if (!idx || idx->opt == BGP_VMOPT_ASSUME_COMTCH) {
		vm->errCode = BGPEVMBADCOMTCH; // TODO: BGPEVMBADCOMIDX;
		return;
	}

	Boolean isMatching = FALSE;

	Bgpcommiter it;
	Bgpattr    *attr = Bgp_VmDoComSetup(vm, &it, BGP_ATTR_COMMUNITY);
	if (vm->errCode)
		return;
	if (!attr)
		goto done;

	CLRBITSET(idx->bitset, idx->bitsetWords);
	if (idx->opt == BGP_VMOPT_ASSUME_ACOMTC)
		isMatching = Bgp_VmDoAcomtcFast(idx, &it);
	else
		isMatching = Bgp_VmDoAcomtcSlow(idx, &it);

	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

done:
	BGP_VMPUSH(vm, isMatching);
}
