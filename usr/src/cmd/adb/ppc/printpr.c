/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)printpr.c 1.15	96/06/18 SMI"

/*
 * adb - PowerPC specific print routines
 *	printregs
 *	printstack
 *	fp_print
 */
#ifndef KADB
#include <sys/ptrace.h>
#include <ieeefp.h>
#include <string.h>
#include <stdlib.h>
#endif
#include "adb.h"
#include "symtab.h"
#ifndef KADB
#include "allregs.h"
#include "fpascii.h"
#endif
#include <sys/stack.h>

int fpa_disasm = 0, fpa_avail = 0, mc68881 = 0;
int nprflag = 0;

#ifndef KADB
static void
print_reg_val(const char *fmt, const int i, const int val)
{
	if ((kernel == LIVE || kernel == CMN_ERR_PANIC) && /* volatile regs */
		((i >= R_R3) && (i < R_R11))) {
		(void) printf("?");
		return;
	} else
		(void) printf(fmt, val);

	valpr(val, DSP);

	return;
}
#endif

printregs(void)
{
	register int i, j, val;
#ifdef KADB
        register short *dtp;
        short dt[3];
        char buf[257];
#else
	static void print_reg_val(const char *, const int, const int);
#endif

	db_printf(5, "printregs: called");
	j = (LAST_NREG + 1) / 2;
	for (i = 0; i < j; i++) {
		val = readreg(i);
#ifdef KADB
		printf("%s%5t%R", regnames[i], val);
		printf("%18t");
		valpr(val, DSP);
	
		if (j + i > LAST_NREG)
			break;
		printf("%40t");
		val = readreg(j + i);
		printf("%s%5t%R", regnames[j + i], val);
		printf("%58t");
		valpr(val, DSP);
#else
		(void)printf("%s%8t", regnames[i]);
		print_reg_val("%R  ", i, val);
		if (i != (LAST_NREG+1)/2) {
			register int j = i + LAST_NREG/2 + 1;

			val = readreg(j);
			(void) printf("%40t%s%8t", regnames[j]);
			print_reg_val("%R  ", j, val);
		}
#endif
		(void) printf("\n");
	}
#ifdef KADB
	printf("\n");
#else
	(void) printf("\n");
#endif
	(void) print_dis(Reg_PC, 0);
}

/*
 * look at the procedure prolog of the current called procedure.
 * figure out which registers we saved, and where they are
 */

findregs(sp, addr)
	register struct stackpos *sp;
	register caddr_t addr;
{
	/* this is too messy to even think about right now */
	db_printf(1, "findregs: XXX not done yet!");
}

/*
 * printstack -- implements "$c" and "$C" commands:  print out the
 * stack frames.  printstack was moved from print.c due to heavy
 * machine-dependence.
 */

