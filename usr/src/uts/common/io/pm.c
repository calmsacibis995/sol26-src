/*
 * Copyright (c) 1991 - 1996, by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pm.c	1.44	96/10/08 SMI"


/*
 * pm	pseudo-device to support power management of all devices.
 *	Assumes: all device components wake up on &
 *		 the pm_info pointer in dev_info is NULL
 *	Not DDI complient
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/callb.h>		/* callback registration during CPR */
#include <sys/conf.h>		/* driver flags and functions */
#include <sys/open.h>		/* OTYP_CHR definition */
#include <sys/stat.h>		/* S_IFCHR definition */
#include <sys/pathname.h>	/* name -> dev_info xlation */
#include <sys/ddi_impldefs.h>	/* dev_info node fields */
#include <sys/kmem.h>		/* memory alloc stuff */
#include <sys/debug.h>
#include <sys/pm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/epm.h>
#include <sys/vfs.h>
#include <sys/mode.h>
#include <sys/mkdev.h>

#define	PM_MAXMIN	256

#define	PM_DIRECT	0x1	/* Device is power-managed directly */
#define	PM_UNCONFIG	0x2	/* Device has not been configured for autopm */
#define	PM_REMOVED	0x4	/* A direct-PM device which has been removed */

/*
 * Structure used in linked list of disabled auto power management
 * devices.
 */
typedef struct pm_dis_dev_info {
	struct pm_dis_dev_info	*next;
	minor_t			minor;
	dev_info_t		dip;
} pm_dis_dev_info_t;

/*
 * The soft state of the power manager
 */
typedef struct {
	dev_info_t	*pm_dip;
	/*
	 * Protects driver from deleting nodes while it is
	 * using them.  It is used when either driver threads
	 * or user threads (ioctls) enter this driver.
	 */
	krwlock_t	pm_unload;
	/*
	 * Single threads scans and accesses to scan schedule
	 */
	kmutex_t	pm_scan;
	int		pm_scan_id;
	callb_id_t	pm_callb_id;
	int		pm_schedule;	/* time between scans in ticks */
	int		pm_devices;	/* number of managed devices */
	u_char		pm_minors[PM_MAXMIN];
	pm_dis_dev_info_t	*pm_dis_dev_list;
} pm_unit;

typedef struct {
	kmutex_t	node_lock;	/* For this data structure */
	int		components;	/* Number of components in device */
	int		*threshold;	/* Thresholds */
	int		*cmpt_pwr;	/* Current power levels */
	u_int		dev_pm_state;	/* PM state of a device */
	int		dependents;	/* Number of dependents */
	dev_info_t	*dependent;	/* if any */
	int		dependees;	/* Number of dependees */
	dev_info_t	*dependee;
} pm_info_t;

/* Global variables for driver */
static void	*pm_state;
static int	pm_instance = -1;

extern int	nulldev();
extern int	nodev();
extern int	hz;

static int	pm_open(dev_t *, int, int, cred_t *);
static int	pm_close(dev_t, int, int, cred_t *);
static int	pm_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int	e_ddi_get_num_components(dev_info_t *dip);
static void	e_ddi_set_pm_info(dev_info_t *dip, void *value);
static void *	e_ddi_get_pm_info(dev_info_t *dip);
static int	pm_get_timestamps(dev_info_t *dip, time_t **valuep,
			int *length);
static int	pm_get_prop(dev_info_t *dip, char *name, void *valuep,
			int *lengthp);

static struct cb_ops pm_cb_ops = {
	pm_open,	/* open */
	pm_close,	/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	pm_ioctl,	/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,	/* prop_op */
	NULL,		/* streamtab */
	D_NEW | D_MP	/* driver compatibility flag */
};

static int pm_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int pm_identify(dev_info_t *dip);
static int pm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int pm_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

static struct dev_ops pm_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	pm_getinfo,		/* info */
	pm_identify,		/* identify */
	nulldev,		/* probe */
	pm_attach,		/* attach */
	pm_detach,		/* detach */
	nodev,			/* reset */
	&pm_cb_ops,		/* driver operations */
	(struct bus_ops *)NULL,	/* bus operations */
	NULL			/* power */
};


static struct modldrv modldrv = {
	&mod_driverops,
	"power manager driver v1.44",
	&pm_ops
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, 0
};

/* Local functions */
static void		dev_is_needed(dev_info_t *, int, int);
static void		start_scan();
static int		scan(dev_info_t *);
static int		power_up(dev_info_t *, int, int, pm_info_t *);
static int		power_dev(dev_info_t *, int, int, pm_info_t *);
static int		reset_info(dev_info_t *, void *);
static int		rem_info(dev_info_t *, void *);
static pm_info_t	*add_info(dev_info_t *, pm_unit *);
static dev_info_t	*name_to_dip(char *);
static dev_info_t	*find_dip(dev_info_t *, char *, int);
static void		pm_callb(void *, int);
static int		pm_get_norm_pwrs(dev_info_t *, u_int **, int *);
static int		pm_check_permission(char *, cred_t *);
#ifdef DEBUG
static int		print_info(dev_info_t *, void *);
#endif

int
_init(void)
{
	int	e;

	ddi_soft_state_init(&pm_state, sizeof (pm_unit), 1);
	if ((e = mod_install(&modlinkage)) != 0)  {
		ddi_soft_state_fini(&pm_state);
	}

	return (e);
}

int
_fini(void)
{
	int	e;

	if ((e = mod_remove(&modlinkage)) == 0)  {
		ddi_soft_state_fini(&pm_state);
	}
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
pm_identify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "pm") == 0) {
		return (DDI_IDENTIFIED);
	} else
		return (DDI_NOT_IDENTIFIED);
}

