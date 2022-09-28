/*
 * Copyright (c) 1992-1993, by Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*		All Rights Reserved				*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation		*/
/*		All Rights Reserved				*/

/*	This Module contains Proprietary Information of Microsoft	*/
/*	Corporation and should be treated as Confidential.		*/

#ident "@(#)fpu.c	1.29	96/10/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/reg.h>
#include <sys/psw.h>
#include <sys/trap.h>
#include <sys/fault.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/map.h>
#include <sys/pcb.h>
#include <sys/lwp.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/fp.h>
#include <sys/siginfo.h>
#include <sys/archsystm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#ifdef _VPIX
#include <sys/v86.h>
#endif

extern void fpdisable();
extern void fpsave();
extern void fpksave();
extern void fpinit();
extern void fprestore();

extern void installctx(kthread_id_t, int, void (*)(),
	void (*)(), void (*)(), void (*)());

void fp_fork();
void fp_free();
void fp_save();
void fp_restore();
extern void fp_null();

static int fpe_sicode(int sw);

extern int fpu_exists;
extern int fp_kind;

/* Note: Following definitions are copied from sunddi.h */
#define	DDI_INTR_CLAIMED	1	/* interrupt is claimed */
#define	DDI_INTR_UNCLAIMED	0	/* interrupt is not claimed */

/*
 * Copy the floating point context of the forked thread.
 *
 * VPIX Note: For VPIX process we don't need to duplicate the FPU context of
 *	 v86 mode task because the child becomes a regular task
 *	 (see vc_forkret()in vc.c).
 *
 * XXX - WEITEK support needed.
 */
void
fp_fork(t, ct)
	kthread_id_t t, ct;
{
	klwp_id_t clwp = ct->t_lwp;
	register struct pcb *cpcb, *pcb;
	register struct fpu_ctx *cfp;		/* child's fpu context */
	register struct fpu_ctx *fp;		/* parent's fpu context */

	ASSERT(fp_kind != FP_NO && fp_kind != FP_SW);

	pcb = &t->t_lwp->lwp_pcb;
	fp = &pcb->pcb_fpu;
	cpcb = &clwp->lwp_pcb;
	cfp = &cpcb->pcb_fpu;

	/*
	 * If the parent FPU state is still associated with FPU hw then
	 * save it.
	 */
	fp_save(fp);	/* let fp_save() do the work */
	*cfp = *fp; /* copy it to the child */
	installctx(ct, (int)cfp, fp_null, fp_null, fp_fork, fp_free);
}

/*
 * Free any state associated with floating point context.
 * Fp_free can be called in three cases:
 * 1) from reaper -> thread_free -> ctxfree -> fp_free
 *	fp context belongs to a thread on deathrow
 *	nothing to do,  thread will never be resumed
 *	thread calling ctxfree is reaper
 *
 * 2) from exec -> ctxfree -> fp_free
 *	fp context belongs to the current thread
 *	must disable fpu, thread calling ctxfree is curthread
 *
 * 3) from restorecontext -> setfpregs -> fp_free
 *	we have a modified context in the memory (lwp->pcb_fpu)
 *	disable fpu and release the fp context for the CPU
 *
 */
void
fp_free(fp)
	register struct fpu_ctx *fp;
{
	ASSERT(fp_kind != FP_NO && fp_kind != FP_SW);

	if (fp->fpu_flags & FPU_VALID)
		return;

	kpreempt_disable();
/*
 * We want to do fpsave rather than fpdisable ? so that we can
 * keep the fpu_flags as FPU_VALID tracking the CR0_TS bit
 */
	fp->fpu_flags |= FPU_VALID;
	(void) fpdisable();
	kpreempt_enable();
}

/*
 * fp_save(fp)
 *      struct fpu_ctx *fp;
 * Store the floating point state and disable the floating point unit.
 *
 * XXX - WEITEK support needed.
 */

/* ARGSUSED */
void
fp_save(fp)
	register struct fpu_ctx *fp;
{
	ASSERT(fp_kind != FP_NO && fp_kind != FP_SW);

	kpreempt_disable();
	if (!fp || fp->fpu_flags & FPU_VALID) {
		kpreempt_enable();
		return;
	}

	fpsave(&fp->fpu_regs); 	/* save FPU state */
	fp->fpu_flags |= FPU_VALID;
	kpreempt_enable();
}

