/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)pcs.c	1.30	96/06/18 SMI"

/*
 * adb - process control
 */

#include "adb.h"
#include <sys/ptrace.h>
#if defined(KADB) && defined(i386)
#include <sys/frame.h>
#endif

/* breakpoints */
struct	bkpt *bkpthead;

int	loopcnt;
int	ndebug;
int	savdot;

int	datalen = 1;

struct bkpt *
bkptlookup(addr)
	addr_t addr;
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt)
#if defined (KADB)	/* kadb requires an exact match */
		if (bp->flag && bp->loc == addr)
#else
		if (bp->flag && bp->loc <= addr && addr < bp->loc + bp->len)
#endif
			break;
	return (bp);
}

/* return true if bkptr overlaps any other breakpoint in the list */
static int
bpoverlap(struct bkpt *bkptr)
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (bp == bkptr || bp->flag == 0)
			continue;
		if (bkptr->loc + bkptr->len > bp->loc &&
		    bkptr->loc < bp->loc + bp->len)
			return (1);
	}

	return (0);
}

struct bkpt *
get_bkpt(addr_t where, int type, int len)
{
	struct bkpt *bkptr;

	db_printf(5, "get_bkpt: where=%X", where);
	/* If there is one there all ready, clobber it. */
	bkptr = bkptlookup(where);
	if (bkptr) {
		bkptr->loc = where;
		bkptr->flag = 0;
	}

	/* Look for the first free entry on the list. */
	for (bkptr = bkpthead; bkptr; bkptr = bkptr->nxtbkpt)
		if (bkptr->flag == 0)
			break;

	/* If there wasn't one, get one and link it in the list. */
	if (bkptr == 0) {
		bkptr = (struct bkpt *)malloc(sizeof *bkptr);
		if (bkptr == 0)
			error("bkpt: no memory");
		bkptr->nxtbkpt = bkpthead;
		bkpthead = bkptr;
		bkptr->flag = 0;
	}
#ifdef INSTR_ALIGN_MASK
	if (type == BPINST && (where & INSTR_ALIGN_MASK)) {
		error( BPT_ALIGN_ERROR );
	}
#endif INSTR_ALIGN_MASK
	bkptr->loc = where;		/* set the location */
	bkptr->type = type;		/* and the type */
	bkptr->len = len;		/* and the length */
	(void) readproc(bkptr->loc, (char *) &(bkptr->ins), SZBPT);
	db_printf(5, "get_bkpt: returns %X", bkptr);
	return bkptr;
}