static int
pm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	pm_unit	*unitp;
	void			(*fn)();

	switch (cmd) {

	case DDI_ATTACH:
		if (pm_instance != -1)		/* Only allow one instance */
			return (DDI_FAILURE);
		fn = dev_is_needed;
		if (ddi_prop_create(DDI_DEV_T_NONE, ddi_root_node(),
		    DDI_PROP_CANSLEEP, "pm-driver", (caddr_t)&fn, sizeof (fn))
		    != DDI_PROP_SUCCESS)
			return (DDI_FAILURE);

		pm_instance = ddi_get_instance(dip);
		if (ddi_soft_state_zalloc(pm_state, pm_instance) != DDI_SUCCESS)
			return (DDI_FAILURE);

		unitp = ddi_get_soft_state(pm_state, pm_instance);
		unitp->pm_dip = dip;
		unitp->pm_schedule = 5 * hz;	/* Scan every 5 seconds */
		unitp->pm_devices = 0;
		mutex_init(&unitp->pm_scan, "pm scan", MUTEX_DRIVER, NULL);
		rw_init(&unitp->pm_unload, "pm unload", RW_DRIVER, NULL);

		if (ddi_create_minor_node(dip, "pm", S_IFCHR, pm_instance,
		    DDI_PSEUDO, 0) != DDI_SUCCESS) {
			ddi_soft_state_free(pm_state, pm_instance);
			return (DDI_FAILURE);
		}
		ddi_report_dev(dip);
		unitp->pm_callb_id = callb_add(pm_callb, (void *)unitp,
		    CB_CL_CPR_DAEMON, "pm");
		unitp->pm_scan_id = timeout(start_scan, (caddr_t)unitp,
		    unitp->pm_schedule);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
pm_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	pm_unit	*unitp;

	switch (cmd) {

	case DDI_DETACH:
		unitp = ddi_get_soft_state(pm_state, pm_instance);
		/*
		 * Wait for external threads to exit driver
		 */
		rw_enter(&unitp->pm_unload, RW_WRITER);

		if (unitp->pm_devices) {
			rw_exit(&unitp->pm_unload);
			return (DDI_FAILURE);
		}

		/*
		 * Cancel scan or wait for scan in progress to finish
		 */
		if (unitp->pm_schedule)
			(void) untimeout(unitp->pm_scan_id);
		callb_delete(unitp->pm_callb_id);
		if (ddi_prop_remove(DDI_DEV_T_NONE, ddi_root_node(),
		    "pm-driver") != DDI_PROP_SUCCESS)
			cmn_err(CE_WARN, "pm: Can't remove property");
		ddi_remove_minor_node(dip, NULL);
		rw_exit(&unitp->pm_unload);
		mutex_destroy(&unitp->pm_scan);
		rw_destroy(&unitp->pm_unload);
		ddi_soft_state_free(pm_state, pm_instance);
		pm_instance = -1;
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
pm_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int	instance;
	pm_unit	*unitp;
	dev_t	dev;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dev = (dev_t)arg;
		instance = getminor(dev);
		if ((unitp = ddi_get_soft_state(pm_state, instance)) == NULL)
			return (DDI_FAILURE);
		*result = unitp->pm_dip;
		return (DDI_SUCCESS);

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t)arg);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}


/*ARGSUSED1*/
static int
pm_open(dev_t *devp, int flag, int otyp, cred_t *cr)
{
	int		minor;
	pm_unit		*unitp;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	if ((unitp = ddi_get_soft_state(pm_state, pm_instance)) == NULL)
		return (ENXIO);

	rw_enter(&unitp->pm_unload, RW_WRITER);
	for (minor = 0; minor < PM_MAXMIN; minor++)
		if (!unitp->pm_minors[minor])
			break;

	if (minor == PM_MAXMIN) {
		rw_exit(&unitp->pm_unload);
		return (ENXIO);
	}

	*devp = makedevice(getmajor(*devp), minor);
	unitp->pm_minors[minor] = 1;
	rw_exit(&unitp->pm_unload);

	return (0);
}

/*ARGSUSED1*/
static int
pm_close(dev_t dev, int flag, int otyp, cred_t *cr)
{
	minor_t		minor_dev;
	pm_unit		*unitp;
	pm_info_t	*info;
	pm_dis_dev_info_t	*cur_dis_dev, *last_dis_dev;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	if ((unitp = ddi_get_soft_state(pm_state, pm_instance)) == NULL)
		return (ENXIO);

	minor_dev = getminor(dev);
	rw_enter(&unitp->pm_unload, RW_WRITER);
	cur_dis_dev = unitp->pm_dis_dev_list;
	last_dis_dev = cur_dis_dev;
	while (cur_dis_dev != NULL) {
		if (minor_dev == cur_dis_dev->minor) {
			info = e_ddi_get_pm_info(cur_dis_dev->dip);
			mutex_enter(&info->node_lock);
			info->dev_pm_state &= ~PM_DIRECT;
			if (info->dev_pm_state & (PM_UNCONFIG|PM_REMOVED)) {
				mutex_exit(&info->node_lock);
				mutex_enter(&unitp->pm_scan);
				(void) rem_info(cur_dis_dev->dip, unitp);
				mutex_exit(&unitp->pm_scan);
			} else
				mutex_exit(&info->node_lock);

			last_dis_dev->next = cur_dis_dev->next;
			if (unitp->pm_dis_dev_list == cur_dis_dev)
				unitp->pm_dis_dev_list = last_dis_dev =
				    cur_dis_dev->next;
			kmem_free(cur_dis_dev, sizeof (pm_dis_dev_info_t));
			if (last_dis_dev == NULL)
				break;
			cur_dis_dev = (last_dis_dev->next) ?
				last_dis_dev->next : last_dis_dev;
			continue;
		}
		last_dis_dev = cur_dis_dev;
		cur_dis_dev = cur_dis_dev->next;
	}

	unitp->pm_minors[minor_dev] = 0;
	rw_exit(&unitp->pm_unload);

	return (0);
}

