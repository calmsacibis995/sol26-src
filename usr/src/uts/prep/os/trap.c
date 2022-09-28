/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)trap.c	1.41	96/10/17 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/syscall.h>
#include <sys/cpuvar.h>
#include <sys/vm.h>
#include <sys/msgbuf.h>
#include <sys/sysinfo.h>
#include <sys/fault.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/mmu.h>
#include <sys/pcb.h>
#include <sys/cpu.h>
#include <sys/psw.h>
#include <sys/pte.h>
#include <sys/reg.h>
#include <sys/trap.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/cmn_err.h>
#include <sys/prsystm.h>
#include <sys/mutex_impl.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>

#include <vm/hat.h>
#include <vm/hat_ppcmmu.h>

#include <vm/seg_kmem.h>
#include <vm/as.h>
#include <vm/seg.h>

#include <sys/procfs.h>

#include <sys/modctl.h>
#include <sys/aio_impl.h>

#include <sys/reboot.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#include <sys/tnf.h>
#include <sys/tnf_probe.h>

extern char *trap_desc();
#define	USER	0x10000		/* user-mode flag added to trap type */


static int tudebug = 0;
static int tudebugbpt = 0;
static int tudebugfpe = 0;
static int alignfaults = 0;
int panic_reg;	/* used by 'adb -k' */

#if defined(TRAPDEBUG) || defined(lint)
static int tdebug = 0;
static int lodebug = 0;
static int faultdebug = 0;
#else
#define	tdebug	0
#define	lodebug	0
#define	faultdebug	0
#endif /* defined(TRAPDEBUG) || defined(lint) */

static void showregs(register u_int type, register struct regs *rp,
	caddr_t addr, register u_int dsisr);

static int
die(u_int type, struct regs *rp, caddr_t addr,
	u_int dsisr, ...)
{
	char *trap_name;
	va_list adx;

	panic_reg = (int)rp;
	type &= ~USER;
	trap_name = trap_desc(type);
	va_start(adx, dsisr);
	(void) setup_panic("trap", adx);
	va_end(adx);

	printf("BAD TRAP: name='%s', type=0x%x rp=0x%x addr=0x%x dsisr=0x%x\n",
	    trap_name, type, rp, addr, dsisr);
	if (type == T_DATA_FAULT && addr < (caddr_t)KERNELBASE) {
		char modname[MODMAXNAMELEN + 1];

		if (mod_containing_pc((caddr_t)rp->r_pc, modname)) {
			printf("BAD TRAP occurred in module \"%s\" due to "
			    "an illegal access to a user address.\n",
			    modname);
		}
	}

	showregs(type, rp, addr, dsisr);
	traceback((caddr_t)rp->r_sp);
	cmn_err(CE_PANIC, "trap");
	return (0);
}

/* this is *very* temporary */
#define	instr_size(rp, addrp, rw)	(4)

/*
 * The following code deals with the fact that the 603 class of PowerPC
 * microprocessors does not report the correct address of the data fault
 * when running in little endian mode.  Instead, it reports a munged
 * address, based of the size of the load/store operand, as follows:
 *
 *	operand_size	address_munge_xor_value
 *	    1-byte		   7
 *	    2-byte		   6
 *	    4-byte		   4
 *	    8-byte		   0
 *
 * Note that is address_munge_xor_value is always "8 - operand_size".
 * This may be useful for the watchpoints.
 *
 * The algorithm used is to crack the instruction uses heuristics to
 * determine the operand size.  There are two classes of load/stores
 * on PowerPC, register + offset (primary opcodes 32-55), and register
 * + index (primary opcode 31).  The first of these uses a simple table
 * lookup to determine the operand size.  The second uses a carefully
 * ordered sequence of tests, based on the current set of opcodes for
 * the load/store instructions.
 */

static int class1[] = {
	4,	/*	lwz	32	*/
	4,	/*	lwzu	33	*/
	7,	/*	lbz	34	*/
	7,	/*	lbzu	35	*/
	4,	/*	stw	36	*/
	4,	/*	stwu	37	*/
	7,	/*	stb	38	*/
	7,	/*	stbu	39	*/
	6,	/*	lhz	40	*/
	6,	/*	lhzu	41	*/
	6,	/*	lha	42	*/
	6,	/*	lhau	43	*/
	6,	/*	sth	44	*/
	6,	/*	sthu	45	*/
	0,	/*	lmw	46	*/
	0,	/*	stmw	47	*/
	4,	/*	lfs	48	*/
	4,	/*	lfsu	49	*/
	0,	/*	lfd	50	*/
	0,	/*	lfdu	51	*/
	4,	/*	stfs	52	*/
	4,	/*	stfsu	53	*/
	0,	/*	stfd	54	*/
	0,	/*	stfdu	55	*/
};

