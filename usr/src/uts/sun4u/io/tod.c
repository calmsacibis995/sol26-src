/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)tod.c 1.6 96/09/24 SMI"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/param.h>
#include <sys/ksynch.h>
#include <sys/uio.h>
#include <sys/modctl.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/open.h>
#include <sys/map.h>
#include <sys/stat.h>
#include <sys/vmmac.h>
#include <sys/clock.h>
#include <sys/tod.h>
#include <sys/todio.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <vm/hat.h>


#define	getsoftc(minor)	\
		((struct tod_softc *)ddi_get_soft_state(statep, (minor)))

/* cb_ops entry point function declarations */


static int	tod_identify(dev_info_t *);
static int	tod_attach(dev_info_t *, ddi_attach_cmd_t);
static int	tod_detach(dev_info_t *, ddi_detach_cmd_t);
static int	tod_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	tod_open(dev_t *, int, int, cred_t *);
static int	tod_close(dev_t, int, int, cred_t *);
static int	tod_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static time_t driver_tod_to_utc(todinfo_t);
static todinfo_t driver_utc_to_tod(time_t);

static int tod_driver_days_thru_month[64] = {
	0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366, 0, 0,
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365, 0, 0,
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365, 0, 0,
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365, 0, 0,
};

struct cb_ops tod_cb_ops = {
	tod_open,
	tod_close,
	nodev,
	nodev,
	nodev,			/* dump */
	nodev,
	nodev,
	tod_ioctl,
	nodev,			/* devmap */
	nodev,
	ddi_segmap,		/* segmap */
	nochpoll,
	ddi_prop_op,
	NULL,			/* for STREAMS drivers */
	D_NEW | D_MP		/* driver compatibility flag */
};

static struct dev_ops tod_ops = {
	DEVO_REV,		/* driver build version */
	0,			/* device reference count */
	tod_getinfo,
	tod_identify,
	nulldev,		/* probe */
	tod_attach,
	tod_detach,
	nulldev,		/* reset */
	&tod_cb_ops,
	(struct bus_ops *) NULL,
	nulldev			/* power */
};

/* module configuration stuff */
static void    *statep;
extern struct mod_ops mod_driverops;


static struct modldrv modldrv = {
	&mod_driverops,
	"tod driver 1.0",
	&tod_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	0
};


int
_init(void)
{
	register int    e;

	/* initialize soft state unit structure */
	if ((e = ddi_soft_state_init(&statep, sizeof (struct tod_softc),
					1)) != 0) {
		return (e);
	}
	/* do mod_install; if it fails, undo soft state init */
	if ((e = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&statep);
	}
	return (e);
}


int
_fini(void)
{
	register int e;


	/* see if we can remove; if not, return error */
	if ((e = mod_remove(&modlinkage)) != 0) {
		return (e);
	}
	/* we really can; undo _init stuff and return success */
	ddi_soft_state_fini(&statep);


	return (DDI_SUCCESS);
}


int
_info(struct modinfo * modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/* ARGSUSED */
static int
tod_getinfo(
	dev_info_t * dip,
	ddi_info_cmd_t cmd,
	void *arg,
	void **result)
{
	register dev_t	dev = (dev_t) arg;
	register int	inst = getminor(dev);
	register int	retval;
	register struct tod_softc *softc;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((softc = getsoftc(inst)) == NULL) {
			*result = (void *) NULL;
			retval = DDI_FAILURE;
		} else {
			*result = (void *) softc->dip;
			retval = DDI_SUCCESS;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *) inst;
		retval = DDI_SUCCESS;
		break;

	default:
		retval = DDI_FAILURE;
	}
	return (retval);
}


