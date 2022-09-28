/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)cresume.c 1.13       95/02/01 SMI"

#include "../../common/libthread.h"
#include <sys/psw.h>
#include <sys/pcb.h>

/*
 * resume the execution of the specified thread on the current
 * LWP.
 *
 * signals are disabled by the caller and are enabled on return
 * from resume via _resume_ret() which happens in threadjmp() or
 * swtchsig().
 */
_resume(t, tempstk, dontsave)
	uthread_t *t;
	caddr_t tempstk;
	int dontsave;
{
	register int lwpfpu_init = 0;
	uthread_t *ct = curthread;
	uthread_t *tt = t;
	uthread_t *zombie = ct;
	int	restore = 0;

	if (curthread->t_state != TS_ZOMB) {
		if (!dontsave) {
			_save_thread(ct);
			/*
			 * determine if current thread has used the fpu.
			 */
			lwpfpu_init = _getlwpfpu();
			if (lwpfpu_init & PCB_FPU_INITIALIZED) {
				_savefpu(ct);
				restore = 1;
			}
		}
		/*
		 * switch to a temporary stack,
		 * and release the current thread's cpu lock,
		 * curthread->t_lock.
		 */
		tt = (uthread_t *)_switch_stack(tempstk, ct, tt);
		ct = curthread;
		zombie = NULL;
	}
	/*
	 * must first acquire the per-thread mutex of the resumed thread
	 * before going any further.
	 */
	_lwp_mutex_lock(&(tt->t_lock));

	/*
	 * check if restore is required.
	 */
	if (restore || tt->t_fpvalid)
		_restorefpu(tt);

	_threadjmp(tt, zombie);
	/* never returns */
}
