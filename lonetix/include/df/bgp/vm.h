// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm.h
 *
 * BGP message filtering engine.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#ifndef DF_BGP_VM_H_
#define DF_BGP_VM_H_

#include "bgp.h"

/**
 * \name BGP VM bytecode
 *
 * \brief Filtering engine instruction OPCODEs and arguments.
 *
 * Bytecode format is:
 * ```
 * LSB               MSB  (NATIVE endianness)
 *  +--------+--------+
 *  | OPCODE | ARG    |
 *  +--------+--------+
 * 0                   16 bit
 *
 * (2 bytes per instruction)
 * ```
 *
 * Bytecode follows native machine endianness, it is NOT endian independent.
 *
 * @{
 */

/// BGP filter VM OPCODE
typedef Uint8 Bgpvmopc;
/// BGP filter VM bytecode instruction
typedef Uint16 Bgpvmbytec;

/// Returns a VM bytecode with the specified opcode and argument.
#define BGP_VMOP(opc, arg) ((Bgpvmbytec) ((((Uint8) (arg)) << 8) | ((Bgpvmopc) opc)))

/// Extract OPCODE from the specified bytecode.
FORCE_INLINE Bgpvmopc BGP_VMOPC(Bgpvmbytec bytec)
{
	return (Bgpvmopc) (bytec & 0xff);
}

/// Extract direct argument from the specified bytecode.
FORCE_INLINE Uint8 BGP_VMOPARG(Bgpvmbytec bytec)
{
	return (bytec >> 8);
}

/// NO-OPERATION - does nothing and moves on
#define BGP_VMOP_NOP    U8_C(0)
/// LOAD - Pushes `ARG` constant on stack, interpreting it as a `Sint8`
#define BGP_VMOP_LOAD   U8_C(1)
/// LOAD UNSIGNED - Pushes `ARG` constant on stack, interpreting it as an `Uint8`
#define BGP_VMOP_LOADU  U8_C(2)
/// LOAD NULL - Push a `NULL` address on stack
#define BGP_VMOP_LOADN  U8_C(3)
/// LOADK - Push a new constant (K) on stack, `ARG` is the index inside `K`
#define BGP_VMOP_LOADK  U8_C(4)
/// CALL - invoke an external native function
#define BGP_VMOP_CALL   U8_C(5)
/// BLOCK OPEN - Push a new matching block (used to implement AND/OR chains)
#define BGP_VMOP_BLK    U8_C(6)
/// END BLOCK - Pops the current matching block
#define BGP_VMOP_ENDBLK U8_C(7)
/// TAG LAST MATCH - Tags last operation's match with `ARG`
#define BGP_VMOP_TAG    U8_C(8)
/// NOT - Boolean negate the topmost stack element
#define BGP_VMOP_NOT    U8_C(9)
/// CONDITIONAL FAIL If TRUE - Fail the current matching `BLK` if topmost stack element is non-zero
#define BGP_VMOP_CFAIL  U8_C(10)
/// CONDITIONAL PASS If TRUE - Pass the current matching `BLK` if topmost stack element is non-zero
#define BGP_VMOP_CPASS  U8_C(11)

/// Jump if zero - Skip over a positive number of instructions if topmost stack element is 0.
#define BGP_VMOP_JZ     U8_C(12)
/// Jump if non-zero - Skip over a positive number of instructions if topmost stack element is not 0.
#define BGP_VMOP_JNZ    U8_C(13)

/// CHECK TYPE - ARG is the `BgpType` to test against
#define BGP_VMOP_CHKT   U8_C(14)
/// CHECK ATTRIBUTE - ARG is the `BgpAttrCode` to test for existence
#define BGP_VMOP_CHKA   U8_C(15)

#define BGP_VMOP_EXCT   U8_C(16)
#define BGP_VMOP_SUPN   U8_C(17)
#define BGP_VMOP_SUBN   U8_C(18)
/// RELATED - Tests whether the BGP message contains prefixes related with the provided ones
#define BGP_VMOP_RELT   U8_C(19)

/// Returns `TRUE` if `opc` belongs to an instruction operating on NETwork prefixes.
FORCE_INLINE Boolean BGP_ISVMOPNET(Bgpvmopc opc)
{
	return opc >= BGP_VMOP_EXCT && opc <= BGP_VMOP_RELT;
}