/*ARGSUSED*/
static int
pm_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cr, int *rval_p)
{
	pm_unit		*unitp;
	pm_request	req;
	dev_info_t	*dip = NULL, *dep, *new_dep;
	pm_info_t	*info, *dep_info;
	pm_dis_dev_info_t	*cur_dis_dev, *last_dis_dev, *new_dis_dev;
	ulong_t		timeval;
	time_t		*timestamp;
	minor_t		minor;
	u_int		*power;
	int		length;
	int		i, j, k, ret = 0;
	int		found = 0;
	void		*propval;

	if ((unitp = ddi_get_soft_state(pm_state, pm_instance)) == NULL)
		return (ENXIO);

#ifdef DEBUG
	if (cmd == 666) {
		ddi_walk_devs(ddi_root_node(), print_info, NULL);
		return (0);
	}
#endif
	if (cmd == PM_SCHEDULE) {
		if (arg >= 0 && !suser(cr))
			return (EPERM);
		mutex_enter(&unitp->pm_scan);
		if (arg >= 0 && unitp->pm_schedule) {
			unitp->pm_schedule = 0;
			mutex_exit(&unitp->pm_scan);
			(void) untimeout(unitp->pm_scan_id);
			mutex_enter(&unitp->pm_scan);
		}
		if (arg > 0 && !unitp->pm_schedule) {
			unitp->pm_schedule = arg * hz;
			unitp->pm_scan_id = timeout(start_scan,
			    (caddr_t)unitp, unitp->pm_schedule);
		}
		*rval_p = unitp->pm_schedule / hz;
		mutex_exit(&unitp->pm_scan);
		return (0);
	}
	if (cmd == PM_REM_DEVICES) {
		if (!suser(cr))
			return (EPERM);
		rw_enter(&unitp->pm_unload, RW_WRITER);
		mutex_enter(&unitp->pm_scan);
		ddi_walk_devs(ddi_root_node(), rem_info, unitp);
		mutex_exit(&unitp->pm_scan);
		rw_exit(&unitp->pm_unload);
		return (0);
	}

	/*
	 * Prepare arguments
	 * Get the pm info structure for this node and lock it
	 */
	if (copyin((caddr_t)arg, (caddr_t)&req, sizeof (req)) != 0)
		return (EFAULT);
	if (!(dip = name_to_dip(req.who)))
		return (ENODEV);

	if (cmd == PM_REPARSE_PM_PROPS) {
		/*
		 * This ioctl is provided only for the ddivs pm test.
		 * We only do it to a driver which explicitly allows us to
		 * do so by exporting a pm-reparse-ok property
		 */
		if (pm_get_prop(dip, "pm-reparse-ok", &propval, &length) !=
		    DDI_SUCCESS) {
			return (EINVAL);
		}
		kmem_free(propval, length);
		if (e_pm_props(dip) == DDI_SUCCESS)
			return (0);
		else
			return (EINVAL);
	}

	if (cmd == PM_DISABLE_AUTOPM) {
		if (ret = pm_check_permission(req.who, cr))
			return (ret);

		minor = getminor(dev);
		rw_enter(&unitp->pm_unload, RW_WRITER);
		if ((info = e_ddi_get_pm_info(dip)) == NULL) {
			if ((info = add_info(dip, unitp)) == NULL) {
				rw_exit(&unitp->pm_unload);
				return (ENODEV);
			}
			mutex_enter(&info->node_lock);
			info->dev_pm_state |= PM_UNCONFIG;
			mutex_exit(&info->node_lock);
		}

		cur_dis_dev = unitp->pm_dis_dev_list;
		last_dis_dev = cur_dis_dev;
		while (cur_dis_dev != NULL) {
			if (dip == cur_dis_dev->dip) {
				found = 1;
				break;
			}
			last_dis_dev = cur_dis_dev;
			cur_dis_dev = cur_dis_dev->next;
		}

		if (!found) {
			new_dis_dev = kmem_zalloc(sizeof (pm_dis_dev_info_t),
							KM_SLEEP);
			new_dis_dev->minor = minor;
			new_dis_dev->dip = dip;
			if (unitp->pm_dis_dev_list == NULL)
				unitp->pm_dis_dev_list = new_dis_dev;
			else
				last_dis_dev->next = new_dis_dev;

			mutex_enter(&info->node_lock);
			info->dev_pm_state |= PM_DIRECT;
			mutex_exit(&info->node_lock);
		} else
			ret = EBUSY;

		rw_exit(&unitp->pm_unload);
		return (ret);
	}

	if (cmd == PM_REM_DEVICE)
		rw_enter(&unitp->pm_unload, RW_WRITER);
	else
		rw_enter(&unitp->pm_unload, RW_READER);


	if (!(info = e_ddi_get_pm_info(dip)) &&
	    (cmd != PM_SET_THRESHOLD || !(info = add_info(dip, unitp)))) {
		rw_exit(&unitp->pm_unload);
		return (ENODEV);
	}

	/*
	 * Now do the real work
	 */
	mutex_enter(&info->node_lock);
	switch (cmd) {
	case PM_GET_IDLE_TIME:
		if (0 > req.select || req.select >= info->components) {
			ret = EINVAL;
			break;
		}
		if (pm_get_timestamps(dip, &timestamp, &length) !=
		    DDI_SUCCESS) {
			cmn_err(CE_WARN, "pm: Can't get timestamps from %s%s",
				ddi_binding_name(dip), ddi_get_name_addr(dip));
			ret = EIO;
			break;
		}
		if (timestamp[req.select]) {
			drv_getparm(TIME, &timeval);
			*rval_p = (timeval - timestamp[req.select]);
		} else
			*rval_p = 0;
		kmem_free(timestamp, length);
		break;

	case PM_GET_NUM_CMPTS:
		*rval_p = info->components;
		break;

	case PM_GET_THRESHOLD:
		if (0 > req.select || req.select >= info->components) {
			ret = EINVAL;
			break;
		}
		*rval_p = info->threshold[req.select];
		break;

	case PM_SET_THRESHOLD:
		if (!suser(cr)) {
			ret = EPERM;
			break;
		}
		if (0 > req.select || req.select >= info->components) {
			ret = EINVAL;
			break;
		}
		info->dev_pm_state &= ~(PM_UNCONFIG|PM_REMOVED);
		info->threshold[req.select] =
		    (req.level < 0) ? INT_MAX : req.level;
		*rval_p = 0;
		break;

	case PM_SET_NORM_PWR:
		if (ret = pm_check_permission(req.who, cr))
			break;

		if (0 > req.select || req.select >= info->components ||
		    req.level <= 0) {
			ret = EINVAL;
			break;
		}
		pm_set_normal_power(dip, req.select, req.level);
		*rval_p = 0;
		break;

	case PM_GET_NORM_PWR:
		if (0 > req.select || req.select >= info->components) {
			ret = EINVAL;
			break;
		}
		if (pm_get_norm_pwrs(dip, &power, &length) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "pm: Can get normal power values for "
			    "%s%s", ddi_binding_name(dip),
			    ddi_get_name_addr(dip));
			ret = EIO;
			break;
		}
		*rval_p = power[req.select];
		kmem_free(power, length);
		break;

	case PM_SET_CUR_PWR:
		if (ret = pm_check_permission(req.who, cr))
			break;

		if (0 > req.select || req.select >= info->components ||
		    req.level < 0) {
			ret = EINVAL;
			break;
		}
		if (info->cmpt_pwr[req.select] == req.level)
			break;

		if (!power_up(dip, req.select, req.level, info))
			ret =  EINVAL;
		*rval_p = 0;
		break;

	case PM_GET_CUR_PWR:
		if (0 > req.select || req.select >= info->components) {
			ret = EINVAL;
			break;
		}
		*rval_p = info->cmpt_pwr[req.select];
		break;

	case PM_GET_NUM_DEPS:
		*rval_p = info->dependents;
		break;

	case PM_GET_DEP: {
		char	buffer[MAXNAMELEN];

		if (0 > req.select || req.select >= info->dependents) {
			ret = EINVAL;
			break;
		}
		dep = info->dependent[req.select];
		(void) sprintf(buffer, "%s@%s", ddi_node_name(dep),
		    ddi_get_name_addr(dep));
		if (strlen(buffer) >= req.size)
			ret = EINVAL;
		else if (copyout(buffer, req.dependent, strlen(buffer)+1) != 0)
			ret = EFAULT;
		*rval_p = 0;
		break;
	}
	case PM_ADD_DEP:
		if (!suser(cr)) {
			ret = EPERM;
			break;
		}
		if (!(dep = name_to_dip(req.dependent)) ||
		    !(dep_info = e_ddi_get_pm_info(dep))) {
			ret = ENODEV;
			break;
		}
		for (i = 0; i != info->dependents &&
		    info->dependent[i] != dep; i++);
		if (i != info->dependents) { 		/* Found */
			ret = EBUSY;
			break;
		}

		length = info->dependents*sizeof (dev_info_t *);
		new_dep = kmem_alloc(length+sizeof (dev_info_t *), KM_SLEEP);
		bcopy((char *)info->dependent, (char *)new_dep, length);
		new_dep[i] = dep;
		kmem_free(info->dependent, length);
		info->dependents++;
		info->dependent = new_dep;

		length = (i = dep_info->dependees)*sizeof (dev_info_t *);
		new_dep = kmem_alloc(length+sizeof (dev_info_t *), KM_SLEEP);
		bcopy((char *)dep_info->dependee, (char *)new_dep, length);
		new_dep[i] = dip;
		kmem_free(dep_info->dependee, length);
		dep_info->dependees++;
		dep_info->dependee = new_dep;
		*rval_p = 0;
		break;

	case PM_REENABLE_AUTOPM:
		if (ret = pm_check_permission(req.who, cr))
			break;

		minor = getminor(dev);
		cur_dis_dev = unitp->pm_dis_dev_list;
		last_dis_dev = cur_dis_dev;
		while (cur_dis_dev != NULL) {
			if ((minor == cur_dis_dev->minor) &&
					(dip == cur_dis_dev->dip)) {
				found = 1;
				break;
			}
			last_dis_dev = cur_dis_dev;
			cur_dis_dev = cur_dis_dev->next;
		}

		if (found) {
			rw_exit(&unitp->pm_unload);
			rw_enter(&unitp->pm_unload, RW_WRITER);
			last_dis_dev->next = cur_dis_dev->next;
			if (unitp->pm_dis_dev_list == cur_dis_dev)
				unitp->pm_dis_dev_list = cur_dis_dev->next;
			kmem_free(cur_dis_dev, sizeof (pm_dis_dev_info_t));

			info->dev_pm_state &= ~PM_DIRECT;
			if (info->dev_pm_state & (PM_UNCONFIG|PM_REMOVED)) {
				mutex_exit(&info->node_lock);
				mutex_enter(&unitp->pm_scan);
				(void) rem_info(dip, unitp);
				mutex_exit(&unitp->pm_scan);
				rw_exit(&unitp->pm_unload);
				return (0);
			}
		} else
			ret = EINVAL;

		break;

	case PM_REM_DEP:
		if (!suser(cr)) {
			ret = EPERM;
			break;
		}
		if (!(dep = name_to_dip(req.dependent)) ||
		    !(dep_info = e_ddi_get_pm_info(dep))) {
			ret = ENODEV;
			break;
		}
		if (!info->dependents) {
			ret = EIO;
			break;
		}
		length = info->dependents * sizeof (dev_info_t *);
		new_dep = kmem_alloc(length-sizeof (dev_info_t *), KM_SLEEP);
		for (i = 0, j = 0; j < info->dependents-1; i++) {
			ASSERT(i < info->dependents);
			if (info->dependent[i] == dep)
				continue;
			new_dep[j++] = info->dependent[i];
		}
		if (i == j && info->dependent[i] != dep) {
			/* Not found */
			kmem_free(new_dep, length-sizeof (dev_info_t *));
			ret = ENODEV;
			break;
		}
		kmem_free(info->dependent, length);
		info->dependents--;
		info->dependent = new_dep;

		length = dep_info->dependees*sizeof (dev_info_t *);
		new_dep = kmem_alloc(length-sizeof (dev_info_t *), KM_SLEEP);
		for (i = 0, j = 0; j < dep_info->dependees - 1; i++) {
			ASSERT(i < dep_info->dependees);
			if (dep_info->dependee[i] == dip)
				continue;
			new_dep[j++] = dep_info->dependee[i];
		}
		kmem_free(dep_info->dependee, length);
		dep_info->dependees--;
		dep_info->dependee = new_dep;
		*rval_p = 0;
		break;

	case PM_REM_DEVICE:
		if (!suser(cr)) {
			ret = EPERM;
			break;
		}
		if (info->dependees) {
			ret = EBUSY;
			break;
		}
		mutex_exit(&info->node_lock);
		mutex_enter(&unitp->pm_scan);
		for (k = 0; k < info->dependents; k++) {
			dep_info = e_ddi_get_pm_info(info->dependent[k]);
			length = dep_info->dependees * sizeof (dev_info_t *);
			new_dep = kmem_alloc(length - sizeof (dev_info_t *),
			    KM_SLEEP);
			for (i = 0, j = 0; j < dep_info->dependees-1; i++) {
				ASSERT(i < dep_info->dependees);
				if (dep_info->dependee[i] == dip)
					continue;
				new_dep[j++] = dep_info->dependee[i];
			}
			kmem_free(dep_info->dependee, length);
			dep_info->dependees--;
			dep_info->dependee = new_dep;
		}
		(void) rem_info(dip, unitp);
		mutex_exit(&unitp->pm_scan);
		rw_exit(&unitp->pm_unload);
		return (ret);

	default:
		ret = ENOTTY;
	}
	mutex_exit(&info->node_lock);
	rw_exit(&unitp->pm_unload);
	return (ret);
}

