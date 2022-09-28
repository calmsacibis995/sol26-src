/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_main.c	1.80	96/09/20 SMI"


/*
 * This module contains the guts of checkpoint-resume mechanism.
 * All code in this module is platform independent.
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/errno.h>
#include <sys/callb.h>
#include <sys/processor.h>
#include <sys/machsystm.h>
#include <sys/clock.h>
#include <sys/vfs.h>
#include <sys/kmem.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/bootconf.h>

extern void cpr_convert_promtime(timestruc_t *);
extern int cpr_alloc_statefile(void);
extern void flush_windows(void);
extern int setjmp(label_t *);
extern void i_cpr_read_prom_mappings();
extern void cpr_abbreviate_devpath(char *, char *);
extern int cpr_set_properties(struct cprinfo *);

static int cpr_suspend(void);
static int cpr_resume(void);
static struct cprinfo *ci;

extern struct cpr_terminator cpr_term;

timestruc_t wholecycle_tv;

/*
 * The main switching point for cpr, this routine starts the ckpt
 * and state file saving routines; on resume the control is
 * returned back to here and it then calls the resume routine.
 */
int
cpr_main()
{
	label_t saveq = ttolwp(curthread)->lwp_qsav;
	int rc = 0;

	ci = kmem_zalloc(sizeof (struct cprinfo), KM_SLEEP);
	/*
	 * Remember where we are for resume
	 */
	if (!setjmp(&ttolwp(curthread)->lwp_qsav)) {
		/*
		 * try to checkpoint the system, if failed return back
		 * to userland, otherwise power off.
		 */
		rc = cpr_suspend();
		if (rc) {
			/*
			 * Something went wrong in suspend, do what we can to
			 * put the system back to an operable state then
			 * return back to userland.
			 */
			(void) cpr_resume();
		}
	} else {
		/*
		 * This is the resumed side of longjmp, restore the previous
		 * longjmp pointer if there is one so this will be transparent
		 * to the world.
		 */
		ttolwp(curthread)->lwp_qsav = saveq;
		CPR->c_flags &= ~C_SUSPENDING;
		CPR->c_flags |= C_RESUMING;

		/*
		 * resume the system back to the original state
		 */
		rc = cpr_resume();
	}
	kmem_free(ci, sizeof (struct cprinfo));

	return (rc);
}

/*
 * Take the system down to a checkpointable state and write
 * the state file, the following are sequentially executed:
 *
 *    - Request all user threads to stop themselves
 *    - push out and invalidate user pages
 *    - bring statefile inode incore to prevent a miss later
 *    - request all daemons to stop
 *    - check and make sure all threads are stopped
 *    - sync the file system
 *    - suspend all devices
 *    - block intrpts
 *    - dump system state and memory to state file
 */