static caddr_t
dar_603(caddr_t dar, int instr)
{
	int xorval;
	int opcode;

	if ((opcode = (instr >> 26) & 0x3f) != 31)
		if (opcode < 32 || opcode > 55) /* oops */
			xorval = 0;
		else
			xorval = class1[opcode - 32];
	else {
		opcode = (instr >> 1) & 0x3ff;
		if ((opcode & 0xf) != 0x7)
			if ((opcode & 0x300) == 0x300)
				xorval = 6;
			else
				xorval = 4;
		else if ((opcode & 0x340) == 0x040)
			xorval = 7;
		else if ((opcode & 0x300) == 0x100)
			xorval = 6;
		else if (((opcode & 0x300) == 0x200) && (opcode & 0x040))
			xorval = 0;
		else
			xorval = 4;
	}

	return ((caddr_t)((int)dar ^ xorval));
}

/*
 * Level2 trap handler:
 *
 * Called from the level1 trap handler when a processor trap occurs.
 *
 * Note: All user-level traps that might call stop() must exit
 * trap() by 'goto out' or by falling through.
 * Note: Argument "trap" is trap type (vector offset)
 */
void
trap(register struct regs *rp, register u_int type, caddr_t dar,
	register u_int dsisr)
{
	caddr_t addr = 0;
	enum seg_rw rw;
	extern int stop_on_fault(u_int, k_siginfo_t *);
	proc_t *p = ttoproc(curthread);
	klwp_id_t lwp = ttolwp(curthread);
	clock_t syst;
	u_int lofault;
	faultcode_t pagefault(), res;
	k_siginfo_t siginfo;
	u_int fault = 0;
	int driver_mutex = 0;	/* get unsafe_driver before returning */
	int mstate;
	int sicode = 0;
	char *badaddr;
	int inst;
	int watchcode;
	int watchpage;
	caddr_t vaddr;
	int sz;
	int ta;

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.trap, 1);
	syst = p->p_stime;

	ASSERT(curthread->t_schedflag & TS_DONT_SWAP);

	if (type == T_DATA_FAULT) {
		if (dsisr & DSISR_STORE)
			rw = S_WRITE;
		else
			rw = S_READ;
		addr = ((cputype & CPU_ARCH) != PPC_603_ARCH) ? dar :
			dar_603(dar, fuiword((int *)rp->r_pc));
	} else {
		rw = S_READ;
		addr = (caddr_t)rp->r_srr0;
	}

	if (tdebug)
		showregs(type, rp, addr, dsisr);

	if (USERMODE(rp->r_msr)) {
		/*
		 * Set up the current cred to use during this trap. u_cred
		 * no longer exists.  t_cred is used instead.
		 * The current process credential applies to the thread for
		 * the entire trap.  If trapping from the kernel, this
		 * should already be set up.
		 */
		if (curthread->t_cred != p->p_cred) {
			crfree(curthread->t_cred);
			curthread->t_cred = crgetcred();
		}
		ASSERT(lwp != NULL);
		type |= USER;
		ASSERT(lwptoregs(lwp) == (struct regs *)rp);
		lwp->lwp_state = LWP_SYS;
#ifdef NPROBE
		if (curthread->t_proc_flag & TP_MSACCT)
#else
		if ((curthread->t_proc_flag & TP_MSACCT) || tnf_tracing_active)
#endif /* NPROBE */
		{
			switch (type) {
			case T_DATA_FAULT + USER:
				mstate = LMS_DFAULT;
				break;
			case T_TEXT_FAULT + USER:
				mstate = LMS_TFAULT;
				break;
			default:
				mstate = LMS_TRAP;
				break;
			}
			/* Kernel probe */
			TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
			    tnf_microstate, state, mstate);

			if (curthread->t_proc_flag & TP_MSACCT)
				mstate = new_mstate(curthread, mstate);
			else
				mstate = LMS_USER;
		} else {
			mstate = LMS_USER;
		}
		siginfo.si_signo = 0;
	} else {
		if (MUTEX_OWNER_LOCK(&unsafe_driver) &&
		    UNSAFE_DRIVER_LOCK_HELD()) {
			driver_mutex = 1;
			mutex_exit(&unsafe_driver);
		}
	}

	switch (type) {

	default:
		if (type & USER) {
			if (tudebug)
				showregs(type, rp, addr, dsisr);
			printf("trap: Unknown trap type %d in user mode\n",
				(type & ~USER));
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGILL;
			siginfo.si_code  = ILL_ILLTRP;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			siginfo.si_trapno = type & ~USER;
			fault = FLTILL;
			break;
		} else
			(void) die(type, rp, addr, dsisr);
			/*NOTREACHED*/

	case T_MACH_CHECK + USER:	/* machine check trap */
		/*
		 * Normally machine check interrupts are not recoverable.
		 * And the least we can do is to try to continue by sending
		 * a signal to the current process (which may not have
		 * caused it if this happend asynchronously).
		 */
		if (tudebug)
			showregs(type, rp, addr, dsisr);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGBUS;
		siginfo.si_code = BUS_ADRERR;
		siginfo.si_addr = (caddr_t)rp->r_pc;
		fault = FLTACCESS;
		break;
	case T_MACH_CHECK:		/* machine check trap */
		/*
		 * Normally machine check interrupts are not recoverable.
		 * If the current thread is expecting this (e.g during
		 * bus probe) then we try to continue by doing a longjmp
		 * back to the probe function assuming that it caused
		 * this error.
		 *
		 * Note: if this has happend asynchronously (which we can't
		 * tell easily) then the above may not be right.
		 */
		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
		/* may have been expected by C (e.g. bus probe) */
		if (curthread->t_nofault) {
			label_t *ftmp;

			ftmp = curthread->t_nofault;
			curthread->t_nofault = 0;
#ifndef LOCKNEST
			if (driver_mutex)
				mutex_enter(&unsafe_driver);
#endif /* LOCKNEST */
			longjmp(ftmp);
		}

		if (curthread->t_lofault) {
			/* lofault is set (e.g bcopy()); set the return pc */
			rp->r_r3 = EFAULT;
			rp->r_pc = curthread->t_lofault;
			goto cleanup;
		}

		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		(void) die(type, rp, addr, dsisr);
		/*NOTREACHED*/
	case T_RESET:			/* reset trap */
	case T_RESET + USER:		/* reset trap */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		(void) die(type, rp, addr, dsisr);
		/*NOTREACHED*/

	case T_ALIGNMENT:	/* alignment error in system mode */

		/* Registers SRR0, SRR1, and DSISR have the information */
		if (curthread->t_lofault && curthread->t_onfault) {
			label_t *ftmp;

			ftmp = curthread->t_onfault;
			curthread->t_onfault = NULL;
			curthread->t_lofault = 0;
#ifndef	LOCKNEST
			if (driver_mutex)
				mutex_enter(&unsafe_driver);
#endif	LOCKNEST
			longjmp(ftmp);
		}
		(void) die(type, rp, addr, dsisr);
		/*NOTREACHED*/

	case T_ALIGNMENT + USER:	/* user alignment error */

		/* Registers SRR0, SRR1, and DSISR have the information */
		if (tudebug)
			showregs(type, rp, addr, dsisr);
		alignfaults++;
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGBUS;
		siginfo.si_code = BUS_ADRALN;
		if (rp->r_pc & 3) 	/* offending address, if pc */
			siginfo.si_addr = (caddr_t)rp->r_pc;
		else
			siginfo.si_addr = dar;
		fault = FLTACCESS;
		break;

	case T_TEXT_FAULT:	/* system text access exception */

		if (lodebug)
			showregs(type, rp, addr, dsisr);

		(void) die(type, rp, addr, dsisr);
		/*NOTREACHED*/

	case T_DATA_FAULT:	/* system data access exception */

		/* Registers SRR0, SRR1, DAR, and DSISR have the information */

		/* may have been expected by C (e.g. bus probe) */
		if (curthread->t_nofault) {
			label_t *ftmp;

			ftmp = curthread->t_nofault;
			curthread->t_nofault = 0;
#ifndef LOCKNEST
			if (driver_mutex)
				mutex_enter(&unsafe_driver);
#endif /* LOCKNEST */
			longjmp(ftmp);
		}

		if (dsisr & DSISR_DATA_BKPT) { /* Only on 601 */
			/* HW break point data access trap */
			if (boothowto & RB_DEBUG)
				debug_enter((char *)NULL);
			else
				(void) die(type, rp, addr, dsisr);
		}

		/*
		 * See if we can handle as pagefault. Save lofault
		 * across this. Here we assume that an address
		 * less than KERNELBASE is a user fault.
		 * We can do this as copy.s routines verify that the
		 * starting address is less than KERNELBASE before
		 * starting and because we know that we always have
		 * KERNELBASE mapped as invalid to serve as a "barrier".
		 */
		lofault = curthread->t_lofault;
		curthread->t_lofault = 0;

		if (curthread->t_proc_flag & TP_MSACCT)
			mstate = new_mstate(curthread, LMS_KFAULT);
		else
			mstate = LMS_SYSTEM;

		if (addr < (caddr_t)KERNELBASE) {
			if (lofault == 0)
				(void) die(type, rp, addr, dsisr);
			res = pagefault(addr,
				(dsisr & DSISR_PROT) ? F_PROT : F_INVAL, rw, 0);
			if (res == FC_NOMAP &&
			    addr < (caddr_t)USRSTACK &&
			    grow((int *)addr))
				res = 0;
		} else {
			res = pagefault(addr,
				(dsisr & DSISR_PROT) ? F_PROT : F_INVAL, rw, 1);
		}

		if (curthread->t_proc_flag & TP_MSACCT)
			(void) new_mstate(curthread, mstate);

		/*
		 * Restore lofault. If we resolved the fault, exit.
		 * If we didn't and lofault wasn't set, die.
		 */
		curthread->t_lofault = lofault;
		if (res == 0)
			goto cleanup;

#ifdef XXX_NEEDED
		/*
		 * Check for mutex_exit hook.  This is to protect mutex_exit
		 * from deallocated locks.  It would be too expensive to use
		 * nofault there.
		 * XXXPPC - review.  Brian, is this needed?
		 */
		{
			void	mutex_exit_nofault(void);
			void	mutex_exit_fault(kmutex_t *);

			if (rp->r_pc == (int)mutex_exit_nofault) {
				rp->r_pc = (int)mutex_exit_fault;
				goto cleanup;
			}
		}
#endif

		if (lofault == 0)
			(void) die(type, rp, addr, dsisr);

		/*
		 * Cannot resolve fault.  Return to lofault.
		 */
		if (lodebug) {
			showregs(type, rp, addr, dsisr);
			traceback((caddr_t)rp->r_sp);
		}
		if (FC_CODE(res) == FC_OBJERR)
			res = FC_ERRNO(res);
		else
			res = EFAULT;
		rp->r_r3 = res;
		rp->r_pc = curthread->t_lofault;
		goto cleanup;

	case T_TEXT_FAULT + USER:	/* user instruction access exception */
		rw = S_EXEC;
		/* FALLTHROUGH */
	case T_DATA_FAULT + USER:	/* user data access exception */

		/* Registers SRR0, SRR1, DAR, and DSISR have the information */

		if (type == T_DATA_FAULT + USER &&
		    (dsisr & DSISR_DATA_BKPT)) {	/* only on 601? */
			/* HW break point data access trap */
			/* XXXPPC - debug registers support? */
			if (tudebug && tudebugbpt)
				showregs(type, rp, addr, dsisr);
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGTRAP;
			siginfo.si_code  = TRAP_BRKPT;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTBPT;
			break;
		}

		if (faultdebug) {
			char *fault_str;

			switch (rw) {
			case S_READ:
				fault_str = "read";
				break;
			case S_WRITE:
				fault_str = "write";
				break;
			case S_EXEC:
				fault_str = "instruction";
				break;
			default:
				fault_str = "";
				break;
			}
			printf("user %s fault:  addr=0x%x dsisr=0x%x\n",
			    fault_str, (int)addr, dsisr);
		}

		ASSERT(!(curthread->t_flag & T_WATCHPT));
		watchpage = (p->p_warea != NULL &&
			pr_is_watchpage(addr, rw));
		vaddr = addr;
		if (watchpage &&
		    (sz = instr_size(rp, &vaddr, rw)) > 0 &&
		    (watchcode = pr_is_watchpoint(&vaddr, &ta,
		    sz, NULL, rw)) != 0) {
			if (ta) {
				do_watch_step(vaddr, sz, rw,
					watchcode, rp->r_pc);
				res = pagefault(addr, F_INVAL, rw, 0);
			} else {
				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_signo = SIGTRAP;
				siginfo.si_code = watchcode;
				siginfo.si_addr = vaddr;
				siginfo.si_trapafter = 0;
				siginfo.si_pc = (caddr_t)rp->r_pc;
				fault = FLTWATCH;
				break;
			}
		} else if (watchpage && rw == S_EXEC) {
			do_watch_step(vaddr, 4, rw, 0, 0);
			res = pagefault(addr, F_INVAL, rw, 0);
		} else if (watchpage) {
			/* XXX never succeeds (for now) */
			if (pr_watch_emul(rp, vaddr, rw))
				goto out;
			do_watch_step(vaddr, 8, rw, 0, 0);
			res = pagefault(addr, F_INVAL, rw, 0);
			break;
		} else if (type == T_TEXT_FAULT + USER) {
			res = pagefault(addr,
			    (rp->r_srr1 & SRR1_PROT) ? F_PROT : F_INVAL, rw, 0);
		} else {
			res = pagefault(addr,
			    (dsisr & DSISR_PROT) ? F_PROT : F_INVAL, rw, 0);
		}
		/*
		 * If pagefault() succeeded, ok.
		 * Otherwise attempt to grow the stack.
		 */
		if (res == 0 ||
		    (res == FC_NOMAP &&
		    type != T_TEXT_FAULT + USER &&
		    addr < (caddr_t)USRSTACK &&
		    grow((int *)addr))) {
			lwp->lwp_lastfault = FLTPAGE;
			lwp->lwp_lastfaddr = addr;
			if (prismember(&p->p_fltmask, FLTPAGE)) {
				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_addr = addr;
				(void) stop_on_fault(FLTPAGE, &siginfo);
			}
			goto out;
		}

		if (tudebug)
			showregs(type, rp, addr, dsisr);
		/*
		 * In the case where both pagefault and grow fail,
		 * set the code to the value provided by pagefault.
		 * We map all errors returned from pagefault() to SIGSEGV.
		 */
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_addr = addr;
		switch (FC_CODE(res)) {
		case FC_HWERR:
		case FC_NOSUPPORT:
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRERR;
			fault = FLTACCESS;
			break;
		case FC_ALIGN:
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRALN;
			fault = FLTACCESS;
			break;
		case FC_OBJERR:
			if ((siginfo.si_errno = FC_ERRNO(res)) != EINTR) {
				siginfo.si_signo = SIGBUS;
				siginfo.si_code = BUS_OBJERR;
				fault = FLTACCESS;
			}
			break;
		default:	/* FC_NOMAP or FC_PROT */
			siginfo.si_signo = SIGSEGV;
			siginfo.si_code =
			    (res == FC_NOMAP) ? SEGV_MAPERR : SEGV_ACCERR;
			fault = FLTBOUNDS;
			break;
		}
		break;

	case T_NO_FPU: /* FPU Not Available */
	case T_FP_ASSIST: /* Floating Point Assist trap (PowerPC) */

		/* this shouldn't happen */
		(void) die(type, rp, (caddr_t)0, 0);
		/*NOTREACHED*/

	case T_FP_ASSIST + USER: /* Floating Point Assist trap (PowerPC) */

		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0, 0);

		if (sicode = fp_assist_fault(rp)) {
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGFPE;
			siginfo.si_code  = sicode;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTFPE;
		}
		break;

	case T_NO_FPU + USER: /* FPU Not Available */

		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0, 0);

		if (sicode = no_fpu_fault(rp)) {
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGFPE;
			siginfo.si_code  = sicode;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTFPE;
		}
		break;

	case T_PGM_CHECK + USER:	/* Program Check exception */

		/* Registers SRR0, and SRR1 have the information */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);

		if (rp->r_srr1 & SRR1_SRR0_VALID)
			badaddr = (caddr_t)(rp->r_pc - 4);
		else
			badaddr = (caddr_t)rp->r_pc;

		bzero((caddr_t)&siginfo, sizeof (siginfo));
		switch (rp->r_srr1 & SRR1_PCHK_MASK) {
			case SRR1_ILL_INST:
				/* Illegal Instruction */
				siginfo.si_signo = SIGILL;
				siginfo.si_code  = ILL_ILLOPC;
				siginfo.si_addr  = (caddr_t)badaddr;
				fault = FLTILL;
				break;
			case SRR1_PRIV_INST:
				/* Privileged Instruction */
				siginfo.si_signo = SIGILL;
				siginfo.si_code  = ILL_PRVOPC;
				siginfo.si_addr  = (caddr_t)badaddr;
				fault = FLTILL;
				break;

			case SRR1_TRAP:
				/* get the trap instruction */
				inst = fuword((int *)rp->r_pc);
				if (inst == BPT_INST) { /* breakpoint trap */
				    siginfo.si_signo = SIGTRAP;
				    siginfo.si_code = TRAP_BRKPT;
				    siginfo.si_addr = (caddr_t)rp->r_pc;
				    fault = FLTBPT;
				    break;
				} else {
				    /* Trap conditional exception */
				    printf("Unrecognized TRAP instruction ");
				    printf("(pc %x instruction %x)\n",
					rp->r_pc, inst);
				    siginfo.si_signo = SIGILL;
				    siginfo.si_code  = ILL_ILLOPC;
				    siginfo.si_addr  = (caddr_t)badaddr;
				    fault = FLTILL;
				    break;
				}

			case SRR1_FP_EN:
				/* FP Enabled exception */
				if (sicode = fpu_en_fault(rp)) {
					siginfo.si_signo = SIGFPE;
					siginfo.si_code  = sicode;
					siginfo.si_addr  = (caddr_t)rp->r_pc;
					fault = FLTFPE;
					/*
					 * Increment PC for user signal
					 * handler.
					 */
					rp->r_pc += 4;
				}
				break;
		}
		break;

	case T_IO_ERROR:	/* I/O Error exception (on 601 only?) */

		/* Registers SRR0, SRR1, and DAR have the information */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
		printf("I/O Error in system mode: addr=0x%x npc=0x%x\n",
			dar, rp->r_srr0);
		goto cleanup;

	case T_IO_ERROR + USER:	/* I/O Error exception (on 601 only?) */

		/* Registers SRR0, SRR1, and DAR have the information */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
		printf("I/O Error in user mode: dar=0x%x pc=0x%x msr=0x%x\n",
			dar, rp->r_pc - 4, rp->r_msr & 0xffff);
		break;

	case T_SINGLE_STEP: /* Trace trap (PowerPC) */
	case T_EXEC_MODE: /* Run Mode trap - MPC601 only */

		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		else
			(void) die(type, rp, (caddr_t)0, 0);
		goto cleanup;

	case T_SINGLE_STEP + USER: /* Trace trap (PowerPC) */
	case T_EXEC_MODE + USER: /* Run Mode trap */

		if (tudebug && tudebugbpt)
			showregs(type, rp, (caddr_t)0, 0);

		bzero((caddr_t)&siginfo, sizeof (siginfo));
		if (type == T_SINGLE_STEP + USER || (rp->r_msr & MSR_SE)) {
			pcb_t *pcb = &lwp->lwp_pcb;

			rp->r_msr &= ~MSR_SE;
			if (pcb->pcb_flags & PCB_NORMAL_STEP) {
				siginfo.si_signo = SIGTRAP;
				siginfo.si_code = TRAP_TRACE;
				siginfo.si_addr  = (caddr_t)rp->r_pc;
				fault = FLTTRACE;
			}
			if (pcb->pcb_flags & PCB_WATCH_STEP)
				fault = undo_watch_step(&siginfo);
			pcb->pcb_flags &= ~(PCB_NORMAL_STEP|PCB_WATCH_STEP);
		} else {
			siginfo.si_signo = SIGTRAP;
			siginfo.si_code  = TRAP_BRKPT;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTBPT;
		}
		break;

	case T_AST + USER:		/* profiling or resched psuedo trap */

		if (lwp->lwp_oweupc && lwp->lwp_prof.pr_scale) {
			mutex_enter(&p->p_pflock);
			addupc((void(*)())rp->r_pc, &lwp->lwp_prof, 1);
			mutex_exit(&p->p_pflock);
			lwp->lwp_oweupc = 0;
		}
		break;
	}

	/*
	 * We can't get here from a system trap
	 */
	ASSERT(type & USER);

	if (fault) {
		/*
		 * Remember the fault and fault adddress
		 * for real-time (SIGPROF) profiling.
		 */
		lwp->lwp_lastfault = fault;
		lwp->lwp_lastfaddr = siginfo.si_addr;
		/*
		 * If a debugger has declared this fault to be an
		 * event of interest, stop the lwp.  Otherwise just
		 * deliver the associated signal.
		 */
		if (siginfo.si_signo != SIGKILL &&
		    prismember(&p->p_fltmask, fault) &&
		    stop_on_fault(fault, &siginfo) == 0)
			siginfo.si_signo = 0;
	}

	if (siginfo.si_signo)
		trapsig(&siginfo, 1);

	if (lwp->lwp_prof.pr_scale) {
		int ticks;
		clock_t tv = p->p_stime;

		ticks = tv - syst;

		if (ticks) {
			mutex_enter(&p->p_pflock);
			addupc((void(*)())rp->r_pc, &lwp->lwp_prof, ticks);
			mutex_exit(&p->p_pflock);
		}
	}

	if (curthread->t_astflag | curthread->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(curthread);
		curthread->t_sig_check = 0;

		/*
		 * for kaio requests that are on the per-process poll queue,
		 * aiop->aio_pollq, they're AIO_POLL bit is set, the kernel
		 * should copyout their result_t to user memory. by copying
		 * out the result_t, the user can poll on memory waiting
		 * for the kaio request to complete.
		 */
		if (p->p_aio)
			aio_cleanup(0);

		/*
		 * If this LWP was asked to hold, call holdlwp(), which will
		 * stop.  holdlwps() sets this up and calls pokelwps() which
		 * sets the AST flag.
		 *
		 * Also check TP_EXITLWP, since this is used by fresh new LWPs
		 * through lwp_rtt().  That flag is set if the lwp_create(2)
		 * syscall failed after creating the LWP.
		 */
		if (ISHOLD(p))
			holdlwp();

		/*
		 * All code that sets signals and makes ISSIG evaluate true must
		 * set t_astflag afterwards.
		 */
		if (ISSIG_PENDING(curthread, lwp, p)) {
			if (issig(FORREAL))
				psig();
			curthread->t_sig_check = 1;
		}

		if (curthread->t_rprof != NULL) {
			realsigprof(0, 0);
			curthread->t_sig_check = 1;
		}
	}