static int
tod_identify(
	dev_info_t * dip)
{

	if (strcmp(ddi_get_name(dip), "tod") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

static int
tod_attach(
	dev_info_t * dip,
	ddi_attach_cmd_t cmd)
{

	register int inst;
	register struct tod_softc *softc = NULL;
	char		name[80];

	switch (cmd) {

	case DDI_ATTACH:

		/* Get the instance of this device.  */
		inst = ddi_get_instance(dip);

		/*
		 * Create minor node.  The minor device number, inst, has no
		 * meaning.  The model number above, which will be added to
		 * the device's softc, is used to direct peculiar behavior.
		 */
		sprintf(name, "tod%d", inst);
		if (ddi_create_minor_node(dip, name, S_IFCHR, inst,
					NULL, NULL) == DDI_FAILURE) {
			ddi_remove_minor_node(dip, NULL);
			goto attach_failed;
		}
		/*
		 * Allocate a soft state structure for this particular
		 * instance.
		 */

		if (ddi_soft_state_zalloc(statep, inst) != DDI_SUCCESS)
			goto attach_failed;

		softc = getsoftc(inst);

		/*
		 * Now that we have a softc structure let's fill in some of
		 * the instance specific information.  Assigning the value of
		 * model to softc->flag zeros out the other bits.
		 */
		softc->dip = dip;

		/*
		 * The tod clock resides in the eeprom node and it's already
		 * mapped in by prom. The V_TODCLKADDR macro is defined in
		 * clock.h.
		 */
		softc->regs = (struct tod_reg *) V_TODCLKADDR;
		softc->cpr_stage = ~TOD_SUSPENDED;
		mutex_init(&softc->mutex, "TOD Driver", MUTEX_DRIVER, NULL);
		ddi_report_dev(dip);
		return (DDI_SUCCESS);

	case DDI_RESUME:
		inst = ddi_get_instance(dip);
		softc = getsoftc(inst);
		mutex_enter(&softc->mutex);
		softc->cpr_stage = ~TOD_SUSPENDED;
		mutex_exit(&softc->mutex);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	/* switch */
	}

attach_failed:

	/*
	 * Free soft state structure if it was created before failure, but
	 * first unmap any registers mapped prior to failure.
	 */
	if (softc) {
		ddi_soft_state_free(statep, inst);
	}
	/* Remove any minor nodes that were created.  */
	ddi_remove_minor_node(dip, NULL);


	return (DDI_FAILURE);
}

static int
tod_detach(
	dev_info_t * dip,
	ddi_detach_cmd_t cmd)
{
	register int inst;
	register struct tod_softc *softc;

	switch (cmd) {
	case DDI_DETACH:

		/*
		 * Bail out if no softc available.
		 */
		inst = ddi_get_instance(dip);
		if ((softc = getsoftc(inst)) == NULL)
			return (ENXIO);

		/*
		 * Unmap registers.
		 */

		mutex_destroy(&softc->mutex);
		/*
		 * Release soft state structure.
		 */
		ddi_soft_state_free(statep, inst);

		/*
		 * Remove minor nodes.
		 */
		ddi_remove_minor_node(dip, NULL);


		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		inst = ddi_get_instance(dip);
		softc = getsoftc(inst);
		mutex_enter(&softc->mutex);
		softc->cpr_stage = TOD_SUSPENDED;
		mutex_exit(&softc->mutex);
		return (DDI_SUCCESS);

	default:
		/*
		 * Need to include this in case unsupported values of cmd are
		 * added.
		 */
		return (DDI_FAILURE);

	}			/* switch */
}

/* ARGSUSED */
static int
tod_open(
	dev_t * devp,
	int flag,
	int otyp,
	cred_t * credp)
{
	register int	inst = getminor(*devp);
	register int	retval = DDI_SUCCESS;
	register	struct tod_softc *softc;

	softc = getsoftc(inst);
	if (softc == NULL)
		retval = ENXIO;
	return (retval);
}


/* ARGSUSED */
static int
tod_close(
	dev_t dev,
	int flag,
	int otyp,
	cred_t * credp)
{
	register int	inst = getminor(dev);
	register	struct tod_softc *softc;
	register int	retval = DDI_SUCCESS;


	softc = getsoftc(inst);
	if (softc == NULL)
		retval = ENXIO;

	return (retval);
}


/* ARGSUSED */
static int
tod_ioctl(
	dev_t dev,
	int cmd,
	intptr_t arg,
	int mode,
	cred_t * credp,
	int *rvalp)
{
	int		inst = getminor(dev);
	register	struct tod_softc *softc;
	todinfo_t	data_buf;
	time_t		input_time;
	char		c;


	if ((softc = getsoftc(inst)) == NULL) {
		return (ENXIO);
	}

	/* command that does not need to use ioctl arguments */
	switch (cmd) {
	case TOD_CLEAR_ALARM:
		mutex_enter(&softc->mutex);

		while (softc->cpr_stage == TOD_SUSPENDED) {
			mutex_exit(&softc->mutex);
			ddi_dev_is_needed(softc->dip, 0, 1);
			mutex_enter(&softc->mutex);
		}

		/*
		 * Comments copied from tod_set() in hardclk.c:
		 * The eeprom (which also contains the tod clock) is
		 * normally marked ro; change it to rw temporarily to
		 * update todc.
		 * This must be done every time the todc is written
		 * since the prom changes the todc mapping back to ro
		 * when it changes nvram variables (e.g. the eeprom cmd).
		 */
		mutex_enter(&tod_lock);

		hat_setattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
			PAGESIZE, PROT_WRITE);

		/* Clear intr flag by reading the register */
		c = softc->regs->flag;
#ifdef lint	/* so that lint won't complain */
		softc->regs->flag = c;
#endif
		softc->regs->intr &= ~TOD_INRT_DEFAULT; /* disable intr */

		hat_clrattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
			PAGESIZE, PROT_WRITE);
		mutex_exit(&tod_lock);

		mutex_exit(&softc->mutex);
		return (DDI_SUCCESS);

	default:
		break;
	}

	if (ddi_copyin((caddr_t)arg, (caddr_t)&input_time,
			sizeof (time_t), mode) != 0) {
		return (EFAULT);
	}

	switch (cmd) {
	case TOD_SET_ALARM:
		mutex_enter(&softc->mutex);
		while (softc->cpr_stage == TOD_SUSPENDED) {
			mutex_exit(&softc->mutex);
			ddi_dev_is_needed(softc->dip, 0, 1);
			mutex_enter(&softc->mutex);
		}

		mutex_enter(&tod_lock);
		hat_setattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
			PAGESIZE, PROT_WRITE);

		data_buf = driver_utc_to_tod(input_time);
		c = softc->regs->flag; /* clear interrupt */


		/* make sure FT bit is off */
		softc->regs->day |= ~0x40;

		softc->regs->intr &= ~TOD_INRT_DEFAULT; /* disable interrupt */
		softc->regs->alarm_date = TOD_BYTE_TO_BCD(data_buf.tod_day);
		softc->regs->alarm_hr = TOD_BYTE_TO_BCD(data_buf.tod_hour);
		softc->regs->alarm_min = TOD_BYTE_TO_BCD(data_buf.tod_min);
		softc->regs->alarm_sec = TOD_BYTE_TO_BCD(data_buf.tod_sec);
		softc->regs->intr |= TOD_INRT_DEFAULT; /* Enable interrupt */
		hat_clrattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
			PAGESIZE, PROT_WRITE);
		mutex_exit(&tod_lock);
		mutex_exit(&softc->mutex);
		break;

	case TOD_GET_DATE:
		mutex_enter(&softc->mutex);
		while (softc->cpr_stage == TOD_SUSPENDED) {
			mutex_exit(&softc->mutex);
			ddi_dev_is_needed(softc->dip, 0, 1);
			mutex_enter(&softc->mutex);
		}
		mutex_enter(&tod_lock);
		hat_setattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
			PAGESIZE, PROT_WRITE);
		softc->regs->cntl |= TOD_READ;
		data_buf.tod_year = YRBASE + TOD_BCD_TO_BYTE(softc->regs->year);
		data_buf.tod_month = TOD_BCD_TO_BYTE(softc->regs->month & 0x17);
		data_buf.tod_day = TOD_BCD_TO_BYTE(softc->regs->date & 0x3f);
		data_buf.tod_dow = TOD_BCD_TO_BYTE(softc->regs->day & 0x7);
		data_buf.tod_hour = TOD_BCD_TO_BYTE(softc->regs->hr & 0x3f);
		data_buf.tod_min = TOD_BCD_TO_BYTE(softc->regs->min & 0x7f);
		data_buf.tod_sec = TOD_BCD_TO_BYTE(softc->regs->sec & 0x7f);
		softc->regs->cntl &= ~TOD_READ;
		input_time = driver_tod_to_utc(data_buf);
		hat_clrattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
			PAGESIZE, PROT_WRITE);
		mutex_exit(&tod_lock);
		mutex_exit(&softc->mutex);
		break;

	default:
		return (EINVAL);
	}

	if (ddi_copyout((caddr_t)&input_time, (caddr_t)arg,
			sizeof (time_t), mode) != 0) {
		return (EFAULT);
	}
	return (DDI_SUCCESS);
}


