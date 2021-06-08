// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm.c
 *
 * BGP VM initialization and execution loop.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/bgp_local.h"
#include "bgp/patricia.h"
#include "bgp/vmintrin.h"
#include "sys/endian.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(DF_BGP_VM_NO_COMPUTED_GOTO)
#define DF_BGP_VM_USES_COMPUTED_GOTO
#include "bgp/vm_gccdef.h"
#else
#include "bgp/vm_cdef.h"
#endif

#define BGP_VM_MINHEAPSIZ (4 * 1024)
#define BGP_VM_STKSIZ     (4 * 1024)

#define BGP_VM_GROWPROGN  128

/* During VM execution instructions update the current BGP message
 * match. But sometimes the match is irrelevant (think about something
 * like:
 *
 * ```
 * LOADU 1
 * NOT
 * CPASS
 * ```
 *
 * This bytecode doesn't examine any actual BGP message segment,
 * to simplify instructions, whenever an irrelevant match is being produced,
 * the following static variable is referenced by `vm->curMatch`,
 * to provide a dummy match that can be updated at will
 * by any VM and is always discarded by `Bgp_VmStoreMatch()`.
 */
static Bgpvmmatch discardMatch;

Judgement Bgp_InitVm(Bgpvm *vm, size_t heapSiz)
{
	size_t siz = BGP_VM_STKSIZ + MAX(heapSiz, BGP_VM_MINHEAPSIZ);
	siz        = ALIGN(siz, ALIGNMENT);

	assert(siz <= 0xffffffffuLL);

	void *heap = malloc(siz);
	if (!heap)
		return Bgp_SetErrStat(BGPENOMEM);

	memset(vm, 0, sizeof(*vm));

	vm->heap      = heap;
	vm->hMemSiz   = siz;
	vm->hHighMark = siz;
	return Bgp_SetErrStat(BGPENOERR);
}

Judgement Bgp_VmEmit(Bgpvm *vm, Bgpvmbytec bytec)
{
	BGP_VMCLRERR(vm);

	if (BGP_VMOPC(bytec) == BGP_VMOP_END)
		return Bgp_SetErrStat(BGPENOERR);  // ignore useless emit

	if (vm->progLen + 1 >= vm->progCap) {
		// Grow the VM program segment
		size_t      newSiz  = vm->progCap + BGP_VM_GROWPROGN;
		Bgpvmbytec *newProg = (Bgpvmbytec *) realloc(vm->prog, newSiz * sizeof(*newProg));
		if (!newProg) {
			// Flag the VM as bad
			vm->setupFailed = TRUE;
			vm->errCode     = BGPENOMEM;

			return Bgp_SetErrStat(BGPENOMEM);
		}

		vm->prog    = newProg;
		vm->progCap = newSiz;
	}

	// Append instruction and follow it with BGP_VMOP_END
	vm->prog[vm->progLen++] = bytec;
	vm->prog[vm->progLen]   = BGP_VMOP_END;
	return Bgp_SetErrStat(BGPENOERR);
}

void *Bgp_VmPermAlloc(Bgpvm *vm, size_t size)
{
	BGP_VMCLRERR(vm);

	size = ALIGN(size, ALIGNMENT);

	if (vm->hLowMark + size > vm->hMemSiz) {
		// Flag the VM as bad
		vm->setupFailed = TRUE;
		vm->errCode     = BGPEVMOOM;

		Bgp_SetErrStat(BGPEVMOOM);
		return NULL;
	}

	void *ptr = (Uint8 *) vm->heap + vm->hLowMark;
	vm->hLowMark += size;

	Bgp_SetErrStat(BGPENOERR);
	return ptr;
}

void *Bgp_VmTempAlloc(Bgpvm *vm, size_t size)
{
	size = ALIGN(size, ALIGNMENT);

	size_t stksiz = vm->si * sizeof(Bgpvmval);
	if (vm->hLowMark + stksiz + size > vm->hHighMark) UNLIKELY {
		// NOTE: VM is being executed, don't set BGP error state
		//       it will be updated by Bgp_VmExec() as needed
		vm->errCode = BGPEVMOOM;
		return NULL;
	}

	assert(vm->hHighMark >= size);

	vm->hHighMark -= size;
	return (Uint8 *) vm->heap + vm->hHighMark;
}

void Bgp_VmTempFree(Bgpvm *vm, size_t size)
{
	size = ALIGN(size, ALIGNMENT);

	assert(size + vm->hHighMark <= vm->hMemSiz);
	vm->hHighMark += size;
}