out:	/* We can't get here from a system trap */
	ASSERT(type & USER);

	if (ISHOLD(p))
		holdlwp();

	/*
	 * Set state to LWP_USER here so preempt won't give us a kernel
	 * priority if it occurs after this point.  Call CL_TRAPRET() to
	 * restore the user-level priority.
	 *
	 * It is important that no locks (other than spinlocks) be entered
	 * after this point before returning to user mode (unless lwp_state
	 * is set back to LWP_SYS).
	 */
	lwp->lwp_state = LWP_USER;
	if (curthread->t_trapret) {
		curthread->t_trapret = 0;
		thread_lock(curthread);
		CL_TRAPRET(curthread);
		thread_unlock(curthread);
	}
	if (CPU->cpu_runrun)
		preempt();
	if (curthread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(curthread, mstate);

	/* Kernel probe */
	TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
	    tnf_microstate, state, LMS_USER);

	return;

cleanup:	/* system traps end up here */
	ASSERT(!(type & USER));

	/*
	 * If the unsafe_driver mutex was held by the thread on entry,
	 * we released it so we could call other drivers.  We re-enter it here.
	 */
	if (driver_mutex)
		mutex_enter(&unsafe_driver);
}

/*
 * Patch non-zero to disable preemption of threads in the kernel.
 */
