/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_mod.c	1.54	96/09/19 SMI"

/*
 * System call to checkpoint and resume the currently running kernel
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/syscall.h>
#include <sys/cred.h>
#include <sys/uadmin.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>

extern int cpr_is_supported(void);

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, "checkpoint resume"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

kmutex_t	cpr_slock;	/* cpr serial lock */
cpr_t		cpr_state;
int		cpr_debug;
int		cpr_test_mode; /* true if called via uadmin testmode */

/*
 * All the loadable module related code follows
 */
int
_init(void)
{
	register int e;

	if ((e = mod_install(&modlinkage)) == 0) {
		mutex_init(&cpr_slock, "checkpoint serial lock",
		    MUTEX_DEFAULT, (void *)0);
		mutex_init(&CPR->c_dlock, "checkpoint hold driver lock",
		    MUTEX_DEFAULT, (void *)0);
		cv_init(&CPR->c_holddrv_cv, "checkpoint hold driver cv",
		    CV_DEFAULT, NULL);
	}
	return (e);
}

int
_fini(void)
{
	register int e;

	if ((e = mod_remove(&modlinkage)) == 0) {
		mutex_destroy(&cpr_slock);
		mutex_destroy(&CPR->c_dlock);
		cv_destroy(&CPR->c_holddrv_cv);
	}
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
cpr(int fcn)
{
	register int rc = 0;
	extern int cpr_init(int);
	extern void cpr_done(void);

	switch (fcn) {

	case AD_CPR_NOCOMPRESS:
	case AD_CPR_COMPRESS:
	case AD_CPR_FORCE:
		cpr_test_mode = 0;
		break;

	case AD_CPR_TESTNOZ:
	case AD_CPR_TESTZ:
		cpr_test_mode = 1;
		break;

	case AD_CPR_CHECK:
		if (!cpr_is_supported())
			return (ENOTSUP);

		return (0);

	case AD_CPR_PRINT:

		CPR_STAT_EVENT_END("POST CPR DELAY");
		cpr_stat_event_print();
		return (0);

	case AD_CPR_DEBUG0:
		cpr_debug = 0;
		return (0);

	case AD_CPR_DEBUG1:
		cpr_debug |= LEVEL1;
		return (0);

	case AD_CPR_DEBUG2:
		cpr_debug |= LEVEL2;
		return (0);

	case AD_CPR_DEBUG3:
		cpr_debug |= LEVEL3;
		return (0);

	case AD_CPR_DEBUG4:
		cpr_debug |= LEVEL4;
		return (0);

	case AD_CPR_DEBUG5:
		cpr_debug |= LEVEL5;
		return (0);

	case AD_CPR_DEBUG9:
		cpr_debug |= LEVEL6;
		return (0);

	default:
		return (ENOTSUP);
	}

	/*
	 * acquire cpr serial lock and init cpr state structure.
	 */
	if (rc = cpr_init(fcn))
		return (rc);

	/*
	 * Call the main cpr routine. If we are succesful, we will be coming
	 * down from the resume side, otherwise we are still in suspend.
	 */
	if (rc = cpr_main()) {
		extern void cpr_console_clear(void);

		CPR->c_flags |= C_ERROR;
		cpr_console_clear(); /* change back to "black on white" */
		cmn_err(CE_WARN, "Suspend operation failed..."
		"system state is restored");

	} else if (CPR->c_flags & C_SUSPENDING) {
		extern void cpr_power_down();
		/*
		 * Back from a successful checkpoint
		 */
		if (fcn == AD_CPR_TESTZ || fcn == AD_CPR_TESTNOZ) {
			mdboot(0, AD_BOOT, "");
			/* NOTREACHED */
		}

		/*
		 * If cpr_power_down() succeeds, it'll not return.
		 */
		cpr_power_down();

		halt("Done. Please Switch Off");
		/* NOTREACHED */
	}
	/*
	 * For resuming: release resources and the serial lock.
	 */
	cpr_done();
	return (rc);
}
