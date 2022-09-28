/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)accesspr.c	1.11	96/06/18 SMI"

#include "adb.h"
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/vmmac.h>
#include <sys/signal.h>
#include <sys/ptrace.h>
#include <sys/errno.h>
#include "fpascii.h"
#include "symtab.h"
#ifndef KADB
#include "allregs.h"
#endif

/*
 * adb's idea of the current value of most of the
 * processor registers lives in "adb_regs".
 */
#ifdef KADB
struct regs adb_regs;
#define adb_oreg (adb_regs)
#define adb_ireg (adb_regs)
#define adb_lreg (adb_regs)
#else
struct allregs adb_regs;
#endif

#ifdef KADB
/* nwindow is now a variable whose value is known to kadb.
*/
extern int nwindows;

#ifdef	NWINDOW
#	undef	NWINDOW			/* do it my way */
#endif	/* NWINDOW */
#define NWINDOW nwindows	/* # of implemented windows */

#define CWP  (((adb_regs.r_psr & 15) +1) % NWINDOW)

#define adb_sp	 (adb_oreg.r_r1)

#else /* !KADB */
/*
 * Libkvm is used (by adb only) to dig things out of the kernel
 */
#include <kvm.h>
#include <sys/ucontext.h>

extern kvm_t *kvmd;					/* see main.c */
extern struct asym *trampsym;				/* see setupsr.c */


/*
 * Read a word from kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kread(addr, p)
	unsigned addr;
	int *p;
{
	db_printf(5, "kread: addr=%u, p=%X", addr, p);
	if (kvm_read(kvmd, (long) addr, (char *) p, sizeof *p) != sizeof *p)
		return -1;
	db_printf(5, "kread: success");
	return 0;
}

/*
 * Write a word to kernel virtual address space (-k only)
 * Return 0 if success, else -1.
 */
kwrite(addr, p)
	unsigned addr;
	int *p;
{
	db_printf(5, "kwrite: addr=%u, p=%X", addr, p);
	if (kvm_write(kvmd, (long)addr, (char *) p, sizeof *p) != sizeof *p)
		return -1;
	db_printf(5, "kwrite: success");
	return 0;
}
#endif /* !KADB */

extern	struct stackpos exppos;


tbia(void)
{
#ifndef KADB
	db_printf(5, "tbia: sets exppos.k_fp to 0");
	return exppos.k_fp = 0;
#else
	return 0;
#endif	
}



/*
 * Construct an informative error message
 */
static void
regerr(reg)
	int reg;
{
	static char rw_invalid[60];

	if (reg < 0  ||  reg > NREGISTERS)
	    sprintf(rw_invalid, "Invalid register number (%d)", reg);
	else
	    sprintf(rw_invalid, "Invalid register %s (%d)",
							regnames[reg], reg);
	errflg = rw_invalid;
	db_printf(3, "regerr: errflg=%s", errflg);
}

/*
 * reg_address is given an adb register code;
 * it fills in the (global)adb_raddr structure.
 * "Fills in" means that it figures out the register type
 * and the address of where adb keeps its idea of that register's
 * value (i.e., in adb's own (global)adb_regs structure).
 *
 * reg_address is called by setreg() and readreg();
 * it returns nothing.
 */
