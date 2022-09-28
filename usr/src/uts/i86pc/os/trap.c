/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*		All Rights Reserved   				*/
/*								*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/
/*								*/
/*	#ident  "@(#)kern-os:trap.c     1.4.3.4"		*/
/*								*/
/*	Copyright (c) 1987, 1988 Microsoft Corporation  	*/
/*		All Rights Reserved   				*/
/*								*/
/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */


#pragma ident	"@(#)trap.c	1.62	96/10/17 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/core.h>
#include <sys/syscall.h>
#include <sys/cpuvar.h>
#include <sys/vm.h>
#include <sys/msgbuf.h>
#include <sys/sysinfo.h>
#include <sys/fault.h>
#include <sys/stack.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/fp.h>
#include <sys/trap.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/prsystm.h>
#include <sys/mutex_impl.h>
#include <sys/machsystm.h>
#include <sys/archsystm.h>

#include <vm/hat.h>

#include <vm/seg_kmem.h>
#include <vm/as.h>
#include <vm/seg.h>

#include <sys/procfs.h>

#include <sys/reboot.h>
#include <sys/debug.h>
#include <sys/debugreg.h>
#include <sys/modctl.h>
#include <sys/aio_impl.h>
#include <sys/tnf.h>
#include <sys/tnf_probe.h>
#include <sys/cred.h>
#include <sys/x86_archext.h>

#ifdef _VPIX
#include <sys/v86.h>
#endif

#define	USER	0x10000		/* user-mode flag added to trap type */

char	*trap_type[] = {
	"Divide Error Exception",	/* trap id 0 	*/
	"Debug Trap",			/* trap id 1	*/
	"NMI Interrupt",		/* trap id 2	*/
	"BreakPoint",			/* trap id 3 	*/
	"Overflow Fault",		/* trap id 4 	*/
	"Bounds Fault",			/* trap id 5 	*/
	"Illegal Instruction",		/* trap id 6 	*/
	"Coprocessor Not Available",	/* trap id 7 	*/
	"Double Fault",			/* trap id 8 	*/
	"Extension overrun fault",	/* trap id 9 	*/
	"Invalid TSS",			/* trap id 10 	*/
	"Segment Not Present",		/* trap id 11 	*/
	"Stack Exception",		/* trap id 12 	*/
	"General Protection Fault",	/* trap id 13 	*/
	"Page Fault",			/* trap id 14 	*/
	"Intel Reserved Trap (#15)",	/* trap id 15 	*/
	"Coprocessor Error",		/* trap id 16 	*/
	"Intel Reserved Trap (#17)",	/* trap id 17 	*/
};

#define	TRAP_TYPES	(sizeof (trap_type) / sizeof (trap_type[0]))

static int tudebug = 0;
static int tudebugbpt = 0;
static int tudebugfpe = 0;
int panic_reg;

#if defined(TRAPDEBUG) || defined(lint)
static int tdebug = 0;
static int lodebug = 0;
static int faultdebug = 0;
#else
#define	tdebug	0
#define	lodebug	0
#define	faultdebug	0
#endif /* defined(TRAPDEBUG) || defined(lint) */

static void showregs(register unsigned int type, register struct regs *rp,
	caddr_t addr);
static void disp_tss(void);
static int kern_gpfault(register int *rp);

static int
die(type, rp, addr)
	unsigned type;
	struct regs *rp;
	caddr_t addr;
{
	char *trap_name;

	panic_reg = (int)rp;
	type &= ~USER;
	trap_name = type < TRAP_TYPES ? trap_type[type] : "trap";
	(void) setup_panic(trap_name, (va_list)NULL);

	printf("BAD TRAP\n");
	showregs(type, rp, addr);
	if (type == T_DBLFLT)
		(void) disp_tss();
	/* traceback((caddr_t)rp->r_sp); XXX - later? */
	cmn_err(CE_PANIC, trap_name);
	return (0);
}

/*
 * The bytes of an lcall instruction used for the syscall trap.
 * static u_char lcall[7] = { 0x9a, 0, 0, 0, 0, 0x7, 0 };
 * static u_char lcall[7] = { 0x9a, 0, 0, 0, 0, 0x27, 0 };
 */
#define	LCALLSIZE	7

/*
 * Test to see if the instruction at pc is an lcall instruction.
 */
static int
instr_is_lcall(proc_t *p, caddr_t pc)
{
	u_char instr[LCALLSIZE];
	int rval = 0;
	int mapped = 0;

	if (p->p_warea)
		mapped = pr_mappage(pc, LCALLSIZE, S_READ, 1);
	if (_copyin(pc, (caddr_t)instr, LCALLSIZE) == 0 &&
	    instr[0] == 0x9a &&
	    instr[1] == 0 &&
	    instr[2] == 0 &&
	    instr[3] == 0 &&
	    instr[4] == 0 &&
	    (instr[5] == 0x7 || instr[5] == 0x27) &&
	    instr[6] == 0)
		rval = 1;
	if (mapped)
		pr_unmappage(pc, LCALLSIZE, S_READ, 1);
	return (rval);
}

