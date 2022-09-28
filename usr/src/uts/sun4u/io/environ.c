/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)environ.c 1.19	96/10/17 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/obpdefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ivintr.h>
#include <sys/autoconf.h>
#include <sys/intreg.h>
#include <sys/modctl.h>
#include <sys/proc.h>
#include <sys/fhc.h>
#include <sys/environ.h>

/* Useful debugging Stuff */
#include <sys/nexusdebug.h>

/*
 * Function prototypes
 */
static int environ_identify(dev_info_t *devi);

static int environ_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

static int environ_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static int environ_init(struct environ_soft_state *softsp);

void environ_add_temp_kstats(struct environ_soft_state *softsp);

static void overtemp_wakeup(void);

static void environ_overtemp_poll(void);

/*
 * Configuration data structures
 */
static struct cb_ops environ_cb_ops = {
	nulldev,		/* open */
	nulldev,		/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	nulldev,		/* read */
	nulldev,		/* write */
	nulldev,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab */
	D_MP | D_NEW		/* Driver compatibility flag */
};

static struct dev_ops environ_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt */
	ddi_no_info,		/* getinfo */
	environ_identify,	/* identify */
	nulldev,		/* probe */
	environ_attach,		/* attach */
	environ_detach,		/* detach */
	nulldev,		/* reset */
	&environ_cb_ops,	/* cb_ops */
	(struct bus_ops *)0,	/* bus_ops */
	nulldev			/* power */
};

void *environp;			/* environ soft state hook */

/*
 * Mutex used to protect the soft state list and their data.
 */
static kmutex_t overtemp_mutex;

/* The CV is used to wakeup the thread when needed. */
static kcondvar_t overtemp_cv;

/* linked list of all environ soft states */
struct environ_soft_state *tempsp_list = NULL;

/* overtemp polling routine timeout delay */
static int overtemp_timeout_sec = OVERTEMP_TIMEOUT_SEC;

/* Should the environ_overtemp_poll thread be running? */
static int environ_do_overtemp_thread = 1;

/* Indicates whether or not the overtemp thread has been started */
static int environ_overtemp_thread_started = 0;

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"Environment Leaf",	/* name of module */
	&environ_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

#ifndef lint
static char _depends_on[] = "drv/fhc";
#endif  /* lint */

/*
 * These are the module initialization routines.
 */

int
_init(void)
{
	int error;

	if ((error = ddi_soft_state_init(&environp,
	    sizeof (struct environ_soft_state), 1)) != 0)
		return (error);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	ddi_soft_state_fini(&environp);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
environ_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	if ((strcmp(name, "environment") == 0) ||
	    (strcmp(name, "environ") == 0)) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

static int
environ_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct environ_soft_state *softsp;
	int instance;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(devi);

	if (ddi_soft_state_zalloc(environp, instance) != DDI_SUCCESS)
		return (DDI_FAILURE);

	softsp = ddi_get_soft_state(environp, instance);

	/* Set the dip in the soft state */
	softsp->dip = devi;

	/*
	 * The DDI documentation on ddi_getprop() routine says that
	 * you should always use the real dev_t when calling it,
	 * but all calls found in uts use either DDI_DEV_T_ANY
	 * or DDI_DEV_T_NONE. No notes either on how to find the real
	 * dev_t. So we are doing it in two steps.
	 */
	softsp->pdip = ddi_get_parent(softsp->dip);

	if ((softsp->board = (int) ddi_getprop(DDI_DEV_T_ANY, softsp->pdip,
	    DDI_PROP_DONTPASS, OBP_BOARDNUM, -1)) == -1) {
		cmn_err(CE_WARN, "environ%d: unable to retrieve %s property",
			instance, OBP_BOARDNUM);
		goto bad;
	}

	DPRINTF(ENVIRON_ATTACH_DEBUG, ("environ: devi= 0x%x\n, softsp=0x%x,",
		(int) devi, (int) softsp));

	/*
	 * Init the temperature device here. We start the overtemp
	 * polling thread here.
	 */
	if (environ_init(softsp) != DDI_SUCCESS)
		goto bad;

	ddi_report_dev(devi);

	if (environ_overtemp_thread_started == 0) {

		/*
		 * set up the overtemp mutex and condition variable before
		 * starting the thread.
		 */
		mutex_init(&overtemp_mutex, "Overtemp Mutex", MUTEX_DEFAULT,
			DEFAULT_WT);

		cv_init(&overtemp_cv, "Overtemp CV", CV_DRIVER, NULL);

		/* Start the overtemp polling thread now. */
		if (thread_create(NULL, PAGESIZE,
		    (void (*)())environ_overtemp_poll, 0, 0, &p0, TS_RUN, 60)
		    == NULL) {
			cmn_err(CE_WARN, "environ%d: cannot start overtemp"
				" thread", instance);
			mutex_destroy(&overtemp_mutex);
			cv_destroy(&overtemp_cv);
		} else {
			environ_overtemp_thread_started++;
		}
	}

	return (DDI_SUCCESS);

bad:
	ddi_soft_state_free(environp, instance);
	return (DDI_FAILURE);
}

