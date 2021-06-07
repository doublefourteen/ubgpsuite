// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm_optab.h
 *
 * Computed goto table for GNUC optimized BGP VM execution loop.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * File defines a GNUC-specific computed goto table.
 *
 * It can be used as an alternative to a regular switch to
 * accelerate the BGP filtering engine execution loop (Bgp_VmExec()).
 *
 * Constraints:
 * - Array length: 256 (8-bit OPCODE width)
 * - Label address MUST follow the convention EX_<OPCODE NAME> (e.g. EX_NOP, EX_EXCT)
 * - Any unused OPCODE MUST be set to EX_SIGILL
 *
 * Operations on the computed goto table are defined in: bgp/vm_gccdef.h
 *
 * \see [GCC Documentation](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html)
 *
 * \warning KEEP TABLE IN SYNC WITH OPCODES IN: bgp/vm.h
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverride-init"

static void *const bgp_vmOpTab[256] = {
	// Following clears everything else in the array to SIGILL,
	// 8 instructions per line (256 &&EX_SIGILL)

	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,
	&&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL, &&EX_SIGILL,

	// Following initializes valid OPCODEs

	[BGP_VMOP_NOP]    = &&EX_NOP,
	[BGP_VMOP_LOAD]   = &&EX_LOAD,
	[BGP_VMOP_LOADU]  = &&EX_LOADU,
	[BGP_VMOP_LOADN]  = &&EX_LOADN,
	[BGP_VMOP_LOADK]  = &&EX_LOADK,
	[BGP_VMOP_CALL]   = &&EX_CALL,
	[BGP_VMOP_BLK]    = &&EX_BLK,
	[BGP_VMOP_ENDBLK] = &&EX_ENDBLK,
	[BGP_VMOP_TAG]    = &&EX_TAG,
	[BGP_VMOP_NOT]    = &&EX_NOT,
	[BGP_VMOP_CFAIL]  = &&EX_CFAIL,
	[BGP_VMOP_CPASS]  = &&EX_CPASS,
	[BGP_VMOP_JZ]     = &&EX_JZ,
	[BGP_VMOP_JNZ]    = &&EX_JNZ,
	[BGP_VMOP_CHKT]   = &&EX_CHKT,
	[BGP_VMOP_CHKA]   = &&EX_CHKA,
	[BGP_VMOP_EXCT]   = &&EX_EXCT,
	[BGP_VMOP_SUPN]   = &&EX_SUPN,
	[BGP_VMOP_SUBN]   = &&EX_SUBN,
	[BGP_VMOP_RELT]   = &&EX_RELT,
	[BGP_VMOP_ASMTCH] = &&EX_ASMTCH,
	[BGP_VMOP_FASMTC] = &&EX_FASMTC,
	[BGP_VMOP_COMTCH] = &&EX_COMTCH,
	[BGP_VMOP_ACOMTC] = &&EX_ACOMTC,
	[BGP_VMOP_END]    = &&EX_END
};

#pragma GCC diagnostic pop