void
reg_address(reg)
	int reg;
{
#ifdef KADB
	register struct regs *arp = &adb_regs;
#else
	register struct allregs *arp = &adb_regs;
#endif
	register struct adb_raddr *ra = &adb_raddr;

	db_printf(5, "reg_address: reg=%D", reg);
	ra->ra_type = r_normal;
	ra->ra_raddr = & arp->r_r0;
	switch (reg) {
		 case REG_RN(R_R0):
			ra->ra_raddr = & arp->r_r0;
			break;
		 case REG_RN(R_R1):
			ra->ra_raddr = & arp->r_r1;
			break;
		 case REG_RN(R_R2):
			ra->ra_raddr = & arp->r_r2;
			break;
		 case REG_RN(R_R3):
			ra->ra_raddr = & arp->r_r3;
			break;
		 case REG_RN(R_R4):
			ra->ra_raddr = & arp->r_r4;
			break;
		 case REG_RN(R_R5):
			ra->ra_raddr = & arp->r_r5;
			break;
		 case REG_RN(R_R6):
			ra->ra_raddr = & arp->r_r6;
			break;
		 case REG_RN(R_R7):
			ra->ra_raddr = & arp->r_r7;
			break;
		 case REG_RN(R_R8):
			ra->ra_raddr = & arp->r_r8;
			break;
		 case REG_RN(R_R9):
			ra->ra_raddr = & arp->r_r9;
			break;
		 case REG_RN(R_R10):
			ra->ra_raddr = & arp->r_r10;
			break;
		 case REG_RN(R_R11):
			ra->ra_raddr = & arp->r_r11;
			break;
		 case REG_RN(R_R12):
			ra->ra_raddr = & arp->r_r12;
			break;
		 case REG_RN(R_R13):
			ra->ra_raddr = & arp->r_r13;
			break;
		 case REG_RN(R_R14):
			ra->ra_raddr = & arp->r_r14;
			break;
		 case REG_RN(R_R15):
			ra->ra_raddr = & arp->r_r15;
			break;
		 case REG_RN(R_R16):
			ra->ra_raddr = & arp->r_r16;
			break;
		 case REG_RN(R_R17):
			ra->ra_raddr = & arp->r_r17;
			break;
		 case REG_RN(R_R18):
			ra->ra_raddr = & arp->r_r18;
			break;
		 case REG_RN(R_R19):
			ra->ra_raddr = & arp->r_r19;
			break;
		 case REG_RN(R_R20):
			ra->ra_raddr = & arp->r_r20;
			break;
		 case REG_RN(R_R21):
			ra->ra_raddr = & arp->r_r21;
			break;
		 case REG_RN(R_R22):
			ra->ra_raddr = & arp->r_r22;
			break;
		 case REG_RN(R_R23):
			ra->ra_raddr = & arp->r_r23;
			break;
		 case REG_RN(R_R24):
			ra->ra_raddr = & arp->r_r24;
			break;
		 case REG_RN(R_R25):
			ra->ra_raddr = & arp->r_r25;
			break;
		 case REG_RN(R_R26):
			ra->ra_raddr = & arp->r_r26;
			break;
		 case REG_RN(R_R27):
			ra->ra_raddr = & arp->r_r27;
			break;
		 case REG_RN(R_R28):
			ra->ra_raddr = & arp->r_r28;
			break;
		 case REG_RN(R_R29):
			ra->ra_raddr = & arp->r_r29;
			break;
		 case REG_RN(R_R30):
			ra->ra_raddr = & arp->r_r30;
			break;
		 case REG_RN(R_R31):
			ra->ra_raddr = & arp->r_r31;
			break;
		 case REG_RN(R_CR):
			ra->ra_raddr = & arp->r_cr;	
			break;
		 case REG_RN(R_LR):
			ra->ra_raddr = & arp->r_lr;	
			break;
		 case REG_RN(R_CTR):
			ra->ra_raddr = & arp->r_ctr;
			break;
		 case REG_RN(R_XER):
			ra->ra_raddr = & arp->r_xer;
			break;
		 case REG_RN(R_PC):
			ra->ra_raddr = & arp->r_pc;
			break;
		 case REG_RN(R_MSR):
			ra->ra_raddr = & arp->r_msr;
			break;
#ifndef	KADB
		 case REG_RN(REG_FPSCR):
			ra->ra_raddr = & arp->r_fpscr;
			break;
		 case REG_RN(R_F0):
			ra->ra_raddr = (int*)& arp->r_f0;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F1):
			ra->ra_raddr = (int*)& arp->r_f1;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F2):
			ra->ra_raddr = (int*)& arp->r_f2;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F3):
			ra->ra_raddr = (int*)& arp->r_f3;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F4):
			ra->ra_raddr = (int*)& arp->r_f4;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F5):
			ra->ra_raddr = (int*)& arp->r_f5;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F6):
			ra->ra_raddr = (int*)& arp->r_f6;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F7):
			ra->ra_raddr = (int*)& arp->r_f7;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F8):
			ra->ra_raddr = (int*)& arp->r_f8;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F9):
			ra->ra_raddr = (int*)& arp->r_f9;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F10):
			ra->ra_raddr = (int*)& arp->r_f10;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F11):
			ra->ra_raddr = (int*)& arp->r_f11;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F12):
			ra->ra_raddr = (int*)& arp->r_f12;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F13):
			ra->ra_raddr = (int*)& arp->r_f13;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F14):
			ra->ra_raddr = (int*)& arp->r_f14;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F15):
			ra->ra_raddr = (int*)& arp->r_f15;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F16):
			ra->ra_raddr = (int*)& arp->r_f16;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F17):
			ra->ra_raddr = (int*)& arp->r_f17;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F18):
			ra->ra_raddr = (int*)& arp->r_f18;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F19):
			ra->ra_raddr = (int*)& arp->r_f19;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F20):
			ra->ra_raddr = (int*)& arp->r_f20;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F21):
			ra->ra_raddr = (int*)& arp->r_f21;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F22):
			ra->ra_raddr = (int*)& arp->r_f22;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F23):
			ra->ra_raddr = (int*)& arp->r_f23;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F24):
			ra->ra_raddr = (int*)& arp->r_f24;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F25):
			ra->ra_raddr = (int*)& arp->r_f25;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F26):
			ra->ra_raddr = (int*)& arp->r_f26;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F27):
			ra->ra_raddr = (int*)& arp->r_f27;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F28):
			ra->ra_raddr = (int*)& arp->r_f28;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F29):
			ra->ra_raddr = (int*)& arp->r_f29;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F30):
			ra->ra_raddr = (int*)& arp->r_f30;
			ra->ra_type = r_floating;
			break;
		 case REG_RN(R_F31):
			ra->ra_raddr = (int*)& arp->r_f31;
			ra->ra_type = r_floating;
			break;