Boolean Bgp_VmExec(Bgpvm *vm, Bgpmsg *msg)
{
	// Fundamental sanity checks
	if (vm->setupFailed) UNLIKELY {
		vm->errCode = BGPEBADVM;
		goto cant_run;
	}
	if (!vm->prog) UNLIKELY {
		vm->errCode = BGPEVMNOPROG;
		goto cant_run;
	}

	// Setup initial VM state
	Boolean result = TRUE;  // assume PASS unless CFAIL says otherwise

	vm->pc        = 0;
	vm->si        = 0;
	vm->nblk      = 0;
	vm->nmatches  = 0;
	vm->hHighMark = vm->hMemSiz;
	vm->msg       = msg;
	vm->curMatch  = &discardMatch;
	vm->matches   = NULL;
	BGP_VMCLRERR(vm);

	// Populate computed goto table if necessary
#ifdef DF_BGP_VM_USES_COMPUTED_GOTO
#include "bgp/vm_optab.h"
#endif

	// Execute bytecode according to the #included vm_<impl>def.h
	Bgpvmbytec ir;  // Instruction Register

	while (TRUE) {
		// FETCH stage
		FETCH(ir, vm);

		// DECODE-DISPATCH stage
		DISPATCH(BGP_VMOPC(ir)) {
		// EXECUTE stage
		EXECUTE(NOP): UNLIKELY;
			break;
		EXECUTE(LOAD):
			Bgp_VmDoLoad(vm, (Sint8) BGP_VMOPARG(ir));
			break;
		EXECUTE(LOADU):
			Bgp_VmDoLoadu(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(LOADN):
			Bgp_VmDoLoadn(vm);
			break;
		EXECUTE(LOADK):
			Bgp_VmDoLoadk(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(CALL):
			Bgp_VmDoCall(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(BLK):
			vm->nblk++;
			break;
		EXECUTE(ENDBLK):
			if (vm->nblk == 0) UNLIKELY {
				vm->errCode = BGPEVMBADENDBLK;
				goto terminate;
			}

			vm->nblk--;
			break;
		EXECUTE(TAG):
			Bgp_VmDoTag(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(NOT):
			Bgp_VmDoNot(vm);

			EXPECT(CFAIL, ir, vm);
			EXPECT(CPASS, ir, vm);
			break;
		EXECUTE(CFAIL):
			if (Bgp_VmDoCfail(vm)) {
				result = FALSE;  // immediate terminate on FAIL
				goto terminate;
			}

			break;
		EXECUTE(CPASS):
			if (Bgp_VmDoCpass(vm))
				goto terminate;  // immediate PASS

			break;
		EXECUTE(JZ):
			if (!BGP_VMCHKSTKSIZ(vm, 1)) UNLIKELY
				break;

			if (!BGP_VMPEEK(vm, -1)) {
				// Zero, do jump
				vm->pc += BGP_VMOPARG(ir);
				if (vm->pc > vm->progLen) UNLIKELY
					vm->errCode = BGPEVMBADJMP;  // jump target out of bounds

			} else
				BGP_VMPOP(vm);  // no jump, pop the stack and move on

			break;
		EXECUTE(JNZ):
			if (!BGP_VMCHKSTKSIZ(vm, 1)) UNLIKELY
				break;

			if (BGP_VMPEEK(vm, -1)) {
				// Non-Zero, do jump
				vm->pc += BGP_VMOPARG(ir);
				if (vm->pc > vm->progLen) UNLIKELY
					vm->errCode = BGPEVMBADJMP;  // jump target out of bounds

			} else
				BGP_VMPOP(vm);  // no jump, pop the stack and move on

			break;
		EXECUTE(CHKT):
			Bgp_VmDoChkt(vm, (BgpType) BGP_VMOPARG(ir));
			break;
		EXECUTE(CHKA):
			Bgp_VmDoChka(vm, (BgpAttrCode) BGP_VMOPARG(ir));
			break;
		EXECUTE(EXCT):
			Bgp_VmDoExct(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(SUPN):
			Bgp_VmDoSupn(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(SUBN):
			Bgp_VmDoSubn(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(RELT):
			Bgp_VmDoRelt(vm, BGP_VMOPARG(ir));
			break;
		EXECUTE(ASMTCH):
			Bgp_VmDoAsmtch(vm);
			break;
		EXECUTE(FASMTC):
			Bgp_VmDoFasmtc(vm);
			break;
		EXECUTE(COMTCH):
			Bgp_VmDoComtch(vm);
			break;
		EXECUTE(ACOMTC):
			Bgp_VmDoAcomtc(vm);
			break;
		EXECUTE(END): UNLIKELY;
			// Implicitly PASS current match
			vm->curMatch->isPassing = TRUE;
			goto terminate;
		EXECUTE_SIGILL: UNLIKELY;
			vm->errCode = BGPEVMILL;
			break;
		}

		if (vm->errCode) UNLIKELY
			goto terminate;  // error encountered, abort execution
	}

terminate:
	if (Bgp_SetErrStat(vm->errCode) != OK) UNLIKELY
		result = FALSE;

	return result;

cant_run:
	Bgp_SetErrStat(vm->errCode);
	return FALSE;
}

Judgement Bgp_VmStoreMsgTypeMatch(Bgpvm *vm, Boolean isMatching)
{
	BGP_VMPUSH(vm, isMatching);

	vm->curMatch = (Bgpvmmatch *) Bgp_VmTempAlloc(vm, sizeof(*vm->curMatch));
	if (!vm->curMatch) UNLIKELY
		return NG;

	Bgphdr *hdr = BGP_HDR(vm->msg);

	vm->curMatch->pc         = BGP_VMCURPC(vm);
	vm->curMatch->tag        = 0;
	vm->curMatch->base       = (Uint8 *) hdr;
	vm->curMatch->lim        = (Uint8 *) (hdr + 1);
	vm->curMatch->pos        = &hdr->type;
	vm->curMatch->isMatching = isMatching;

	Bgp_VmStoreMatch(vm);
	return OK;
}

void Bgp_VmStoreMatch(Bgpvm *vm)
{
	if (vm->curMatch == &discardMatch)
		return;  // discard store request

	// Prepend match to matches list, still keep the `curMatch` pointer
	// around in case result is updated by following instructions (e.g. NOT)
	vm->curMatch->nextMatch = vm->matches;
	vm->matches             = vm->curMatch;
	vm->nmatches++;
}

Boolean Bgp_VmDoCpass(Bgpvm *vm)
{
	/* POPS:
	 * -1: Last operation Boolean result (as a Sint64, 0 for FALSE)
	 *
	 * PUSHES:
	 * * On PASS result:
	 *   - TRUE
	 * * Otherwise:
	 *   - Nothing.
	 *
	 * SIDE-EFFECTS:
	 * - Breaks current BLK on PASS
	 * - Updates `curMatch->isPassing` flag accordingly
	 */

	if (!BGP_VMCHKSTKSIZ(vm, 1)) UNLIKELY
		return FALSE;  // error, let vm->errCode handle this

	Boolean shouldTerm = FALSE;  // unless proven otherwise

	// If stack top is non-zero we FAIL
	if (BGP_VMPEEK(vm, -1)) {
		// Leave TRUE on stack and break current BLK, this is a PASS
		vm->curMatch->isPassing = TRUE;

		if (vm->nblk > 0)
			Bgp_VmDoBreak(vm);
		else
			shouldTerm = TRUE;  // no more BLK

	} else {
		// Pop the stack and move on, no PASS
		vm->curMatch->isPassing = FALSE;
		BGP_VMPOP(vm);
	}

	// Current match information has been collected,
	// discard anything up to the next relevant operation
	vm->curMatch = &discardMatch;

	return shouldTerm;
}

Boolean Bgp_VmDoCfail(Bgpvm *vm)
{
	/* POPS:
	 * -1: Last operation Boolean result (as a Sint64, 0 for FALSE)
	 *
	 * PUSHES:
	 * * On FAIL result:
	 *   - FALSE
	 * * Otherwise:
	 *   - Nothing.
	 *
	 * SIDE-EFFECTS:
	 * - Breaks current BLK on FAIL
	 * - Updates `curMatch->isPassing` flag accordingly
	 */

	if (!BGP_VMCHKSTKSIZ(vm, 1)) UNLIKELY
		return FALSE;  // error, let vm->errCode handle this

	Boolean shouldTerm = FALSE;  // unless proven otherwise

	// If stack top is non-zero we FAIL
	Bgpvmval *v = BGP_VMSTKGET(vm, -1);
	if (v->val) {
		// Push FALSE and break current BLK, this is a FAIL
		vm->curMatch->isPassing = FALSE;

		v->val = FALSE;
		if (vm->nblk > 0)
			Bgp_VmDoBreak(vm);
		else
			shouldTerm = TRUE;  // no more BLK

	} else {
		// Pop the stack and move on, no FAIL
		vm->curMatch->isPassing = TRUE;
		BGP_VMPOP(vm);
	}

	// Current match information has been collected,
	// discard anything up to the next relevant operation
	vm->curMatch = &discardMatch;

	return shouldTerm;
}

void Bgp_VmDoChkt(Bgpvm *vm, BgpType type)
{
	/* PUSHES:
	 * TRUE if type matches, FALSE otherwise
	 */

	if (!BGP_VMCHKSTK(vm, 1)) UNLIKELY
		return;

	Boolean isMatching = (BGP_VMCHKMSGTYPE(vm, type) != NULL);
	Bgp_VmStoreMsgTypeMatch(vm, isMatching);
}

void Bgp_VmDoChka(Bgpvm *vm, BgpAttrCode code)
{
	/* PUSHES:
	 * TRUE if attribute exists inside UPDATE message, FALSE otherwise
	 */

	if (!BGP_VMCHKSTK(vm, 1)) UNLIKELY
		return;

	Bgpupdate *update = (Bgpupdate *) BGP_VMCHKMSGTYPE(vm, BGP_UPDATE);
	if (!update) {
		Bgp_VmStoreMsgTypeMatch(vm, /*isMatching=*/FALSE);
		return;
	}

	// Attribute lookup
	Bgpattrseg *tpa = Bgp_GetUpdateAttributes(update);
	if (!tpa) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

	Bgpattr *attr = Bgp_GetUpdateAttribute(tpa, code, vm->msg->table);
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

	Boolean isMatching = (attr != NULL);
	BGP_VMPUSH(vm, isMatching);

	// Create a new match
	vm->curMatch = (Bgpvmmatch *) Bgp_VmTempAlloc(vm, sizeof(*vm->curMatch));
	if (!vm->curMatch) UNLIKELY
		return;

	vm->curMatch->pc         = BGP_VMCURPC(vm);
	vm->curMatch->tag        = 0;
	vm->curMatch->base       = tpa->attrs;
	vm->curMatch->lim        = &tpa->attrs[beswap16(tpa->len)];
	vm->curMatch->pos        = attr;
	vm->curMatch->isMatching = isMatching;

	Bgp_VmStoreMatch(vm);
}

static Judgement Bgp_VmStartNets(Bgpvm *vm, Bgpmpiter *it, Uint8 mode)
{
	if (!BGP_VMCHKMSGTYPE(vm, BGP_UPDATE)) {
		Bgp_VmStoreMsgTypeMatch(vm, /*isMatching=*/FALSE);
		return NG;
	}

	switch (mode) {
	case BGP_VMOPA_NLRI:
		Bgp_StartMsgNlri(&it->rng, vm->msg);
		it->nextAttr = NULL;
		break;
	case BGP_VMOPA_WITHDRAWN:
		Bgp_StartMsgWithdrawn(&it->rng, vm->msg);
		it->nextAttr = NULL;
		break;
	case BGP_VMOPA_ALL_NLRI:
		Bgp_StartAllMsgNlri(it, vm->msg);
		break;
	case BGP_VMOPA_ALL_WITHDRAWN:
		Bgp_StartAllMsgWithdrawn(it, vm->msg);
		break;
	default: UNLIKELY;
		vm->errCode = BGPEVMBADOP;
		return NG;
	}
	if (Bgp_GetErrStat(NULL)) UNLIKELY {
		vm->errCode = BGPEVMMSGERR;
		return NG;
	}

	return OK;
}

static void Bgp_VmCollectNetMatch(Bgpvm *vm, Bgpmpiter *it)
{
	// Push on stack first --
	// we know we have at least one available spot on the stack for
	// any network operation (we POP at least one Patricia address from it).
	// Perform the temporary allocation afterwards.
	// This saves one check on stack space
	BGP_VMPUSH(vm, TRUE);

	vm->curMatch = (Bgpvmmatch *) Bgp_VmTempAlloc(vm, sizeof(*vm->curMatch));
	if (!vm->curMatch) UNLIKELY
		return;

	vm->curMatch->pc         = BGP_VMCURPC(vm);
	vm->curMatch->tag        = 0;
	vm->curMatch->base       = BGP_CURMPBASE(it);
	vm->curMatch->lim        = BGP_CURMPLIM(it);
	vm->curMatch->pos        = BGP_CURMPPFX(it);
	vm->curMatch->isMatching = TRUE;

	Bgp_VmStoreMatch(vm);
}

static void Bgp_VmNetMatchFailed(Bgpvm *vm)
{
	BGP_VMPUSH(vm, FALSE);  // NOTE: See `Bgp_VmCollectNetMatch()`

	vm->curMatch = (Bgpvmmatch *) Bgp_VmTempAlloc(vm, sizeof(*vm->curMatch));
	if (!vm->curMatch) UNLIKELY
		return;
	
	vm->curMatch->pc         = BGP_VMCURPC(vm);
	vm->curMatch->tag        = 0;
	vm->curMatch->base       = NULL;
	vm->curMatch->lim        = NULL;
	vm->curMatch->pos        = NULL;
	vm->curMatch->isMatching = FALSE;
	
	Bgp_VmStoreMatch(vm);
}

static const Patricia emptyTrie4 = { AFI_IP };
static const Patricia emptyTrie6 = { AFI_IP6 };

void Bgp_VmDoExct(Bgpvm *vm, Uint8 arg)
{
	// POPS:
	// -1: AFI_IP6 Patricia (may be NULL)
	// -2: AFI_IP  Patricia (may be NULL)
	//
	// PUSHES:
	// TRUE on EXACT match of any network prefix in message, FALSE otherwise

	if (!BGP_VMCHKSTKSIZ(vm, 2)) UNLIKELY
		return;

	Bgpmpiter it;
	const Patricia *trie6 = (Patricia *) BGP_VMPOPA(vm);
	const Patricia *trie  = (Patricia *) BGP_VMPOPA(vm);
	if (!trie6) trie6 = &emptyTrie6;
	if (!trie)  trie  = &emptyTrie4;

	if (trie->afi != AFI_IP || trie6->afi != AFI_IP6) UNLIKELY {
		vm->errCode = BGPEVMBADOP;
		return;
	}
	if (Bgp_VmStartNets(vm, &it, arg) != OK) UNLIKELY
		return;  // error already set

	Prefix *pfx;
	while ((pfx = Bgp_NextMpPrefix(&it)) != NULL) {
		RawPrefix *match = NULL;

		// Select appropriate PATRICIA
		switch (pfx->afi) {
		case AFI_IP:
			match = Pat_SearchExact(trie,  PLAINPFX(pfx));
			break;
		case AFI_IP6:
			match = Pat_SearchExact(trie6, PLAINPFX(pfx));
			break;

		default: UNREACHABLE; return;
		}

		if (match) {
			Bgp_VmCollectNetMatch(vm, &it);
			return;
		}
	}
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

	Bgp_VmNetMatchFailed(vm);
}

void Bgp_VmDoSubn(Bgpvm *vm, Uint8 arg)
{
	// POPS:
	// -1: AFI_IP6 Patricia (may be NULL)
	// -2: AFI_IP  Patricia (may be NULL)
	//
	// PUSHES:
	// TRUE on SUBN match of any network prefix in message, FALSE otherwise

	if (!BGP_VMCHKSTKSIZ(vm, 2)) UNLIKELY
		return;

	Bgpmpiter it;
	const Patricia *trie6 = (Patricia *) BGP_VMPOPA(vm);
	const Patricia *trie  = (Patricia *) BGP_VMPOPA(vm);
	if (!trie6) trie6 = &emptyTrie6;
	if (!trie)  trie  = &emptyTrie4;

	if (trie->afi != AFI_IP || trie6->afi != AFI_IP6 ) UNLIKELY {
		vm->errCode = BGPEVMBADOP;
		return;
	}
	if (Bgp_VmStartNets(vm, &it, arg) != OK) UNLIKELY
		return;  // error already set

	Prefix *pfx;
	while ((pfx = Bgp_NextMpPrefix(&it)) != NULL) {
		Boolean isMatching = FALSE;

		// Select appropriate PATRICIA
		switch (pfx->afi) {
		case AFI_IP:
			isMatching = Pat_IsSubnetOf(trie,  PLAINPFX(pfx));
			break;
		case AFI_IP6:
			isMatching = Pat_IsSubnetOf(trie6, PLAINPFX(pfx));
			break;

		default: UNREACHABLE; return;
		}

		if (isMatching) {
			Bgp_VmCollectNetMatch(vm, &it);
			return;
		}
	}
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

	Bgp_VmNetMatchFailed(vm);
}

void Bgp_VmDoSupn(Bgpvm *vm, Uint8 arg)
{
	// POPS:
	// -1: AFI_IP6 Patricia (may be NULL)
	// -2: AFI_IP  Patricia (may be NULL)
	//
	// PUSHES:
	// TRUE on SUPN match of any network prefix in message, FALSE otherwise

	if (!BGP_VMCHKSTKSIZ(vm, 2)) UNLIKELY
		return;

	Bgpmpiter it;
	const Patricia *trie6 = (Patricia *) BGP_VMPOPA(vm);
	const Patricia *trie  = (Patricia *) BGP_VMPOPA(vm);
	if (!trie6) trie6 = &emptyTrie6;
	if (!trie)  trie  = &emptyTrie4;

	if (trie->afi != AFI_IP || trie6->afi != AFI_IP6) UNLIKELY {
		vm->errCode = BGPEVMBADOP;
		return;
	}
	if (Bgp_VmStartNets(vm, &it, arg) != OK) UNLIKELY
		return;  // error already set

	Prefix *pfx;
	while ((pfx = Bgp_NextMpPrefix(&it)) != NULL) {
		Boolean isMatching = FALSE;

		// Select appropriate PATRICIA
		switch (pfx->afi) {
		case AFI_IP:
			isMatching = Pat_IsSupernetOf(trie,  PLAINPFX(pfx));
			break;

		case AFI_IP6:
			isMatching = Pat_IsSupernetOf(trie6, PLAINPFX(pfx));
			break;

		default: UNREACHABLE; return;
		}

		if (isMatching) {
			Bgp_VmCollectNetMatch(vm, &it);
			return;
		}
	}
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

	Bgp_VmNetMatchFailed(vm);
}

void Bgp_VmDoRelt(Bgpvm *vm, Uint8 arg)
{
	// POPS:
	// -1: AFI_IP6 Patricia (may be NULL)
	// -2: AFI_IP  Patricia (may be NULL)
	//
	// PUSHES:
	// TRUE on SUPN match of any network prefix in message, FALSE otherwise

	if (!BGP_VMCHKSTKSIZ(vm, 2)) UNLIKELY
		return;

	Bgpmpiter it;
	const Patricia *trie6 = (Patricia *) BGP_VMPOPA(vm);
	const Patricia *trie  = (Patricia *) BGP_VMPOPA(vm);
	if (!trie6) trie6 = &emptyTrie6;
	if (!trie)  trie  = &emptyTrie4;

	if (trie->afi != AFI_IP || trie6->afi != AFI_IP6) UNLIKELY {
		vm->errCode = BGPEVMBADOP;
		return;
	}
	if (Bgp_VmStartNets(vm, &it, arg) != OK) UNLIKELY
		return;  // error already set

	Prefix *pfx;
	while ((pfx = Bgp_NextMpPrefix(&it)) != NULL) {
		Boolean isMatching = FALSE;

		// Select appropriate PATRICIA
		switch (pfx->afi) {
		case AFI_IP:
			isMatching = Pat_IsRelatedOf(trie,  PLAINPFX(pfx));
			break;
		case AFI_IP6:
			isMatching = Pat_IsRelatedOf(trie6, PLAINPFX(pfx));
			break;

		default: UNREACHABLE; return;
		}

		if (isMatching) {
			Bgp_VmCollectNetMatch(vm, &it);
			return;
		}
	}
	if (Bgp_GetErrStat(NULL)) {
		vm->errCode = BGPEVMMSGERR;
		return;
	}

	Bgp_VmNetMatchFailed(vm);
}

void Bgp_ResetVm(Bgpvm *vm)
{
	vm->nk        = 0;
	vm->nfuncs    = 0;
	vm->nmatches  = 0;
	vm->progLen   = 0;
	vm->hLowMark  = 0;
	vm->hHighMark = vm->hMemSiz;

	BGP_VMCLRSETUP(vm);
	BGP_VMCLRERR(vm);

	memset(vm->k, 0, sizeof(vm->k));
	memset(vm->funcs, 0, sizeof(vm->funcs));

	vm->matches = NULL;
}

void Bgp_ClearVm(Bgpvm *vm)
{
	free(vm->heap);
	free(vm->prog);
}