subpcs(modif)
	int modif;
{
	register int check;
	int execsig;
	int runmode;
	struct bkpt *bkptr;
	char *comptr;
	int i, line, hitbp = 0;
	char *m;
	struct stackpos pos;
	int type, len;

	db_printf(4, "subpcs: modif='%c'", modif);
	execsig = 0;
	loopcnt = count;
	switch (modif) {

#if 0
	case 'D':
		dot = filextopc(dot);
		if (dot == 0)
			error("don't know pc for that source line");
		/* fall into ... */
#endif
	case 'd':
		bkptr = bkptlookup(dot);
		if (bkptr == 0)
			error("no breakpoint set");
		else if (kernel)
			(void) printf("Not possible with -k option.\n");
		else
			db_printf(2, "subpcs: bkptr=%X", bkptr);
		bkptr->flag = 0;
#if defined(KADB) && defined(sun4u)
		if (wplookup(bkptr->type))
			ndebug--;
#endif
		return;

	case 'z':			/* zap all breakpoints */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		for (bkptr = bkpthead; bkptr; bkptr = bkptr->nxtbkpt) {
			bkptr->flag = 0;
		}
		ndebug = 0;
		return;

#if 0
	case 'B':
		dot = filextopc(dot);
		if (dot == 0)
			error("don't know pc for that source line");
		/* fall into ... */
#endif
	case 'b':		/* set instruction breakpoint */
	case 'a':		/* set data breakpoint (access) */
	case 'w':		/* set data breakpoint (write) */
	case 'p':		/* set data breakpoint (exec) */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
#if defined(KADB) && defined(sun4u)
	case 'f':		/* set data breakpoint (physical) */
		/*
		 * Fusion only has one virtual and one physical address
		 * watchpoint register, so let the user know when they've
		 * overwritten the existing watchpoint.
		 */
		if (ndebug) {
			if (modif == 'f') {
				if (bkptr = wplookup(BPPHYS)) {
					printf("overwrote previous watchpoint at PA %X\n",
					    bkptr->loc);
				}
				bkptr->loc = dot;
				ndebug--;
			} else if (modif != 'b' &&
			    (bkptr = wplookup(BPACCESS)) ||
			    (bkptr = wplookup(BPWRITE)) ||
			    (bkptr = wplookup(BPDBINS))) {
				printf("overwrote previous watchpoint at VA %X\n",
				    bkptr->loc);
				bkptr->loc = dot;
				ndebug--;
			}
		}
#endif	/* KADB && sun4u */
		if (modif == 'b') {
			type = BPINST;
			len = SZBPT;
		} else if (ndebug == NDEBUG) {
			error("bkpt: no more debug registers");
			/* NOTREACHED */
		} else if (modif == 'p') {
	    		type = BPDBINS;
			len = length;
#if defined(KADB)
			len = 1;
			ndebug++;
#endif
		} else if (modif == 'a') {
			type = BPACCESS;
			len = length;
#if defined(KADB)
			len = datalen;
			ndebug++;
#endif
		} else if (modif == 'w') {
			type = BPWRITE;
			len = length;
#if defined(KADB)
			len = datalen;
			ndebug++;
#endif
		}
#if defined(KADB) && defined(sun4u)
		else if (modif == 'f') {
	    		type = BPPHYS;
			len = 1;
			ndebug++;
		}
#endif
		bkptr = get_bkpt(dot, type, len);
		db_printf(2, "subpcs: bkptr=%X", bkptr);
		bkptr->initcnt = bkptr->count = count;
		bkptr->flag = BKPTSET;
		if (bpoverlap(bkptr)) {
			(void) printf("watched area overlaps other areas.\n");
			bkptr->flag = 0;
		}
		check = MAXCOM-1;
		comptr = bkptr->comm;
		(void) rdc(); lp--;
		do
			*comptr++ = readchar();
		while (check-- && lastc != '\n');
		*comptr = 0; lp--;
		if (check)
			return;
		error("bkpt command too long");
		/* NOTREACHED */
#if defined(KADB) && (defined(i386) || defined(__ppc))
	case 'I':
	case 'i':
		printf("%x\n", inb(address));
		return;

	case 'O':
	case 'o':
		outb(address, count);
		return;

#else
#ifndef KADB
	case 'i':
	case 't':
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (!hadaddress)
			error("which signal?");
		if (expv <= 0 || expv >=NSIG)
			error("signal number out of range");
		sigpass[expv] = modif == 'i';
		return;

	case 'l':
		if (!pid)
			error("no process");
		if (!hadaddress)
			error("no lwpid specified");
		db_printf(2, "subpcs: expv=%D, pid=%D", expv, pid);
		(void) set_lwp(expv, pid);
		return;

	case 'k':
		if (kernel)
			(void) printf("Not possible with -k option.\n");
		if (pid == 0)
			error("no process");
		printf("%d: killed", pid);
		endpcs();
		return;

	case 'r':
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		setup();
		runmode = PTRACE_CONT;
		subtty();
		db_printf(2, "subpcs: running pid=%D", pid);
		break;

	case 'A':                       /* attach process */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (pid)
			error("process already attached");
		if (!hadaddress)
			error("specify pid in current radix");
		if (ptrace(PTRACE_ATTACH, address) == -1)
			error("can't attach process");
		pid = address;
		bpwait(0);
		printf("process %d stopped at:\n", pid);
		print_dis(Reg_PC, 0);
		userpc = (addr_t)dot;
		return;

	case 'R':                       /* release (detach) process */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (!pid)
			error("no process");
		if (ptrace(PTRACE_DETACH, pid, readreg(Reg_PC), SIGCONT) == -1)
			error("can't detach process");
		pid = 0;
		return;
#endif !KADB
#endif !KADB && !i386

	case 'e':			/* execute instr. or routine */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
#ifdef sparc
		/* Look for an npc that does not immediately follow pc.
		 * If that's the case, then look to see if the immediately
		 * preceding instruction was a call.  If so, we're in the
		 * delay slot of a pending call we that want to skip.
		 */
		if ((userpc + 4 != readreg(Reg_NPC)) &&
			/* Is the preceding instruction a CALL? */
			(((bchkget(userpc - 4, ISP) >> 6)  == SR_CALL_OP) ||
			/* Or, is the preceding instruction a JMPL? */
			(((bchkget(userpc - 4, ISP) >> 6) == SR_FMT3a_OP) &&
			((bchkget(userpc - 3, ISP) >> 2) == SR_JUMP_OP)))) {
			/* If there isn't a breakpoint there all ready */
			if (!bkptlookup(userpc + 4)) {
	       			bkptr = get_bkpt(userpc + 4, BPINST, SZBPT);
				bkptr->flag = BKPT_TEMP;
				bkptr->count = bkptr->initcnt = 1;
				bkptr->comm[0] = '\n';
				bkptr->comm[1] = '\0';
			}
			goto colon_c;
		}
		else
			modif = 's';	/* Behave as though it's ":s" */
		/* FALL THROUGH */
#endif sparc
#ifdef i386
		/*
		 * Look for an call instr. If it is a call set break
		 * just after call so run thru the break point and stop
		 * at returned address.
		 */
		{
			unsigned op1;
			int	brkinc = 0;
			int	pc = readreg(REG_PC);
			
			op1 = (get (pc, DSP) & 0xff);
			/* Check for simple case first */
			if (op1 == 0xe8) 
				brkinc = 5; /* length of break instr */
			else if (op1 == 0xff) {
				/* This is amuch more complex case (ie pointer
				   to function call). */
				op1 = (get (pc+1, DSP) & 0xff);
				switch (op1) 
				{
				      case 0x15:
				      case 0x90:
					brkinc = 6;
					break;
				      case 0x50:
				      case 0x51:
				      case 0x52:
				      case 0x53:
				      case 0x55:
				      case 0x56:
				      case 0x57:
					brkinc = 3;
				      case 0x10:
				      case 0xd0:
				      case 0xd2:
				      case 0xd3:
				      case 0xd6:
				      case 0xd7:
					brkinc = 2;
				}
			}
			if (brkinc)
			{
				/* If there isn't a breakpoint there all ready */
				if (!bkptlookup(userpc + brkinc)) {
					bkptr = get_bkpt(userpc + brkinc,
					    BPINST, SZBPT);
					bkptr->flag = BKPT_TEMP;
					bkptr->count = bkptr->initcnt = 1;
					bkptr->comm[0] = '\n';
					bkptr->comm[1] = '\0';
				}
				goto colon_c;
			}
			else
			    modif = 's';     /* Behave as though it's ":s" */
		}
		
		/* FALL THROUGH */
#endif i386

	case 's':
	case 'S':
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (pid) {
			execsig = getsig(signo);
			db_printf(2, "subpcs: execsig=%D", execsig);
		} else {
			setup();
			loopcnt--;
		}
		runmode = PTRACE_SINGLESTEP;
#if 0
		if (modif == 's')
			break;
		if ((pctofilex(userpc), filex) == 0)
			break;
		subtty();
		for (i = loopcnt; i > 0; i--) {
			line = (pctofilex(userpc), filex);
			if (line == 0)
				break;
			do {
				loopcnt = 1;
				if (runpcs(runmode, execsig)) {
					hitbp = 1;
					break;
				}
				if (interrupted)
					break;
			} while ((pctofilex(userpc), filex) == line);
			loopcnt = 0;
		}
#endif
		break;

	case 'u':			/* Continue to end of routine */
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		stacktop(&pos);
#ifdef	sparc
		savdot = pos.k_caller + 8;
#elif defined(__ppc)
		savdot = get(pos.k_fp + FR_SAVFP, DSP);
		savdot = get(savdot + FR_SAVPC, DSP);
#else	/* sparc */
		savdot = get(pos.k_fp + FR_SAVPC, DSP);
#endif	/* sparc */
		db_printf(2, "subpcs: savdot=%X", savdot);
		bkptr = get_bkpt(savdot, BPINST, SZBPT);
		bkptr->flag = BKPT_TEMP;
		/* Associate this breakpoint with the caller's fp/sp. */
#if defined(KADB) && defined(i386)
		bkptr->count = pos.k_fp;
#endif
		bkptr->initcnt = 1;
		bkptr->comm[0] = '\n';
		bkptr->comm[1] = '\0';
		/* Fall through */

	case 'c':
	colon_c:
		if (kernel) {
			(void) printf("Not possible with -k option.\n");
			return;
		}
		if (pid == 0)
			error("no process");
		runmode = PTRACE_CONT;
		execsig = getsig(signo);
		db_printf(2, "subpcs: execsig=%D", execsig);
		subtty();
		break;
#if defined(KADB) && defined(sun4u)
	case 'x':		/* switch cpu */
	{
		extern int to_cpu, switched;

		to_cpu = dot;
		if (to_cpu < 0 || to_cpu > NCPU)
			error("bad CPU number");
		hadaddress = 0;
		switched = 1;
		execsig = getsig(signo);
		runmode = PTRACE_CONT;
	}
	break;
#endif	/* KADB && sun4u */

	default:
		db_printf(3, "subpcs: bad modifier");
		error("bad modifier");
	}

	if (hitbp || (loopcnt > 0 && runpcs(runmode, execsig))) {
		m = "breakpoint at:\n";
	} else {
#if defined(KADB) && defined(sun4u)
		if ((int)tookwp())
			m = "watchpoint:\n";
		else
			m = "stopped at:\n";
#elif defined(KADB)
		m = "stopped at:\n";
#else
		int sig = Prstatus.pr_lwp.pr_info.si_signo;
		int code = Prstatus.pr_lwp.pr_info.si_code;
		adbtty();
		delbp();
		if (sig == SIGTRAP &&
		    (code == TRAP_RWATCH ||
		    code == TRAP_WWATCH ||
		    code == TRAP_XWATCH)) {
			int pc = (int)Prstatus.pr_lwp.pr_info.si_pc;
			int addr = (int)Prstatus.pr_lwp.pr_info.si_addr;
			char *rwx;

			switch (code) {
			case TRAP_RWATCH:
				if (trapafter)
					rwx = "%16twas read at:\n";
				else
					rwx = "%16twill be read:\n";
				break;
			case TRAP_WWATCH:
				if (trapafter)
					rwx = "%16twas written at:\n";
				else
					rwx = "%16twill be written:\n";
				break;
			case TRAP_XWATCH:
				if (trapafter)
					rwx = "%16twas executed:\n";
				else
					rwx = "%16twill be executed:\n";
				break;
			}
			psymoff(addr, DSYM, rwx);
			print_dis(0, pc);
			dot = addr;
		} else {
			printf("stopped at:\n");
			print_dis(Reg_PC, 0);
		}
		resetcounts();
		return;
#endif
	}
	adbtty();
	delbp();
	printf(m);
	print_dis(Reg_PC, 0);
	resetcounts();
}