/*
 * fp_restore(fp)
 *      struct fpu_ctx *fp;
 *
 * Restore the FPU context for the thread:
 * The possibilities are:
 *	1. No active FPU context: Load the new context into the FPU hw
 *	   and enable the FPU.
 *
 * XXX - WEITEK support needed.
 */

/* ARGSUSED */
void
fp_restore(fp)
	register struct fpu_ctx *fp;
{
	ASSERT(fp_kind != FP_NO && fp_kind != FP_SW);

	fprestore(&fp->fpu_regs); 	/* load the new FPU context */
	fp->fpu_flags &= ~FPU_VALID; /* to indicate HW has the state */
}

/*
 * fpnoextflt(rp)
 *	struct regs *rp;
 *
 * This routine is called from trap() when User thread takes No Extension
 * Fault. The possiblities are:
 *	1. User thread has executed a FP instruction for the first time.
 *	   Save current FPU context if any. Initialize FPU, setup FPU
 *	   context for the thread and enable FP hw.
 *	2. Thread's pcb has a valid FPU state: Restore the FPU state and
 *	   enable FP hw.
 *
 * XXX - WEITEK support needed.
 */
int
fpnoextflt(struct regs *rp)
{
	register klwp_id_t lwp = ttolwp(curthread);
	register struct fpu_ctx *fp = &lwp->lwp_pcb.pcb_fpu;
#ifdef _VPIX
	register struct fpu_ctx *vfp;
	v86_t *v86p;
	int vmflag = rp->r_ps & PS_VM;

	v86p = (v86_t *)curthread->t_v86data;
	if (v86p && vmflag)
		fp = &v86p->vp_fpu;
#endif

	ASSERT(fp_kind != FP_SW);

	kpreempt_disable();
	/*
	 * Now we can enable the interrupts.
	 * (NOTE: fp-no-coprocessor come thru interrupt gate)
	 */
	sti();

	if (!fpu_exists) { /* check for FPU hw exists */
#ifdef _VPIX
		if (rp->r_ps & PS_VM) {
			v86setpseudo(curthread, V86VI_COPROC);
			kpreempt_enable();
			return (0);
		}
#endif
		if (fp_kind == FP_NO) {
			long inst;

			/*
			 * When the system has no floating point support,
			 * i.e. no FP hardware and no emulator, skip the
			 * two kinds of FP instruction that occur in
			 * fpstart.  Allows processes that do no real FP
			 * to run normally.
			 */
			inst = fuword((int *)rp->r_pc);
			if ((inst & 0xFFFF) == 0x7dd9 ||
			    (inst & 0xFFFF) == 0x6dd9) {
				rp->r_pc += 3;
				kpreempt_enable();
				return (0);
			}
		}

		/*
		 * If we have neither a processor extension nor
		 * an emulator, kill the process OR panic the kernel.
		 */
		kpreempt_enable();
		return (1); /* error */
	}

	if (fp->fpu_flags & FPU_EN) { /* case 2 */
		fprestore(&fp->fpu_regs); 	/* load the new FPU context  */
		fp->fpu_flags &= ~FPU_VALID; 	/* indicate HW has the state */
	} else { /* case 1 */
		installctx(curthread, (int)fp, fp_null, fp_null,
				fp_fork, fp_free);
		fpinit(); 	/* initialize the hardware */
		fp->fpu_flags = FPU_EN;
	}
	kpreempt_enable();
	return (0);
}


/*
 * fpextovrflt()
 *      handle a processor extension overrun fault
 * Returns non zero for error.
 * XXX - WEITEK support needed.
 */

/* ARGSUSED */
int
fpextovrflt(struct regs *rp)
{
	u_int cur_cr0;

	ASSERT(fp_kind != FP_SW && fp_kind != FP_NO);

	/* re-initialize the extension */
	cur_cr0 = cr0();
	fpinit();		/* initialize the FPU hardware */
	setcr0(cur_cr0);
	sti();

	return (1); 		/* error, send SIGSEGV signal to the thread */
}

/*
 * fpexterrflt()
 *	handle a processor extension error fault
 * Returns non zero for error.
 */