/* The following are copied from ./uts/common/os/timers.c */

/*
 * Routines to convert standard UNIX time (seconds since Jan 1, 1970)
 * into year/month/day/hour/minute/second format, and back again.
 * Note: these routines require tod_lock held to protect cached state.
 */

static todinfo_t
driver_utc_to_tod(time_t utc)
{
	int dse, day, month, year;
	todinfo_t tod;

	if (utc < 0)			/* should never happen */
		utc = 0;

	dse = utc / 86400;		/* days since epoch */

	tod.tod_sec = utc % 60;
	tod.tod_min = (utc % 3600) / 60;
	tod.tod_hour = (utc % 86400) / 3600;
	tod.tod_dow = (dse + 4) % 7 + 1;	/* epoch was a Thursday */

	year = dse / 365 + 72;	/* first guess -- always a bit too large */
	do {
		year--;
		day = dse - 365 * (year - 70) - ((year - 69) >> 2);
	} while (day < 0);

	month = ((year & 3) << 4) + 1;
	while (day >= tod_driver_days_thru_month[month + 1])
		month++;

	tod.tod_day = day - tod_driver_days_thru_month[month] + 1;
	tod.tod_month = month & 15;
	tod.tod_year = year;

	return (tod);
}

static time_t
driver_tod_to_utc(todinfo_t tod)
{
	time_t utc;
	int year = tod.tod_year;
	int month = tod.tod_month + ((year & 3) << 4);


	utc = (year - 70);		/* next 3 lines: utc = 365y + y/4 */
	utc += (utc << 3) + (utc << 6);
	utc += (utc << 2) + ((year - 69) >> 2);
	utc += tod_driver_days_thru_month[month] + tod.tod_day - 1;
	utc = (utc << 3) + (utc << 4) + tod.tod_hour;	/* 24 * day + hour */
	utc = (utc << 6) - (utc << 2) + tod.tod_min;	/* 60 * hour + min */
	utc = (utc << 6) - (utc << 2) + tod.tod_sec;	/* 60 * min + sec */

	return (utc);
}
