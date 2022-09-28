/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.16	96/06/18 SMI"

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/errno.h>
#include <sys/psw.h>
#include <sys/trap.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/debug/debugger.h>
#include <sys/bootconf.h>
#include <sys/elf.h>
#include <sys/link.h>
#include "adb.h"

extern int errno;

int cur_cpuid=0x8000; /* The id of our current cpu. High bit set so that we */
		      /* don't get a false comparison success first time */
int istrap = 0;
int ishtrap = 0;

int _setjmp(int *);
int _set_pc(int *, func_t);
int _longjmp(int *, int);
long peekl(long *);

static jmp_buf debugregs;	/* context for debugger */
static jmp_buf mainregs;	/* context for debuggee */
static jmp_buf_ptr curregs;	/* pointer to saved context for each process */
struct regs *regsave; 		/* temp save area--align to double */
struct regs *reg; 		/* temp save area--align to double */

extern char rstk[], etext[], edata[];
extern int estack;

/*
 * Sys_trap trap handlers.
 */

int step_expected=0;

struct	regs *i_fparray[16];	/* array for NCPU frame pointers */

/*
 * Miscellanous fault error handler
 */
fault(struct regs *i_fp, int type, int dar, int dsisr)
{
	register int ondebug_stack;
	extern int first_time;
	extern int go2;
	extern int interactive;
	static int save_ee;		/* disable interrupts while stepping */
	extern void		    *cif_handler;	/* Open Firmware interface */
	extern struct bootops	    *bootops;	/* bootops interface */
	extern int		     from_the_kernel;	/* locore entry */
	extern			     initstk;
	extern Elf32_Boot	     elfbootvec[EB_MAX];
	int i, j, *r1;

	if (first_time) {

		/*
		 * Initialize the kernel regs here.  This is done this way
		 * so that our transition from user to debugger and back
		 * only occurs in fault(), i.e., in this function.
		 */
		first_time = 0;	/* this can only be done once */

		reg = regsave = i_fp;
		i_fp->r_pc = go2;
		i_fp->r_r1 = initstk;
		i_fp->r_r3 = (int)&cif_handler; /* boot svc function area */
		i_fp->r_r4 = (int)&from_the_kernel;
		i_fp->r_r5 = (int)&bootops; /* boot properties root node */
		i_fp->r_r6 = (int)elfbootvec;
		i_fp->r_r7 = 0;
		i_fp->r_r8 = 0;
		i_fp->r_r9 = 0;
		i_fp->r_r10 = 0;

		if (interactive & RB_KRTLD)
			goto common_exit;

		return (0);
	}
#if 0
	printf("\n");
	printf("kadb: trap type = %x", type);
	printf("     msr   = %x\n", i_fp->r_msr);
	printf("kadb:        sp = %x", i_fp->r_r1);
	printf("     pc    = %x\n", i_fp->r_pc);
	printf("kadb:      addr = %x", dar);
	printf("     dsisr = %x\n", dsisr);
	printf("\n");
	{ int i, *ip;
	  for (i = 0, ip = (int *)&i_fp->r_r0; i < 40; i += 8, ip += 8) {
		printf("regs [%d-%d]: %x %x %x %x %x %x %x %x\n", i, i+7,
			ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7]);
	  }
	}
#endif

	ondebug_stack = (getsp() > (int)&rstk && getsp() < (int)estack);

