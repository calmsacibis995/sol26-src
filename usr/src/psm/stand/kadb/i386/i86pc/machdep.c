/*
 * Copyright (c) 1986-1996, Sun Microsystems, Inc.
 */

#pragma ident	"@(#)machdep.c	1.12	96/10/24 SMI"

/* from machdep.c: 1.28	89/12/07 SMI" */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/reboot.h>
#include <sys/vmmac.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/pte.h>
#include <sys/psw.h>
#include <sys/trap.h>
#include <sys/clock.h>
#include <sys/t_lock.h>
#include <sys/reg.h>
#include <sys/asm_linkage.h>
#include <sys/frame.h>
#include <setjmp.h>

#include <sys/stack.h>
#include <sys/debug/debug.h>
#include <sys/debug/debugger.h>
#include <sys/bootconf.h>
#include <sys/bootsvcs.h>
#include <sys/promif.h>

/*
 * We want to use the kadb printf function here,
 * not the one provided with adb, nor the one
 * provided with the boot_syscalls interface!
 */
#undef printf

extern int errno;

int cur_cpuid = -1;	/* The id of our current cpu. Initialized to -1 to  */
			/* indicate not currently in kadb */
int istrap = 0;
int ishtrap = 0;

int fake_bpt;			/* place for a fake breakpoint at startup */
jmp_buf debugregs;		/* context for debugger */
jmp_buf mainregs;		/* context for debuggee */
jmp_buf_ptr curregs;		/* pointer to saved context for each process */
struct regs *regsave; 		/* temp save area--align to double */
struct regs *reg; 		/* temp save area--align to double */

extern char rstk[], etext[], edata[];
extern int estack;

/*
 * Definitions for registers in jmp_buf
 */
#define	JB_PC	5
#define	JB_SP	4
#define	JB_FP	3
#define	STKFRAME 8	 /* return addr + old frame pointer */

void Setpgmap();

static volatile u_long counter_vaddr, intreg_vaddr;

/*
 * Sys_trap trap handlers.
 */

/*
 * level15 (memory error) interrupt.
 */
level15()
{
	/*
	 * For now, the memory error regs are not mapped into the debugger,
	 * so we just print a message.
	 */
	prom_printf("memory error\n");
}

int step_expected = 0;

struct	regs *i_fparray[16];	/* array for NCPU frame pointers */

/*
 * Miscellanous fault error handler
 */
fault(i_fp)
struct regs *i_fp;
{
	register int ondebug_stack;
	register u_int *pc;
	register u_int realpc;
	int	set;
	int	reason = 0;
	extern int first_time;
	extern int go2;
	extern int interactive;
	extern struct boot_syscalls *sysp;	/* function pointer table */

	if (first_time) {

		/*
		 * We use a fake break point to get from main() to
		 * here before we call the user/debugged program.
		 * This is done so that our transition from user to
		 * debugger and back only occures in fault().
		 */
		first_time = 0;	/* this can only be done once */

		/* setup our initail reg values */
		reg = regsave = i_fp;
		i_fp->r_eip = go2;
		i_fp->r_ecx = (int)&sysp; /* boot svc function area */
		i_fp->r_ebx = (int)&bootops; /* boot properties root node */
		i_fp->r_edi = 0;
		i_fp->r_esi = 0;
		i_fp->r_eax = 0;
		i_fp->r_edx = 0x12344321;   /* kadb magic number */
		i_fp->r_ebp = 0;	/* so stack trace knows when to stop */

		if (interactive & RB_KRTLD) {
			(void) cmd(i_fp);
			if (dotrace) {
				scbstop = 1;
				dotrace = 0;
			}
		}
		return (0);
	}

	/*
	 * Quick return to let kernel handle user-level debugging.
	 */
	if ((i_fp->r_trapno == T_BPTFLT || i_fp->r_trapno == T_SGLSTP) &&
	    (u_int)i_fp->r_eip < (u_int)USERLIMIT)
		return (1);

	ondebug_stack = (getsp() > (int)&rstk && getsp() < (int)estack);

	i_fp->r_esp += 0x14;		/* real stack before excpt. */

	/*
	 * Assume that nofault won't be non-NULL if we're not in kadb.
	 * ondebug_stack won't be true early on, and it's possible to
	 * get an innocuous data fault looking for kernel symbols then.
	 */
	if ((i_fp->r_trapno == T_GPFLT ||
	    i_fp->r_trapno == T_PGFLT) && nofault) {
		jmp_buf_ptr sav = nofault;

		nofault = NULL;
		_longjmp(sav, 1);
		/*NOTREACHED*/
	}

	regsave = i_fp;