/* this is *very* temporary */
#define	instr_size(rp, addrp, rw)	(4)

/*
 * Called from the trap handler when a processor trap occurs.
 *
 * Note: All user-level traps that might call stop() must exit
 * trap() by 'goto out' or by falling through.
 */
void
trap(rp, addr)
	register struct regs *rp;
	caddr_t addr;
{
	register kthread_id_t cur_thread = curthread;
	enum seg_rw rw;
	unsigned type;
	extern int stop_on_fault(u_int, k_siginfo_t *);
	proc_t *p = ttoproc(cur_thread);
	klwp_id_t lwp = ttolwp(cur_thread);
	clock_t syst;
	u_int lofault;
	faultcode_t pagefault(), res, errcode;
	k_siginfo_t siginfo;
	u_int fault = 0;
	int driver_mutex = 0;	/* get unsafe_driver before returning */
	int mstate;
	u_int debugstatus, dreg7;
	int sicode = 0;
	int	user_genprotfault();
	int watchcode;
	int watchpage;
	caddr_t vaddr;
	int sz;
	int ta;

	type = rp->r_trapno;
	CPU_STAT_ADDQ(CPU, cpu_sysinfo.trap, 1);
	syst = p->p_stime;

	ASSERT(cur_thread->t_schedflag & TS_DONT_SWAP);


	if (type == T_PGFLT) {
		errcode = rp->r_err;
		if (errcode & PF_ERR_WRITE)
			rw = S_WRITE;
		else if ((caddr_t)rp->r_eip == addr)
			rw = S_EXEC;
		else
			rw = S_READ;
	}

	if (tdebug)
		showregs(type, rp, addr);

	if (USERMODE(rp->r_cs) || (rp->r_ps & PS_VM)) {
		/*
		 * Set up the current cred to use during this trap. u_cred
		 * no longer exists.  t_cred is used instead.
		 * The current process credential applies to the thread for
		 * the entire trap.  If trapping from the kernel, this
		 * should already be set up.
		 */
		if (cur_thread->t_cred != p->p_cred) {
			crfree(cur_thread->t_cred);
			cur_thread->t_cred = crgetcred();
		}
		ASSERT(lwp != NULL);
		type |= USER;
		ASSERT(lwptoregs(lwp) == rp);
		lwp->lwp_state = LWP_SYS;
#ifdef NPROBE
		if (cur_thread->t_proc_flag & TP_MSACCT)
#else
		if ((cur_thread->t_proc_flag & TP_MSACCT) || tnf_tracing_active)
#endif /* NPROBE */
		{
			switch (type) {
			case T_PGFLT + USER:
				if ((caddr_t)rp->r_eip == addr)
					mstate = LMS_TFAULT;
				else
					mstate = LMS_DFAULT;
				break;
			default:
				mstate = LMS_TRAP;
				break;
			}
			/* Kernel probe */
			TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
			    tnf_microstate, state, mstate);
			if (cur_thread->t_proc_flag & TP_MSACCT)
				mstate = new_mstate(cur_thread, mstate);
			else
				mstate = LMS_USER;
		} else {
			mstate = LMS_USER;
		}
		bzero((caddr_t)&siginfo, sizeof (siginfo));
#ifdef MERGE386
		if (cur_thread->t_v86data != NULL && type != T_PGFLT + USER) {
			vm86_trap(rp);
		}
#endif
	} else {
		/* not user mode or Virtual 8086 mode */
		if (MUTEX_OWNER_LOCK(&unsafe_driver) &&
		    UNSAFE_DRIVER_LOCK_HELD()) {
			/*
			 * till kadb is fixed to stop other CPU,
			 * cannot use this ?
			 */
			/*	ASSERT(!(type & USER));  */
			driver_mutex = 1;
			mutex_exit(&unsafe_driver);
		}
	}

	switch (type) {

	default:
		if (type & USER) {
			if (tudebug)
				showregs(type, rp, (caddr_t)0);
			printf("trap: Unknown trap type %d in user mode\n",
				(type&~USER));
			siginfo.si_signo = SIGILL;
			siginfo.si_code  = ILL_ILLTRP;
			siginfo.si_addr  = (caddr_t)rp->r_eip;
			siginfo.si_trapno = type &~ USER;
			fault = FLTILL;
			break;
		}
		else
			(void) die(type, rp, addr);
			/*NOTREACHED*/

	case T_PGFLT:		/* system page fault */
		/* may have been expected by C (e.g. bus probe) */
		if (cur_thread->t_nofault) {
			label_t *ftmp;

			ftmp = cur_thread->t_nofault;
			cur_thread->t_nofault = 0;
#ifndef LOCKNEST
			if (driver_mutex)
				mutex_enter(&unsafe_driver);
#endif /* LOCKNEST */
			longjmp(ftmp);
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
		lofault = cur_thread->t_lofault;
		cur_thread->t_lofault = 0;

		if (cur_thread->t_proc_flag & TP_MSACCT)
			mstate = new_mstate(cur_thread, LMS_KFAULT);
		else
			mstate = LMS_SYSTEM;

		if (addr < (caddr_t)KERNELBASE) {
			res = pagefault(addr,
			    (errcode & PF_ERR_PROT)? F_PROT: F_INVAL, rw, 0);
			if (res == FC_NOMAP &&
			    addr < (caddr_t)USRSTACK &&
			    grow((int *)addr))
				res = 0;
		} else {
			res = pagefault(addr,
			    (errcode & PF_ERR_PROT)? F_PROT: F_INVAL, rw, 1);
		}
		if (cur_thread->t_proc_flag & TP_MSACCT)
			(void) new_mstate(cur_thread, mstate);

		/*
		 * Restore lofault. If we resolved the fault, exit.
		 * If we didn't and lofault wasn't set, die.
		 */
		cur_thread->t_lofault = lofault;
		if (res == 0)
			goto cleanup;

		/*
		 * Check for mutex_exit hook.  This is to protect mutex_exit
		 * from deallocated locks.  It would be too expensive to use
		 * nofault there.
		 */
		{
			void	mutex_exit_nofault(void);
			void	mutex_exit_fault(kmutex_t *);

			if (rp->r_eip == (int)mutex_exit_nofault) {
				rp->r_eip = (int)mutex_exit_fault;
				goto cleanup;
			}
		}

		if (lofault == 0)
			(void) die(type, rp, addr);

		/*
		 * Cannot resolve fault.  Return to lofault.
		 */
		if (lodebug) {
			showregs(type, rp, addr);
			/* traceback((caddr_t)rp->r_sp); XXX - later */
		}
		if (FC_CODE(res) == FC_OBJERR)
			res = FC_ERRNO(res);
		else
			res = EFAULT;
		rp->r_r0 = res;
		rp->r_eip = cur_thread->t_lofault;
		goto cleanup;

	case T_PGFLT + USER:	/* user page fault */
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
				fault_str = "exec";
				break;
			default:
				fault_str = "";
				break;
			}
			printf("user %s fault:  addr=0x%x errcode=0x%x\n",
			    fault_str, (int)addr, errcode);
		}