#endif	/* KADB */
		 default:
			regerr(reg);
			ra->ra_type = r_invalid;
			break;
        }
	db_printf(2, "reg_address: reg=%D, name='%s'\n\tra_type=%D, ra_raddr=%X, *(ra_raddr)=%X",
		   reg, (reg >= 0 && reg < LAST_NREG) ? regnames[reg] : "?",
			adb_raddr.ra_type, adb_raddr.ra_raddr,
						*(adb_raddr.ra_raddr));
}

setreg(reg, val)
	int reg;
	int val;
{
	db_printf(4, "setreg: reg=%D, val=%D", reg, val);
	reg_address(reg);
	switch (adb_raddr.ra_type) {
		 case r_floating: 
#ifndef	KADB
			*(double*)(adb_raddr.ra_raddr) = (double)val;
			break;	
#endif	/* KADB */
		 case r_normal: /* Normal one -- we have a good address */
			*(adb_raddr.ra_raddr) = val;
			if (reg == REG_PC) {
				userpc = (addr_t)val;
				db_printf(2, "setreg: userPC=%X", userpc);
			}
			break;
		 default:	/* should never get here */
			db_printf(2, "setreg: bogus ra_type: %D\n",
							adb_raddr.ra_type);
			break;
	}
}


/*
 * readreg -- retrieve value of register reg from adb_regs.
 */
readreg(reg)
	int reg;
{
	int val = 0;

	db_printf(4, "readreg:  reg=%D, ra_raddr=%X",
					    reg, adb_raddr.ra_raddr);
	reg_address(reg);
	switch (adb_raddr.ra_type) {
		 case r_floating: 
#ifndef	KADB
			val = (int)adb_raddr.ra_raddr; /* return address */
			break;	
#endif	/* KADB */
		 case r_normal: /* Normal one -- we have a good address */
			val = *(adb_raddr.ra_raddr);
			break;
		 default:	/* should never get here */
			db_printf(2, "readreg: bogus ra_type: %D\n",
							adb_raddr.ra_type);
			break;
	}
	db_printf(4, "readreg:  returns %X", val);
	return val;
}

/*
 * For ptrace(SETREGS or GETREGS) to work, the registers must be in
 * the form that they take in the core file (instead of the form used
 * by the access routines in this file, i.e., the full machine state).
 * These routines copy the relevant registers.
 */
