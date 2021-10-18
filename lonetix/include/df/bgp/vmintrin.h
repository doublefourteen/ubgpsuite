// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vmintrin.h
 *
 * BGP VM engine operation intrinsics.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * Utilities in this file are meant for low level VM interaction,
 * usually to implement actual VM extensions.
 */

#ifndef DF_BGP_VMINTRIN_H_
#define DF_BGP_VMINTRIN_H_

#include "bgp/vm.h"

/// Get current VM program counter.
FORCE_INLINE Uint32 BGP_VMCURPC(Bgpvm *vm)
{
	return vm->pc - 1;  // PC always references *next* instruction
}

/// Get VM stack base pointer.
FORCE_INLINE Bgpvmval *BGP_VMSTK(Bgpvm *vm)
{
	return (Bgpvmval *) ((Uint8 *) vm->heap + vm->hLowMark);
}

/// Get stack value at index `idx`, -1 is topmost value, -2 is second to topmost...
FORCE_INLINE Bgpvmval *BGP_VMSTKGET(Bgpvm *vm, Sint32 idx)
{
	Bgpvmval *stk = BGP_VMSTK(vm);

	return &stk[vm->si + idx];
}

/// Equivalent to `BGP_VMSTKGET()`, but returns stack content as `Sint64`.
FORCE_INLINE Sint64 BGP_VMPEEK(Bgpvm *vm, Sint32 idx)
{
	return BGP_VMSTKGET(vm, idx)->val;
}

/// Equivalent to `BGP_VMSTKGET()`, but returns stack content as `void *`.
FORCE_INLINE void *BGP_VMPEEKA(Bgpvm *vm, Sint32 idx)
{
	return BGP_VMSTKGET(vm, idx)->ptr;
}

/// Pop `n` values from VM stack, **assumes stack is large enough**.
FORCE_INLINE void BGP_VMPOPN(Bgpvm *vm, Uint32 n)
{
	vm->si -= n;
}

/// Pop topmost stack value in VM, returning its value as `Sint64`, **assumes stack is not empty**.
FORCE_INLINE Sint64 BGP_VMPOP(Bgpvm *vm)
{
	Bgpvmval *stk = BGP_VMSTK(vm);

	return stk[--vm->si].val;
}

/// Like `BGP_VMPOP()`, but returns value as `void *`.
FORCE_INLINE void *BGP_VMPOPA(Bgpvm *vm)
{
	Bgpvmval *stk = BGP_VMSTK(vm);

	return stk[--vm->si].ptr;
}

/// Push `v` to stack, **assumes enough stack space is available**.
FORCE_INLINE void BGP_VMPUSH(Bgpvm *vm, Sint64 v)
{
	Bgpvmval *stk = BGP_VMSTK(vm);

	stk[vm->si++].val = v;
}

/// Like `BGP_VMPUSH()`, but pushes a pointer.
FORCE_INLINE void BGP_VMPUSHA(Bgpvm *vm, void *p)
{
	Bgpvmval *stk = BGP_VMSTK(vm);

	stk[vm->si++].ptr = p;
}

/// Ensure at least `n` elements may be popped from the stack.
FORCE_INLINE Boolean BGP_VMCHKSTKSIZ(Bgpvm *vm, Uint32 n)
{
	if (vm->si < n) {
		vm->errCode = BGPEVMUFLOW;
		return FALSE;
	}

	return TRUE;
}

/// Ensure at least `n` elements may be pushed to the stack.
FORCE_INLINE Boolean BGP_VMCHKSTK(Bgpvm *vm, Uint32 n)
{
	size_t siz = vm->si + n;

	siz *= sizeof(Bgpvmval);
	if (vm->hHighMark - vm->hLowMark < siz) {
		vm->errCode = BGPEVMOFLOW;
		return FALSE;
	}

	return TRUE;
}

/**
 * \brief Test whether `vm->msg` header type matches `type`.
 *
 * \return Pointer to message header on successful match, `NULL`
 *         otherwise.
 */
FORCE_INLINE Bgphdr *BGP_VMCHKMSGTYPE(Bgpvm *vm, BgpType type)
{
	Bgphdr *hdr = BGP_HDR(vm->msg);

	return (hdr->type == type) ? hdr : NULL;
}