int IGNORE_KERNEL_PREEMPTION = 0; /* XXX - delete this someday */

struct kpreempt_cnts {		/* kernel preemption statistics */
	int	kpc_idle;	/* executing idle thread */
	int	kpc_intr;	/* executing interrupt thread */
	int	kpc_clock;	/* executing clock thread */
	int	kpc_blocked;	/* thread has blocked preemption (t_preempt) */
	int	kpc_notonproc;	/* thread is surrendering processor */
	int	kpc_inswtch;	/* thread has ratified scheduling decision */
	int	kpc_prilevel;	/* processor interrupt level is too high */
	int	kpc_apreempt;	/* asynchronous preemption */
	int	kpc_spreempt;	/* synchronous preemption */
}	kpreempt_cnts;

/*
 * kernel preemption: forced rescheduling, preempt the running kernel thread.
 *	the argument is old PIL for an interrupt,
 *	or the distingished value KPREEMPT_SYNC.
 */
void
kpreempt(int asyncspl)
{
	if (IGNORE_KERNEL_PREEMPTION) {
		aston(CPU->cpu_dispthread);
		return;
	}
	/*
	 * Check that conditions are right for kernel preemption
	 */
	do {
		if (curthread->t_preempt) {
			extern kthread_id_t	clock_thread;
			/*
			 * either a privileged thread (idle, panic, interrupt)
			 *	or will check when t_preempt is lowered
			 */
			if (curthread->t_pri < 0)
				kpreempt_cnts.kpc_idle++;
			else if (curthread->t_flag & T_INTR_THREAD) {
				kpreempt_cnts.kpc_intr++;
				if (curthread == clock_thread)
					kpreempt_cnts.kpc_clock++;
			} else
				kpreempt_cnts.kpc_blocked++;
			aston(CPU->cpu_dispthread);
			return;
		}
		if (curthread->t_state != TS_ONPROC ||
		    curthread->t_disp_queue != &CPU->cpu_disp) {
			/* this thread will be calling swtch() shortly */
			kpreempt_cnts.kpc_notonproc++;
			if (CPU->cpu_thread != CPU->cpu_dispthread) {
				/* already in swtch(), force another */
				kpreempt_cnts.kpc_inswtch++;
				siron();
			}
			return;
		}
		if (getpil() >= LOCK_LEVEL) {
			/*
			 * We can't preempt this thread if it is at
			 * a PIL > LOCK_LEVEL since it may be holding
			 * a spin lock (like sched_lock).
			 */
			siron();	/* check back later */
			kpreempt_cnts.kpc_prilevel++;
			return;
		}

		if (asyncspl != KPREEMPT_SYNC)
			kpreempt_cnts.kpc_apreempt++;
		else
			kpreempt_cnts.kpc_spreempt++;

		curthread->t_preempt++;
		preempt();
		curthread->t_preempt--;
	} while (CPU->cpu_kprunrun);
}