/// AS PATH MATCH - Tests BGP message AS PATH against a match expression
#define BGP_VMOP_ASMTCH U8_C(20)
/// FAST AS PATH MATCH - AS PATH test using precompiled AS PATH match expression
#define BGP_VMOP_FASMTC U8_C(21)
/// COMMUNITY MATCH - COMMUNITY test using a precompiled COMMUNITY match expression
#define BGP_VMOP_COMTCH U8_C(22)
/// ALL COMMUNITY MATCH - Like COMTCH, but requires all communities to be present inside message
#define BGP_VMOP_ACOMTC U8_C(23)

/// END - Terminate VM execution with the latest result
#define BGP_VMOP_END    U8_C(24)

// #define BGP_VMOP_MOVK      MOVE K - Move topmost K index to ARG K index
// #define BGP_VMOP_DISCRD    DISCARD - discard vm->curMatch if any

// Bytecode `ARG` values for `NET` class instructions
#define BGP_VMOPA_NLRI          U8_C(0)
#define BGP_VMOPA_MPREACH       U8_C(1)
#define BGP_VMOPA_ALL_NLRI      U8_C(2)
#define BGP_VMOPA_WITHDRAWN     U8_C(3)
#define BGP_VMOPA_MPUNREACH     U8_C(4)
#define BGP_VMOPA_ALL_WITHDRAWN U8_C(5)

// Special `Asn` values for `ASMTCH` and `FASMTC` instructions

#define ASNNOTFLAG  BIT(61)

/// Matches any ASN **except** the provided one.
#define ASNNOT(asn) ((Asn) ((asn) | ASNNOTFLAG))

/// Tests whether `asn` is a negative ASN match.
FORCE_INLINE Boolean ISASNNOT(Asn asn)
{
	return (asn & ASNNOTFLAG) != 0;
}

/// Match with the AS_PATH start (^)
#define ASN_START  ((Asn) 0x0000000100000000LL)
/// Match with the AS PATH end ($).
#define ASN_END    ((Asn) 0x0000000200000000LL)
/// Match with any ASN (.).
#define ASN_ANY    ((Asn) 0x0000000300000000LL)
/// Match zero or more ASN (*).
#define ASN_STAR   ((Asn) 0x0000000400000000LL)
/// Match with zero or one ASN (?).
#define ASN_QUEST  ((Asn) 0x0000000500000000LL)
/// Match the previous ASN one or more times (+).
#define ASN_PLUS   ((Asn) 0x0000000600000000LL)
/// Introduce a new group (opening paren for expressions like `( a b c )`)
#define ASN_NEWGRP ((Asn) 0x0000000700000000LL)
/// Introduce alternative inside matching expression (pipe symbol for expressions like `( a b | b c )`)
#define ASN_ALT    ((Asn) 0x0000000800000000LL)
/// Terminate a group expression (closing paren for expressions like `( a b c )`)
#define ASN_ENDGRP ((Asn) 0x0000000900000000LL)

/** @} */

/**
 * \brief Stack slot.
 *
 * The VM stack is a sequence of `Bgpvmval` cells,
 * interpreted by each instruction as dictated by the opcode.
 */
typedef union Bgpvmval Bgpvmval;
union Bgpvmval {
	void  *ptr;  ///< Value as a pointer
	Sint64 val;  ///< Value as a signed integral
};

/// Match info for AS matching expressions.
typedef struct Bgpvmasmatch Bgpvmasmatch;
struct Bgpvmasmatch {
	Bgpvmasmatch *next;
	Aspathiter    spos;
	Aspathiter    epos;
};

/// Optimization modes for some matching instructions (e.g. BGP_VMOP_COMTCH/BGP_VMOP_ACOMTC).
typedef enum {
	BGP_VMOPT_NONE,           ///< Do not optimize.

	// For COMTCH/ACOMTC
	BGP_VMOPT_ASSUME_COMTCH,  ///< Assume instruction is going to be `COMTCH`
	BGP_VMOPT_ASSUME_ACOMTC   ///< Assume instruction is going to be `ACOMTC`
} BgpVmOpt;