static int
cpr_suspend()
{
	int rc = 0;
	void  (*fn)();
	extern void cpr_send_notice();

	wholecycle_tv = i_cpr_todget();
	CPR_STAT_EVENT_START("Suspend Total");

	i_cpr_read_prom_mappings();

	/*
	 * read needed current openprom info.
	 */
	if ((rc = cpr_get_bootinfo(ci)))
		return (rc);

	/*
	 * We need to validate cprinfo file before fs functionality
	 * is disabled.
	 */
	if ((rc = cpr_validate_cprinfo(ci)))
		return (rc);

	i_cpr_save_machdep_info();

	if ((rc = cpr_mp_offline()))
		return (rc);

	/* cpr_signal_user(SIGFREEZE); */

	/*
	 * Ask the user threads to stop by themselves, but
	 * if they don't or can't after 3 retires, we give up on CPR.
	 * The 3 retry is not a random number because 2 is possible if
	 * a thread has been forked before the parent thread is stopped.
	 */
	DEBUG1(errp("\nstopping user threads..."));
	CPR_STAT_EVENT_START("  stop users");
	cpr_set_substate(C_ST_USER_THREADS);
	if (rc = cpr_stop_user_threads())
		return (rc);
	CPR_STAT_EVENT_END("  stop users");
	DEBUG1(errp("done\n"));

	cpr_send_notice();

	cpr_save_time();

	/*
	 * Stop all daemon activities
	 */
	DEBUG1(errp("stopping kernel daemons..."));
	callb_execute_class(CB_CL_CPR_DAEMON, CB_CODE_CPR_CHKPT);
	DEBUG1(errp("done\n"));

	/*
	 * Use sync_all to swap out all user pages and find out how much
	 * extra space needed for user pages that don't have back store
	 * space left.
	 */
	CPR_STAT_EVENT_START("  swapout upages");
	vfs_sync(SYNC_ALL);
	CPR_STAT_EVENT_END("  swapout upages");

	CPR_STAT_EVENT_START("  alloc statefile");
	cpr_set_substate(C_ST_STATEF_ALLOC); /* must be before realloc label */
realloc:
	if (rc = cpr_alloc_statefile())
		return (rc);
	CPR_STAT_EVENT_END("  alloc statefile");

	/*
	 * Sync the filesystem to preserve its integrity.
	 *
	 * This sync is also used to flush out all B_DELWRI buffers (fs cache)
	 * which are mapped and neither dirty nor referened before
	 * cpr_invalidate_pages destroies them. fsflush does similar thing.
	 */
	sync();

	/*
	 * destroy all clean file mapped kernel pages
	 */
	CPR_STAT_EVENT_START("  clean pages");
	DEBUG1(errp("cleaning up mapped pages..."));
	callb_execute_class(CB_CL_CPR_VM, CB_CODE_CPR_CHKPT);
	DEBUG1(errp("done\n"));
	CPR_STAT_EVENT_END("  clean pages");


	cpr_set_substate(C_ST_DRIVERS);
	CPR_STAT_EVENT_START("  stop drivers");
	fn = cpr_hold_driver;	/* callback to hold driver thread */
	if (ddi_prop_create(DDI_DEV_T_NONE, ddi_root_node(), 0,
		"cpr-driver", (caddr_t)&fn, sizeof (fn)) != DDI_PROP_SUCCESS)
		/*
		 * No memory to create properties for drivers.
		 * Let the system resume back to where it was.
		 */
		return (ENOMEM);

	DEBUG1(errp("suspending drivers..."));
	if ((rc = cpr_suspend_devices(ddi_root_node())))
		return (rc);
	DEBUG1(errp("done\n"));
	CPR_STAT_EVENT_END("  stop drivers");

	/*
	 * To safely save the callout table, we need to disable the clock
	 * interrupt during the period of copying the table.
	 */
	i_cpr_disable_clkintr();
	callb_execute_class(CB_CL_CPR_CALLOUT, CB_CODE_CPR_CHKPT);
	i_cpr_enable_clkintr();

	/*
	 * It's safer to do tod_get before we disable all intr.
	 */
	CPR_STAT_EVENT_START("  write statefile");
	/*
	 * it's time to ignore the outside world, stop the real time
	 * clock and disable any further intrpt activity.
	 */

	i_cpr_handle_xc(1);	/* turn it on to disable xc assertion */
	i_cpr_stop_intr();
	DEBUG1(errp("interrupt is stopped\n"));

	/*
	 * getting ready to write ourself out, flush the register
	 * windows to make sure that our stack is good when we
	 * come back on the resume side.
	 */
	flush_windows();

	/*
	 * FATAL: NO MORE MEMORY ALLOCATION ALLOWED AFTER THIS POINT!!!
	 *
	 * The system is quiesced at this point, we are ready to either dump
	 * to the state file for a extended sleep or a simple shutdown for
	 * systems with non-volatile memory.
	 */
	cpr_set_substate(C_ST_DUMP);

	if ((rc = cpr_dump(C_VP)) == ENOSPC) {
		cpr_set_substate(C_ST_STATEF_ALLOC_RETRY);
		(void) cpr_resume();
		goto realloc;
	} else {
		if (rc == 0) {
			(void) strcpy(ci->ci_bootfile, CPRBOOT);
			(void) strcpy(ci->ci_autoboot, "true");
			(void) strcpy(ci->ci_diagsw, "false");
			/*
			 * On some versions of the prom, a fully qualified
			 * device path can be truncated when stored in
			 * the boot-device nvram property.  This call
			 * generates the shortest unambiguous equivalent.
			 */
			cpr_abbreviate_devpath(rootfs.bo_name,
			    ci->ci_bootdevice);
			rc = cpr_set_properties(ci);
		}
	}
	return (rc);
}

/*
 * Bring the system back up from a checkpoint, at this point
 * the VM has been minimally restored by boot, the following
 * are executed sequentially:
 *
 *    - machdep setup and enable interrupts (mp startup if it's mp)
 *    - resume all devices
 *    - restart daemons
 *    - put all threads back on run queue
 */
