// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm_dump.c
 *
 * BGP VM bytecode dump.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 */

#include "bgp/vmintrin.h"

#include "sys/dbg.h"
#include "numlib.h"
#include "strlib.h"

#include <assert.h>
#include <string.h>

#define LINEDIGS     5
#define MAXOPCSTRLEN 6
#define CODELINELEN  50
#define MAXINDENT    8
#define INDENTLEN    2
#define COMMENTLEN   70

static const char *OpcString(Bgpvmopc opc)
{
	// NOTE: Invariant: strlen(return) <= MAXOPCSTRLEN
	switch (opc) {
	case BGP_VMOP_NOP:    return "NOP";
	case BGP_VMOP_LOAD:   return "LOAD";
	case BGP_VMOP_LOADU:  return "LOADU";
	case BGP_VMOP_LOADN:  return "LOADN";
	case BGP_VMOP_LOADK:  return "LOADK";
	case BGP_VMOP_CALL:   return "CALL";
	case BGP_VMOP_BLK:    return "BLK";
	case BGP_VMOP_ENDBLK: return "ENDBLK";
	case BGP_VMOP_TAG:    return "TAG";
	case BGP_VMOP_NOT:    return "NOT";
	case BGP_VMOP_CFAIL:  return "CFAIL";
	case BGP_VMOP_CPASS:  return "CPASS";
	case BGP_VMOP_JZ:     return "JZ";
	case BGP_VMOP_JNZ:    return "JNZ";
	case BGP_VMOP_CHKT:   return "CHKT";
	case BGP_VMOP_CHKA:   return "CHKA";
	case BGP_VMOP_EXCT:   return "EXCT";
	case BGP_VMOP_SUPN:   return "SUPN";
	case BGP_VMOP_SUBN:   return "SUBN";
	case BGP_VMOP_RELT:   return "RELT";
	case BGP_VMOP_ASMTCH: return "ASMTCH";
	case BGP_VMOP_FASMTC: return "FASMTC";
	case BGP_VMOP_COMTCH: return "COMTCH";
	case BGP_VMOP_ACOMTC: return "ACOMTC";
	case BGP_VMOP_END:    return "END";
	default:              return "???";
	}
}

static const char *BgpTypeString(BgpType typ)
{
	switch (typ) {
	case BGP_OPEN:          return "OPEN";
	case BGP_UPDATE:        return "UPDATE";
	case BGP_NOTIFICATION:  return "NOTIFICATION";
	case BGP_KEEPALIVE:     return "KEEPALIVE";
	case BGP_ROUTE_REFRESH: return "ROUTE_REFRESH";
	case BGP_CLOSE:         return "CLOSE";
	default:                return NULL;
	}
}