#if NEED_SPRG_VALUES
printf("sprg  0=%x  1=%x  2=%x\n", sprg0(), sprg1(), sprg2());
#endif
	/*
	 * Assume that nofault will be NULL if we're not in kadb.
	 * ondebug_stack won't be true early on, and it's possible to
	 * get an innocuous data fault looking for kernel symbols then.
	 */
	if ((type == T_DATA_FAULT) && nofault) {
		jmp_buf_ptr sav = nofault;

		nofault = NULL;
		_longjmp ( sav, 1);
		/*NOTREACHED*/
	}

	regsave = i_fp;

	/*
	 * Is this is a single step or a break point?
	 */
	if ((type == T_EXEC_MODE) || (type == T_SINGLE_STEP)) {
		if (!step_expected)
			return (1);	/* kernel should field it */
		if (save_ee == MSR_EE)
			i_fp->r_msr |= MSR_EE;
		i_fp->r_msr &= ~MSR_SE;
		goto common_exit;
	}
	if (type == T_PGM_CHECK) {
		if (peekl((long *)(i_fp->r_pc + PCFUDGE)) == bpt) {
			if ((bkptlookup(i_fp->r_pc + PCFUDGE)==0))
				return (1);
			istrap = 1;
			goto common_exit;
		} else
			return (1);
	}

	/*
	 * Is this a programmed entry?
	 */
	if (type == 0x12340adb)
		goto common_exit;

	/*
	 * If we are on the debugger stack and
	 * abort_jmp is set, do a longjmp to it.
	 */
	if (abort_jmp && ondebug_stack) {
		printf("abort jump: trap %x sp %x pc %x\n",
			type, getsp(), i_fp->r_pc);
printf("fault: problem in kernel debugger\n");
printf("fault: type is 0x%x\n", type);
printf("fault: registers are at 0x%x\n", i_fp);
printf("fault: stack pointer is 0x%x\n", getsp());
printf("fault: called from %x\n", caller());
printf("fault: trap type = %x", type);
printf("     msr   = %x\n", i_fp->r_msr);
printf("fault:        sp = %x", i_fp->r_r1);
printf("     pc    = %x\n", i_fp->r_pc);
printf("fault:      addr = %x", dar);
printf("     dsisr = %x\n", dsisr);
		_longjmp ( abort_jmp, 1);
		/*NOTREACHED*/
	}

	/*
	 * Ok, the user faulted while not in the
	 * debugger. Enter the main cmd loop
	 * so that the user can look around...
	 */
	/*
	 * There is a problem here since we really need to tell cmd()
	 * the current registers.  We would like to call cmd() in locore
	 * but the interface is not really set up to handle this (yet?)
	 */

	prom_printf("Trap vector is %x\n", type);
	prom_printf("Register contents follows:\n");
	prom_printf("    cr = %8x, lr = %8x, ctr = %8x, xer = %8x\n",
		i_fp->r_cr, i_fp->r_lr, i_fp->r_ctr, i_fp->r_xer);
	prom_printf("    pc = %8x, msr = %8x, dar = %8x, dsisr = %8x\n",
		i_fp->r_pc, i_fp->r_msr, dar, dsisr);
	for (j = 0; j < 32; j += 8) {
		prom_printf("r%2d-%2d: ", j, j+7);
		for (i = 0; i < 8; i++) {
			prom_printf("%8x ", ((int *)&i_fp->r_r0)[j + i]);
		}
		prom_printf("\n");
	}
	prom_printf("Stack backtrace:\n");
	for (r1 = (int *)i_fp->r_r1; r1 != 0; r1 = (int *)*r1) {
		prom_printf("    %8x\n", r1[1]);
	}
	prom_printf("Type any key to continue ");
	prom_getchar();
	prom_printf("\n");

#if 0
printf("\n");
printf("fault: type is 0x%x\n", type);
printf("fault: registers are at 0x%x\n", i_fp);
printf("fault: stack pointer is 0x%x\n", getsp());
printf("fault: called from %x\n", caller());
printf("fault: trap type = %x", type);
printf("     msr   = %x\n", i_fp->r_msr);
printf("fault:        sp = %x", i_fp->r_r1);
printf("     pc    = %x\n", i_fp->r_pc);
printf("fault:      addr = %x", dar);
printf("     dsisr = %x\n", dsisr);
printf("\n");
#endif

common_exit:
	cmd(i_fp);
	step_expected = (i_fp->r_msr & MSR_SE);
	if (step_expected) {
		if (emulate(i_fp))
			goto common_exit;
		save_ee = i_fp->r_msr & MSR_EE;
		i_fp->r_msr &= ~MSR_EE;
	}
	_exitto("fault: returning to debugee...\n\n");
	return (0);
}