/* ARGSUSED */
int
fpexterrflt(struct regs *rp)
{
	u_short fpsw;
	fpu_ctx_t *fp;

	ASSERT(fp_kind != FP_NO);

	/*
	 * If floating point is being emulated, turn interrupts on before
	 * calling fperr_reset because emulating the FP instructions in
	 * fperr_reset is slow and no protection from interrupts is
	 * necessary.
	 */
	if (fp_kind == FP_SW)
		sti();

	fpsw = fperr_reset(); /* reset FPU exceptions */
	fp = &ttolwp(curthread)->lwp_pcb.pcb_fpu;
	ASSERT(fp_kind != FP_NO);
	/*
	 * Now we can enable the interrupts.
	 * (NOTE: fp exceptions come thru interrupt gate)
	 */
	sti();

	if (!(fpsw & FPS_ES)) { /* No exception */
		return (0);
	}

#ifdef _VPIX
	if (rp->r_ps & PS_VM) {
		/*
		 * Virtual 86 task is using the FPU, send a pseudorupt to it.
		 */
		v86setpseudo(curthread, V86VI_COPROC);
		return (0);
	}
#endif
	if (fpu_exists)
		fp_save(fp); 		/* save the FPU state */
	fp->fpu_regs.fp_reg_set.fpchip_state.status = fpsw;
	return (fpe_sicode(fpsw)); 	/* error, send SIGFPE signal to the */
					/* thread */
}

/*
 * fpsoftintr()
 *	Send the SIGFPE signal to the thread.
 */
void
fpsoftintr(caddr_t arg)
{
	k_siginfo_t siginfo;
	kthread_id_t t;
	proc_t *p;
	u_short fpsw;

	t = (kthread_id_t) arg;
	p = ttoproc(t);

	fpsw =
	    ttolwp(t)->lwp_pcb.pcb_fpu.fpu_regs.fp_reg_set.fpchip_state.status;
	/* send the SIGFPE signal to the thread */
	bzero((caddr_t)&siginfo, sizeof (siginfo));
	siginfo.si_signo = SIGFPE;
	siginfo.si_code = fpe_sicode(fpsw);
	siginfo.si_addr = (caddr_t) (lwptoregs(ttolwp(t))->r_eip);
	mutex_enter(&p->p_lock);
	sigaddq(p, t, &siginfo, KM_NOSLEEP);
	mutex_exit(&p->p_lock);
}

/*
 * fpintr()
 *	Handle a processor extension error interrupt on the AT386 machines.
 *	Interrupt comes on IRQ 13 (same as dma interrupt on EISA).
 * We don't need to disable kernel preemption since this is either an intpt
 * thread or a > LOCK_LEVEL intpt
 */

/*ARGSUSED*/
u_int
fpintr(caddr_t arg)
{
	kthread_id_t t;
	fpu_ctx_t *fp;
	u_short fpsw;
	u_int cur_cr0;

	ASSERT(fp_kind != FP_NO && fp_kind != FP_SW);

	fpintr_reset();	/* clear the NDP busy state */

	cur_cr0 = cr0();
	fpsw = fperr_reset(); /* reset FPU exceptions */
	if (!(fpsw & FPS_ES)) { /* No exception */
		setcr0(cur_cr0);
		return (DDI_INTR_UNCLAIMED);
	}

	ASSERT(ttolwp(curthread) != NULL);

	fp = &ttolwp(curthread)->lwp_pcb.pcb_fpu;
	fp_save(fp); 		/* save the FPU state */
	fp->fpu_regs.fp_reg_set.fpchip_state.status = fpsw;

#ifdef _VPIX
	rp = lwptoregs(ttolwp(curthread));
	if (rp->r_ps & PS_VM) {
		/*
		 * Virtual 86 task is using the FPU, send a pseudorupt to it.
		 */
		v86setpseudo(curthread, V86VI_COPROC);
		return (DDI_INTR_CLAIMED);
	}
#endif

	/*
	 * request soft interrupt for sending the signal
	 */
	t = ttolwp(curthread)->lwp_thread;
	softcall(fpsoftintr, (caddr_t)t);
	return (DDI_INTR_CLAIMED);
}

/*
 * Return the si_code corresponding to the FP exception from FPU status word.
 */
static int
fpe_sicode(int sw)
{

	if (sw & FPS_IE)
		return (FPE_FLTINV);

	if (sw & FPS_ZE)
		return (FPE_FLTDIV);

	if (sw & FPS_OE)
		return (FPE_FLTOVF);

	if (sw & FPS_UE)
		return (FPE_FLTUND);

	if (sw & FPS_PE)
		return (FPE_FLTRES);

	return (FPE_FLTINV);	/* default si_code for other exceptions */
}