/*
 * Entry point for drivers. Exported by property "pm-driver"
 * Will not turn a component off ('cos need to check dependencies).
 */
static void
dev_is_needed(dev_info_t *who, int cmpt, int level)
{
	pm_info_t	*info;
	pm_unit		*unitp;

	ASSERT(level > 0);
	unitp = ddi_get_soft_state(pm_state, pm_instance);
	rw_enter(&unitp->pm_unload, RW_READER);
	/*
	 * Check if the power manager is active and if the
	 * the device is power managed
	 * if not, the device must be on.
	 */
	info = e_ddi_get_pm_info(who);
	if (!info) {
		rw_exit(&unitp->pm_unload);
		return;
	}
	mutex_enter(&info->node_lock);
	if (cmpt < 0 || cmpt >= info->components ||
	    info->cmpt_pwr[cmpt] == level) {
		mutex_exit(&info->node_lock);
		rw_exit(&unitp->pm_unload);
		return;
	}
	/* Let's do it */
	if (!power_up(who, cmpt, level, info)) {
		cmn_err(CE_WARN, "Device %s:%d failed to power up.",
		    ddi_node_name(who), ddi_get_instance(who));
		cmn_err(CE_WARN,
		    "Please see your system administrator or reboot.");
	}
	mutex_exit(&info->node_lock);
	rw_exit(&unitp->pm_unload);
}