#ifdef WEITEK_later	/* XXX - needs review */
		if (((unsigned long) faultadr & WEITEK_ADDRS)
			== WEITEK_VADDR) { /* Weitek Address? */
			if (weitek_kind & WEITEK_HW) {	/* chip present */

				if (weitek_pt < 0)
					init_weitek_pt();

				map_weitek_pt();
				flushtlb();
				u.u_weitek = WEITEK_HW;
				init_weitek();
				weitek_proc = u.u_procp;
				return (0);
			}
		} else {
#endif /* WEITEK_later */
		ASSERT(!(curthread->t_flag & T_WATCHPT));
		watchpage = (p->p_warea != NULL &&
			pr_is_watchpage(addr, rw));
		/*
		 * The lcall (system call) instruction fetches one word
		 * from the stack, at the stack pointer, because of the
		 * way the call gate is constructed.  This is a bogus
		 * read and should not be counted as a read watchpoint.
		 * We work around the problem here by testing to see if
		 * this situation applies and, if so, simply jumping to
		 * the code in locore.s that fields the system call trap.
		 * The registers on the stack are already set up properly
		 * due to the match between the call gate sequence and the
		 * trap gate sequence.  We just have to adjust the pc.
		 */
		if (watchpage && addr == (caddr_t)rp->r_usp &&
		    rw == S_READ && instr_is_lcall(p, (caddr_t)rp->r_eip)) {
			extern void watch_syscall(void);

			rp->r_eip += LCALLSIZE;
			watch_syscall();	/* never returns */
		}
		vaddr = addr;
		if (watchpage &&
		    (sz = instr_size(rp, &vaddr, rw)) > 0 &&
		    (watchcode = pr_is_watchpoint(&vaddr, &ta,
		    sz, NULL, rw)) != 0) {
			if (ta) {
				do_watch_step(vaddr, sz, rw,
					watchcode, rp->r_eip);
				res = pagefault(addr, F_INVAL, rw, 0);
			} else {
				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_signo = SIGTRAP;
				siginfo.si_code = watchcode;
				siginfo.si_addr = vaddr;
				siginfo.si_trapafter = 0;
				siginfo.si_pc = (caddr_t)rp->r_eip;
				fault = FLTWATCH;
				break;
			}
		} else if (watchpage && rw == S_EXEC) {
			do_watch_step(vaddr, 32, rw, 0, 0);
			res = pagefault(addr, F_INVAL, rw, 0);
		} else if (watchpage) {
			/* XXX never succeeds (for now) */
			if (pr_watch_emul(rp, vaddr, rw))
				goto out;
			do_watch_step(vaddr, 8, rw, 0, 0);
			res = pagefault(addr, F_INVAL, rw, 0);
			break;
		} else {
			res = pagefault(addr,
			    (errcode & PF_ERR_PROT)? F_PROT: F_INVAL, rw, 0);
		}
#ifdef WEITEK_later
		}