/*
 * Print out a traceback for kernel traps
 */
/* ARGSUSED */
void
traceback(caddr_t sp)
{
}

/*
 * General system stack backtrace
 */
void
tracedump(void)
{
	label_t l;

	(void) setjmp(&l);
	traceback((caddr_t)l.val[1]);
}

/*
 * Print out debugging info.
 */
static void
showregs(register u_int type, register struct regs *rp,
	caddr_t addr, register u_int dsisr)
{
	int s;
	int usermode;

	s = spl7();
	usermode = (type & USER);
	type &= ~USER;
	if (u.u_comm[0])
		printf("%s: ", u.u_comm);
	if (type <= T_EXEC_MODE)
		printf("%s\n", trap_desc(type));
	if (type == T_DATA_FAULT || type == T_TEXT_FAULT) {
		union ptes pte;

		mmu_getpte(addr, &pte);
		printf("%s fault at addr=0x%x,pte=[0]0x%x [1]0x%x dsisr=0x%x\n",
		    usermode ? "User" : "Kernel", addr, pte.pte_words[0],
		    pte.pte_words[1], dsisr);
	} else if (addr) {
		printf("addr=0x%x\n", addr);
	}

	printf("pid=%d, pc=0x%x, sp=0x%x, msr=0x%x\n",
	    (ttoproc(curthread) && ttoproc(curthread)->p_pidp) ?
	    (ttoproc(curthread)->p_pid) : 0, rp->r_pc, rp->r_sp,
	    rp->r_msr);
	printf("gpr0-gpr7:   %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r0, rp->r_r1, rp->r_r2, rp->r_r3, rp->r_r4,
	    rp->r_r5, rp->r_r6, rp->r_r7);
	printf("gpr8-gpr15:  %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r8, rp->r_r9, rp->r_r10, rp->r_r11, rp->r_r12,
	    rp->r_r13, rp->r_r14, rp->r_r15);
	printf("gpr16-gpr23: %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r16, rp->r_r17, rp->r_r18, rp->r_r19, rp->r_r20,
	    rp->r_r21, rp->r_r22, rp->r_r23);
	printf("gpr24-gpr31: %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r24, rp->r_r25, rp->r_r26, rp->r_r27, rp->r_r28,
	    rp->r_r29, rp->r_r30, rp->r_r31);
	printf("cr %x, lr %x, ctr %x, xer %x, mq %x\n",
	    rp->r_cr, rp->r_lr, rp->r_ctr, rp->r_xer, rp->r_mq);
	printf("srr1 %x dsisr %x\n", rp->r_srr1, dsisr);

	(void) splx(s);
}