	/*
	 * Is this is a single step or a break point?
	 */
	if (i_fp->r_trapno == T_BPTFLT || i_fp->r_trapno == T_SGLSTP) {
		/*
		 * If this is single step make sure the trace bit is off.
		 * Hardware should do this for us - but it does not do it
		 * every time.
		 */
		if (i_fp->r_trapno == T_SGLSTP)
		{
			int rdr6;
			extern int kcmntrap;
			extern uchar_t dr_cpubkptused[NCPU];

			rdr6 = dr6();
			if (!step_expected && kcmntrap &&
			    !(((unsigned)cur_cpuid < NCPU ?
			    dr_cpubkptused[cur_cpuid] : 0xf) & rdr6))
				return (1);	/* kernel should field it */
			if (rdr6 & 0xf)		/* hardware break point */
				ishtrap = 1;
			i_fp->r_efl &= ~0x100;
		} else {
			/*
			 * We set the extern 'istrap' flag to alert the
			 * bpwait routine that the eip must be set back
			 * one byte
			 */
			if (Peekc(i_fp->r_eip - 1) == 0xcc) {
				extern int kcmntrap;
				if (kcmntrap &&
				    (bkptlookup(i_fp->r_eip-1) == 0))
					return (1);
				istrap = 1;
			}
		}
		cmd(i_fp);
		return (0);
	}

	/*
	 * Is this Programmed entry (int 20)
	 */
	if (i_fp->r_trapno == 20) {
		cmd(i_fp);
		step_expected = (i_fp->r_efl & 0x100);
		return (0);
	}

#ifdef	DO_FAULT_TRACE
	traceback(i_fp->r_ebp);
#endif
	/*
	 * If we are on the debugger stack and
	 * abort_jmp is set, do a longjmp to it.
	 */
	if (abort_jmp && ondebug_stack) {
		printf("abort jump: trap %x sp %x pc %x\n",
			i_fp->r_trapno, getsp(), i_fp->r_eip);
		printf("etext %x estack %x edata %x nofault %x\n",
			etext, estack, edata, nofault);
		_longjmp(abort_jmp, 1);
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

	printf("fault: trap: [%x] error: [%x] sp: [%x]"
	    " pc: [%x] fault-address(cr2): [%x]\n",
	    i_fp->r_trapno, i_fp->r_err, i_fp->r_esp, i_fp->r_eip, cr2());
	delayl(0xffffff);

#ifdef DEBUG
	ml_pause();
#endif
	cmd(i_fp);	/* error not resolved, enter debugger */

	step_expected = (i_fp->r_efl & 0x100);
	return (0);
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

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		return ((int)val);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

short
peek(addr)
	short *addr;
{
	short val;

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (val);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}
long
peekl(addr)
	long *addr;
{
	long val;

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (val);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
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

poketext(addr, val)
	int *addr;
	int val;
{
	return (pokel(addr, val));
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
	char *fp;
	int res;
	extern void _exit();

	if (curregs != 0) {
		printf("bad call to spawn\n");
		_exit();
	}
	if ((res = _setjmp(mainregs)) == 0) {
		/*
		 * Setup top (null) stack frame.
		 */
		sp -= STKFRAME;
		((struct frame *)sp)->fr_savpc = 0;
		((struct frame *)sp)->fr_savfp = 0;
		/*
		 * Setup stack frame for routine with return to exit.
		 */
		fp = sp;
		sp -= STKFRAME;
		((struct frame *)sp)->fr_savpc = (int)_exit;
		((struct frame *)sp)->fr_savfp = (int)fp;
		/*
		 * Setup new return stack frame with routine return value.
		 */
		fp = sp;
		sp -= STKFRAME;
		((struct frame *)sp)->fr_savpc = (int)routine;
		((struct frame *)sp)->fr_savfp = (int)fp;

		/* copy entire jump buffer to debugregs */
		bcopy((caddr_t)mainregs, (caddr_t)debugregs, sizeof (jmp_buf));
		debugregs[JB_FP] = (int)sp;	/* set sp */

		curregs = debugregs;

		_longjmp(debugregs, 1);		/* jump to new context */
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
extern	void 	(*kxc_call)(int, int, int, int, int, void());
extern	void 	get_smothered();
/*
 * Main interpreter command loop.
 */
cmd(i_fp)
struct regs *i_fp;
{
	int addr;

	dorun = 0;

	/*
	 * See if the sp says that we are already on the debugger stack
	 */
	reg = regsave;
	addr = getsp();
	if (addr > (int)&rstk && addr < (int)estack) {
		printf("Already in debugger!\n");
		delayl(0xffffff);
		return;
	}

	if (xc_initted && *xc_initted)
		goto_kernel(0, 0, 0, 2, ~(1 << cur_cpuid), get_smothered,
		    kxc_call);

	do {
		doswitch();
		if (dorun == 0)
			printf("cmd: nothing to do\n");
	} while (dorun == 0);

	step_expected = (i_fp->r_efl & 0x100);

	/* we don't need to splx since we are returning to the caller */
	/* and will reset his/her state */
}


/*
 * This is a stub since common code calles this routine
 */
void
Setpgmap(v, pte)
	caddr_t v;
	int pte;
{
}

#ifdef	DO_FAULT_TRACE
traceback(sp)
	caddr_t sp;
{

typedef struct {
				struct frame sf;
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
		if (fp == fp->sf.fr_savfp) {
			printf("FP loop at %x", fp);
			break;
		}
		tryabort(1);
		printf("Called from %x, fp=%x, args=%x %x %x %x %x %x\n",
		    fp->sf.fr_savpc, fp->sf.fr_savfp,
		    fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
		    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5]);

		fp = fp->sf.fr_savfp;
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
