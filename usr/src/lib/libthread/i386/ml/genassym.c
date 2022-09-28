/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident   "@(#)genassym.c 1.11     96/05/20     SMI"

#include <libthread.h>
#include <signal.h> /* for SIG_SETMASK */
#include <sys/psw.h>

/* Structure returned by fnstenv */
struct fpuenv {
	int cntl;	/* NP control word */
	int status;	/* status word (flags, etc) */
	int tag;	/* tag of which regs busy */
	int misc[4];	/* other stuff, 28 bytes total */
};

/*
 * Used to generate WAITER_MASK below.
 */
#define	default_waiter_mask 0x000000ff

main()
{
	register struct thread *t = (struct thread *)0;
	register struct tls *tls = (struct tls *)0;
	register mutex_t *mp = (mutex_t *)0;
	struct fpuenv *fp = (struct fpuenv *)0;
	lwp_mutex_t lm;

	printf("#define\tTHREAD_SIZE\t0x%x\n", sizeof (struct thread));
	printf("#define\tT_FPU\t0x%x\n", &t->t_resumestate.rs_fpu);
	printf("#define\tT_FPUSTATE\t0x%x\n",
	    &t->t_resumestate.rs_fpu.fp_reg_set.fpchip_state.state);
	printf("#define\tT_PC\t0x%x\n", &t->t_pc);
	printf("#define\tT_SP\t0x%x\n", &t->t_sp);
	printf("#define\tT_BP\t0x%x\n", &t->t_bp);
	printf("#define\tT_EDI\t0x%x\n", &t->t_edi);
	printf("#define\tT_ESI\t0x%x\n", &t->t_esi);
	printf("#define\tT_EBX\t0x%x\n", &t->t_ebx);
	printf("#define\tT_LOCK\t0x%x\n", &t->t_lock);
	printf("#define\tT_STOP\t0x%x\n", &t->t_stop);
	printf("#define\tT_PSIG\t0x%x\n", &t->t_psig);
	printf("#define\tT_HOLD\t0x%x\n", &t->t_hold);
	printf("#define\tT_FLAG\t0x%x\n", &t->t_flag);
	printf("#define\tT_SSIG\t0x%x\n", &t->t_ssig);
	printf("#define\tT_NOSIG\t0x%x\n", &t->t_nosig);
	printf("#define\tT_PENDING\t0x%x\n", &t->t_pending);
	printf("#define\tT_LINK\t0x%x\n", &t->t_link);
	printf("#define\tT_PREV\t0x%x\n", &t->t_prev);
	printf("#define\tT_NEXT\t0x%x\n", &t->t_next);
	printf("#define\tT_TLS\t0x%x\n", &t->t_tls);
	printf("#define\tT_STATE\t0x%x\n", &t->t_state);
	printf("#define\tT_STKSIZE\t0x%x\n", &t->t_stksize);
	printf("#define\tT_TID\t0x%x\n", &t->t_tid);
	printf("#define\tT_LWPID\t0x%x\n", &t->t_lwpid);
	printf("#define\tT_IDLE\t0x%x\n", &t->t_idle);
	printf("#define\tT_FORW\t0x%x\n", &t->t_forw);
	printf("#define\tT_BACKW\t0x%x\n", &t->t_backw);
	printf("#define\tT_IDLETHREAD\t0x%x\n", T_IDLETHREAD);
	printf("#define\tFP_CNTL\t0x%x\n", &fp->cntl);
	printf("#define\tFP_STATUS\t0x%x\n", &fp->status);
	printf("#define\tFP_TAG\t0x%x\n", &fp->tag);
	printf("#define\tFP_SIZE\t0x%x\n", sizeof (struct fpuenv));
	printf("#define\tMUTEX_LOCK\t0x%x\n", &mp->mutex_lockw);
	printf("#define\tMUTEX_OWNER\t0x%x\n", &mp->mutex_owner);
	printf("#define\tMUTEX_WANTED\t0x%x\n", 0);
	printf("#define\tT_USROPTS\t0x%x\n", &t->t_usropts);
	printf("#define M_LOCK_WORD 0x%x\n",
	    (char *)&lm.mutex_lockword - (char *)&lm);
	printf("#define M_WAITER_MASK 0x%x\n",
	    (uint_t)default_waiter_mask <<
	    (8*((char *)&lm.mutex_waiters - (char *)&lm.mutex_lockword)));
	/* PROBE_SUPPORT begin */
	printf("#define\tT_TPDP\t0x%x\n", &t->t_tpdp);
	/* PROBE_SUPPORT end */

	/* cancellation support being */
	printf("#define\tT_CANSTATE\t0x%x\n", &t->t_can_state);
	printf("#define\tT_CANTYPE\t0x%x\n", &t->t_can_type);
	printf("#define\tT_CANPENDING\t0x%x\n", &t->t_can_pending);
	printf("#define\tT_CANCELABLE\t0x%x\n", &t->t_cancelable);

	printf("#define\tTC_PENDING\t0x%x\n", TC_PENDING);
	printf("#define\tTC_DISABLE\t0x%x\n", TC_DISABLE);
	printf("#define\tTC_ENABLE\t0x%x\n", TC_ENABLE);
	printf("#define\tTC_ASYNCHRONOUS\t0x%x\n", TC_ASYNCHRONOUS);
	printf("#define\tTC_DEFERRED\t0x%x\n", TC_DEFERRED);
	printf("#define\tPTHREAD_CANCELED\t0x%x\n", PTHREAD_CANCELED);
	/* cancellation support ends */

	printf("#define\tTS_ZOMB\t0x%x\n", TS_ZOMB);
	printf("#define\tTS_ONPROC\t0x%x\n", TS_ONPROC);
	printf("#define\tSIG_SETMASK\t0x%x\n", SIG_SETMASK);
	printf("#define\tPAGESIZE\t0x%x\n", PAGESIZE);
#ifdef TRACE_INTERNAL
	printf("#define\tTR_T_SWTCH\t0x%x\n", TR_T_SWTCH);
#endif
	exit(0);
}