/*
 * Is this breakpoint a watchpoint?
 */
int
iswp(int type)
{
	switch (type) {
	case BPACCESS:
	case BPWRITE:
	case BPDBINS:
	case BPPHYS:
		return (1);
	}
	return (0);
}

/* loop up a watchpoint bkpt structure by address */
struct bkpt *
wptlookup(addr)
	addr_t addr;
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt)
		if (bp->flag && iswp(bp->type) &&
		    bp->loc <= addr && addr < bp->loc + bp->len)
			break;
	return (bp);
}

#if !defined(KADB)
/* each KADB platform has to define one of these */
/* this is the user-level generic one */
struct bkpt *
tookwp()
{
	register struct bkpt *bp = NULL;

	/* if we are sitting on a watchpoint, find its bkpt structure */
	int sig = Prstatus.pr_lwp.pr_info.si_signo;
	int code = Prstatus.pr_lwp.pr_info.si_code;
	if (sig == SIGTRAP &&
	    (code == TRAP_RWATCH ||
	    code == TRAP_WWATCH ||
	    code == TRAP_XWATCH))
		bp = wptlookup((unsigned)Prstatus.pr_lwp.pr_info.si_addr);
	return (bp);
}
#endif

/* This is here for the convenience of both adb and kadb */
char *
map(request)		 /* for debugging purposes only */
	int request;
{
	static char buffer[16];

	switch (request) {
	case PTRACE_TRACEME:	return "PTRACE_TRACEME";
	case PTRACE_PEEKTEXT:	return "PTRACE_PEEKTEXT";
	case PTRACE_PEEKDATA:	return "PTRACE_PEEKDATA";
	case PTRACE_PEEKUSER:	return "PTRACE_PEEKUSER";
	case PTRACE_POKETEXT:	return "PTRACE_POKETEXT";
	case PTRACE_POKEDATA:	return "PTRACE_POKEDATA";
	case PTRACE_POKEUSER:	return "PTRACE_POKEUSER";
	case PTRACE_CONT:	return "PTRACE_CONT";
	case PTRACE_KILL:	return "PTRACE_KILL";
	case PTRACE_SINGLESTEP:	return "PTRACE_SINGLESTEP";
	case PTRACE_ATTACH:	return "PTRACE_ATTACH";
	case PTRACE_DETACH:	return "PTRACE_DETACH";
	case PTRACE_GETREGS:	return "PTRACE_GETREGS";
	case PTRACE_SETREGS:	return "PTRACE_SETREGS";
	case PTRACE_GETFPREGS:	return "PTRACE_GETFPREGS";
	case PTRACE_SETFPREGS:	return "PTRACE_SETFPREGS";
	case PTRACE_READDATA:	return "PTRACE_READDATA";
	case PTRACE_WRITEDATA:	return "PTRACE_WRITEDATA";
	case PTRACE_READTEXT:	return "PTRACE_READTEXT";
	case PTRACE_WRITETEXT:	return "PTRACE_WRITETEXT";
	case PTRACE_GETFPAREGS:	return "PTRACE_GETFPAREGS";
	case PTRACE_SETFPAREGS:	return "PTRACE_SETFPAREGS";
	case PTRACE_GETWINDOW:	return "PTRACE_GETWINDOW";
	case PTRACE_SETWINDOW:	return "PTRACE_SETWINDOW";
	case PTRACE_SYSCALL:	return "PTRACE_SYSCALL";
	case PTRACE_DUMPCORE:	return "PTRACE_DUMPCORE";
	case PTRACE_SETWRBKPT:	return "PTRACE_SETWRBKPT";
	case PTRACE_SETACBKPT:	return "PTRACE_SETACBKPT";
	case PTRACE_CLRDR7:	return "PTRACE_CLRDR7";
	case PTRACE_TRAPCODE:	return "PTRACE_TRAPCODE";
	case PTRACE_SETBPP:	return "PTRACE_SETBPP";
	case PTRACE_WPPHYS:	return "PTRACE_WPPHYS";
	}
	(void) sprintf(buffer, "PTRACE_%d", request);
	return (buffer);
}