#endif /* WEITEK_later */
		/*
		 * If pagefault() succeeded, ok.
		 * Otherwise attempt to grow the stack.
		 */
		if (res == 0 ||
		    (res == FC_NOMAP &&
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
			showregs(type, rp, addr);
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
			    (res == FC_NOMAP)? SEGV_MAPERR : SEGV_ACCERR;
			fault = FLTBOUNDS;
			break;
		}
		break;

	case T_ILLINST + USER:	/* invalid opcode fault */
		if (tudebug)
			showregs(type, rp, (caddr_t)0);
#ifdef _VPIX
		/*
		 * Virtual interrupt for dual mode process thread.
		 */
		if (cur_thread->t_v86data != NULL &&
			v86setpseudo(cur_thread, V86VI_INVOP)) {
			break;
		}
#endif
		siginfo.si_signo = SIGILL;
		siginfo.si_code  = ILL_ILLOPC;
		siginfo.si_addr  = (caddr_t)rp->r_eip;
		fault = FLTILL;
		break;

	case T_ZERODIV + USER:		/* integer divide by zero */
		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0);
#ifdef _VPIX
		/*
		 * Virtual interrupt for dual mode process thread.
		 */
		if (cur_thread->t_v86data != NULL &&
			v86setpseudo(cur_thread, V86VI_DIV0)) {
			break;
		}
#endif
		siginfo.si_signo = SIGFPE;
		siginfo.si_code  = FPE_INTDIV;
		siginfo.si_addr  = (caddr_t)rp->r_eip;
		fault = FLTIZDIV;
		break;

	case T_OVFLW + USER:	/* integer overflow */
		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0);
#ifdef _VPIX
		/*
		 * Virtual interrupt for dual mode process thread.
		 */
		if (cur_thread->t_v86data != NULL &&
			v86setpseudo(cur_thread, V86VI_OVERFLOW)) {
			break;
		}
#endif
		siginfo.si_signo = SIGFPE;
		siginfo.si_code  = FPE_INTOVF;
		siginfo.si_addr  = (caddr_t)rp->r_eip;
		fault = FLTIOVF;
		break;

	case T_NOEXTFLT + USER:	/* no extension fault */
		if (tudebug && tudebugfpe)
			showregs(type, rp, addr);
		if (fpnoextflt(rp)) {
			siginfo.si_signo = SIGFPE;
			siginfo.si_code  = ILL_ILLOPC;
			siginfo.si_addr  = (caddr_t)rp->r_eip;
			fault = FLTFPE;
		}
		break;

	case T_EXTOVRFLT + USER:	/* extension overrun fault */
		if (tudebug && tudebugfpe)
			showregs(type, rp, addr);
		if (fpextovrflt(rp)) {
			siginfo.si_signo = SIGSEGV;
			siginfo.si_code  = SEGV_MAPERR;
			siginfo.si_addr  = (caddr_t)rp->r_eip;
			fault = FLTBOUNDS;
		}
		break;

	case T_EXTERRFLT + USER:	/* extension error fault */
		if (tudebug && tudebugfpe)
			showregs(type, rp, addr);
		if (sicode = fpexterrflt(rp)) {
			siginfo.si_signo = SIGFPE;
			siginfo.si_code  = sicode;
			siginfo.si_addr  = (caddr_t)rp->r_eip;
			fault = FLTFPE;
		}
		break;

	/*
	 * XXX Note:	Currently KADB captures all breakpoint traps and it
	 *		will return to this trap handler for usermode
	 *		breakpoint traps. So, we should not see the following
	 *		two cases for kernel breakpoint traps unless the KADB
	 *		interface changes.
	 */
	case T_BPTFLT:	/* breakpoint trap */
		if (tudebug && tudebugbpt)
			showregs(type, rp, (caddr_t)0);

		/* XXX - needs review on debugger interface? */
		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		else
			(void) die(type, rp, addr);
		break;

	case T_SGLSTP: /* single step/hw breakpoint exception */
		if (tudebug && tudebugbpt)
			showregs(type, rp, (caddr_t)0);

		if (lwp != NULL && (lwp->lwp_pcb.pcb_flags & DEBUG_ON)) {
			debugstatus =
			    lwp->lwp_pcb.pcb_dregs.debugreg[DR_STATUS];
			dreg7 = lwp->lwp_pcb.pcb_dregs.debugreg[DR_CONTROL];
		} else {
			/* can't happen? */
			debugstatus = dr6();
			dreg7 = dr7();
			setdr6(0);
			cmn_err(CE_WARN,
		"Unexpected INT 1 in system mode, DEBUG_ON not set, dr6=%x",
				debugstatus);
		}

		/* Mask out disabled breakpoints */
		if (!(dreg7 & DR_ENABLE0))
			debugstatus &= ~DR_TRAP0;
		if (!(dreg7 & DR_ENABLE1))
			debugstatus &= ~DR_TRAP1;
		if (!(dreg7 & DR_ENABLE2))
			debugstatus &= ~DR_TRAP2;
		if (!(dreg7 & DR_ENABLE3))
			debugstatus &= ~DR_TRAP3;
		if (lwp != NULL && (lwp->lwp_pcb.pcb_flags & DEBUG_ON)) {
			lwp->lwp_pcb.pcb_dregs.debugreg[DR_STATUS] =
			    debugstatus;
			lwp->lwp_pcb.pcb_flags &= ~DEBUG_ON;
		}

		/* Now evaluate how we got here */
		if (lwp != NULL && (debugstatus & DR_SINGLESTEP)) {
			extern int sys_call();
			extern int sig_clean();

			/*
			 * The i86 single-steps even through lcalls which
			 * change the privilege level. So we take a trap at
			 * the first instruction in privileged mode.
			 *
			 * Set a flag to indicate that upon completion of
			 * the system call, deal with the single-step trap.
			 */
			if (rp->r_eip == (int)sys_call ||
			    rp->r_eip == (int)sig_clean) {

				rp->r_ps &= ~PS_T; /* turn off trace */
				lwp->lwp_pcb.pcb_flags |= DEBUG_PENDING;
				cur_thread->t_post_sys = 1;
				goto cleanup;
			}
		}

		/* XXX - needs review on debugger interface? */
		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		else
			(void) die(type, rp, addr);
		break;

	case T_NMIFLT:	/* NMI interrupt */
		printf("Unexpected NMI in system mode\n");
		goto cleanup;

	case T_NMIFLT + USER:	/* NMI interrupt */
		printf("Unexpected NMI in user mode\n");
		break;

	case T_GPFLT:	/* general protection violation */
		if (tudebug)
			showregs(type, rp, (caddr_t)0);
		if (kern_gpfault((int *)rp))
			(void) die(type, rp, addr);
		goto cleanup;

	case T_BOUNDFLT + USER:	/* bound fault */