Judgement Bgp_VmStoreMsgTypeMatch(Bgpvm *vm, Boolean);

void Bgp_VmStoreMatch(Bgpvm *vm);

/// Implement `LOAD`.
FORCE_INLINE void Bgp_VmDoLoad(Bgpvm *vm, Sint8 val)
{
	if (!BGP_VMCHKSTK(vm, 1))
		return;

	BGP_VMPUSH(vm, val);
}

/// Implement `LOADU`.
FORCE_INLINE void Bgp_VmDoLoadu(Bgpvm *vm, Uint8 val)
{
	if (!BGP_VMCHKSTK(vm, 1))
		return;

	BGP_VMPUSH(vm, val);
}

/// Implement `LOADK` of `vm->k[idx]`.
FORCE_INLINE void Bgp_VmDoLoadk(Bgpvm *vm, Uint8 idx)
{
	if (idx >= vm->nk) {
		vm->errCode = BGPEVMBADK;
		return;
	}
	if (!BGP_VMCHKSTK(vm, 1))
		return;

	Bgpvmval *stk = BGP_VMSTK(vm);

	stk[vm->si++] = vm->k[idx];
}

/// Implement `LOADN`.
FORCE_INLINE void Bgp_VmDoLoadn(Bgpvm *vm)
{
	if (!BGP_VMCHKSTK(vm, 1)) {
		vm->errCode = BGPEVMOFLOW;
		return;
	}

	BGP_VMPUSHA(vm, NULL);
}

/// Break out of current `BLK`.
FORCE_INLINE void Bgp_VmDoBreak(Bgpvm *vm)
{
	Bgpvmopc opc;

	do
		opc = BGP_VMOPC(vm->prog[vm->pc++]);
	while (opc != BGP_VMOP_ENDBLK && opc != BGP_VMOP_END);

	if (opc == BGP_VMOP_ENDBLK)
		vm->nblk--;
}


/// Execute `CALL` of function `vm->funcs[idx]`.
FORCE_INLINE void Bgp_VmDoCall(Bgpvm *vm, Uint8 idx)
{
	void (*fn)(Bgpvm *);

	if (idx >= vm->nfuncs) {
		vm->errCode = BGPEVMBADFN;
		return;
	}

	fn = vm->funcs[idx];
	if (fn) fn(vm);
}

/**
 * \brief Implement `CPASS`, `CFAIL`, `ORPASS`, `ORFAIL`, depending
 *        on break condition and value.
 */
Boolean Bgp_VmDoBreakPoint(Bgpvm *vm, Boolean breakIf, Boolean onBreak);

/// Implement `TAG` instruction with argument `tag`.
FORCE_INLINE void Bgp_VmDoTag(Bgpvm *vm, Uint8 tag)
{
	vm->curMatch->tag = tag;
}

/**
 * \brief Implements VM `NOT` instruction.
 *
 * Negate stack topmost value.
 */
FORCE_INLINE void Bgp_VmDoNot(Bgpvm *vm)
{
	// Expected STACK:
	// -1: Any value interpreted as Sint64

	if (!BGP_VMCHKSTKSIZ(vm, 1))
		return;

	Bgpvmval *v = BGP_VMSTKGET(vm, -1);

	v->val = !v->val;
}

/// Implements `CHKT` with argument `type`.
void Bgp_VmDoChkt(Bgpvm *vm, BgpType type);

/// Implements `CHKA` with argument `code`.
void Bgp_VmDoChka(Bgpvm *vm, BgpAttrCode code);

void Bgp_VmDoExct(Bgpvm *vm, Uint8 arg);
void Bgp_VmDoSupn(Bgpvm *vm, Uint8 arg);
void Bgp_VmDoSubn(Bgpvm *vm, Uint8 arg);
void Bgp_VmDoRelt(Bgpvm *vm, Uint8 arg);

void Bgp_VmDoAsmtch(Bgpvm *vm);

void Bgp_VmDoComtch(Bgpvm *vm);
void Bgp_VmDoAcomtc(Bgpvm *vm);

#endif