typedef struct Bgpmatchcomm Bgpmatchcomm;
struct Bgpmatchcomm {
	Boolean8 maskHi;  // don't match HI (match of type *:N)
	Boolean8 maskLo;  // don't match LO (match of type N:*)
	Bgpcomm  c;
};

/**
 * \brief Matching operation result on a BGP message.
 *
 * Collect relevant information on a matching operation, including the
 * direct byte range and position inside BGP message data.
 *
 * This structure may be used to further process BGP data after the filtering
 * is complete. A `Bgpvmmatch` structure is generated for several BGP VM
 * OPCODEs, and is only valid up to the next [Bgp_VmExec()](@ref Bgp_VmExec)
 * call on the same VM that originated them.
 */
typedef struct Bgpvmmatch Bgpvmmatch;
struct Bgpvmmatch {
	Uint32      pc;          ///< instruction index that originated this match
	Boolean8    isMatching;  ///< whether this result declares a match or a mismatch
	Boolean8    isPassing;   ///< whether this result produced a `PASS` or a `FAIL` inside filter
	Uint8       tag;         ///< optional tag id for this match (as set by `TAG`)
	Uint8      *base, *lim;  ///< relevant BGP message segment, if any
	void       *pos;         ///< pointer to detailed match-specific information
	Bgpvmmatch *nextMatch;   ///< next match in chain (`NULL` if this is the last element)
};

/// Filtering engine error code (a subset of [BgpRet](@ref BgpRet).
typedef Sint8 BgpvmRet;

/// Maximum number of VM constants inside [Bgpvm](@ref Bgpvm).
#define MAXBGPVMK      256
/// Maximum number of VM callable functions inside [Bgpvm](@ref Bgpvm).
#define MAXBGPVMFN     32
/// Maximum allowed nested grouping levels inside a `BGP_VMOP_ASMTCH`.
#define MAXBGPVMASNGRP 32

/**
 * \brief Bytecode-based virtual machine operating on BGP messages.
 *
 * Extensible programmable BGP message matching and filtering engine.
 */
typedef struct Bgpvm Bgpvm;
struct Bgpvm {
	Uint32      pc;           ///< VM program counter inside `prog`
	Uint32      nblk;         ///< nested conditional block count
	Uint32      nmatches;     ///< current execution matches count (length of the `matches` list)
	Uint16      si;           ///< VM Stack index
	Uint16      nk;           ///< count of constants (K) available in `k`
	Uint8       nfuncs;       ///< count of functions (FN) available in `funcs`
	Boolean8    setupFailed;  ///< whether a `Bgp_VmEmit()` or `Bgp_VmPermAlloc()` on this VM ever failed.
	BgpvmRet    errCode;      ///< whether the VM encountered an error
	Uint32      hLowMark;     ///< VM heap low memory mark
	Uint32      hHighMark;    ///< VM heap high memory mark
	Uint32      hMemSiz;      ///< VM heap size in bytes
	Uint32      progLen;      ///< bytecode program instruction count
	Uint32      progCap;      ///< bytecode segment instruction capacity
	Bgpmsg     *msg;          ///< current BGP message being processed
	Bgpvmbytec *prog;         ///< VM program bytecode, `progLen` instructions (`prog[progLen]` is always `BGP_VMOP_END`)
	/**
	 * Filtering VM heap, managed as follows:
	 * ```
	 *                   hLowMark                    hHighMark
	 * heap -+             v                           v
	 *       v STACK grows upwards .-.->          <.-.-. TEMP grows downwards
	 *       +=====================\-------------------/=============+
	 *       | PERM ALLOCS | STACK  >                 <  TEMP ALLOCS |
	 *       +=====================/-------------------\=============+
	 *       [--------------------- hMemSiz -------------------------]
	 *
	 * PERM ALLOCS: Allocations that last forever (until the VM is freed)
	 *              - such allocations CANNOT happen while VM is running
	 *
	 * TEMP ALLOCS: Allocations that last up to the next Bgp_VmExec()
	 *              - such allocations may only take place while VM is running
	 * ```
	 */
	void       *heap;
	Bgpvmmatch *curMatch;                     ///< current match being updated during VM execution
	Bgpvmmatch *matches;                      ///< matches produced during execution (contains `nmatches` elements)
	Bgpvmval    k[MAXBGPVMK];                 ///< VM constants (K), `nk` allocated
	void      (*funcs[MAXBGPVMFN])(Bgpvm *);  ///< VM functions (FN), `nfuncs` allocated
};