#define	MTMSR ((31 << 26) + (146 << 1))
#define	MFMSR ((31 << 26) + (83 << 1))

int
emulate(struct regs *rp)
{
	long instr, significant;
	int r;

	if ((instr = get(rp->r_pc, DSP)) != -1) {
		significant = instr & 0xfc1fffff;
		r = (instr >> 21) & 0x1f;		/* general register */
		if (significant == MTMSR) {		/* mtmsr */
			rp->r_msr = *((long *)(&rp->r_r0) + r);
			rp->r_pc += 4;
			return (1);
		} else if (significant == MFMSR) {	/* mfmsr */
			*((long *)(&rp->r_r0) + r) = rp->r_msr & ~MSR_SE;
			rp->r_pc += 4;
			return (1);
		}
	}
	return (0);
}

print_sp(sp)
{
	printf("callers are %x ",
	    cal1(sp));
	printf("%x ",
	    cal2(sp));
	printf("%x\n",
	    cal3(sp));
}

static jmp_buf_ptr saved_jb;
static jmp_buf jb;
extern int debugging;

/*
 * Peekc is so named to avoid a naming conflict
 * with adb which has a variable named peekc
 */
int
Peekc(addr)
	char *addr;
{
	u_char val;

	errno = EFAULT;
	saved_jb = nofault;
	nofault = jb;
	if (!_setjmp(jb)) {
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		errno = 0;
		return ((int)(val & 0xff));
	}
	/* a fault occured */
	nofault = saved_jb;
	return (-1);
}

short
peek(addr)
	short *addr;
{
	short val, tmp;

	errno = EFAULT;
	saved_jb = nofault;
	nofault = jb;
	if (!_setjmp(jb)) {
if (((int)addr & 0x1) == 0)
		val = *addr;
else {
if ((tmp = Peekc((char *)addr)) == -1)
	return -1;
val = tmp;
if ((tmp = Peekc((char *)addr+1)) == -1)
	return -1;
val += tmp << 8;
}
		errno = 0;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (val);
	}
	/* a fault occured */
	nofault = saved_jb;
	return (-1);
}

long
peekl(addr)
	long *addr;
{
	long val, tmp;

	errno = EFAULT;
	saved_jb = nofault;
	nofault = jb;
	if (!_setjmp(jb)) {
		if (((int)addr & 0x3) == 0)
			val = *addr;
		else {
			if ((tmp = Peekc((char *)addr)) == -1)
				return -1;
			val = tmp;
			if ((tmp = Peekc((char *)addr+1)) == -1)
				return -1;
			val += tmp << 8;
			if ((tmp = Peekc((char *)addr+2)) == -1)
				return -1;
			val += tmp << 16;
			if ((tmp = Peekc((char *)addr+3)) == -1)
				return -1;
			val += tmp << 24;
		}
		errno = 0;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (val);
	}
	/* a fault occured */
	nofault = saved_jb;
	return (-1);
}

int
pokec(addr, val)
	char *addr;
	char val;
{

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		*addr = val;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (0);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

int
pokel(addr, val)
	long *addr;
	long val;
{
	int retval = 0;

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		if (((int)addr & 0x3) == 0)
			*addr = val;
		else {	/* deal with unaligned writes, e.g., "foo/w val" */
			char *cp = (char *)&val;
			if ((pokec((char *)addr+0, *cp++) == -1) ||
			    (pokec((char *)addr+1, *cp++) == -1) ||
			    (pokec((char *)addr+2, *cp++) == -1) ||
			    (pokec((char *)addr+3, *cp++) == -1)) {
				retval = -1;
			}
		}
	}

	nofault = saved_jb;
	if (retval == -1)
		errno = EFAULT;
	return (retval);
}

poketext(addr, val)
	int *addr;
	int val;
{
	int retval;

	retval = pokel(addr, val);
	sync_instruction_memory(addr, 4);
	return (retval);
}

scopy(from, to, count)
	register char *from;
	register char *to;
	register int count;
{
	register int val;

	for (; count > 0; count--) {
		if ((val = Peekc(from++)) == -1)
			goto err;
		if (pokec(to++, val) == -1)
			goto err;
	}
	return (0);
err:
	errno = EFAULT;
	return (-1);
}

