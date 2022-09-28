/*
 * Copyright (c) 1994-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)genassym.c	1.32	96/05/20 SMI"

#include "libthread.h"
#include <signal.h> /* for SIG_SETMASK */
#include <sys/psw.h>

main()
{
	register struct thread *t = (struct thread *)0;
	register struct tls *tls = (struct tls *)0;
	register mutex_t *mp = (mutex_t *)0;
	lwp_mutex_t	lm;

	printf("#define\tTHREAD_SIZE\t0x%x\n", sizeof (struct thread));
	printf("#define\tT_PC\t0x%x\n", &t->t_pc);
	printf("#define\tT_SP\t0x%x\n", &t->t_sp);
	printf("#define\tT_CR\t0x%x\n", &t->t_cr);

	printf("#define\tT_R13\t0x%x\n", &t->t_r13);
	printf("#define\tT_R14\t0x%x\n", &t->t_r14);
	printf("#define\tT_R15\t0x%x\n", &t->t_r15);
	printf("#define\tT_R16\t0x%x\n", &t->t_r16);
	printf("#define\tT_R17\t0x%x\n", &t->t_r17);
	printf("#define\tT_R18\t0x%x\n", &t->t_r18);
	printf("#define\tT_R19\t0x%x\n", &t->t_r19);
	printf("#define\tT_R20\t0x%x\n", &t->t_r20);
	printf("#define\tT_R21\t0x%x\n", &t->t_r21);
	printf("#define\tT_R22\t0x%x\n", &t->t_r22);
	printf("#define\tT_R23\t0x%x\n", &t->t_r23);
	printf("#define\tT_R24\t0x%x\n", &t->t_r24);
	printf("#define\tT_R25\t0x%x\n", &t->t_r25);
	printf("#define\tT_R26\t0x%x\n", &t->t_r26);
	printf("#define\tT_R27\t0x%x\n", &t->t_r27);
	printf("#define\tT_R28\t0x%x\n", &t->t_r28);
	printf("#define\tT_R29\t0x%x\n", &t->t_r29);
	printf("#define\tT_R30\t0x%x\n", &t->t_r30);
	printf("#define\tT_R31\t0x%x\n", &t->t_r31);

	printf("#define\tT_F14\t0x%x\n", &t->t_f14);
	printf("#define\tT_F15\t0x%x\n", &t->t_f15);
	printf("#define\tT_F16\t0x%x\n", &t->t_f16);
	printf("#define\tT_F17\t0x%x\n", &t->t_f17);
	printf("#define\tT_F18\t0x%x\n", &t->t_f18);
	printf("#define\tT_F19\t0x%x\n", &t->t_f19);
	printf("#define\tT_F20\t0x%x\n", &t->t_f20);
	printf("#define\tT_F21\t0x%x\n", &t->t_f21);
	printf("#define\tT_F22\t0x%x\n", &t->t_f22);
	printf("#define\tT_F23\t0x%x\n", &t->t_f23);
	printf("#define\tT_F24\t0x%x\n", &t->t_f24);
	printf("#define\tT_F25\t0x%x\n", &t->t_f25);
	printf("#define\tT_F26\t0x%x\n", &t->t_f26);
	printf("#define\tT_F27\t0x%x\n", &t->t_f27);
	printf("#define\tT_F28\t0x%x\n", &t->t_f28);
	printf("#define\tT_F29\t0x%x\n", &t->t_f29);
	printf("#define\tT_F30\t0x%x\n", &t->t_f30);
	printf("#define\tT_F31\t0x%x\n", &t->t_f31);
	printf("#define\tT_FPSCR\t0x%x\n", &t->t_fpscr);
	printf("#define\tT_FPVALID\t0x%x\n", &t->t_fpvalid);

	printf("#define\tT_LOCK\t0x%x\n", &t->t_lock);
	printf("#define\tT_STOP\t0x%x\n", &t->t_stop);
	printf("#define\tT_PSIG\t0x%x\n", &t->t_psig);
	printf("#define\tT_HOLD\t0x%x\n", &t->t_hold);
	printf("#define\tT_SSIG\t0x%x\n", &t->t_ssig);
	printf("#define\tT_NOSIG\t0x%x\n", &t->t_nosig);
	printf("#define\tT_PENDING\t0x%x\n", &t->t_pending);
	printf("#define\tT_LINK\t0x%x\n", &t->t_link);
	printf("#define\tT_PREV\t0x%x\n", &t->t_prev);
	printf("#define\tT_NEXT\t0x%x\n", &t->t_next);
	printf("#define\tT_TLS\t0x%x\n", &t->t_tls);
	printf("#define\tT_STATE\t0x%x\n", &t->t_state);
	printf("#define\tT_FLAG\t0x%x\n", &t->t_flag);
	printf("#define\tT_STKSIZE\t0x%x\n", &t->t_stksize);
	printf("#define\tT_TID\t0x%x\n", &t->t_tid);
	printf("#define\tT_LWPID\t0x%x\n", &t->t_lwpid);
	printf("#define\tT_IDLE\t0x%x\n", &t->t_idle);
	printf("#define\tT_FORW\t0x%x\n", &t->t_forw);
	printf("#define\tT_BACKW\t0x%x\n", &t->t_backw);
	printf("#define\tMUTEX_LOCK\t0x%x\n", &mp->mutex_lockw);
	printf("#define\tM_LOCK_WORD\t0x%x\n",
	    (char *)&lm.mutex_lockword - (char *)&lm);
	printf("#define\tM_LOCK_MASK\t0x%x\n",
#ifdef _LITTLE_ENDIAN
	    1 << (8*((char *)&lm.mutex_lockw - (char *)&lm.mutex_lockword)));
#else
	    1 << (24-8*((char *)&lm.mutex_lockw - (char *)&lm.mutex_lockword)));
#endif
	printf("#define\tMUTEX_WAITERS\t0x%x\n", &mp->mutex_waiters);
	printf("#define\tMUTEX_OWNER\t0x%x\n", &mp->mutex_owner);
	printf("#define\tMUTEX_WANTED\t0x%x\n", 0);
	printf("#define\tM_WAITER_BIT\t%d\n",
#ifdef _LITTLE_ENDIAN
	    (31 - 8 * (&lm.mutex_waiters - &lm.mutex_lockword)));
#else
	    (7 + 8 * (&lm.mutex_waiters - &lm.mutex_lockword)));
#endif
	printf("#define\tT_USROPTS\t0x%x\n", &t->t_usropts);
	/* PROBE_SUPPORT begin */
	printf("#define\tT_TPDP\t0x%x\n", &t->t_tpdp);
	/* PROBE_SUPPORT end */

	/* cancellation support begin */
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
	printf("#define\tT_IDLETHREAD\t0x%x\n", T_IDLETHREAD);
	printf("#define\tTS_ONPROC\t0x%x\n", TS_ONPROC);
	printf("#define\tSIG_SETMASK\t0x%x\n", SIG_SETMASK);
	printf("#define\tMSR_FP\t0x%x\n", MSR_FP);
	printf("#define\tPAGESIZE\t0x%x\n", PAGESIZE);
#ifdef TRACE_INTERNAL
	printf("#define\tTR_T_SWTCH\t0x%x\n", TR_T_SWTCH);
#endif
	exit(0);
}