#ifdef _VPIX
		/*
		 * Virtual interrupt for dual mode process thread.
		 */
		if ((cur_thread->t_v86data != NULL) &&
			v86setpseudo(cur_thread, V86VI_BOUND)) {
			break;
		}
#endif
	case T_GPFLT + USER:	/* general protection violation */
		if (user_genprotfault(rp) == 0)
			break;
		/*FALLTHROUGH*/
	case T_STKFLT + USER:	/* stack fault */
	case T_TSSFLT + USER:	/* invalid TSS fault */
	case T_SEGFLT + USER:	/* segment not present fault */
		if (tudebug)
			showregs(type, rp, (caddr_t)0);
		siginfo.si_signo = SIGSEGV;
		siginfo.si_code  = SEGV_MAPERR;
		siginfo.si_addr  = (caddr_t)rp->r_eip;
		fault = FLTBOUNDS;
		break;

	case T_ALIGNMENT + USER:	/* user alignment error (486) */
		if (tudebug)
			showregs(type, rp, (caddr_t)0);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGBUS;
		siginfo.si_code = BUS_ADRALN;
		siginfo.si_addr = (caddr_t)rp->r_eip;
		fault = FLTACCESS;
		break;

	case T_SGLSTP + USER: /* single step/hw breakpoint exception */
		if (tudebug && tudebugbpt)
			showregs(type, rp, (caddr_t)0);

		/* XXX needs review on save/restore of debug regs in locore */
		if (lwp->lwp_pcb.pcb_flags & DEBUG_ON) {
			debugstatus =
			    lwp->lwp_pcb.pcb_dregs.debugreg[DR_STATUS];
			dreg7 = lwp->lwp_pcb.pcb_dregs.debugreg[DR_CONTROL];
		} else {
			/* can't happen? */
			debugstatus = dr6();
			dreg7 = dr7();
			setdr6(0);
			cmn_err(CE_WARN,
		"Unexpected INT 1 in user mode, DEBUG_ON not set, dr6=%x",
				debugstatus);
		}

		/* Mask out disabled breakpoints */
		if (!(dreg7 & DR_ENABLE0))
			debugstatus &= ~DR_TRAP0;
		if (!(dreg7 & DR_ENABLE1))
			debugstatus &= ~DR_TRAP1;
		if (!(dreg7 & DR_ENABLE2))
			debugstatus &= ~DR_TRAP2;
		if (!(dreg7 & DR_ENABLE3))
			debugstatus &= ~DR_TRAP3;
		if (lwp->lwp_pcb.pcb_flags & DEBUG_ON) {
			lwp->lwp_pcb.pcb_dregs.debugreg[DR_STATUS] =
			    debugstatus;
			lwp->lwp_pcb.pcb_flags &= ~DEBUG_ON;
		}

		/* Was it single-stepping or from a debug register setting ? */
		if ((debugstatus & DR_SINGLESTEP) || (debugstatus & DR_TRAPS)) {
			/* turn off trace */
#ifdef _VPIX
			/*
			 * Virtual interrupt for dual mode process thread.
			 */
			if (cur_thread->t_v86data != NULL &&
				v86setpseudo(cur_thread, V86VI_SGLSTP)) {
				break;
			}
#endif
			if (debugstatus & DR_SINGLESTEP) {
				pcb_t *pcb = &lwp->lwp_pcb;

				rp->r_ps &= ~PS_T;
				if (pcb->pcb_flags & NORMAL_STEP) {
					siginfo.si_signo = SIGTRAP;
					siginfo.si_code = TRAP_TRACE;
					siginfo.si_addr = (caddr_t)rp->r_eip;
					fault = FLTTRACE;
				}
				if (pcb->pcb_flags & WATCH_STEP)
					fault = undo_watch_step(&siginfo);
				pcb->pcb_flags &= ~(NORMAL_STEP|WATCH_STEP);
			} else {
				siginfo.si_signo = SIGTRAP;
				siginfo.si_code = TRAP_BRKPT;
				siginfo.si_addr = (caddr_t)rp->r_eip;
				fault = FLTBPT;
			}
		} else {
			cmn_err(CE_WARN,
				"Unexpected INT 1 in user mode, dr6=%x",
				debugstatus);
		}
		break;

	case T_BPTFLT + USER:	/* breakpoint trap */
		if (tudebug && tudebugbpt)
			showregs(type, rp, (caddr_t)0);
