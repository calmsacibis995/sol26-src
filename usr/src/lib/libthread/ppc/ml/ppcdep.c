/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppcdep.c	1.57	95/08/25 SMI"

#ifdef TLS
#define	NOTHREAD
#endif

#include "libthread.h"
#include <sys/reg.h>

#ifdef	TLS

int _threadoffset = (int)&thread;
/*
 * given a pointer to TLS find the pointer to struct thread.
 */
uthread_t *
_tlstot(char *tls)
{
	if (tls == NULL)
		_panic("tlstot, no TLS");
	return ((uthread_t *)(tls + (int)&_thread));
}

int
_t_tls()
{
	return ((int)&_thread);
}

#else

int _threadoffset = 0;

#endif /* TLS */

/* PROBE_SUPPORT begin */
extern void * _thread_probe_getfunc(void);
void * (*thr_probe_getfunc_addr)(void) = _thread_probe_getfunc;
/* PROBE_SUPPORT end */

/*
 * load a function call into a thread.
 */
int
_thread_call(uthread_t *t, void (*func)(), void *arg)
{
	int	* stackp;

	/*
	 * create a "c" call frame for "thread_start", the caller
	 * of "func".
	 */
	stackp = (int *) t->t_sp;
	*stackp = 0;	/* Back-chain */
	*(stackp+1) = 0;	/* LR */
	t->t_r14 = (int)func;	/* arguments to thread_start */
	t->t_r15 = (int)arg;
	/*
	 * set thread to resume at thread_start()
	 */
	t->t_pc = (uint)_thread_start;
	t->t_sp = (uint)t->t_sp;
	return (0);
}

void
_thread_ret(struct thread *t, void (*func)())
{
	t->t_pc = (uint)func;
}

int
_lwp_exec(struct thread *t, long retpc, caddr_t sp, void (*func)(),
	int flags, lwpid_t *lwpidp)
{
	ucontext_t uc;
	int ret;

	ASSERT(t != NULL);
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_LWP_EXEC_START, "_lwpexec start");
	/*
	 * to avoid clearing the entire ucontext_t structure, only
	 * the following necessary fields are cleared.
	 */
	uc.uc_link = 0;

	uc.uc_mcontext.gregs[R_PC] = (int)func;
	uc.uc_mcontext.gregs[R_R1] = (int)(sp);
	uc.uc_mcontext.gregs[R_LR] = (int)retpc;
#ifdef TLS
	uc.uc_mcontext.gregs[R_R2] = (int)t->t_tls;
#else
	uc.uc_mcontext.gregs[R_R2] = (int)t;
#endif
	uc.uc_mcontext.gregs[R_R14] = t->t_r14;
	uc.uc_mcontext.gregs[R_R15] = t->t_r15;

	/*
	 * Push down the signal mask only for the ASLWP. This routine is
	 * called for two other types of threads: bound and aging. For both
	 * these threads, it would be incorrect to push down the mask:
	 *      - for bound threads: their calls to thr_sigsetmask() should
	 *        also take advantage of fast masking - so the underlying LWP
	 *        always has to have all signals unblocked.
	 *      - for aging threads: they pick up new threads to run and when
	 *              they do, they need to have all signals unblocked.
	 *
	 */
	if (flags & __LWP_ASLWP)
		uc.uc_sigmask = t->t_hold;
	else
		uc.uc_sigmask = _null_sigset;

	if (lwpidp == NULL)
		_panic("lwp_exec: NULL lwpidp");
	ret = _lwp_create(&uc, flags, lwpidp);
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_LWP_EXEC_END, "_lwpexec end");
	return (ret);
}