/* ARGSUSED */
static int
environ_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
	case DDI_DETACH:
		/*
		 * XXX think about what it means to remove the interrupt
		 * in light of the existing timeout mechanisms
		 */
	default:
		return (DDI_FAILURE);
	}
}

static int
environ_init(struct environ_soft_state *softsp)
{
	u_char tmp;

	/*
	 * If this environment node is on a CPU-less system board, i.e.,
	 * board type MEM_TYPE, then we do not want to map in, read
	 * the temperature register, create the polling entry for
	 * the overtemp polling thread, or create a kstat entry.
	 *
	 * The reason for this is that when no CPU modules are present
	 * on a CPU/Memory board, then the thermistors are not present,
	 * and the output of the A/D convertor is the max 8 bit value (0xFF)
	 */
	if (get_board_type(softsp->board) == MEM_BOARD) {
		return (DDI_SUCCESS);
	}

	/*
	 * Map in the temperature register. Once the temperature register
	 * is mapped, the timeout thread can read the temperature and
	 * update the temperature in the softsp.
	 */
	if (ddi_map_regs(softsp->dip, 0,
	    (caddr_t *)&softsp->temp_reg, 0, 0)) {
		cmn_err(CE_WARN, "environ%d: unable to map temperature "
			"register", ddi_get_instance(softsp->dip));
		return (DDI_FAILURE);
	}

	/* Initialize the temperature */
	init_temp_arrays(&softsp->tempstat);

	/*
	 * Do a priming read on the ADC, and throw away the first value
	 * read. This is a feature of the ADC hardware. After a power cycle
	 * it does not contains valid data until a read occurs.
	 */
	tmp = *(softsp->temp_reg);

	/* Wait 30 usec for ADC hardware to stabilize. */
	DELAY(30);

#ifdef lint
	tmp = tmp;
#endif

	/*
	 * Now add this soft state structure to the front of the linked list
	 * of soft state structures.
	 */
	mutex_enter(&overtemp_mutex);
	softsp->next = tempsp_list;
	tempsp_list = softsp;
	mutex_exit(&overtemp_mutex);

	/* Create kstats for this instance of the environ driver */
	environ_add_temp_kstats(softsp);

	return (DDI_SUCCESS);
}

static void
overtemp_wakeup(void)
{
	/*
	 * grab mutex to guarantee that our wakeup call
	 * arrives after we go to sleep -- so we can't sleep forever.
	 */
	mutex_enter(&overtemp_mutex);
	cv_signal(&overtemp_cv);
	mutex_exit(&overtemp_mutex);
}

/*
 * This function polls all the system board digital temperature registers
 * and stores them in the history buffers using the fhc driver support
 * routines.
 * The temperature detected must then be checked against our current
 * policy for what to do in the case of overtemperature situations. We
 * must also allow for manufacturing's use of a heat chamber.
 */
static void
environ_overtemp_poll(void)
{
	struct environ_soft_state *list;

	/* The overtemp data strcutures are protected by a mutex. */
	mutex_enter(&overtemp_mutex);

	while (environ_do_overtemp_thread) {

		/*
		 * for each environment node that has been attached,
		 * read it and check for overtemp.
		 */
		for (list = tempsp_list; list != NULL; list = list->next) {
			if (list->temp_reg == NULL) {
				continue;
			}

			update_temp(list->pdip, &list->tempstat,
				*(list->temp_reg));
		}

		/* now have this thread sleep for a while */
		(void) timeout(overtemp_wakeup, NULL, overtemp_timeout_sec*hz);

		cv_wait(&overtemp_cv, &overtemp_mutex);

	}
	mutex_exit(&overtemp_mutex);

	thread_exit();
	/* NOTREACHED */
}

void
environ_add_temp_kstats(struct environ_soft_state *softsp)
{
	struct  kstat   *tksp;

	/*
	 * Create the overtemp kstat required for the environment driver.
	 * The kstat instances are tagged with the physical board number
	 * instead of ddi instance number.
	 */
	if ((tksp = kstat_create("unix", softsp->board,
	    OVERTEMP_KSTAT_NAME, "misc", KSTAT_TYPE_RAW,
	    sizeof (struct temp_stats), KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "environ%d: temp kstat_create failed",
			ddi_get_instance(softsp->dip));
		return;
	}

	tksp->ks_update = overtemp_kstat_update;
	tksp->ks_private = (void *) &softsp->tempstat;

	kstat_install(tksp);
}