#ifdef _VPIX
		/*
		 * Virtual interrupt for dual mode process thread.
		 */
		if (cur_thread->t_v86data != NULL &&
			v86setpseudo(cur_thread, V86VI_BRKPT)) {
			break;
		}
#endif
		/*
		 * int 3 (the breakpoint instruction) leaves the pc referring
		 * to the address one byte after the breakpointed address.
		 * If the SBPTADJ flag has been set via /proc, We adjust
		 * it back so it refers to the breakpointed address.
		 */
		if (p->p_flag & SBPTADJ)
			rp->r_eip--;
		siginfo.si_signo = SIGTRAP;
		siginfo.si_code  = TRAP_BRKPT;
		siginfo.si_addr  = (caddr_t)rp->r_eip;
		fault = FLTBPT;
		break;

	case T_AST + USER:		/* profiling or resched psuedo trap */
		if (lwp->lwp_oweupc && lwp->lwp_prof.pr_scale) {
			mutex_enter(&p->p_pflock);
			addupc((void(*)())rp->r_eip, &lwp->lwp_prof, 1);
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
			addupc((void(*)())rp->r_eip, &lwp->lwp_prof, ticks);
			mutex_exit(&p->p_pflock);
		}
	}

	if (cur_thread->t_astflag | cur_thread->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(cur_thread);
		cur_thread->t_sig_check = 0;
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
		if (ISSIG_PENDING(cur_thread, lwp, p)) {
			if (issig(FORREAL))
				psig();
			cur_thread->t_sig_check = 1;
		}

		if (cur_thread->t_rprof != NULL) {
			realsigprof(0, 0);
			cur_thread->t_sig_check = 1;
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

	if (cur_thread->t_trapret) {
		cur_thread->t_trapret = 0;
		thread_lock(cur_thread);
		CL_TRAPRET(cur_thread);
		thread_unlock(cur_thread);
	}
	if (CPU->cpu_runrun)
		preempt();
	if (cur_thread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(cur_thread, mstate);

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
int IGNORE_KERNEL_PREEMPTION = 0;	/* XXX - delete this someday */

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
	register kthread_id_t cur_thread = curthread;

	if (IGNORE_KERNEL_PREEMPTION) {
		aston(CPU->cpu_dispthread);
		return;
	}
	/*
	 * Check that conditions are right for kernel preemption
	 */
	do {
		if (cur_thread->t_preempt) {
			extern kthread_id_t	clock_thread;
			/*
			 * either a privileged thread (idle, panic, interrupt)
			 *	or will check when t_preempt is lowered
			 */
			if (cur_thread->t_pri < 0)
				kpreempt_cnts.kpc_idle++;
			else if (cur_thread->t_flag & T_INTR_THREAD) {
				kpreempt_cnts.kpc_intr++;
				if (cur_thread == clock_thread)
					kpreempt_cnts.kpc_clock++;
			} else
				kpreempt_cnts.kpc_blocked++;
			aston(CPU->cpu_dispthread);
			return;
		}
		if (cur_thread->t_state != TS_ONPROC ||
		    cur_thread->t_disp_queue != &CPU->cpu_disp) {
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

		cur_thread->t_preempt++;
		preempt();
		cur_thread->t_preempt--;
	} while (CPU->cpu_kprunrun);
}

/*
 * Print out a traceback for kernel traps
 * XXX FIX IT.
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
showregs(register unsigned int type, register struct regs *rp, caddr_t addr)
{
	int *r, s;

	s = spl7();
	type &= ~USER;
	if (u.u_comm[0])
		printf("%s: ", u.u_comm);
	if (type < TRAP_TYPES)
		printf("%s\n", trap_type[type]);
	else switch (type) { /* XXX - review the cases */
	case T_SYSCALL:
		printf("Syscall Trap:\n");
		break;
	case T_AST:
		printf("AST\n");
		break;
	default:
		printf("Bad Trap = %d\n", type);
		break;
	}
	if (type == T_PGFLT) {
		/* LINTED constant in conditional context */
		if (good_addr(addr)) {
			struct pte pte;

			mmu_getpte(addr, &pte);
			printf("%s fault at addr=0x%x, pte=0x%x\n",
			    (((rp->r_ps & PS_VM) || USERMODE(rp->r_cs))?
					"User": "Kernel"),
			    (int)addr, *(int *)&pte);
		} else {
			printf("Bad %s fault at addr=0x%x\n",
			    (((rp->r_ps & PS_VM) || USERMODE(rp->r_cs))?
			    "user": "kernel"), (int)addr);
		}
	} else if (addr) {
		printf("addr=0x%x\n", (int)addr);
	}
	printf("pid=%d, pc=0x%x, sp=0x%x, eflags=0x%x\n",
	    (ttoproc(curthread) && ttoproc(curthread)->p_pidp) ?
	    (int)(ttoproc(curthread)->p_pid) : 0, rp->r_eip, rp->r_usp,
	    rp->r_ps);

	r = (int *)rp;
	printf("\neip(%x), eflags(%x), ebp(%x), uesp(%x), esp(%x)\n",
		r[EIP], r[EFL], r[EBP], r[UESP], r[ESP]);
	printf("eax(%x), ebx(%x), ecx(%x), edx(%x), esi(%x), edi(%x)\n",
		r[EAX], r[EBX], r[ECX], r[EDX], r[ESI], r[EDI]);
	printf("cr0(%x), cr2(%x), cr3(%x)\n", cr0(), cr2(), cr3());
	printf("cs(%x) ds(%x) ss(%x) es(%x) fs(%x) gs(%x)\n",
		0xFFFF&r[CS], 0xFFFF&r[DS], 0xFFFF&r[SS],
		0xFFFF&r[ES], 0xFFFF&r[FS], 0xFFFF&r[GS]);
	(void) splx(s);
}

/*
 * Handle General Protection faults caused by user modifying/corrupting
 * his stack where the context was saved during his own signal processing
 * or thru debugger (/proc filesystem access).
 * Returns true if kernel should panic.
 */

static int
kern_gpfault(register int *rp)
{
	klwp_id_t lwp = ttolwp(curthread);
	register int *prevrp;

	/*
	 * The following comments are taken from USL/SVR4:trap.c. The code
	 * that is supposed to fix the problems mentioned below doesn't
	 * really do much and the only case it really fixes is when the
	 * u_sigufault (see SVR4:user.h) is set. Please review the
	 * USL/SVR4 code if you are trying to fix anything more here.
	 * XXX - CHECK.
	 ***
	 *** Returns true if the kernel should panic.
	 ***
	 *** ptrace(), the user who trashes his stack during signal handling,
	 *** the [23]87 emulators, and i286 emulator,
	 *** and perhaps other circumstances will occasionally set various user
	 *** registers or selectors to real bad values. Unfortunately, the
	 *** values of the user registers seem to cause a k_trap() instead of
	 *** a u_trap() in the iret instruction.
	 *** PROBLEM: Suppose user signal handling code trashes its stack and
	 *** sets itself as a signal-handler. Then the program (properly) grows
	 *** a user stack until it can no longer be grown, fails sendsig(), and
	 *** dumps a core().
	 *** PROBLEM: The kernel accidentally trashes its own stack, but doesn't
	 *** discover this until the common_iret. Since the value of EIP can
	 *** be anything at all (the user could have set it to, for example,
	 *** a kernel address), we have no choice but treat this as a user-trap
	 *** and deliver a SIGSEGV to the user process. But to protect
	 *** ourselves, we must check that we are not delivering a signal to
	 *** a system process.
	 *** PROBLEM: The stack is where we were about to return to user-mode,
	 *** almost entirely popped. The user's accumulators and segments were
	 *** restored before the fault and the old flag and error code have been
	 *** destroyed. There is no privilege level transition when the
	 *** processor faults on the iret. As a consequence, there is no ESP
	 *** on the stack.  Don't we want to record the correct context for
	 *** sdb and the user's process? Yes. Before we deliver the signal,
	 *** copy the bogus iret registers from the area beneath the current
	 *** frame into the current frame. psig() will either deliver the
	 *** signal (and push these registers onto the user stack) or output
	 *** the contents of the kernel stack to a core file. But afterwards,
	 *** completely pop the kernel stack.
	 */

	if ((lwp != NULL) && lwp->lwp_gpfault) {
		/*
		** It had just popped off the ERR code when it got
		** the fault in the iret, and the new fault
		** pushed new EFL flags on. Copy this stuff into
		** the current frame for use by ptrace() and core().
		*/
		prevrp = &rp[EFL] - ERR;
		rp[EIP] = prevrp[EIP];
		rp[CS] = prevrp[CS];
		rp[EFL] = prevrp[EFL];
		rp[UESP] = prevrp[UESP];
		rp[SS] = prevrp[SS];
		lwp->lwp_cursig = SIGSEGV;
		exit((core("core", ttoproc(curthread), CRED(),
			(rlim_t)U_CURLIMIT(&u, RLIMIT_CORE), SIGSEGV) ?
				CLD_DUMPED|CLD_KILLED : CLD_KILLED), SIGSEGV);
		return (0);
	}

	return (1);	/* XXX let the kernel panic :-( */
}

/*
 * disp_tss() - Display the TSS structure
 */

static void
disp_tss(void)
{
	char *tss_str = "tss386.%s:\t0x%x\n";  /* Format string */
	struct tss386 *tss;

	tss = (struct tss386 *)CPU->cpu_tss; /* Ptr to TSS struct */

	printf(tss_str, "t_link", tss->t_link);
	printf(tss_str, "t_esp0", tss->t_esp0);
	printf(tss_str, "t_ss0", tss->t_ss0);
	printf(tss_str, "t_esp1", tss->t_esp1);
	printf(tss_str, "t_ss1", tss->t_ss1);
	printf(tss_str, "t_esp2", tss->t_esp2);
	printf(tss_str, "t_ss2", tss->t_ss2);
	printf(tss_str, "t_cr3", tss->t_cr3);
	printf(tss_str, "t_eip", tss->t_eip);
	printf(tss_str, "t_eflags", tss->t_eflags);
	printf(tss_str, "t_eax", tss->t_eax);
	printf(tss_str, "t_ecx", tss->t_ecx);
	printf(tss_str, "t_edx", tss->t_edx);
	printf(tss_str, "t_ebx", tss->t_ebx);
}


int	enable_rdwrmsr;
u_int	p6_msr_addr[] = {0x0, 0x01, 0x10, 0x1b, 0xc1, 0xc2, 0xfe, 0x179,
			0x17a, 0x186, 0x187, 0x1d9, 0x1db, 0x1dc, 0x1dd,
			0x1de, 0x250, 0x258, 0x259, 0x2ff};
int
user_genprotfault(struct regs *rp)
{
	u_int	eaxedx[2], ecx, newip;
	u_short	*sinst_ptr;
	int	i, bad_value;

	if (!suser(CRED()) || !enable_rdwrmsr || !(x86_feature & X86_MSR))
		return (1);
	sinst_ptr = (u_short *)rp->r_eip;
	ecx = rp->r_ecx;
	bad_value = 1;
	switch	(x86_feature & X86_CPU_TYPE) {
	case X86_P6:
		for (i = 0; i < sizeof (p6_msr_addr); i++) {
			if (p6_msr_addr[i] == ecx) {
				bad_value = 0;
				break;
			}
		}
		if (!bad_value)
			break;
		else if (((ecx >= REG_MTRRPHYSBASE0 &&
			ecx <= REG_MTRRPHYSMASK7)) ||
			((ecx >= REG_MTRR4K1) && (ecx <= REG_MTRR4K8)) ||
			((ecx >= REG_MC0_CTL) && (ecx <= REG_MC5_MISC)))
			bad_value = 0;
		break;
	case X86_P5:
		if ((int)ecx >= P5_MCHADDR && ecx <= P5_CTR1)
			bad_value = 0;
		break;
	case X86_K5:
		if ((ecx == K5_MCHADDR) || (ecx == K5_MCHTYPE) ||
			(ecx == K5_TSC) || (ecx == K5_TR12))
			bad_value = 0;
		break;
	}
	if (bad_value)
		return (1);

	if (*sinst_ptr == INST_RDMSR) {
		rdmsr(rp->r_ecx, eaxedx);
		rp->r_eax = eaxedx[0];
		rp->r_edx = eaxedx[1];
	} else if (*sinst_ptr == INST_WRMSR) {
		eaxedx[1] = rp->r_edx;
		eaxedx[0] = rp->r_eax;
		wrmsr(ecx, eaxedx);
	} else
		return (1);
	newip = (u_int)sinst_ptr;
	newip += RDWRMSR_INST_LEN;
	rp->r_eip = newip;
	return (0);
}