static int
cpr_resume()
{
	extern void cpr_statef_close();
	timestruc_t pwron_tv;
	int rc = 0;

	/*
	 * The following switch is used for resume the system that was suspended
	 * to a different level.
	 */
	DEBUG1(errp("\nEntering cpr_resume...\n"));
	switch (CPR->c_substate) {
	case C_ST_DUMP:
	case C_ST_STATEF_ALLOC_RETRY:
		break;

	case C_ST_DRIVERS:
		goto driver;

	case C_ST_STATEF_ALLOC:
		goto alloc;

	case C_ST_USER_THREADS:
		goto user;

	default:
		goto others;
	}

	/*
	 * setup debugger trapping.
	 */
	i_cpr_set_tbr();

	/*
	 * tell prom to monitor keys before the kernel come alive
	 */
	start_mon_clock();

	/*
	 * For sun4m, setup IOMMU before resuming devices.
	 */
	i_cpr_machdep_setup();

	/* kmem_ready = 1; */
	/*
	 * IMPORTANT:  SENSITIVE RESUME SEQUENCE
	 *
	 * DO NOT ADD ANY INITIALIZATION STEP BEFORE THIS POINT!!
	 */

	/*
	 * start other CPUs
	 */
	if (ncpus > 1 && CPR->c_substate != C_ST_STATEF_ALLOC_RETRY) {
		callb_execute_class(CB_CL_CPR_MPSTART, CB_CODE_CPR_RESUME);
		DEBUG1(errp("MP started.\n"));
	}

	/*
	 * let the tmp callout catch up.
	 */
	callb_execute_class(CB_CL_CPR_CALLOUT, CB_CODE_CPR_RESUME);

	i_cpr_enable_intr();
	i_cpr_handle_xc(0);	/* turn it off to allow xc assertion */

	/*
	 * statistics gathering
	 */
	cpr_convert_promtime(&pwron_tv);

	CPR_STAT_EVENT_END_TMZ("  write statefile", &cpr_term.tm_shutdown);
	CPR_STAT_EVENT_END_TMZ("Suspend Total", &cpr_term.tm_shutdown);

	CPR_STAT_EVENT_START_TMZ("Resume Total", &pwron_tv);

	CPR_STAT_EVENT_START_TMZ("  prom time", &pwron_tv);
	CPR_STAT_EVENT_END_TMZ("  prom time", &cpr_term.tm_cprboot_start);

	CPR_STAT_EVENT_START_TMZ("  read statefile",
		&cpr_term.tm_cprboot_start);
	CPR_STAT_EVENT_END_TMZ("  read statefile", &cpr_term.tm_cprboot_end);

driver:
	DEBUG1(errp("resuming devices..."));
	CPR_STAT_EVENT_START("  start drivers");
	/*
	 * The policy here is to continue resume everything we can if we did
	 * not successfully finish suspend; and panic if we are coming back
	 * from a fully suspended system.
	 */
	rc = cpr_resume_devices(ddi_root_node());
	if (rc && CPR->c_substate == C_ST_DUMP)
		cmn_err(CE_PANIC, "failed to resume one or more devices\n");
	else if (rc)
		cmn_err(CE_WARN, "failed to resume one or more devices\n");
	CPR_STAT_EVENT_END("  start drivers");
	DEBUG1(errp("done\n"));

	if (ddi_prop_remove(DDI_DEV_T_NONE, ddi_root_node(),
		"cpr-driver") != DDI_PROP_SUCCESS)
		cmn_err(CE_WARN, "cpr: Can't remove property");

	cv_broadcast(&CPR->c_holddrv_cv);

	/*
	 * This resume is due to a retry of allocating the statefile.
	 */
	if (CPR->c_substate == C_ST_STATEF_ALLOC_RETRY)
		return (0);

alloc:
	cpr_statef_close();
	/*
	 * put all threads back to where they belong,
	 * get the kernel daemons straightened up too.
	 */
	DEBUG1(errp("starting kernel daemons..."));
	callb_execute_class(CB_CL_CPR_DAEMON, CB_CODE_CPR_RESUME);
	DEBUG1(errp("done\n"));

user:
	DEBUG1(errp("starting user threads..."));
	cpr_start_user_threads();
	prom_printf("Done.\n");
	DEBUG1(errp("done\n"));

others:
	/*
	 * now that all the drivers are going, kernel kbd driver can
	 * take over, turn off prom monitor clock
	 */
	stop_mon_clock();

	cpr_stat_record_events();

	cpr_restore_time();
	cpr_void_cprinfo(ci);

	DEBUG1(errp("Sending SIGTHAW ..."));
	cpr_signal_user(SIGTHAW);
	DEBUG1(errp("done\n"));


	if (cpr_mp_online())
		cmn_err(CE_WARN, "failed to online all of the processors\n");

	CPR_STAT_EVENT_END("Resume Total");

	CPR_STAT_EVENT_START_TMZ("WHOLE CYCLE", &wholecycle_tv);
	CPR_STAT_EVENT_END("WHOLE CYCLE");

	DEBUG1(errp("\nThe system is back where you left!\n"));

	CPR_STAT_EVENT_START("POST CPR DELAY");

#ifdef CPR_STAT
	CPR_STAT_EVENT_START_TMZ("PWROFF TIME", &cpr_term.tm_shutdown);
	CPR_STAT_EVENT_END_TMZ("PWROFF TIME", &pwron_tv);

	CPR_STAT_EVENT_PRINT();
#endif CPR_STAT

	return (rc);
}
