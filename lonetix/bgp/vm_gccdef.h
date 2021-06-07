// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm_gccdef.h
 *
 * `#define`s for GNUC optimized BGP VM execution loop.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * \note This file should be `#include`d by `bgp/vm.c`
 */

#ifdef DF_BGP_VMDEF_H_
#error "Only one vm_<impl>def.h file may be #include-d"
#endif
#define DF_BGP_VMDEF_H_

#define _CONCAT(x, y)  x ## y
#define _XCONCAT(x, y) _CONCAT(x, y)

#ifdef __clang__
// No __attribute__ on labels in CLANG
#define LIKELY
#else

#define LIKELY \
	_XCONCAT(_BRANCH_PREDICT_HINT, __COUNTER__): \
	__attribute__((__hot__, __unused__))

#endif

#ifdef __clang__
// No __attribute__ on labels in CLANG
#define UNLIKELY
#else

#define UNLIKELY \
	_XCONCAT(_BRANCH_PREDICT_HINT, __COUNTER__): \
	__attribute__((__cold__, __unused__))

#endif

#define FETCH(ir, vm) (ir = (vm)->prog[(vm)->pc++])

#define EXPECT(opcode, ir, vm) \
	do { \
		if (__builtin_expect( \
			BGP_VMOPC((vm)->prog[(vm)->pc]) == BGP_VMOP_ ## opcode, \
			1                                                       \
		)) { \
			ir = (vm)->prog[(vm)->pc++]; \
			goto EX_ ## opcode;          \
		} \
	} while (0)

#define DISPATCH(opcode) \
	_Pragma("GCC diagnostic push");                  \
	_Pragma("GCC diagnostic ignored \"-Wpedantic\"") \
	goto *bgp_vmOpTab[opcode];                       \
	_Pragma("GCC diagnostic pop")                    \
	switch (opcode)  // This keeps consistency with regular vm_cdef.h

#define EXECUTE(opcode) case BGP_VMOP_ ## opcode: EX_ ## opcode

#define EXECUTE_SIGILL default: EX_SIGILL