static const char *BgpAttrString(BgpAttrCode code)
{
	switch (code) {
	case BGP_ATTR_ORIGIN:                                   return "ORIGIN";
	case BGP_ATTR_AS_PATH:                                  return "AS_PATH";
	case BGP_ATTR_NEXT_HOP:                                 return "NEXT_HOP";
	case BGP_ATTR_MULTI_EXIT_DISC:                          return "MULTI_EXIT_DISC";
	case BGP_ATTR_LOCAL_PREF:                               return "LOCAL_PREF";
	case BGP_ATTR_ATOMIC_AGGREGATE:                         return "ATOMIC_AGGREGATE";
	case BGP_ATTR_AGGREGATOR:                               return "AGGREGATOR";
	case BGP_ATTR_COMMUNITY:                                return "COMMUNITY";
	case BGP_ATTR_ORIGINATOR_ID:                            return "ORIGINATOR_ID";
	case BGP_ATTR_CLUSTER_LIST:                             return "CLUSTER_LIST";
	case BGP_ATTR_DPA:                                      return "DPA";
	case BGP_ATTR_ADVERTISER:                               return "ADVERTISER";
	case BGP_ATTR_RCID_PATH_CLUSTER_ID:                     return "RCID_PATH_CLUSTER_ID";
	case BGP_ATTR_MP_REACH_NLRI:                            return "MP_REACH_NLRI";
	case BGP_ATTR_MP_UNREACH_NLRI:                          return "MP_UNREACH_NLRI";
	case BGP_ATTR_EXTENDED_COMMUNITY:                       return "EXTENDED_COMMUNITY";
	case BGP_ATTR_AS4_PATH:                                 return "AS4_PATH";
	case BGP_ATTR_AS4_AGGREGATOR:                           return "AS4_AGGREGATOR";
	case BGP_ATTR_SAFI_SSA:                                 return "SAFI_SSA";
	case BGP_ATTR_CONNECTOR:                                return "CONNECTOR";
	case BGP_ATTR_AS_PATHLIMIT:                             return "AS_PATHLIMIT";
	case BGP_ATTR_PMSI_TUNNEL:                              return "PMSI_TUNNEL";
	case BGP_ATTR_TUNNEL_ENCAPSULATION:                     return "TUNNEL_ENCAPSULATION";
	case BGP_ATTR_TRAFFIC_ENGINEERING:                      return "TRAFFIC_ENGINEERING";
	case BGP_ATTR_IPV6_ADDRESS_SPECIFIC_EXTENDED_COMMUNITY: return "IPV6_ADDRESS_SPECIFIC_EXTENDED_COMMUNITY";
	case BGP_ATTR_AIGP:                                     return "AIGP";
	case BGP_ATTR_PE_DISTINGUISHER_LABELS:                  return "PE_DISTINGUISHER_LABELS";
	case BGP_ATTR_ENTROPY_LEVEL_CAPABILITY:                 return "ENTROPY_LEVEL_CAPABILITY";
	case BGP_ATTR_LS:                                       return "LS";
	case BGP_ATTR_LARGE_COMMUNITY:                          return "LARGE_COMMUNITY";
	case BGP_ATTR_BGPSEC_PATH:                              return "BGPSEC_PATH";
	case BGP_ATTR_COMMUNITY_CONTAINER:                      return "COMMUNITY_CONTAINER";
	case BGP_ATTR_PREFIX_SID:                               return "PREFIX_SID";
	case BGP_ATTR_SET:                                      return "SET";
	default:                                                return NULL;
	}
}

static const char *NetOpArgString(Uint8 opa)
{
	switch (opa) {
	case BGP_VMOPA_NLRI:          return "NLRI";
	case BGP_VMOPA_MPREACH:       return "MP_REACH_NLRI";
	case BGP_VMOPA_ALL_NLRI:      return "ALL_NLRI";
	case BGP_VMOPA_WITHDRAWN:     return "WITHDRAWN";
	case BGP_VMOPA_MPUNREACH:     return "MP_UNREACH_NLRI";
	case BGP_VMOPA_ALL_WITHDRAWN: return "ALL_WITHDRAWN";
	default:                      return NULL;
	}
}

static char *ExplainJump(char *buf, Uint32 ip, Uint8 disp, Uint32 progLen)
{
	char *p = buf;

	strcpy(p, "to line: "); p += 9;

	Uint32 target = ip + 1;
	target       += 1 + disp;

	p = Utoa(target, p);
	if (target > progLen)
		strcpy(p, " (JUMP TARGET OUT OF BOUNDS!)");

	return buf;
}

static char *Indent(char *p, Bgpvmopc opc, int level)
{
	int n    = CLAMP(level, 0, MAXINDENT);
	int last = n - 1;

	for (int i = 0; i < n; i++) {
		*p++ = (opc == BGP_VMOP_ENDBLK && i == last) ? '+' : '|';
		for (int j = 1; j < INDENTLEN; j++)
			*p++ = (opc == BGP_VMOP_ENDBLK && i == last) ? '-' : ' ';
	}
	return p;
}