/// Clear the `errCode` error flag on `vm`.
FORCE_INLINE void BGP_VMCLRERR(Bgpvm *vm)
{
	vm->errCode = BGPENOERR;
}

/// Clear the `setupFailed` error flag on `vm`.
FORCE_INLINE void BGP_VMCLRSETUP(Bgpvm *vm)
{
	vm->setupFailed = FALSE;
}

FORCE_INLINE Sint32 Bgp_VmNewk(Bgpvm *vm)
{
	if (vm->nk >= MAXBGPVMK)
		return -1;

	return vm->nk++;
}

FORCE_INLINE Sint32 Bgp_VmNewFn(Bgpvm *vm)
{
	if (vm->nfuncs >= MAXBGPVMFN)
		return -1;

	return vm->nfuncs++;
}

FORCE_INLINE Sint32 BGP_VMSETKA(Bgpvm *vm, Sint32 kidx, void *ptr)
{
	if (kidx >= 0)
		vm->k[kidx].ptr = ptr;

	return kidx;
}

FORCE_INLINE Sint32 BGP_VMSETK(Bgpvm *vm, Sint32 kidx, Sint64 val)
{
	if (kidx >= 0)
		vm->k[kidx].val = val;

	return kidx;
}

FORCE_INLINE Sint32 BGP_VMSETFN(Bgpvm *vm, Sint32 idx, void (*fn)(Bgpvm *))
{
	if (idx >= 0)
		vm->funcs[idx] = fn;

	return idx;
}

/**
 * \brief Initialize a new VM with the specified heap size.
 *
 * \return `OK` on success, `NG` on out of memory, sets BGP error, VM error,
 *         and, on failure, VM setup failure flag.
 */
Judgement Bgp_InitVm(Bgpvm *vm, size_t heapSiz);

/**
 * \brief Emit a VM bytecode instruction to `vm`.
 *
 * \return `OK` if instruction was added successfully, `NG` on out of memory,
 *         sets BGP error, VM error and, on failure, VM setup failure flag.
 *
 * \note Emitting `BGP_VMOP_END` has no effect.
 */
Judgement Bgp_VmEmit(Bgpvm *vm, Bgpvmbytec bytec);
/**
 * \brief Precompile an AS PATH match expression for use with BGP_VMOP_FASMTC.
 *
 * \return Pointer suitable as the argument of `BGP_VMOP_FASMTC` instruction
 *         on success, `NULL` on failure.
 *         Precompiled expression is stored inside `vm` permanent heap.
 */
void *Bgp_VmCompileAsMatch(Bgpvm *vm, const Asn *match, size_t n);
void *Bgp_VmCompileCommunityMatch(Bgpvm *vm, const Bgpmatchcomm *match, size_t n, BgpVmOpt opt);

/**
 * \brief Perform a permanent heap allocation of `size` bytes to `vm`.
 *
 * \return `vm` heap pointer to memory zone on success, `NULL` on failure,
 *         sets BGP error, VM error, and, on failure, `vm` setup failure flag.
 *
 * \note This function may only be called if `vm` is not executing!
 */
void *Bgp_VmPermAlloc(Bgpvm *vm, size_t size);
void *Bgp_VmTempAlloc(Bgpvm *vm, size_t size);
void  Bgp_VmTempFree(Bgpvm *vm, size_t size);

/**
 * \brief Execute `vm` bytecode on `msg`.
 *
 * \return `TRUE` if `vm` terminated with PASS, `FALSE` if it terminated on
 *         FAIL, or if an error was encountered. Sets BGP error and VM error.
 */
Boolean Bgp_VmExec(Bgpvm *vm, Bgpmsg *msg);
/// Print `vm` bytecode dump to stream.
void Bgp_VmDumpProgram(Bgpvm *vm, void *streamp, const StmOps *ops);

/// Reset `vm` state, but keep allocated memory for further setup.
void Bgp_ResetVm(Bgpvm *vm);

/// Clear `vm` and free all memory.
void Bgp_ClearVm(Bgpvm *vm);

#endif