printstack(modif)
	int modif;
{
	int i, val, nargs, spa;
	int oldfp;
	int skipframe, regs_lr;
	char *name, savedname[256];
	struct stackpos pos;
	struct asym *savesym;
#ifdef KADB
	struct regs regs_trap;
#else
	struct allregs regs_trap;
#endif

	
	db_printf(4, "printstack: modif='%c'", modif);
	if (hadaddress) {
		int delfp;
		pos.k_fp = address;
		oldfp = pos.k_fp;
		pos.k_fp = get(pos.k_fp + FR_SAVFP, DSP);
		delfp = pos.k_fp - oldfp;
		if ((delfp < 0) || (delfp > 0x100000) || (errflg != NULL))
			return;
		pos.k_pc = get(pos.k_fp + FR_SAVPC, DSP);
		if (pos.k_pc == 0 || errflg != NULL)
			return;
		pos.k_nargs = 0;
		pos.k_entry = MAXINT;
		/* sorry, we cannot find our registers without knowing our pc */
		for (i = 0; i < NREGISTERS; pos.k_regloc[i++] = 0)
			;
		findentry(&pos);
		(void) findsym(pos.k_pc, ISYM);
#ifdef	KADB
		if (cursym && (strcmp(cursym->s_name, "swtch") == 0))
			from_resume(&pos, oldfp);
		oldfp = pos.k_fp;
		pos.k_fp = get(pos.k_fp + FR_SAVFP, DSP);
		delfp = pos.k_fp - oldfp;
		if ((delfp < 0) || (delfp > 0x100000) || (errflg != NULL))
			return;
		db_printf(2, "printstack: pos.k_fp=%X, pos.k_nargs=0, pos.k_pc=MAXINT, pos.k_entry=MAXINT", address);
#endif
	} else
		oldfp = stacktop(&pos);

	skipframe = 0;
	regs_lr = 0;
	errflg = NULL;
	while (count) {
		count--;
		chkerr();
		if (regs_lr != 0 && skipframe == 1) {
			pos.k_pc = regs_lr;
			regs_lr = 0;
			skipframe = 0;
		}
		db_printf(2, "printstack: pos.k_pc %s MAXINT", 
					(pos.k_pc == MAXINT) ? "==" : "!=");
		if (pos.k_pc == MAXINT) {
			name = "?";
			pos.k_pc = 0;
		} else {
			val =  findsym(pos.k_pc, ISYM);
			db_printf(2, "printstack: cursym=%X", cursym);
			if (cursym &&
			    ((strcmp(cursym->s_name, "Syslimit") == 0) ||
			     (strcmp(cursym->s_name, "SMALL_BLK_SIZE") == 0))) {
				name = "0";
				val = pos.k_pc;
			} else if (cursym) {
				name = cursym->s_name;
			} else {
				name = "?";
				val = MAXINT;
			}
		}

		if (skipframe != 1 && modif == 'C')
			printf("%X ", oldfp);
		if (skipframe != 1) printf("%s(", name);
		db_printf(2, "printstack: pos.k_nargs=%D", pos.k_nargs);
		if (skipframe != 1 && (nargs = pos.k_nargs)) {
			u_int *ip;
			u_int *spp = &pos.k_regspace[R_R3];
			for (ip = &pos.k_regloc[R_R3]; --nargs >= 0; ip++) {
				if (*ip == 0)
					printf("?");
				else if (*spp == TARGADRSPC)
					printf("%X", get(*ip, DSP));
				else
					printf("%X", *(long *)*ip);
				if (nargs > 0)
					printf(", ");
				spp++;
			}
		}
		if (val == MAXINT) {
			if (skipframe != 1) printf(") at %X\n", pos.k_pc);
		} else
			if (skipframe != 1) printf(") + %X\n", val);
		if (skipframe)
			skipframe--;
#ifdef KADB
/* XXXPPC */

		/*
 		 * This code will try to handle all the cases that put us
		 * in the kernel.
		 * 1.) An exception (ie trap).
		 * 2.) A system call.
		 * 3.) A hardware interrupt.
		 * 4.) We started up Solaris from boot
		 */
		if ((strcmp(name, "trap") == 0) ||
		    (strcmp (name, "sys_call") == 0)) {
			
			/*
			 * At this point k_fp points to the bp from locore
			 * which points to the regs structure.
			 */
			int Ltrapargs = pos.k_fp; /* add of pointer to reg struct pointer */

			/*
			 * Now get the next stack frame from the regs struct
			 */
			if (strcmp(name, "sys_call") == 0) {
				u_int *ip = (u_int *)(Ltrapargs + MINFRAME);
				printf("   system call number %D,", 
					get(ip, DSP));
				ip += 3;
				printf(" arguments %%r3 - %%r8 are:\n      ");
				for (i = 3; i <= 8; i++) {
					printf(" %X", get(ip, DSP));
					ip++;
				}
				printf("\n");
			}

			if (strcmp(name, "trap") == 0) {
			    int type = get(pos.k_regloc[R_R4], DSP) & 0xffff;
			    char *nm;
			    switch (type) {
				case T_RESET:
				    nm = "reset"; break;
				case T_MACH_CHECK:
				    nm = "machine check"; break;
				case T_DATA_FAULT:
				    nm = "data access"; break;
				case T_TEXT_FAULT:
				    nm = "instruction access"; break;
				case T_INTERRUPT:
				    nm = "external interrupt"; break;
				case T_ALIGNMENT:
				    nm = "alignment"; break;
				case T_PGM_CHECK:
				    nm = "program check"; break;
				case T_NO_FPU:
				    nm = "no FPU available"; break;
				case T_DECR:
				    nm = "Decrementer trap"; break;
				case T_IO_ERROR:
				    nm = "I/O ERROR"; break;
				case T_SYSCALL:
				    nm = "system call"; break;
				case T_SINGLE_STEP:
				    nm = "trace mode"; break;
				case T_FP_ASSIST:
				    nm = "floating-point assist"; break;
				case T_PERF_MI:
				    nm = "Performance monitor interrupt"; break;
				case T_TLB_IMISS:
				    nm = "instruction translation miss"; break;
				case T_TLB_DLOADMISS:
				    nm = "data load translation miss"; break;
				case T_TLB_DSTOREMISS:
				    nm = "data store translation miss"; break;
				case T_IABR:
				    nm = "instruction address breakpoint"; break;
				case T_SYS_MGMT:
				    nm = "system management interrupt"; break;
				case T_EXEC_MODE:
				    nm = "run mode exceptions"; break;
				default:
				    nm = "unknown"; break;
			    }
			    printf("TRAP TYPE %X = %s; ", type, nm);
			    if (type == T_DATA_FAULT)
				printf("fault address is %X",
				    get(pos.k_regloc[R_R5], DSP));
			    printf("\naddress of registers %X\n",
				get(pos.k_regloc[R_R3], DSP));
			}

			pos.k_pc = get(Ltrapargs + MINFRAME +
			    (int)&regs_trap.r_pc - (int)&regs_trap.r_r0, DSP);
			regs_lr = get(Ltrapargs + MINFRAME +
					((int)&regs_trap.r_lr
					- (int)&regs_trap), DSP);
			findsym(pos.k_pc, ISYM);
			(void) strcpy(savedname, cursym->s_name);
			findsym(regs_lr, ISYM);
			if (strcmp(cursym->s_name, savedname) == 0)
				regs_lr = 0;
			skipframe = 2;

			pos.k_fp = get(Ltrapargs + MINFRAME +
			    (int)&regs_trap.r_r1 - (int)&regs_trap.r_r0, DSP);
			oldfp = pos.k_fp;

			pos.k_nargs = 0;
			pos.k_entry = MAXINT;
			if (pos.k_pc == 0)	/* temp hack */
				break;
			if (pos.k_fp == 0)
				break;
			if (pos.k_fp == 0xffffffff)
				break;
			pos.k_flags = 0;
			for (i = 0; i < NREGISTERS; i++) {
				pos.k_regloc[i] = Ltrapargs + MINFRAME +
				    REGADDR(i);
				pos.k_regspace[i] = TARGADRSPC;
			}
			findentry(&pos);
		} else {
			int startint = lookup("cmnint")->s_value;
			int endint = lookup("cmntrap")->s_value;

			/*
			 * Get the next stack frame.
			 * If frame pointer is null
			 * we are at the end of the stack.
			 */
			oldfp = pos.k_fp;
			nextframe(&pos);
			if (pos.k_fp == 0 || errflg != NULL)
				break;
			/*
			 * Check if we are in locore interrupt entry code
			 * Or if there was a thread switch.
			 * Either way we have a regs structure to deal with
			 *
			 * Note that the fp delta check is a real hack!
			 * But there is not really a better way to detect
			 * a thread switch.
			 */
			if ((pos.k_pc >= startint && pos.k_pc <= endint))
#if 0
thread switch ...
   || (pos.k_fp >= oldfp+0x2000 || pos.k_fp <= oldfp-0x2000))
#endif
			{ /* add of reg struct */
				int Ltrapargs = pos.k_fp + MINFRAME;
				if ((pos.k_pc >= startint &&
							pos.k_pc <= endint))
					printf("INTERRUPT - "
						"address of regs %X\n",
						Ltrapargs);
				else
					printf("STACK SWITCH "
						"address of regs %X\n",
						Ltrapargs);

				/*
				 * We do not print the locore routines in the
				 * Trace. So get the next stack frame.
				 */
				pos.k_pc = get(Ltrapargs +
						((int)&regs_trap.r_pc
						- (int)&regs_trap), DSP);
				regs_lr = get(Ltrapargs +
						((int)&regs_trap.r_lr
						- (int)&regs_trap), DSP);

				findsym(pos.k_pc, ISYM);
				(void) strcpy(savedname, cursym->s_name);
				findsym(regs_lr, ISYM);
				if (strcmp(cursym->s_name, savedname) == 0)
					regs_lr = 0;
				skipframe = 2;

				pos.k_fp = get(Ltrapargs +
						((int)&regs_trap.r_r1
						- (int)&regs_trap), DSP);
				oldfp = pos.k_fp;
				pos.k_nargs = 0;
				pos.k_entry = MAXINT;
				for (i = 0; i < NREGISTERS; i++) {
					pos.k_regloc[i] = Ltrapargs + MINFRAME +
					    REGADDR(i);
					pos.k_regspace[i] = TARGADRSPC;
				}
				findentry(&pos);
				if (errflg != NULL)
					break;
			}
		}
		if (pos.k_fp == 0)
			break;

#else
		/*
		 * For adb the stack trace is simple.
		 * look for the start of it all (ie main)
		 * or a null frame pointer.
		 */
		oldfp = pos.k_fp;
		if (!strcmp(name, "main") || nextframe(&pos) == 0
							    || errflg != NULL)
				break;
#endif
     }
}