static void
start_scan(pm_unit *unitp)
{

	mutex_enter(&unitp->pm_scan);
	(void) scan(ddi_root_node());
	if (unitp->pm_schedule)
		unitp->pm_scan_id = timeout(start_scan, (caddr_t)unitp,
		    unitp->pm_schedule);
	mutex_exit(&unitp->pm_scan);
}

/*
 * scan:	This function scans the dev_info tree for idling devices and
 *		attempts to power them down.
 */
static int
scan(dev_info_t *dip)
{
	int		phys_dep = 0;	/* True, if dependency exists */
	unsigned long	timeval;
	time_t		*timestamp;
	int		i, j, size;
	pm_info_t	*info, *dep_info;

	for (; dip != NULL; dip = ddi_get_next_sibling(dip)) {
		if (!ddi_get_driver(dip))
			continue;
		info = e_ddi_get_pm_info(dip);
		if (info && !mutex_tryenter(&info->node_lock)) {
			phys_dep = 1;
			continue;
		}
		if (scan(ddi_get_child(dip)) || !info) {
			if (info)
				mutex_exit(&info->node_lock);
			phys_dep = 1;
			continue;
		}
		if (info->dev_pm_state & PM_DIRECT) {
			phys_dep = 1;
			mutex_exit(&info->node_lock);
			continue;
		}
		/*
		 * Check for logical deps
		 */
		for (i = 0; i < info->dependents; i++) {
			dep_info = e_ddi_get_pm_info(info->dependent[i]);
			if (!dep_info) {
				cmn_err(CE_WARN, "pm: Null pm_info for %s%s",
				    ddi_binding_name(dip),
				    ddi_get_name_addr(dip));
				break;
			}
			for (j = 0; j < dep_info->components; j++) {
				if (dep_info->cmpt_pwr[j] != 0)
					break;
			}
			if (j != dep_info->components)
				break;
		}
		if (i != info->dependents) {
			phys_dep = 1;
			mutex_exit(&info->node_lock);
			continue;
		}
		/*
		 * Check each component
		 */
		drv_getparm(TIME, &timeval);
		if (pm_get_timestamps(dip, &timestamp, &size) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "pm: Can't get timestamps from %s%s",
				ddi_binding_name(dip), ddi_get_name_addr(dip));
			mutex_exit(&info->node_lock);
			continue;
		}
		ASSERT(info->components == (size / (sizeof (ulong_t))));
		for (i = 0; i < info->components; i++) {
			if (info->cmpt_pwr[i] == 0)
				continue;		/* Already off */
			if ((timeval - timestamp[i]) < info->threshold[i] ||
			    !timestamp[i] || !power_dev(dip, i, 0, info))
				phys_dep = 1;
		}
		kmem_free(timestamp, size);
		mutex_exit(&info->node_lock);
	}
	return (phys_dep);
}

/*
 * power_up:	turns device on, and all dependents
 *		assumes device is power manageable & level is non-zero
 *		& component exists.
 */