regs_to_core(void)
{
#ifdef KADB
	struct regs *a = &adb_regs;
#else
	struct allregs *a = &adb_regs;
#endif

	Prstatus.pr_lwp.pr_reg[R_R0] = a->r_r0;
	Prstatus.pr_lwp.pr_reg[R_R1] = a->r_r1;
	Prstatus.pr_lwp.pr_reg[R_R2] = a->r_r2;
	Prstatus.pr_lwp.pr_reg[R_R3] = a->r_r3;
	Prstatus.pr_lwp.pr_reg[R_R4] = a->r_r4;
	Prstatus.pr_lwp.pr_reg[R_R5] = a->r_r5;
	Prstatus.pr_lwp.pr_reg[R_R6] = a->r_r6;
	Prstatus.pr_lwp.pr_reg[R_R7] = a->r_r7;
	Prstatus.pr_lwp.pr_reg[R_R8] = a->r_r8;
	Prstatus.pr_lwp.pr_reg[R_R9] = a->r_r9;
	Prstatus.pr_lwp.pr_reg[R_R10] = a->r_r10;
	Prstatus.pr_lwp.pr_reg[R_R11] = a->r_r11;
	Prstatus.pr_lwp.pr_reg[R_R12] = a->r_r12;
	Prstatus.pr_lwp.pr_reg[R_R13] = a->r_r13;
	Prstatus.pr_lwp.pr_reg[R_R14] = a->r_r14;
	Prstatus.pr_lwp.pr_reg[R_R15] = a->r_r15;
	Prstatus.pr_lwp.pr_reg[R_R16] = a->r_r16;
	Prstatus.pr_lwp.pr_reg[R_R17] = a->r_r17;
	Prstatus.pr_lwp.pr_reg[R_R18] = a->r_r18;
	Prstatus.pr_lwp.pr_reg[R_R19] = a->r_r19;
	Prstatus.pr_lwp.pr_reg[R_R20] = a->r_r20;
	Prstatus.pr_lwp.pr_reg[R_R21] = a->r_r21;
	Prstatus.pr_lwp.pr_reg[R_R22] = a->r_r22;
	Prstatus.pr_lwp.pr_reg[R_R23] = a->r_r23;
	Prstatus.pr_lwp.pr_reg[R_R24] = a->r_r24;
	Prstatus.pr_lwp.pr_reg[R_R25] = a->r_r25;
	Prstatus.pr_lwp.pr_reg[R_R26] = a->r_r26;
	Prstatus.pr_lwp.pr_reg[R_R27] = a->r_r27;
	Prstatus.pr_lwp.pr_reg[R_R28] = a->r_r28;
	Prstatus.pr_lwp.pr_reg[R_R29] = a->r_r29;
	Prstatus.pr_lwp.pr_reg[R_R30] = a->r_r30;
	Prstatus.pr_lwp.pr_reg[R_R31] = a->r_r31;
	Prstatus.pr_lwp.pr_reg[R_CR] = a->r_cr;
	Prstatus.pr_lwp.pr_reg[R_LR] = a->r_lr;
	Prstatus.pr_lwp.pr_reg[R_CTR] = a->r_ctr;
	Prstatus.pr_lwp.pr_reg[R_XER] = a->r_xer;
	Prstatus.pr_lwp.pr_reg[R_PC] = a->r_pc;
	Prstatus.pr_lwp.pr_reg[R_MSR] = a->r_msr;
#ifndef	KADB
	if (fpa_avail) {
		Prfpregs.fpu_fpscr = a->r_fpscr;
		Prfpregs.fpu_regs[R_F0 - R_F0] = a->r_f0;
		Prfpregs.fpu_regs[R_F1 - R_F0] = a->r_f1;
		Prfpregs.fpu_regs[R_F2 - R_F0] = a->r_f2;
		Prfpregs.fpu_regs[R_F3 - R_F0] = a->r_f3;
		Prfpregs.fpu_regs[R_F4 - R_F0] = a->r_f4;
		Prfpregs.fpu_regs[R_F5 - R_F0] = a->r_f5;
		Prfpregs.fpu_regs[R_F6 - R_F0] = a->r_f6;
		Prfpregs.fpu_regs[R_F7 - R_F0] = a->r_f7;
		Prfpregs.fpu_regs[R_F8 - R_F0] = a->r_f8;
		Prfpregs.fpu_regs[R_F9 - R_F0] = a->r_f9;
		Prfpregs.fpu_regs[R_F10 - R_F0] = a->r_f10;
		Prfpregs.fpu_regs[R_F11 - R_F0] = a->r_f11;
		Prfpregs.fpu_regs[R_F12 - R_F0] = a->r_f12;
		Prfpregs.fpu_regs[R_F13 - R_F0] = a->r_f13;
		Prfpregs.fpu_regs[R_F14 - R_F0] = a->r_f14;
		Prfpregs.fpu_regs[R_F15 - R_F0] = a->r_f15;
		Prfpregs.fpu_regs[R_F16 - R_F0] = a->r_f16;
		Prfpregs.fpu_regs[R_F17 - R_F0] = a->r_f17;
		Prfpregs.fpu_regs[R_F18 - R_F0] = a->r_f18;
		Prfpregs.fpu_regs[R_F19 - R_F0] = a->r_f19;
		Prfpregs.fpu_regs[R_F20 - R_F0] = a->r_f20;
		Prfpregs.fpu_regs[R_F21 - R_F0] = a->r_f21;
		Prfpregs.fpu_regs[R_F22 - R_F0] = a->r_f22;
		Prfpregs.fpu_regs[R_F23 - R_F0] = a->r_f23;
		Prfpregs.fpu_regs[R_F24 - R_F0] = a->r_f24;
		Prfpregs.fpu_regs[R_F25 - R_F0] = a->r_f25;
		Prfpregs.fpu_regs[R_F26 - R_F0] = a->r_f26;
		Prfpregs.fpu_regs[R_F27 - R_F0] = a->r_f27;
		Prfpregs.fpu_regs[R_F28 - R_F0] = a->r_f28;
		Prfpregs.fpu_regs[R_F29 - R_F0] = a->r_f29;
		Prfpregs.fpu_regs[R_F30 - R_F0] = a->r_f30;
		Prfpregs.fpu_regs[R_F31 - R_F0] = a->r_f31;
	}
#endif	/* KADB */
	db_printf(4, "regs_to_core: copied regs from adb_regs to Prstatus.pr_lwp.pr_reg[]");
	return 0;
}

