/*
 *	Copyright (c) 1995 Sun Microsystems, Inc.
 *	All rights reserved.
 */
#ident	"@(#)install_utrap.c	1.3	96/02/28 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/machpcb.h>
#include <sys/utrap.h>
#ifdef SF_ERRATA_30 /* call causes fp-disabled */
#include <sys/cmn_err.h>
#endif

int
install_utrap(utrap_entry_t type,
		utrap_handler_t new_handler,
		utrap_handler_t *old_handlerp)
{
	register klwp_id_t lwp = ttolwp(curthread);
	register struct machpcb *mpcb = lwptompcb(lwp);
	utrap_handler_t *ov, *nv;
	proc_t	*p = curproc;
	int idx;

	/*
	 * Check trap number.
	 */
	switch (type) {
	case UTRAP_V8P_FP_DISABLED:
#ifdef SF_ERRATA_30 /* call causes fp-disabled */
		{
		extern int spitfire_call_bug;

		if (spitfire_call_bug) {
			cmn_err(CE_WARN, "UTRAP_V8P_FP_DISABLED "
			    "not supported for cpu version < 2.2");
			return ((int) set_errno(ENOSYS));
		}
		}
#endif /* SF_ERRATA_30 */
		idx = MPCB_TRAP1;
		break;
	case UTRAP_V8P_MEM_ADDRESS_NOT_ALIGNED:
		idx = MPCB_TRAP0;
		break;
	default:
		return ((int) set_errno(EINVAL));
	}
	/*
	 * Be sure handler address is word aligned.
	 */
	nv = (utrap_handler_t *)new_handler;
	if (nv != UTRAP_UTH_NOCHANGE) {
		if (((uint_t)nv) & 0x3)
			return ((int) set_errno(EINVAL));
	}

	/*
	 * Use the process lock to atomically install the handler.
	 */
	mutex_enter(&p->p_lock);
	ov = mpcb->mpcb_traps[idx];
	if (old_handlerp != NULL) {
		if (suword((int *)old_handlerp, (int)ov) == -1) {
			mutex_exit(&p->p_lock);
			return ((int) set_errno(EINVAL));
		}
	}
	if (new_handler != (utrap_handler_t)UTRAP_UTH_NOCHANGE)
		mpcb->mpcb_traps[idx] = nv;
	mutex_exit(&p->p_lock);
	return (0);
}