static int
power_up(dev_info_t *who, int component, int level, pm_info_t *info)
{
	dev_info_t	*parent, *dependee;
	u_int		*power;
	int		size;
	int		i, j;
	pm_info_t	*par_info, *dep_info;

	parent = ddi_get_parent(who);
	if (parent && (par_info = e_ddi_get_pm_info(parent))) {
		mutex_enter(&par_info->node_lock);
		if (!(par_info->dev_pm_state & PM_DIRECT) &&
		    (pm_get_norm_pwrs(parent, &power, &size) == DDI_SUCCESS)) {
			for (i = 0; i < par_info->components; i++) {
				if (!par_info->cmpt_pwr[i] &&
				    !power_up(parent, i, power[i], par_info))
					break;
			}
			kmem_free(power, size);
			if (i != par_info->components) {
				mutex_exit(&par_info->node_lock);
				return (0);
			}
		}
		mutex_exit(&par_info->node_lock);
	}
	for (i = 0; i < info->dependees; i++) {
		dependee = info->dependee[i];
		dep_info = e_ddi_get_pm_info(dependee);
		mutex_enter(&dep_info->node_lock);
		if (!(dep_info->dev_pm_state & PM_DIRECT)) {
			if (pm_get_norm_pwrs(dependee, &power, &size) !=
			    DDI_SUCCESS) {
				cmn_err(CE_WARN, "pm: Can't get normal power "
				    "values for %s%s",
				    ddi_binding_name(dependee),
				    ddi_get_name_addr(dependee));
				mutex_exit(&dep_info->node_lock);
				continue;
			}
			for (j = 0; j < dep_info->components; j++) {
				if (!dep_info->cmpt_pwr[j] &&
				    !power_up(dependee, j, power[j], dep_info))
					break;
			}
			kmem_free(power, size);
			if (j != dep_info->components) {
				cmn_err(CE_WARN, "pm: Can't awaken dependee");
				mutex_exit(&dep_info->node_lock);
				break;
			}
		}
		mutex_exit(&dep_info->node_lock);
	}
	if (i != info->dependees)
		return (0);
	/*
	 * Here we handle the implied dependency on component 0
	 */
	if (component != 0) {
		ASSERT(mutex_owned(&info->node_lock));
		if (info->cmpt_pwr[0] == 0) {
			if (pm_get_norm_pwrs(who,
			    &power, &size) != DDI_SUCCESS) {
				cmn_err(CE_WARN, "pm: Can't get normal power "
				    "values for %s%s", ddi_binding_name(who),
				    ddi_get_name_addr(who));
				return (0);
			}
			power_dev(who, 0, power[0], info);
			kmem_free(power, size);
		}
	}
	return (power_dev(who, component, level, info));
}

/*
 * Powers a device, suspending or resuming the device if necessary
 */
static int
power_dev(dev_info_t *dip, int component, int level, pm_info_t *info)
{
	struct dev_ops	*ops;
	int		(*fn)(dev_info_t *, int, int);
	int		power_op_ok;
	int		old_level = 0, resume_needed = 0;

	if (!(ops = ddi_get_driver(dip)))
		return (0);
	if ((ops->devo_rev < 2) || !(fn = ops->devo_power))
		return (0);

	/*
	 * If this is component 0 and we are going to take the
	 * power away, we need to detach it with DDI_PM_SUSPEND command.
	 */
	if (component == 0 && level == 0 &&
	    devi_detach(dip, DDI_PM_SUSPEND) != DDI_SUCCESS) {
		/* We could not suspend before turning cmpt zero off */
		return (0);
	}

	if ((*fn)(dip, component, level) == DDI_SUCCESS) {
		old_level = info->cmpt_pwr[component];
		info->cmpt_pwr[component] = level;
		power_op_ok = 1;
	} else {
		cmn_err(CE_WARN, "pm: Can't set power level of %s to "
			"level %d.", ddi_node_name(dip), level);
		power_op_ok = 0;
	}


	/*
	 * We will have to resume the device if either of the following cases
	 * is true:
	 * case 1:
	 *	This is component 0 and we have successfully powered it up
	 *	from 0 to a non-zero level.
	 * case 2:
	 *	This is component 0 and we have failed to power it down from
	 *	a non-zero level to 0. Resume is needed because we have just
	 *	detached it with DDI_PM_SUSPEND command.
	 */
	if (component == 0) {
		if (power_op_ok) {
			if (old_level == 0 && level != 0)
				resume_needed = 1;
		} else {
			if (old_level != 0 && level == 0)
				resume_needed = 1;
		}
	}

	if (resume_needed && devi_attach(dip, DDI_RESUME) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "pm: Can't resume %s", ddi_node_name(dip));
		power_op_ok = 0;
	}

	return (power_op_ok);
}

/*
 * This function recognizes names in two forms and returns the corresponding
 * dev-info node.  The name must be either a pathname leading to a special
 * file, or it must specify the device in its name@address form
 * (cf. /etc/path_to_inst). The name can include the parent(s) name to
 * provide uniqueness.  The first matched name is returned.
 */
static dev_info_t *
name_to_dip(char *pathname)
{
	dev_info_t	*dip;
	vnode_t		*vp;
	char		dev_name[MAXNAMELEN];
	size_t		len;
	major_t		major;

	if (copyinstr(pathname, dev_name, MAXNAMELEN, &len))
		return (NULL);

	if (dev_name[0] == '/')
		dip = find_dip(ddi_get_child(ddi_root_node()), dev_name + 1, 0);
	else
		dip = find_dip(ddi_root_node(), dev_name, 1);
	if (dip)
		return (dip);

	if (lookupname(dev_name, UIO_SYSSPACE, FOLLOW, NULL, &vp))
		return (NULL);

	if (vp->v_type != VCHR && vp->v_type != VBLK) {
		VN_RELE(vp);
		return (NULL);
	}

	major = getmajor(vp->v_rdev);
	if (ddi_hold_installed_driver(major)) {
		if (dip = dev_get_dev_info(vp->v_rdev, 0))
			ddi_rele_driver(major);
		ddi_rele_driver(major);
	}

	VN_RELE(vp);
	return (dip);
}