#ifndef KADB

core_to_regs()
{
	const int mask = 0xffff;	/* mask off selector registers */
	register int reg;
	register struct allregs *a = &adb_regs;

	a->r_r0 = Prstatus.pr_lwp.pr_reg[R_R0];
	a->r_r1 = Prstatus.pr_lwp.pr_reg[R_R1];
	a->r_r2 = Prstatus.pr_lwp.pr_reg[R_R2];
	a->r_r3 = Prstatus.pr_lwp.pr_reg[R_R3];
	a->r_r4 = Prstatus.pr_lwp.pr_reg[R_R4];
	a->r_r5 = Prstatus.pr_lwp.pr_reg[R_R5];
	a->r_r6 = Prstatus.pr_lwp.pr_reg[R_R6];
	a->r_r7 = Prstatus.pr_lwp.pr_reg[R_R7];
	a->r_r8 = Prstatus.pr_lwp.pr_reg[R_R8];
	a->r_r9 = Prstatus.pr_lwp.pr_reg[R_R9];
	a->r_r10 = Prstatus.pr_lwp.pr_reg[R_R10];
	a->r_r11 = Prstatus.pr_lwp.pr_reg[R_R11];
	a->r_r12 = Prstatus.pr_lwp.pr_reg[R_R12];
	a->r_r13 = Prstatus.pr_lwp.pr_reg[R_R13];
	a->r_r14 = Prstatus.pr_lwp.pr_reg[R_R14];
	a->r_r15 = Prstatus.pr_lwp.pr_reg[R_R15];
	a->r_r16 = Prstatus.pr_lwp.pr_reg[R_R16];
	a->r_r17 = Prstatus.pr_lwp.pr_reg[R_R17];
	a->r_r18 = Prstatus.pr_lwp.pr_reg[R_R18];
	a->r_r19 = Prstatus.pr_lwp.pr_reg[R_R19];
	a->r_r20 = Prstatus.pr_lwp.pr_reg[R_R20];
	a->r_r21 = Prstatus.pr_lwp.pr_reg[R_R21];
	a->r_r22 = Prstatus.pr_lwp.pr_reg[R_R22];
	a->r_r23 = Prstatus.pr_lwp.pr_reg[R_R23];
	a->r_r24 = Prstatus.pr_lwp.pr_reg[R_R24];
	a->r_r25 = Prstatus.pr_lwp.pr_reg[R_R25];
	a->r_r26 = Prstatus.pr_lwp.pr_reg[R_R26];
	a->r_r27 = Prstatus.pr_lwp.pr_reg[R_R27];
	a->r_r28 = Prstatus.pr_lwp.pr_reg[R_R28];
	a->r_r29 = Prstatus.pr_lwp.pr_reg[R_R29];
	a->r_r30 = Prstatus.pr_lwp.pr_reg[R_R30];
	a->r_r31 = Prstatus.pr_lwp.pr_reg[R_R31];
	a->r_cr = Prstatus.pr_lwp.pr_reg[R_CR];
	a->r_lr = Prstatus.pr_lwp.pr_reg[R_LR];
	a->r_ctr = Prstatus.pr_lwp.pr_reg[R_CTR];
	a->r_xer = Prstatus.pr_lwp.pr_reg[R_XER];
	a->r_pc = Prstatus.pr_lwp.pr_reg[R_PC];
	a->r_msr = Prstatus.pr_lwp.pr_reg[R_MSR];
#ifndef	KADB
	if (fpa_avail) {
		a->r_fpscr = Prfpregs.fpu_fpscr;
		a->r_f0 = Prfpregs.fpu_regs[R_F0 - R_F0];
		a->r_f1 = Prfpregs.fpu_regs[R_F1 - R_F0];
		a->r_f2 = Prfpregs.fpu_regs[R_F2 - R_F0];
		a->r_f3 = Prfpregs.fpu_regs[R_F3 - R_F0];
		a->r_f4 = Prfpregs.fpu_regs[R_F4 - R_F0];
		a->r_f5 = Prfpregs.fpu_regs[R_F5 - R_F0];
		a->r_f6 = Prfpregs.fpu_regs[R_F6 - R_F0];
		a->r_f7 = Prfpregs.fpu_regs[R_F7 - R_F0];
		a->r_f8 = Prfpregs.fpu_regs[R_F8 - R_F0];
		a->r_f9 = Prfpregs.fpu_regs[R_F9 - R_F0];
		a->r_f10 = Prfpregs.fpu_regs[R_F10 - R_F0];
		a->r_f11 = Prfpregs.fpu_regs[R_F11 - R_F0];
		a->r_f12 = Prfpregs.fpu_regs[R_F12 - R_F0];
		a->r_f13 = Prfpregs.fpu_regs[R_F13 - R_F0];
		a->r_f14 = Prfpregs.fpu_regs[R_F14 - R_F0];
		a->r_f15 = Prfpregs.fpu_regs[R_F15 - R_F0];
		a->r_f16 = Prfpregs.fpu_regs[R_F16 - R_F0];
		a->r_f17 = Prfpregs.fpu_regs[R_F17 - R_F0];
		a->r_f18 = Prfpregs.fpu_regs[R_F18 - R_F0];
		a->r_f19 = Prfpregs.fpu_regs[R_F19 - R_F0];
		a->r_f20 = Prfpregs.fpu_regs[R_F20 - R_F0];
		a->r_f21 = Prfpregs.fpu_regs[R_F21 - R_F0];
		a->r_f22 = Prfpregs.fpu_regs[R_F22 - R_F0];
		a->r_f23 = Prfpregs.fpu_regs[R_F23 - R_F0];
		a->r_f24 = Prfpregs.fpu_regs[R_F24 - R_F0];
		a->r_f25 = Prfpregs.fpu_regs[R_F25 - R_F0];
		a->r_f26 = Prfpregs.fpu_regs[R_F26 - R_F0];
		a->r_f27 = Prfpregs.fpu_regs[R_F27 - R_F0];
		a->r_f28 = Prfpregs.fpu_regs[R_F28 - R_F0];
		a->r_f29 = Prfpregs.fpu_regs[R_F29 - R_F0];
		a->r_f30 = Prfpregs.fpu_regs[R_F30 - R_F0];
		a->r_f31 = Prfpregs.fpu_regs[R_F31 - R_F0];
	}
#endif	/* KADB */
	db_printf(4, "core_to_regs: copied regs from Prstatus.pr_lwp.pr_reg[] to adb_regs");
	return 0;
} 
#endif



 
writereg(i, val)
	register int i, val;
{
#ifndef KADB
	extern fpa_avail;
#endif

	db_printf(4, "writereg: i=%D, val=%D", i, val);
#ifndef KADB
	if (i < 0 || i >= NREGISTERS) {
		errno = EIO;
		return 0;
	}
#endif
	setreg(i, val);

#ifdef KADB
	ptrace(PTRACE_SETREGS, pid, &adb_regs, 0, 0);
	ptrace(PTRACE_GETREGS, pid, &adb_regs, 0, 0);
#else /* !KADB */
	regs_to_core();

	if (ptrace(PTRACE_SETREGS, pid, &Prstatus.pr_lwp.pr_reg, 0) == -1) {
		perror(corfil);
		db_printf(3, "writereg: ptrace failed, errno=%D", errno);
	}
	if (ptrace(PTRACE_GETREGS, pid, &Prstatus.pr_lwp.pr_reg, 0) == -1) {
		perror(corfil);
		db_printf(3, "writereg: ptrace failed, errno=%D", errno);
	}
	db_printf(2, "writereg: PC=%X", Prstatus.pr_lwp.pr_reg[R_PC]);

	if (fpa_avail) {
		if (ptrace(PTRACE_SETFPREGS, pid, &Prfpregs, 0) == -1) {
			perror(corfil);
			db_printf(3, "writereg: ptrace failed, errno=%D",
									errno);
		}
		if (ptrace(PTRACE_GETFPREGS, pid, &Prfpregs, 0) == -1) {
			perror(corfil);
			db_printf(3, "writereg: ptrace failed, errno=%D",
									errno);
		}
	}
	core_to_regs();

#endif /* !KADB */

	db_printf(2, "writereg: i=%D, val=%X, readreg(i)=%X",
						i, val, readreg(i)) ;
	return sizeof(int);
}