/*
 * Given a trap type (vector address), return trap description.
 */
char *
trap_desc(register int type)
{
	register char *pdesc;

	switch (type) {
	case T_RESET:
		pdesc = "Reset Trap";
		break;
	case T_MACH_CHECK:
		pdesc = "Machine Check Trap";
		break;
	case T_DATA_FAULT:
		pdesc = "Data Access Trap";
		break;
	case T_TEXT_FAULT:
		pdesc = "Instruction Access Trap";
		break;
	case T_INTERRUPT:
		pdesc = "External Interrupt";
		break;
	case T_ALIGNMENT:
		pdesc = "Alignment Trap";
		break;
	case T_PGM_CHECK:
		pdesc = "Program Check Trap";
		break;
	case T_NO_FPU:
		pdesc = "FPU Not Available";
		break;
	case T_DECR:
		pdesc = "Decrementer Trap";
		break;
	case T_IO_ERROR:
		pdesc = "I/O Error Trap";
		break;
	case T_SYSCALL:
		pdesc = "System Call Trap";
		break;
	case T_SINGLE_STEP:
		pdesc = "Single Step Trap";
		break;
	case T_FP_ASSIST:
		pdesc = "Floating-point Assist Trap";
		break;
	case T_EXEC_MODE:
		pdesc = "Run Mode Exception";
		break;
	case T_AST:
		pdesc = "AST";
		break;
	case T_TLB_DLOADMISS:
		pdesc = "TLB Data Load Miss exception";
		break;
	case T_TLB_DSTOREMISS:
		pdesc = "TLB Data Store Miss exception";
		break;
	case T_TLB_IMISS:
		pdesc = "TLB Instruction Miss exception";
		break;
	case T_PERF_MI:
		pdesc = "Performance Monitor Interrupt";
		break;
	case T_IABR:
		pdesc = "Instruction Address Breakpoint Exception";
		break;
	case T_SYS_MGMT:
		pdesc = "System Management Exception";
		break;
	default:
		pdesc = "Unknown Trap";
		break;
	}

	return (pdesc);
}
