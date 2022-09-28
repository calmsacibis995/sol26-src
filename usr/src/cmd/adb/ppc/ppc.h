/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ppc.h	1.8	96/06/18 SMI"

#ifndef _PPC_H_
#define _PPC_H_

#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/mmu.h>
#include <sys/psw.h>
#if defined(KADB)
#	include <sys/trap.h>
#endif /* KADB */

/*
 * adb has used "addr_t" as == "unsigned" in a typedef, forever.
 * Now in 4.0 there is suddenly a new "addr_t" typedef'd as "char *".
 *
 * About a million changes would have to be made in adb if this #define
 * weren't able to unto that damage.
 */
#define addr_t unsigned

/*
 * setreg/readreg/writereg use the adb_raddr structure to communicate
 * with one another, and with the floating point conversion routines.
 */
struct adb_raddr {
    enum reg_type { r_normal, r_floating,  r_invalid }
	     ra_type;
    int	    *ra_raddr;
};

/*
 * adb keeps its own idea of the current value of most of the
 * processor registers, in an "regs" structure.  This is used
 * in different ways for kadb, adb -k, and normal adb.  The struct
 * is defined in regs.h, and the variable (adb_regs) is decleared
 * in accessir.c.
 */

/*
 * adb's internal register codes for the sparc
 */

/* Integer Unit (IU)'s "r registers" */
/* this chunk of define's was taken from the 386i code */
#define REG_RN(n)	(n)
#define	REG_FP		R_R1	/* XXXPPC (WRONG) Frame pointer */
#define	REG_SP		R_R1	/* Stack pointer */
#define	REG_PC		R_PC

#define R_SP		REG_SP
#define R_PSR		R_MSR

#define	Reg_FP		REG_FP	/* Frame pointer */
#define	Reg_SP		REG_SP	/* Stack pointer */
#define	Reg_PC		REG_PC

#define LAST_NREG	37	/* last normal (non-Floating) register */

#define REG_FPSCR       38
#define NREGISTERS      39+32

#define	REGADDR(r)	(4 * (r))


#ifndef	REGNAMESINIT
extern
#endif /* !REGNAMESINIT */
char	*regnames[NREGISTERS]
#ifdef REGNAMESINIT
    = {
	/* IU general regs */
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9",
        "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19",
        "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29",
        "r30", "r31",
        "cr", "lr", "pc", "msr", "ctr", "xer",

	/* FPU regs */ "fpscr",
        "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9",
        "f10", "f11", "f12", "f13", "f14", "f15", "f16", "f17", "f18", "f19",
        "f20", "f21", "f22", "f23", "f24", "f25", "f26", "f27", "f28", "f29",
        "f30", "f31"
   }
#endif /* REGNAMESINIT */
    ;

#define	U_PAGE	UADDR

#define	TXTRNDSIZ	SEGSIZ

#define	MAXINT		0x7fffffff
#define	MAXFILE		0xffffffff

/*
 * All 32 bits are valid on our i86 port:  VADDR_MASK is a no-op.
 * It's only here for the sake of some shared code in kadb.
 */
#define VADDR_MASK	0xffffffff

#define	INSTACK(x)	( (x) >= STKmin && (x) <= STKmax )
int STKmax, STKmin;

/*
 * This doesn't work, since the kernel is loaded into arbitrary (though
 * contiguous on a per-segment basis) chunks of physical memory as
 * determined by the prom.
 *
 * #define	KVTOPH(x) (((x) >= KERNELBASE)? (x) - KERNELBASE: (x))
 */

/*
 * A "stackpos" contains everything we need to know in
 * order to do a stack trace.
 */
struct stackpos {
	 u_int	k_pc;		/* where we are in this proc */
	 u_int	k_fp;		/* this proc's frame pointer */
	 u_int	k_nargs;	/* # of args passed to the func */
	 u_int	k_entry;	/* this proc's entry point */
	 u_int	k_caller;	/* PC of the call that called us */
	 u_int	k_flags;	/* sigtramp & leaf info */
	 u_int	k_regloc[NREGISTERS]; /* register location */
	 u_int	k_regspace[NREGISTERS]; /* Target Addr Space OR k/adb */
#define	TARGADRSPC	1
#define	ADBADRSPC	2
};
	/* Flags for k_flags:  */
#define K_LEAF		1	/* this is a leaf procedure */
#define K_CALLTRAMP	2	/* caller is _sigtramp */
#define K_SIGTRAMP	4	/* this is _sigtramp */
#define K_TRAMPED	8	/* this was interrupted by _sigtramp */

/*
 * Useful stack frame offsets.
 */
#define FR_SAVFP	0
#define FR_SAVPC	4

/***********************************************************************/
/*
 *	Breakpoint instructions
 */

/*
 * A breakpoint instruction lives in the extern "bpt".
 * Let's be explicit about it this time.
 */
#define SZBPT 4
#define PCFUDGE (0)

#ifdef BPT_INIT
/* The breakpoint instruction is 'twi 0x1f,0,0' */
#define KADB_BP (0x0fe00000)
	int bpt = KADB_BP;
#else	/* !BPT_INIT */
extern int bpt;
#endif /* !BPT_INIT */

/***********************************************************************/
/*
 *	These defines reduce the number of #ifdefs.
 */
#define t_srcinstr(item)  (item)
#define ins_type u_long
#define	INSTR_ALIGN_MASK 3
#define	INSTR_ALIGN_ERROR "address must be aligned on a 4-byte boundary"
#define	BPT_ALIGN_ERROR "breakpoint must be aligned on a 4-byte boundary"

#endif		/* _PPC_H_ */