/************** stack trace support functions ************/

#define	MFLR(i)		(i == ((31 << 26) | (0x100 << 11) | (339 << 1)))
#define	STW_R0(i)	(i == 0x90010004)
#define	STWU(i)		((i & 0xffff0000) == \
			    (((unsigned)(37 << 10) | 0x21) << 16))
#define	STW(i)		((((i >> 26) & 0x3f) == 36) && \
			 (STW_OFF(i) < 0x8000) && \
			 (((i >> 16) & 0x1f) == 1))
#define	STW_FROM(i)	((i >> 21) & 0x1f)
#define	STW_OFF(i)	(i & 0xffff)
#define	MR(i)		(((i & 0xfc000000) == (unsigned)(24 << 26)) && \
			 ((i & 0xffff) == 0) && \
			 (MR_TO(i) >= 13) && \
			 (MR_FROM(i) <= 10))
#define	MR_TO(i)	((i >> 16) & 0x1f)
#define	MR_FROM(i)	((i >> 21) & 0x1f)

static u_long
preamble(u_long start_pc)
{
	long inst;

	while (1) {
		inst = get(start_pc, ISP);
		if (MFLR(inst) || STWU(inst) || STW_R0(inst))
			start_pc += 4;
		else
			break;
	}
	return (start_pc);
}