static char *CommentCodeLine(char *line, const char *comment)
{
	char *p = line;

	p += Df_strpadr(p, ' ', CODELINELEN);

	*p++ = ' ';
	*p++ = ';'; *p++ = ' ';

	size_t n = strlen(comment);
	if (3 + n >= COMMENTLEN) {
		n = COMMENTLEN - 3 - 3;

		memcpy(p, comment, n);
		p += n;
		*p++ = '.'; *p++ = '.'; *p++ = '.';
	} else {
		memcpy(p, comment, n);
		p += n;
	}

	*p = '\0';
	return p;
}

void Bgp_VmDumpProgram(Bgpvm *vm, void *streamp, const StmOps *ops)
{
    char explainbuf[64];
	char buf[256];

	int indent = 0;

	// NOTE: <= so it includes trailing END
	for (Uint32 ip = 0; ip <= vm->progLen; ip++) {
		Bgpvmbytec ir  = vm->prog[ip];
		Bgpvmopc   opc = BGP_VMOPC(ir);
		Uint8      opa = BGP_VMOPARG(ir);

		const char *opcnam = OpcString(opc);
		assert(strlen(opcnam) <= MAXOPCSTRLEN);

		char *p = buf;

		// Line number
		Utoa(ip+1, p);
		p += Df_strpadl(p, '0', LINEDIGS);

		*p++ = ':';
		*p++ = ' ';

		// Instruction hex dump
		*p++ = '0';
		*p++ = 'x';
		Xtoa(ir, p);
		p += Df_strpadl(p, '0', XDIGS(ir));
		*p++ = ' ';

		// Code indent
		p = Indent(p, opc, indent);

		// Opcode
		strcpy(p, opcnam);
		p += Df_strpadr(p, ' ', MAXOPCSTRLEN);

		// Instruction argument
		const char *opastr = NULL;
		switch (opc) {
		case BGP_VMOP_LOAD:
			*p++ = ' ';

			p = Itoa((Sint8) opa, p);

			break;
		case BGP_VMOP_LOADU:
		case BGP_VMOP_JZ:
		case BGP_VMOP_JNZ:
			*p++ = ' ';

			p = Utoa(opa, p);
			if (opc == BGP_VMOP_JZ || opc == BGP_VMOP_JNZ)
			    opastr = ExplainJump(explainbuf, ip, opa, vm->progLen);

			break;
		case BGP_VMOP_TAG:
		case BGP_VMOP_CHKT:
		case BGP_VMOP_CHKA:
		case BGP_VMOP_EXCT:
		case BGP_VMOP_SUBN:
		case BGP_VMOP_SUPN:
		case BGP_VMOP_RELT:
			*p++ = ' ';
			*p++ = '0'; *p++ = 'x';

			Xtoa(opa, p);
			p += Df_strpadl(p, '0', XDIGS(opa));

			if (opc == BGP_VMOP_CHKT)
				opastr = BgpTypeString(opa);
			else if (opc == BGP_VMOP_CHKA)
				opastr = BgpAttrString(opa);
			else
				opastr = NetOpArgString(opa);

			break;
		case BGP_VMOP_LOADK:
			*p++ = ' ';
			*p++ = 'K';
			*p++ = '[';
			p    = Utoa(opa, p);
			*p++ = ']';
			*p   = '\0';
			break;
		case BGP_VMOP_CALL:
			*p++ = ' ';
			*p++ = 'F';
			*p++ = 'N';
			*p++ = '[';
			p    = Utoa(opa, p);
			*p++ = ']';
			*p   = '\0';

			if (opa < vm->nfuncs) {
				Funsym fsym;

				fsym.func = (void (*)(void)) vm->funcs[opa];
				opastr    = Sys_GetSymbolName(fsym.sym);
			}
			break;
		default:
			break;
		}

		// Optional comment after CODELINELEN columns
		if (opastr)
			p = CommentCodeLine(buf, opastr);

		// Flush line (no need for '\0')
		*p++ = '\n';
		ops->Write(streamp, buf, p - buf);

		// Update indent
		if (opc == BGP_VMOP_BLK)
			indent++;
		if (opc == BGP_VMOP_ENDBLK)
			indent--;
	}
}