pjb(int *debugregs)
{
	int i;

	printf("+++++++++++++++++++++++++++++++++++++++\n");
	for (i = 0; i < 32; i++) {
		printf("%x ", debugregs[i]);
		if ((i & 3) == 3)
			printf("\n");
	}
	printf("---------------------------------------\n");
}

/*
 * Setup a new context to run at routine using stack whose
 * top (end) is at sp.  Assumes that the current context
 * is to be initialized for mainregs and new context is
 * to be set up in debugregs.
 */
spawn(sp, routine)
	char *sp;
	func_t routine;
{
	int res;

	if (curregs != 0) {
		printf("bad call to spawn\n");
		_exit ();
	}
	if ((res = _setjmp(mainregs)) == 0) {
#if 0
		pjb((int *)mainregs);
#endif
		/*
		 * Setup new return stack frame, and enter "routine"
		 */
		sp -= MINFRAME;	/* minimum stack frame */

		/* copy entire jump buffer to debugregs */
		bcopy((caddr_t)mainregs, (caddr_t)debugregs, sizeof (jmp_buf));

		/* change stacks */
		((struct frame *)debugregs)->fr_savfp = (struct frame *)sp;

		/* _set_pc deals with PowerPC function descriptor dereference */
		_set_pc(debugregs, routine);		  /* change the pc */

		curregs = debugregs;

#if 0
		pjb((int *)debugregs);
#endif
		_longjmp(debugregs, 1);	/* jump to new context */
		/*NOTREACHED*/
		
	}
}

doswitch()
{
	int res;

	if ((res = _setjmp(curregs)) == 0) {
		/*
		 * Switch curregs to other descriptor
		 */
		if (curregs == mainregs) {
			curregs = debugregs;
		} else /* curregs == debugregs */ {
			curregs = mainregs;
		}
		_longjmp(curregs, 1);
		/*NOTREACHED*/
	}
	/*
	 * else continue on in new context
	 */
}

extern	int	*xc_initted;
extern	int	addr_to_goto;
extern	void 	(*kxc_call)(int,int,int,int,int,void ());
extern	void 	get_smothered();
/*
 * Main interpreter command loop.
 */
cmd(i_fp)
struct regs *i_fp;
{
	int addr, set;

	dorun = 0;

	/*
	 * See if the sp says that we are already on the debugger stack
	 */
	reg = regsave;
	addr = getsp();
	if ( addr > (int )&rstk && addr < (int )estack ) {
		printf("Already in debugger!\n");
		printf("continue? ");
		getchar();
		printf("\n");
		return;
	}

	do {
		doswitch();
		if (dorun == 0) {
			printf("cmd: nothing to do?   ");
			printf("Type any key to continue. ");
			getchar();
			printf("\n");
		}
	} while (dorun == 0);

	step_expected = (i_fp->r_msr & MSR_SE);
}


#ifdef	DO_FAULT_TRACE
traceback(sp)
	caddr_t sp;
{

typedef struct {
				stack_frame sf;
				int fr_arg[6];
				} trace_frame;

	register u_int tospage;
	register trace_frame *fp;

	if ((int)sp & (STACK_ALIGN-1))
		printf("traceback: misaligned sp = %x\n", sp);

	tospage = (u_int)btoc(sp);
	fp = (trace_frame *)sp;
	printf("Begin traceback... sp = %x\n", sp);

	while (btoc((u_int)fp) == tospage) {
		if (fp == fp->sf.r_ebp) {
			printf("FP loop at %x", fp);
			break;
		}
		tryabort(1);
		printf("Called from %x, fp=%x, args=%x %x %x %x %x %x\n",
		    fp->sf.r_r1, fp->sf.r_ebp,
		    fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
		    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5]);

		fp = fp->sf.r_ebp;
		if (fp == 0)
			break;
	}
	printf("End traceback...\n");
}
#endif

/* a stub function called from common code */
struct bkpt *
tookwp()
{
	return (NULL);
}