static int
use_lr(struct stackpos *sp)
{
	long inst;
	u_long start_pc = sp->k_entry;

	if (start_pc == MAXINT)
		return(0);
	while (1) {
		if (start_pc == sp->k_pc)
			break;
		inst = get(start_pc, ISP);
		if (MFLR(inst) || STWU(inst) || STW_R0(inst)) {
			if (STWU(inst))
				return (0);
			start_pc += 4;
		} else
			break;
	}
	return (1);
}

/*
 * Modify k_regloc based on "stw nonvol,N(sp)" reverse execution.
 * Do not reverse execute beyond endpc if endpc is non-zero.
 * Return value is the pc after all of the "stw" instructions.
 */
static u_long
restore_nonvols(struct stackpos *sp, u_long pc, int restore_reg, u_long endpc)
{
	long inst;

	while (1) {
		if (endpc && (pc >= endpc))
			break;
		inst = get(pc, ISP);
		if (STW(inst)) {
			if (restore_reg) {
				sp->k_regloc[STW_FROM(inst)] =
				    sp->k_fp + STW_OFF(inst);
				sp->k_regspace[STW_FROM(inst)] =
				    TARGADRSPC;
			}
			pc += 4;
		} else
			break;
	}
	return (pc);
}

static void
scan_mr(struct stackpos *sp, u_long cur_pc, int do_move, u_long endpc)
{
	long inst;
	int max_arg_reg = 0;
	int tmp_reg;

	while (1) {
		if (endpc && (cur_pc >= endpc))
			break;
		inst = get(cur_pc, ISP);
		if (!MR(inst))
			break;
		tmp_reg = MR_FROM(inst);
		if (tmp_reg >= max_arg_reg)
			max_arg_reg = tmp_reg;
		if (do_move) {
			sp->k_regloc[tmp_reg] = sp->k_regloc[MR_TO(inst)];
			sp->k_regspace[tmp_reg] = sp->k_regspace[MR_TO(inst)];
		}
		cur_pc += 4;
	}
	sp->k_nargs = (max_arg_reg >= 3) ? max_arg_reg - 2 : 0;
}

/*
 *	Assume current_regs is valid.  Analyze function preamble
 *	to restore argument registers, and determine the number
 *	of arguments (as best we can).
 */

static void
restore_args(struct stackpos *sp)
{
	u_long cur_pc;
	u_long start_pc;
	int tmp_reg;

	for (tmp_reg = 3; tmp_reg <= 10; tmp_reg++)
		sp->k_regloc[tmp_reg] = 0;

	if ((start_pc = sp->k_entry) == MAXINT)
		return;

	start_pc = preamble(start_pc);

	/* move beyond the non-volatile register saving */
	cur_pc = restore_nonvols(sp, start_pc, 0, 0);

	/* reverse execute the move register instructions */
	(void) scan_mr(sp, cur_pc, 1, 0);

	/* restore non-volatile registers */
	(void) restore_nonvols(sp, start_pc, 1, 0);
}

/*
 * Assume k_entry is the entry point of interest.  Make no
 * assumptions about whether the stack frame has been created
 * yet.  Do not reverse execute beyond k_pc.  If this function
 * has created a stack frame, adjust k_fp to point to the next
 * stack frame.
 */
static void
restore_nonvol_top(struct stackpos *sp)
{
	u_long start_pc;
	u_long cur_pc;
	long inst;
	int adjust_fp = 0;

	/* find function entry */
	start_pc = sp->k_entry;
	if (start_pc == MAXINT)
		return;

	while (1) {
		if (start_pc == sp->k_pc)
			goto done;
		inst = get(start_pc, ISP);
		if (MFLR(inst) || STWU(inst) || STW_R0(inst)) {
			if (STWU(inst))
				adjust_fp = 1;
			start_pc += 4;
		} else
			break;
	}
	if (start_pc == sp->k_pc)
		goto done;

	/* move beyond the non-volatile register saving */
	cur_pc = restore_nonvols(sp, start_pc, 0, sp->k_pc);

	/* reverse execute the move register instructions */
	(void) scan_mr(sp, cur_pc, 1, sp->k_pc);

	/* restore the non-volatile registers */
	(void) restore_nonvols(sp, start_pc, 1, sp->k_pc);

done:
	if (adjust_fp)
		sp->k_fp = get(sp->k_fp, DSP);
}

