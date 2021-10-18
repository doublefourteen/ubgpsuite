// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * \file bgp/vm_asmtch.c
 *
 * Implements ASMTCH and FASMTC, and ASN match IR compilation.
 *
 * \copyright The DoubleFourteen Code Forge (C) All Rights Reserved
 * \author    Lorenzo Cogotti
 *
 * This code is a modified version of the Plan9 regexp library matching algorithm.
 * The Plan9 regexp library is available under the Lucent Public License
 * at: https://9fans.github.io/plan9port/unix/libregexp9.tgz
 *
 * \see [Regular Expression Matching Can Be Simple And Fast](https://swtch.com/~rsc/regexp/regexp1.html)
 */

#include "bgp/vmintrin.h"
#include "sys/endian.h"

#include <assert.h>
#include <setjmp.h>
#include <string.h>

// Define this to dump the compiled AS MATCH expressions to stderr
//#define DF_DEBUG_ASMTCH
#ifdef NDEBUG
// Force DF_DEBUG_ASMTCH off on release build
#undef DF_DEBUG_ASMTCH
#endif

#ifdef DF_DEBUG_ASMTCH
#include "sys/con.h"
#endif

#define AS    126
#define NAS   127
#define START 128  // start, used for marker on stack
#define RPAR  129  // right parens, )
#define LPAR  130  // left parens, (
#define ALT   131  // alternation, |
#define CAT   132  // concatentation, implicit operator
#define STAR  133  // closure, *
#define PLUS  134  // a+ == aa*
#define QUEST 135  // a? == a|nothing, i.e. 0 or 1 a's
#define ANY   192  // any character except newline, .
#define NOP   193  // no operation
#define BOL   194  // beginning of line, ^
#define EOL   195  // end of line, $
#define STOP  255  // terminate: match found

#define BOTTOM (START - 1)

#define NSTACK     32
#define LISTSIZ    10
#define BIGLISTSIZ (32 * LISTSIZ)

#define ASNBOLFLAG BIT(60)  // used to mark the initial ASN, so that BOL operator works

#define ASN_EOL    -1       // equals to -1 returned by Bgp_NextAsPath()

// Automaton instruction.
typedef struct Nfainst Nfainst;
struct Nfainst{
	int type;  // instruction type, any of the macros above

	union {  // NOTE: keep `next` and `left` in the same union!
		Nfainst *next;  // next instruction in chain
		Nfainst *left;  // left output state, for ALT instructions
	};
	union {
		Uint32   asn;    // ASN match, for AS/NAS instructions
		Uint16   grpid;  // match group id, for LPAR/RPAR instructions
		Nfainst *right;  // right output state, for ALT instructions
	};
};

// A block of instructions (used during NFA construction)
typedef struct Nfanode Nfanode;
struct Nfanode {
	Nfainst *first, *last;
};

// Start and end position of a subexpression match
typedef struct {
	Aspathiter spos;
	Aspathiter epos;
} Nfamatch;

typedef struct Nfastate Nfastate;
struct Nfastate {
	Nfainst *ip;
	Nfamatch se[MAXBGPVMASNGRP];
};

// State list used during simulation (clist, nlist)
typedef struct Nfalist Nfalist;
struct Nfalist {
	unsigned ns, lim;
	Nfastate list[FLEX_ARRAY];
};

typedef struct Nfacomp Nfacomp;
struct Nfacomp {
	Nfainst *basep;                 // base instruction pointer
	Nfainst *freep;                 // next free instruction pointer
	Nfanode  andstack[NSTACK];      // operands stack
	Uint16   grpidstack[NSTACK];    // subexpression id stack
	Uint8    opstack[NSTACK];       // operators stack
	Boolean8 lastWasAnd;            // whether last encountered term was an operand
	Uint16   nands, nops, ngrpids;  // counters inside stacks
	Uint16   nparens;               // currently encountered open parens counter
	Uint16   curgrpid;              // next group id
	jmp_buf  oops;                  // compilation error jump
};

typedef struct Nfa Nfa;
struct Nfa {
	Aspathiter spos;  // Current ASN iterator start position
	Aspathiter cur;   // Current ASN iterator position
	unsigned   nmatches;
	Nfamatch   se[MAXBGPVMASNGRP];
};

static NORETURN void comperr(Nfacomp *nc, BgpvmRet err)
{
	assert(err != BGPENOERR);

	longjmp(nc->oops, err);
}

static Nfainst *newinst(Nfacomp *nc, int t)
{
	Nfainst *i = nc->freep++;

	i->type = t;
	i->left = i->right = NULL;
	return i;
}

static void pushator(Nfacomp *nc, Asn t)
{
	if (nc->nops == NSTACK)
		comperr(nc, BGPEVMBADASMTCH);

	nc->opstack[nc->nops++]       = t;
	nc->grpidstack[nc->ngrpids++] = nc->curgrpid;
}

static Nfanode *pushand(Nfacomp *nc, Nfainst *f, Nfainst *l)
{
	if (nc->nands == NSTACK)
		comperr(nc, BGPEVMBADASMTCH);

	Nfanode *n = &nc->andstack[nc->nands++];

	n->first = f;
	n->last  = l;
	return n;
}

static Nfanode *popand(Nfacomp *nc)
{
	if(nc->nands == 0)
		comperr(nc, BGPEVMBADASMTCH);

	return &nc->andstack[--nc->nands];
}

static Uint8 popator(Nfacomp *nc)
{
	if (nc->nops == 0)
		comperr(nc, BGPEVMBADASMTCH);

	--nc->ngrpids;
	return nc->opstack[--nc->nops];
}

static void evaluntil(Nfacomp *nc, int prio)
{
	Nfanode *op1, *op2;
	Nfainst *inst1, *inst2;

	while (prio == RPAR || nc->opstack[nc->nops-1] >= prio) {
		switch (popator(nc)) {
		default: UNREACHABLE;

		case LPAR:
			op1 = popand(nc);

			inst2 = newinst(nc, RPAR);
			inst2->grpid = nc->grpidstack[nc->ngrpids-1];
			op1->last->next = inst2;

			inst1 = newinst(nc, LPAR);
			inst1->grpid = nc->grpidstack[nc->ngrpids-1];
			inst1->next = op1->first;

			pushand(nc, inst1, inst2);
			return;

		case ALT:
			op2 = popand(nc), op1 = popand(nc);

			inst2 = newinst(nc, NOP);
			op2->last->next = inst2;
			op1->last->next = inst2;

			inst1 = newinst(nc, ALT);
			inst1->right = op1->first;
			inst1->left  = op2->first;

			pushand(nc, inst1, inst2);
			break;

		case CAT:
			op2 = popand(nc), op1 = popand(nc);

			op1->last->next = op2->first;

			pushand(nc, op1->first, op2->last);
			break;

		case STAR:
			op2 = popand(nc);

			inst1 = newinst(nc, ALT);
			op2->last->next = inst1;

			inst1->right = op2->first;

			pushand(nc, inst1, inst1);
			break;

		case PLUS:
			op2 = popand(nc);

			inst1 = newinst(nc, ALT);
			op2->last->next = inst1;

			inst1->right = op2->first;

			pushand(nc, op2->first, inst1);
			break;

		case QUEST:
			op2 = popand(nc);

			inst1 = newinst(nc, ALT);
			inst2 = newinst(nc, NOP);
			inst1->left  = inst2;
			inst1->right = op2->first;

			op2->last->next = inst2;

			pushand(nc, inst1, inst2);
			break;
		}
	}
}

static void operator(Nfacomp *nc, int op)
{
	if (op == RPAR) {
		if (nc->nparens == 0)
			comperr(nc, BGPEVMBADASMTCH);

		nc->nparens--;
	}
	if (op == LPAR) {
		nc->curgrpid++;

		if (nc->curgrpid == MAXBGPVMASNGRP)
			comperr(nc, BGPEVMBADASMTCH);

		nc->nparens++;
		if (nc->lastWasAnd)
			operator(nc, CAT);  // add implicit CAT before group
	} else
		evaluntil(nc, op);

	if (op != RPAR)
		pushator(nc, op);

	// Some operators behave like operands
	nc->lastWasAnd = (op == STAR || op == QUEST || op == PLUS || op == RPAR);
}

static void operand(Nfacomp *nc, int t, Asn32 asn)
{
	if (nc->lastWasAnd)
		operator(nc, CAT);  // add implicit CAT

	Nfainst *i = newinst(nc, t);
	if (t == AS || t == NAS)
		i->asn = asn;

	pushand(nc, i, i);
	nc->lastWasAnd = TRUE;
}

static void compinit(Nfacomp *nc, Nfainst *dest)
{
	nc->nands = nc->nops = nc->ngrpids = 0;

	nc->nparens           = 0;
	nc->curgrpid          = 0;
	nc->lastWasAnd        = FALSE;
	nc->basep = nc->freep = dest;
}

static Nfainst *compile(Nfacomp *nc, const Asn *expression, size_t n)
{
	pushator(nc, BOTTOM);

	for (size_t i = 0; i < n; i++) {
		Asn asn = expression[i];

		switch (asn) {
		case ASN_START:        operand(nc,  BOL, 0); break;
		case ASN_END:          operand(nc,  EOL, 0); break;
		case ASN_ANY:          operand(nc,  ANY, 0); break;
		case ASN_STAR:         operator(nc, STAR);   break;
		case ASN_QUEST:        operator(nc, QUEST);  break;
		case ASN_PLUS:         operator(nc, PLUS);   break;
		case ASN_NEWGRP:       operator(nc, LPAR);   break;
		case ASN_ALT:          operator(nc, ALT);    break;
		case ASN_ENDGRP:       operator(nc, RPAR);   break;
		default:
			if (ISASNNOT(asn)) operand(nc, NAS, ASN(asn));
			else               operand(nc, AS,  ASN(asn));

			break;
		}
	}

	evaluntil(nc, START);
	operand(nc, STOP, 0);
	evaluntil(nc, START);
	if (nc->nparens != 0)
		comperr(nc, BGPEVMBADASMTCH);

	return nc->andstack[nc->nands - 1].first;
}

static void optimize(Bgpvm *vm, Nfacomp *nc, size_t bufsiz)
{
	assert(IS_ALIGNED(bufsiz, ALIGNMENT));
	assert(vm->hLowMark >= bufsiz);

	// Get rid of NOP chains
	for (Nfainst *i = nc->basep; i->type != STOP; i++) {
		Nfainst *j = i->next;

		while (j->type == NOP)
			j = j->next;

		i->next = j;
	}

	// Initial program allocation is an upperbound, release excess memory
	size_t siz   = (nc->freep - nc->basep) * sizeof(*nc->basep);
	size_t alsiz = ALIGN(siz, ALIGNMENT);
	assert(alsiz <= bufsiz);

	vm->hLowMark -= (bufsiz - alsiz);
	assert(IS_ALIGNED(vm->hLowMark, ALIGNMENT));
}

#ifdef DF_DEBUG_ASMTCH

static void dumpprog(const Nfainst *prog)
{
	const Nfainst *i = prog;
	while (TRUE) {
		Sys_Printf(STDERR, "%d:\t%#2x", (int) (i - prog), (unsigned) i->type);
		switch (i->type) {
		case ALT:
			Sys_Printf(STDERR, "\t%d\t%d", (int) (i->left - prog), (int) (i->right - prog));
			break;
		case AS:
			Sys_Printf(STDERR, "\tASN(%lu)\t%d", (unsigned long) beswap32(i->asn), (int) (i->next - prog));
			break;
		case NAS:
			Sys_Printf(STDERR, "\t!ASN(%lu)\t%d", (unsigned long) beswap32(i->asn), (int) (i->next - prog));
			break;
		case LPAR: case RPAR:
			Sys_Printf(STDERR, "\tGRP(%d)", (int) i->grpid);
			// FALLTHROUGH
		default:
			Sys_Printf(STDERR, "\t%d", (int) (i->next - prog));
			break;

		case NOP: case STOP:
			break;
		}

		Sys_Print(STDERR, "\n");
		if (i->type == STOP)
			break;

		i++;
	}
}

#endif

// `TRUE` if `pos` comes before `m` starting position
static Boolean isbefore(const Aspathiter *pos, const Nfamatch *m)
{
	return BGP_CURASINDEX(pos) < BGP_CURASINDEX(&m->spos);
}

// `TRUE` if `a` starts at the same position as `b`, but terminates after it
static Boolean islongermatch(const Nfamatch *a, const Nfamatch *b)
{
	return BGP_CURASINDEX(&a->spos) == BGP_CURASINDEX(&b->spos) &&
	       BGP_CURASINDEX(&a->epos) > BGP_CURASINDEX(&b->epos);
}

/* An invalid AS INDEX, there can't be a 65535 AS index,
 * Given that an AS is at least 2 bytes wide and a legal TPA segment
 * is at most of 64K (though in practice its even smaller)
 */
#define BADASIDX 0xffffu

static void clearmatch(Nfamatch *m)
{
	m->spos.asIdx = BADASIDX;
	m->epos.asIdx = 0;
}

static Boolean isnullmatch(const Nfamatch *m)
{
	return BGP_CURASINDEX(&m->spos) == BADASIDX;
}

static void copymatches(Nfamatch *dest, const Nfamatch *src)
{
	do *dest++ = *src; while (!isnullmatch(src++));
}

static Boolean addstartinst(Nfalist *clist, Nfainst *start, const Nfa *nfa)
{
	Nfastate *s;

	// Don't add the instruction twice
	for (unsigned i = 0; i < clist->ns; i++) {
		s = &clist->list[i];

		if (s->ip == start) {
			if (isbefore(&nfa->spos, &s->se[0])) {
				// Move match position
				s->se[0].spos       = nfa->spos;
				s->se[0].epos.asIdx = 0;        // so any end pos is accepted
				clearmatch(&s->se[1]);
			}

			return TRUE;
		}
	}

	if (clist->ns == clist->lim)
		return FALSE;

	// Append to list
	s     = &clist->list[clist->ns++];
	s->ip = start;

	s->se[0].spos       = nfa->spos;
	s->se[0].epos.asIdx = 0;         // so any end pos is accepted
	clearmatch(&s->se[1]);
	return TRUE;
}

static Boolean addinst(Nfalist *nlist, Nfainst *in, const Nfamatch *se)
{
	Nfastate *s;

	// Don't add the same instruction twice
	for (unsigned i = 0; i < nlist->ns; i++) {
		s = &nlist->list[i];

		if (s->ip == in) {
			if (isbefore(&se[0].spos, &s->se[0]))
				copymatches(s->se, se);

			return TRUE;
		}
	}

	if (nlist->ns == nlist->lim)
		return FALSE;  // instruction list overflow

	// Append to list
	s = &nlist->list[nlist->ns++];
	s->ip = in;
	copymatches(s->se, se);

	return TRUE;
}

static void newmatch(Nfastate *s, Nfa *nfa)
{
	// Accept the new match if it is the first one, or it comes before
	// a previous match, or if it is a longer match than the previous one
	if (nfa->nmatches == 0 ||
	    isbefore(&s->se[0].spos, &nfa->se[0]) ||
	    islongermatch(&s->se[0], &nfa->se[0])) {

		copymatches(nfa->se, s->se);
	}

	nfa->nmatches++;
}

static Judgement step(Nfalist *clist, Asn asn, Nfalist *nlist, Nfa *nfa)
{
	nlist->ns = 0;

	for (unsigned i = 0; i < clist->ns; i++) {
		Nfastate *s  = &clist->list[i];
		Nfainst  *ip = s->ip;

eval_next:
		switch (ip->type) {
		default: UNREACHABLE;

		case NOP:
			ip = ip->next;
			goto eval_next;
		case AS:
			if (ip->asn == ASN(asn) && !addinst(nlist, ip->next, s->se))
				return NG;

			break;
		case NAS:
			if (ip->asn != ASN(asn) && !addinst(nlist, ip->next, s->se))
				return NG;

			break;
		case ANY:
			if (asn != ASN_EOL && !addinst(nlist, ip->next, s->se))
				return NG;

			break;
		case BOL:
			if (asn & ASNBOLFLAG) {
				ip = ip->next;
				goto eval_next;
			}

			break;
		case EOL:
			if (asn == ASN_EOL) {
				ip = ip->next;
				goto eval_next;
			}

			break;
		case LPAR:
			for (unsigned i = 0; i < ip->grpid; i++)
				assert(!isnullmatch(&s->se[i]));

			assert(isnullmatch(&s->se[ip->grpid]));

			s->se[ip->grpid].spos = nfa->spos;
			clearmatch(&s->se[ip->grpid+1]);

			ip = ip->next;
			goto eval_next;
		case ALT:
			// Evaluate right branch later IN THIS LIST
			if (!addinst(clist, ip->right, s->se))
				return NG;

			ip = ip->left;  // take left branch now
			goto eval_next;
		case RPAR:
			for (unsigned i = 0; i < ip->grpid; i++)
				assert(!isnullmatch(&s->se[i]));

			assert(!isnullmatch(&s->se[ip->grpid]));
			assert( isnullmatch(&s->se[ip->grpid+1]));

			s->se[ip->grpid].epos = nfa->cur;
			ip = ip->next;
			goto eval_next;
		case STOP:
			// *** MATCH ***
			s->se[0].epos = nfa->cur;
			newmatch(s, nfa);
			break;
		}
	}

	return BGPENOERR;
}

static void collect(Bgpvm *vm, const Nfa *nfa)
{
	Boolean isMatching = (nfa->nmatches > 0);
	BGP_VMPUSH(vm, isMatching);

	vm->curMatch = (Bgpvmmatch *) Bgp_VmTempAlloc(vm, sizeof(*vm->curMatch));
	if (!vm->curMatch)
		return;  // out of memory

	Bgpattrseg *tpa = Bgp_GetUpdateAttributes(BGP_MSGUPDATE(vm->msg));
	assert(tpa != NULL);

	// Generate matches list
	Bgpvmasmatch  *matches  = NULL;
	Bgpvmasmatch **pmatches = &matches;
	for (unsigned i = 0; !isnullmatch(&nfa->se[i]); i++) {
		const Nfamatch *src  = &nfa->se[i];
		Bgpvmasmatch   *dest = (Bgpvmasmatch *) Bgp_VmTempAlloc(vm, sizeof(*dest));
		if (!dest)
			return;  // out of memory

		dest->next = *pmatches;
		dest->spos = src->spos;
		dest->epos = src->epos;
		*pmatches  = dest;

		pmatches = &dest->next;
	}

	vm->curMatch->pc         = BGP_VMCURPC(vm);
	vm->curMatch->tag        = 0;
	vm->curMatch->base       = tpa->attrs;
	vm->curMatch->lim        = &tpa->attrs[beswap16(tpa->len)];
	vm->curMatch->pos        = matches;
	vm->curMatch->isMatching = isMatching;
}

static BgpvmRet execute(Bgpvm *vm, Nfainst *program, unsigned listlen, Nfa *nfa)
{
	// Prepare AS PATH iterator
	BgpvmRet err = BGPENOERR;  // unless found otherwise
	if (Bgp_StartMsgRealAsPath(&nfa->cur, vm->msg) != OK) {
		vm->errCode = BGPEVMMSGERR;
		return vm->errCode;
	}

	// Setup state lists
	Nfalist *clist, *nlist, *t;

	size_t listsiz = offsetof(Nfalist, list[listlen]);
	if (listlen > LISTSIZ) {
		// Allocate on temporary memory
		clist = (Nfalist *) Bgp_VmTempAlloc(vm, listsiz);
		nlist = (Nfalist *) Bgp_VmTempAlloc(vm, listsiz);
	} else {
		// Allocate on stack
		clist = (Nfalist *) alloca(listsiz);
		nlist = (Nfalist *) alloca(listsiz);
	}
	if (!clist || !nlist)
		return vm->errCode;

	clist->lim = nlist->lim = listlen;

	// Simulate NFA, execute once per ASN (including ASN_BOL and ASN_EOL)
	nfa->nmatches = 0;        // clear result list
	clist->ns     = 0;        // clear current list for the first time
	clearmatch(&nfa->se[0]);  // by default no match

	Asn asn, flag = ASNBOLFLAG;  // first ASN is marked as BOL
	do {
		// Copy initial position to start
		nfa->spos = nfa->cur;

		// Always include first instruction if no match took place yet
		if (nfa->nmatches == 0 && !addstartinst(clist, program, nfa)) {
			// State list overflow
			err = BGPEVMASMTCHESIZE;
			break;
		}

		// Fetch new ASN
		asn = Bgp_NextAsPath(&nfa->cur);
		if (asn == -1 && Bgp_GetErrStat(NULL)) {
			err = BGPEVMMSGERR;
			break;
		}

		// Advance NFA evaluating the current ASN
		if (step(clist, asn | flag, nlist, nfa) != OK) {
			// List overflow
			err = BGPEVMASMTCHESIZE;
			break;
		}

		t = clist, clist = nlist, nlist = t;  // swap lists

		flag = 0;  // no more the first ASN
	} while (asn != -1);

	if (listlen > LISTSIZ) {
		Bgp_VmTempFree(vm, listsiz);
		Bgp_VmTempFree(vm, listsiz);
	}

	vm->errCode = err;
	return err;
}

void *Bgp_VmCompileAsMatch(Bgpvm *vm, const Asn *expression, size_t n)
{
	Nfainst *buf, *prog;

	// NOTE: Bgp_VmPermAlloc() already clears VM error and asserts !vm->isRunning

	const size_t maxsiz = ALIGN(6 * n * sizeof(*buf), ALIGNMENT);

	// we request an already aligned chunk
	// so optimize() can make accurate memory adjustments
	buf = Bgp_VmPermAlloc(vm, maxsiz);
	if (!buf)
		return NULL;

	Nfacomp nc;
	compinit(&nc, buf);

	int err;
	if ((err = setjmp(nc.oops)) != 0) {
		vm->errCode   = err;
		vm->hLowMark -= maxsiz;  // release permanent allocation
		return NULL;
	}

	prog = compile(&nc, expression, n);
	optimize(vm, &nc, maxsiz);
#ifdef DF_DEBUG_ASMTCH
	dumpprog(prog);
#endif

	// vm->errCode = BGPENOERR; - already set by Bgp_VmPermAlloc()
	return prog;
}

void Bgp_VmDoAsmtch(Bgpvm *vm)
{
	/* POPS:
	 * -1: Precompiled NFA instructions
	 *
	 * PUSHES:
	 * TRUE on successful match, FALSE otherwise
	 */

	Nfa nfa;

	if (!BGP_VMCHKSTK(vm, 1))
		return;

	Nfainst *prog = (Nfainst *) BGP_VMPOPA(vm);
	if (!prog) {
		vm->errCode = BGPEVMBADASMTCH;
		return;
	}
	if (!BGP_VMCHKMSGTYPE(vm, BGP_UPDATE)) {
		Bgp_VmStoreMsgTypeMatch(vm, /*isMatching=*/FALSE);
		return;
	}

	BgpvmRet status = execute(vm, prog, LISTSIZ, &nfa);
	if (status == BGPEVMASMTCHESIZE)
		status = execute(vm, prog, BIGLISTSIZ, &nfa);

	if (status == BGPENOERR)
		collect(vm, &nfa);
}
