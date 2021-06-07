// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm_cdef.c
 *
 * Portable implementation for BGP VM execution loop
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * Plain C switch based FETCH-DECODE-DISPATCH-EXECUTE BGP filtering
 * engine VM implementation
 *
 * \note File should be `#include`d by bgp/vm.c
 */

#ifdef DF_BGP_VMDEF_H_
#error "Only one vm_<impl>def.h file may be #include-d"
#endif
#define DF_BGP_VMDEF_H_

#define LIKELY
#define UNLIKELY

#define FETCH(ir, vm) (ir = (vm)->prog[(vm)->pc++])

#define EXPECT(opcode, ir, vm) ((void) 0)

#define DISPATCH(opcode) switch (opcode)

#define EXECUTE(opcode)  case BGP_VMOP_ ## opcode

#define EXECUTE_SIGILL   default