static dev_info_t *
find_dip(dev_info_t *dip, char *device, int full_search)
{
	dev_info_t	*ndip = NULL;
	major_t		major;
	char		*child_dev, *addr;
	int		name_len, addr_len;

	for (; dip != NULL; dip = ddi_get_next_sibling(dip)) {
		addr = strchr(device, '@');
		child_dev = strchr(device, '/');
		if ((addr != NULL) &&
		    (child_dev == NULL || addr < child_dev)) {
			/*
			 * We have device = "name@addr..." form
			 */
			name_len = addr - device;
			addr += 1;			/* Skip '@' */
			if (child_dev != NULL) {
				addr_len = child_dev - addr;
				child_dev += 1;
			} else
				addr_len = strlen(addr);
		} else {
			/*
			 * We have device = "name/..." or "name"
			 */
			addr = "";
			addr_len = 1;
			if (child_dev != NULL) {
				name_len = child_dev - device;
				child_dev += 1;		/* Skip '/' */
			} else
				name_len = strlen(device);
		}
		if (strncmp(ddi_node_name(dip), device, name_len) == 0) {
			/* load driver, if necessary */
			if (!ddi_get_name_addr(dip)) {
				major = ddi_name_to_major(ddi_get_name(dip));
				if (ddi_hold_installed_driver(major))
					ddi_rele_driver(major);
			}
			if (ddi_get_name_addr(dip) && !strncmp
			    (ddi_get_name_addr(dip), addr, addr_len))
				return (child_dev == NULL ? dip :
				    find_dip(ddi_get_child(dip), child_dev, 0));
		}
		if (full_search) {
			ndip = find_dip(ddi_get_child(dip), device, 1);
			if (ndip)
				return (ndip);
		}
	}
	return (NULL);
}

static pm_info_t *
add_info(dev_info_t *dip, pm_unit *unitp)
{
	pm_info_t	*info;
	u_int		*norm_pwr;
	int		cmpts, i, length;
	major_t		major;

	ASSERT(e_ddi_get_pm_info(dip) == NULL);
	major = ddi_name_to_major(ddi_get_name(dip));
	if (ddi_hold_installed_driver(major) == NULL)
		return (NULL);
	if (pm_get_norm_pwrs(dip, &norm_pwr, &length) != DDI_SUCCESS) {
		ddi_rele_driver(major);
		return (NULL);
	}
	cmpts = length >> 2;
	info = kmem_zalloc(sizeof (pm_info_t), KM_SLEEP);
	info->components = cmpts;
	info->threshold = kmem_zalloc(cmpts*sizeof (int), KM_SLEEP);
	info->cmpt_pwr = kmem_zalloc(cmpts*sizeof (int), KM_SLEEP);
	for (i = 0; i < cmpts; i++) {
		info->threshold[i] = INT_MAX;
		info->cmpt_pwr[i] = norm_pwr[i];
	}
	kmem_free(norm_pwr, length);
	mutex_init(&info->node_lock, "pm node mutex", MUTEX_DRIVER, NULL);
	e_ddi_set_pm_info(dip, info);
	unitp->pm_devices++;
	return (info);
}

static int
rem_info(dev_info_t *dip, void *arg)
{
	pm_unit		*unitp = (pm_unit *)arg;
	u_int		*level;
	int		i, size = 0;
	pm_info_t	*pm;
	major_t		major;

	pm = e_ddi_get_pm_info(dip);
	if (pm) {
		if (pm->dev_pm_state & PM_DIRECT) {
			pm->dev_pm_state |= PM_REMOVED;
			kmem_free(pm->dependent, pm->dependents *
			    sizeof (dev_info_t *));
			kmem_free(pm->dependee, pm->dependees *
			    sizeof (dev_info_t *));
			pm->dependent = pm->dependee = NULL;
			pm->dependents = pm->dependees = 0;
			return (DDI_WALK_CONTINUE);
		}
		if (pm_get_norm_pwrs(dip, &level, &size) == DDI_SUCCESS) {
			for (i = 0; i < pm->components; i++) {
				if (pm->cmpt_pwr[i] == 0)
					(void) power_dev(dip, i, level[i], pm);
			}
			kmem_free(level, size);
		}
		kmem_free(pm->threshold, pm->components * sizeof (int));
		kmem_free(pm->cmpt_pwr, pm->components * sizeof (int));
		kmem_free(pm->dependent, pm->dependents *
		    sizeof (dev_info_t *));
		kmem_free(pm->dependee, pm->dependees * sizeof (dev_info_t *));
		mutex_destroy(&pm->node_lock);
		kmem_free(pm, sizeof (pm_info_t));
		e_ddi_set_pm_info(dip, NULL);
		major = ddi_name_to_major(ddi_get_name(dip));
		ddi_rele_driver(major);
		unitp->pm_devices--;
	}
	return (DDI_WALK_CONTINUE);
}

static void
pm_callb(void *arg, int code)
{
	pm_unit	*unitp = (pm_unit *)arg;

	switch (code) {
	case CB_CODE_CPR_CHKPT:
		/* Cancel scan or wait for scan in progress to finish */
		if (unitp->pm_schedule)
			(void) untimeout(unitp->pm_scan_id);
		return;

	case CB_CODE_CPR_RESUME:
		/* No external threads in driver */
		rw_enter(&unitp->pm_unload, RW_WRITER);
		ddi_walk_devs(ddi_root_node(), reset_info, NULL);
		rw_exit(&unitp->pm_unload);
		/*
		 * Schedule first scan 5 minutes from now to allow
		 * system to settle down after a resume
		 */
		if (unitp->pm_schedule)
			unitp->pm_scan_id = timeout(start_scan, (caddr_t)unitp,
			    hz * 300);
		return;

	}
}

/*
 * After a system suspend operation, the drivers restore their devices to
 * normal (on) power. So, change pm's information to reflect this.  If the
 * devices are still idle after the delay (above), then we can turn them off.
 */
/*ARGSUSED1*/
static int
reset_info(dev_info_t *dip, void *dummy)
{
	u_int		*level;
	int		i, size = 0;
	pm_info_t	*pm;

	pm = e_ddi_get_pm_info(dip);
	if (pm && (pm_get_norm_pwrs(dip, &level, &size) == DDI_SUCCESS)) {
		for (i = 0; i < pm->components; i++)
#if 0
			/*
			 * XXX - this what it should be...
			 */
			pm->cmpt_pwr[i] = level[i];
#else
			/*
			 * XXX - but because cg14 does not resume to on...
			 */
			if (pm->cmpt_pwr[i] != level[i])
				(void) power_dev(dip, i, pm->cmpt_pwr[i], pm);
#endif
		kmem_free(level, size);
	}
	return (DDI_WALK_CONTINUE);
}