/*
 * Assume k_entry is the entry point of interest.  Determine
 * how many arguments there might be based on "mr nonvol,argreg"
 * instructions found in the function preamble.
 */
static void
get_nargs(struct stackpos *sp)
{
	u_long start_pc;
	long inst;
	int max_arg_reg = 0;
	int tmp_reg;

	sp->k_nargs = 0;
	if (sp->k_entry == MAXINT)
		return;
	start_pc = preamble(sp->k_entry);

	/* move pass the non-volatile register saving */
	start_pc = restore_nonvols(sp, start_pc, 0, 0);
	scan_mr(sp, start_pc, 0, 0);
}

/*
 * stacktop collects information about the topmost (most recently
 * called) stack frame into its (struct stackpos) argument.
 */

stacktop(sp)
	register struct stackpos *sp;
{
	register int i;
	int oldfp;

	db_printf(4, "stacktop: sp=%X", sp);
	db_printf(1, "stacktop: XXX leaf routines? Re: above comment");
	sp->k_pc = readreg(REG_PC);
	sp->k_fp = readreg(REG_FP);
	oldfp = sp->k_fp;
	sp->k_flags = 1;
	sp->k_nargs = 0;
	errflg = NULL;
	for (i = 0; i < NREGISTERS; i++) {
		sp->k_regloc[i] = REGADDR(i) + (int)&adb_regs;
		sp->k_regspace[i] = ADBADRSPC;
	}
	i = findentry(sp);
	restore_nonvol_top(sp);
	get_nargs(sp);
	return (oldfp);
}

/* 
 * set the k_entry field in the stackpos structure, based on k_pc.
 */

findentry(sp)
	register struct stackpos *sp;
{
	db_printf(4, "findentry: sp=%X", sp);
	/*
	 * XXX - In the sparc code there is a set of routines (tramp*)
	 * to handle situations where the stack trace includes a caught
	 * signal.  Don't we need them here?
	 */
	if ((findsym(sp->k_pc, ISYM) == MAXINT) ||
	    strcmp(cursym->s_name, "PR_SCALE") == 0)
		sp->k_entry = MAXINT;
	else
		sp->k_entry = cursym->s_value;
	return 0;
}

/*
 * nextframe replaces the info in sp with the info appropriate
 * for the next frame "up" the stack (the current routine's caller).
 */
nextframe(sp)
	register struct stackpos *sp;
{
	int i;

	/*
	 * Find our entry point. Then find out
	 * which registers we saved, and map them.
	 * Our entry point is the address our caller called.
	 */

	db_printf(4, "nextframe: sp=%X", sp);

	/*
	 * find caller's pc and fp
	 */
	if (sp->k_flags && use_lr(sp)) {
		if (sp->k_regspace[R_LR] == ADBADRSPC)
			sp->k_pc = *(long *)sp->k_regloc[R_LR];
		else
			sp->k_pc = get(sp->k_regloc[R_LR], DSP);
	}
	else
		sp->k_pc = get(sp->k_fp + FR_SAVPC, DSP);
	db_printf(2, "nextframe: sp->k_pc=%X, sp->k_fp=%X",
						sp->k_pc, sp->k_fp);
	if (sp->k_pc == 0) {
		sp->k_fp = 0;
		return (0);
	}

	/* 
	 * now that we have assumed the identity of our caller, find
	 * how many longwords of argument WE were called with.
	 */
	sp->k_flags = 0;
	errflg = NULL;
	(void) findentry(sp);
	restore_args(sp);

	sp->k_fp = get(sp->k_fp + FR_SAVFP, DSP);
	errflg = NULL;
	if ((sp->k_fp == 0xffffffff) || ((sp->k_fp & 0x3) != 0))
		sp->k_fp = 0;

	db_printf(4, "nextframe: returns sp->k_fp=%X", sp->k_fp);
	return sp->k_fp;
}


#ifdef	KADB

#define	LWZ(i)		((((i >> 26) & 0x3f) == 32) && \
			 (((i >> 16) & 0x1f) == 1))
#define	LWZ_FROM(i)	((i >> 21) & 0x1f)
#define	LWZ_OFF(i)	(i & 0xffff)

/*
 * Restore non-volatile register saved in kernel's resume()
 */
from_resume(struct stackpos *sp, long fp)
{
	u_long start_pc;
	long inst;

	/* find function entry */
	if (lookup("resume_exit") == 0)
		return;

	start_pc = cursym->s_value;

	while (1) {
		inst = get(start_pc, ISP);
		if (!LWZ(inst))
			break;
		sp->k_regloc[LWZ_FROM(inst)] =
		    fp + LWZ_OFF(inst);
		sp->k_regspace[LWZ_FROM(inst)] = TARGADRSPC;
		start_pc += 4;
	}
	restore_args(sp);
}
#endif