valpr(val, type)
	int val, type;
{
	register int off;
	
	db_printf(9, "valpr: val=%D, type=%D", val, type);
	off = findsym(val, type);
	if (off != val && off < maxoff) {
		printf("%s", cursym->s_name);
		if (off) {
			printf("+%R", off);
		}
	}
}

#ifndef	KADB
print_fpregs()
{
	int i;
	double *addr;

	for (i=R_F0; i<=R_F30; i+=3) {
		addr = (double *)readreg(i);
		if (i == R_F30) {
			printf("F%d:\t",i-R_F0);
			printf("%-16.9f\t%-16.9f\n",*addr, *(addr+1));
		}
		else {
			printf("F%d:\t",i-R_F0);
			printf("%-16.9f\t%-16.9f\t%-16.9f\n",*addr, *(addr+1),
				*(addr+2));
		}
	}
}
#endif	/* KADB */
/*
 * Machine-dependent routine to handle $R $x $X commands.
 * called by printtrace (../common/print.c)
 */
fp_print(modif)
	int modif;
{
	db_printf(4, "fp_print: modif='%c'", modif);

	switch(modif) {

#ifdef KADB
	/* No floating point code in KADB */
	case 'R':
	case 'x':
	case 'X':
		error("Not legal in kadb");
		return;
	}
#else !KADB
	case 'R':
		error("Not supported");
		return;
	case 'x':
	case 'X':
		print_fpregs();
	}
	return 0;
#endif !KADB
}