#ifdef DEBUG
/*ARGSUSED1*/
static int
print_info(dev_info_t *dip, void *dummy)
{
	pm_info_t	*info;
	int		i;

	if (!dip || !(info = e_ddi_get_pm_info(dip)))
		return (DDI_WALK_CONTINUE);
	cmn_err(CE_CONT, "pm_info for %s\n", ddi_node_name(dip));
	if (info->components && (!info->threshold || !info->cmpt_pwr))
		cmn_err(CE_WARN, "Missing component details");
	if (!info->components && (info->threshold || info->cmpt_pwr))
		cmn_err(CE_WARN, "Extraneous component details");
	for (i = 0; i < info->components; i++) {
		cmn_err(CE_CONT, "\tThreshold[%d] = %d\n", i,
		    info->threshold[i]);
		cmn_err(CE_CONT, "\tCurrent power[%d] = %d\n", i,
		    info->cmpt_pwr[i]);
	}
	if (info->dependents && !info->dependent)
		cmn_err(CE_WARN, "Missing dependent details");
	if (!info->dependents && info->dependent)
		cmn_err(CE_WARN, "Extraneous dependent details");
	for (i = 0; i < info->dependents; i++) {
		cmn_err(CE_CONT, "\tDependent[%d] = %s\n", i,
		    ddi_node_name(info->dependent[i]));
	}
	if (info->dependees && !info->dependee)
		cmn_err(CE_WARN, "Missing dependee details");
	if (!info->dependees && info->dependee)
		cmn_err(CE_WARN, "Extraneous dependee details");
	for (i = 0; i < info->dependees; i++) {
		cmn_err(CE_CONT, "\tDependee[%d] = %s\n", i,
		    ddi_node_name(info->dependee[i]));
	}
	if (info->dev_pm_state & PM_DIRECT)
		cmn_err(CE_CONT, "\tDirect power management\n");
	if (info->dev_pm_state & PM_UNCONFIG)
		cmn_err(CE_CONT, "\tUnconfigured device\n");
	if (info->dev_pm_state & PM_REMOVED)
		cmn_err(CE_CONT, "\tRemoved device\n");
	return (DDI_WALK_CONTINUE);
}
#endif

static int
pm_get_norm_pwrs(dev_info_t *dip, u_int **valuep, int *length)
{
	int components = e_ddi_get_num_components(dip);
	u_int *bufp;
	int size, i;

	/*
	 * If no components via pm_create_components, then try old properties
	 */
	if (components <= 0) {
		if (pm_get_prop(dip, "pm_norm_pwr", &bufp, &size) !=
		    DDI_SUCCESS)
			return (DDI_FAILURE);
	} else {
		size = components * sizeof (u_int);
		bufp = kmem_alloc(size, KM_SLEEP);
		for (i = 0; i < components; i++) {
			bufp[i] = pm_get_normal_power(dip, i);
		}
	}
	*length = size;
	*valuep = bufp;
	return (DDI_SUCCESS);
}

static int
pm_get_timestamps(dev_info_t *dip, time_t **valuep, int *length)
{
	int components = e_ddi_get_num_components(dip);
	time_t *bufp;
	int size, i;

	/*
	 * If no components via pm_create_components, then try old properties
	 */
	if (components <= 0) {
		if (pm_get_prop(dip,
		    "pm_timestamp", &bufp, &size) != DDI_SUCCESS)
			return (DDI_FAILURE);
	} else {
		size = components * sizeof (u_int);
		bufp = kmem_alloc(size, KM_SLEEP);
		for (i = 0; i < components; i++) {
			bufp[i] = DEVI(dip)->devi_components[i].pmc_timestamp;
		}
	}
	*length = size;
	*valuep = bufp;
	return (DDI_SUCCESS);
}

static int
e_ddi_get_num_components(dev_info_t *dip)
{
	if (DEVI(dip)->devi_components) {
		return (DEVI(dip)->devi_num_components);
	}
	return (DDI_FAILURE);
}

static void
e_ddi_set_pm_info(dev_info_t *dip, void *value)
{
	DEVI(dip)->devi_pm_info = value;
}

static void *
e_ddi_get_pm_info(dev_info_t *dip)
{
	return (DEVI(dip)->devi_pm_info);
}

#define	PROP_FLAGS	(DDI_PROP_CANSLEEP | DDI_PROP_DONTPASS |	\
			DDI_PROP_NOTPROM)
static int
pm_get_prop(dev_info_t *dip, char *name, void *valuep, int *lengthp)
{
	struct dev_ops	*drv;
	struct cb_ops	*cb;

	if ((drv = ddi_get_driver(dip)) == NULL)
		return (0);
	if ((cb = drv->devo_cb_ops) != NULL)
		return ((*cb->cb_prop_op)(DDI_DEV_T_ANY, dip,
		    PROP_LEN_AND_VAL_ALLOC, PROP_FLAGS, name,
		    (caddr_t)valuep, lengthp));
	return (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_ALLOC,
	    PROP_FLAGS, name, (caddr_t)valuep, lengthp));
}

static int
pm_check_permission(char *fname, cred_t *cr)
{
	vnode_t		*vp;
	register int	error = 0;
	struct vattr	vattr;
	char		dev_name[MAXNAMELEN];
	size_t		len;

	if (error = copyinstr(fname, dev_name, MAXNAMELEN, &len)) {
		cmn_err(CE_WARN,
		    "pm: Can't copy device name %s into kernel space", fname);
		return (error);
	}

	if (error = lookupname(dev_name, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp)) {
		cmn_err(CE_WARN, "pm: Can't find device file %s ", dev_name);
		VN_RELE(vp);
		return (error);
	}

	vattr.va_mask = AT_UID;
	if (error = VOP_GETATTR(vp, &vattr, 0, CRED())) {
		cmn_err(CE_WARN, "pm: Can't get owner uid of device file %s",
			dev_name);
		VN_RELE(vp);
		return (error);
	}

	if (!suser(cr) && (vattr.va_uid != cr->cr_ruid))
		error = EPERM;

	VN_RELE(vp);
	return (error);
}
