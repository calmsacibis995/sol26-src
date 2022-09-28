/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)envctrl.c	1.19	96/10/17 SMI"

/*
 * ENVCTRL_ Environment Monitoring driver for i2c
 *
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/termio.h>
#include <sys/termios.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <sys/strsun.h>
#include <sys/stropts.h>
#include <sys/strtty.h>
#include <sys/debug.h>
#include <sys/eucioctl.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/kmem.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/conf.h>		/* req. by dev_ops flags MTSAFE etc. */
#include <sys/modctl.h>		/* for modldrv */
#include <sys/stat.h>		/* ddi_create_minor_node S_IFCHR */
#include <sys/open.h>		/* for open params.	 */
#include <sys/uio.h>		/* for read/write */
#include <sys/envctrl.h>		/* definitions for this driver */


/* driver entry point fn definitions */
static int 	envctrl_open(queue_t *, dev_t *, int, int, cred_t *);
static int	envctrl_close(queue_t *, int, cred_t *);
static uint_t 	envctrl_bus_isr(caddr_t);
static uint_t 	envctrl_dev_isr(caddr_t);

/* configuration entry point fn definitions */
static int 	envctrl_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	envctrl_identify(dev_info_t *);
static int	envctrl_attach(dev_info_t *, ddi_attach_cmd_t);
static int	envctrl_detach(dev_info_t *, ddi_detach_cmd_t);

/* Driver private routines */
static void	envctrl_init_bus(struct envctrlunit *);
static int	envctrl_xmit(struct envctrlunit *, caddr_t *, int);
static void	envctrl_recv(struct envctrlunit *, caddr_t *, int);
static void	envctrl_send_byte(struct envctrlunit *, u_char *, u_char);
static int	envctrl_recv_byte(struct envctrlunit *, u_char *);
static void	envctrl_get_sys_temperatures(struct envctrlunit *, u_char *);
static int	envctrl_get_lm75_temp(struct envctrlunit *);
static int	envctrl_get_ps_temp(struct envctrlunit *, u_char);
static int	envctrl_get_cpu_temp(struct envctrlunit *, int);
static void	envctrl_fan_fail_service(struct envctrlunit *);
static void	envctrl_PS_intr_service(struct envctrlunit *, u_char);
static void	envctrl_ps_probe(struct envctrlunit *);
static void	envctrl_tempr_poll(void);
static void	envctrl_led_blink(void);
static void	envctrl_reset_dflop(struct envctrlunit *);
static void	envctrl_enable_devintrs(struct envctrlunit *);
static void	envctrl_stop_clock(struct envctrlunit *);
static void	envctrl_reset_watchdog(struct envctrlunit *, u_char *);
static void	envctrl_abort_seq_handler(char *msg);
static u_char	envctrl_get_fpm_status(struct envctrlunit *);
static void	envctrl_set_fsp(struct envctrlunit *, u_char *);
static int	envctrl_set_dskled(struct envctrlunit *,
				struct envctrl_pcf8574_chip *);
static int	envctrl_get_dskled(struct envctrlunit *,
				struct envctrl_pcf8574_chip *);
/* Kstat routines */
static void	envctrl_add_kstats(struct envctrlunit *);
static int	envctrl_ps_kstat_update(kstat_t *, int);
static int	envctrl_fanstat_kstat_update(kstat_t *, int);
static int	envctrl_encl_kstat_update(kstat_t *, int);
static void	envctrl_init_fan_kstats(struct envctrlunit *);
static void	envctrl_init_encl_kstats(struct envctrlunit *);
static void	envctrl_add_encl_kstats(struct envctrlunit *, int, int, u_char);
static void	envctrl_mod_encl_kstats(struct envctrlunit *, int, int, u_char);


/* Streams Routines */
static int	envctrl_wput(queue_t *, mblk_t *);

/* ioctl handling */
static void 	envctrl_ack_ioctl(queue_t *, mblk_t *);
static void 	envctrl_nack_ioctl(queue_t *, mblk_t *, int);
static void 	envctrl_copyin(queue_t *, mblk_t *, caddr_t, uint_t);
static void 	envctrl_copyout(queue_t *, mblk_t *, caddr_t, uint_t);

/* External routines */
extern void power_down();
extern	void	prom_printf(char *fmt, ...);
extern void (*abort_seq_handler)();

static void    *envctrlsoft_statep;

/* Local Variables */
/* Indicates whether or not the overtemp thread has been started */
static int	envctrl_debug_flags = 0;
static int	envctrl_afb_present = 0;
static int	envctrl_fan_debug = 0;
static int	envctrl_power_off_overide = 0;
static int	envctrl_allow_detach = 0;
static int	envctrl_numcpus = 1;
static int envctrl_handler = 1; /* 1 is the default */
static int overtemp_timeout_hz;
static int blink_timeout_hz;

u_char backaddrs[] = {ENVCTRL_PCF8574_DEV0, ENVCTRL_PCF8574_DEV1,
    ENVCTRL_PCF8574_DEV2};

struct module_info envctrlinfo = {
	/* id, name, min pkt siz, max pkt siz, hi water, low water */
	42, "envctrl", 0, 2048, (1024 * 20), (1024 * 1)
};

static struct qinit envctrl_rinit = {
	putq, NULL, envctrl_open, envctrl_close, NULL, &envctrlinfo, NULL
};

static struct qinit envctrl_wint = {
	envctrl_wput, NULL, envctrl_open, envctrl_close,
	    NULL, &envctrlinfo, NULL
};

struct streamtab envctrl_str_info = {
	&envctrl_rinit, &envctrl_wint, NULL, NULL
};

static struct cb_ops envctrl_cb_ops = {
	nodev,			/* cb_open */
	nodev,			/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&envctrl_str_info,	/* cb_stream */
	(int) (D_NEW | D_MP)	/* cb_flag */
};

/*
 * Declare ops vectors for auto configuration.
 */
struct dev_ops  envctrl_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	envctrl_getinfo,		/* devo_getinfo */
	envctrl_identify,		/* devo_identify */
	nulldev,		/* devo_probe */
	envctrl_attach,		/* devo_attach */
	envctrl_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&envctrl_cb_ops,		/* devo_cb_ops */
	(struct bus_ops *)NULL,	/* devo_bus_ops */
	nulldev			/* devo_power */
};

extern struct mod_ops mod_driverops;

static struct modldrv envctrlmodldrv = {
	&mod_driverops,		/* type of module - driver */
	"I2C ENVCTRL_driver: 1.19 96/10/17",
	&envctrl_ops,
};

static struct modlinkage envctrlmodlinkage = {
	MODREV_1,
	&envctrlmodldrv,
	0
};

int
_init(void)
{
	register int    error;

	if ((error = mod_install(&envctrlmodlinkage)) == 0) {
		ddi_soft_state_init(&envctrlsoft_statep,
			sizeof (struct envctrlunit), 1);
	}

	return (error);
}

int
_fini(void)
{
	register int    error;

	if ((error = mod_remove(&envctrlmodlinkage)) == 0)
		ddi_soft_state_fini(&envctrlsoft_statep);

	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&envctrlmodlinkage, modinfop));
}

static int
envctrl_identify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "SUNW,envctrl") == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}

static int
envctrl_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	register int	instance;
	char		name[16];
	register struct	envctrlunit *unitp;
	struct ddi_device_acc_attr attr;
	int *reg_prop;
	u_int len = 0;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;

	instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		if (!(unitp = ddi_get_soft_state(envctrlsoft_statep, instance)))
			return (DDI_FAILURE);
		mutex_enter(&unitp->umutex);
		if (!unitp->suspended) {
			mutex_exit(&unitp->umutex);
			return (DDI_FAILURE);
		}
		unitp->suspended = 0;
		mutex_exit(&unitp->umutex);
		envctrl_init_bus(unitp);
		mutex_enter(&unitp->umutex);

		envctrl_ps_probe(unitp);
		mutex_exit(&unitp->umutex);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	/* Set up timer values */
	overtemp_timeout_hz = drv_usectohz(OVERTEMP_TIMEOUT_USEC);
	blink_timeout_hz = drv_usectohz(BLINK_TIMEOUT_USEC);

	if (ddi_soft_state_zalloc(envctrlsoft_statep, instance) != 0) {
		cmn_err(CE_WARN, "envctrl failed to zalloc softstate\n");
		goto failed;
	}

	unitp = ddi_get_soft_state(envctrlsoft_statep, instance);

	if (ddi_regs_map_setup(dip, 0, (caddr_t *)&unitp->bus_ctl_regs, 0,
			sizeof (struct envctrl_pcd8584_regs), &attr,
			&unitp->ctlr_handle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "I2c failed to map in bus_control regs\n");
		return (DDI_FAILURE);
	}

	/* add interrupts */

	if (ddi_get_iblock_cookie(dip, 0,
			&unitp->ic_trap_cookie) != DDI_SUCCESS)  {
		cmn_err(CE_WARN, "ddi_get_iblock_cookie FAILED \n");
		goto failed;
	}

	mutex_init(&unitp->umutex, "envctrl mutex ", MUTEX_DRIVER,
		(void *)unitp->ic_trap_cookie);


	if (ddi_add_intr(dip, 0, &unitp->ic_trap_cookie, NULL, envctrl_bus_isr,
			(caddr_t)unitp) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "envctrl_attach failed to add hard intr %d\n",
			instance);
		goto remlock;
	}


	if (ddi_add_intr(dip, 1, &unitp->ic_trap_cookie, NULL, envctrl_dev_isr,
			(caddr_t)unitp) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "envctrl_attach failed to add hard intr %d\n",
			instance);
		goto remhardintr;
	}


	sprintf(name, "envctrl%d", instance);

	if (ddi_create_minor_node(dip, name, S_IFCHR, instance, NULL,
			NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(dip, NULL);
		goto remhardintr1;
	}

	mutex_enter(&unitp->umutex);
	switch (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    ENVCTRL_LED_BLINK, -1)) {
	case 1:
		unitp->activity_led_blink = B_TRUE;
		break;
	case 0:
	default:
		unitp->activity_led_blink = B_FALSE;
		break;
	}
	unitp->num_ps_present = unitp->num_encl_present = 0;
	unitp->num_fans_present = MIN_FAN_BANKS;
	unitp->num_fans_failed = ENVCTRL_CHAR_ZERO;
	unitp->dip = dip;

	(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, ENVCTRL_PANEL_LEDS_PR, &reg_prop, &len);
	ASSERT(len != 0);

	(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, ENVCTRL_PANEL_LEDS_STA, &reg_prop, &len);
	ASSERT(len != 0);

	(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, ENVCTRL_DISK_LEDS_STA, &reg_prop, &len);
	ASSERT(len != 0);

	/* For debug */
	if (envctrl_afb_present) {
		unitp->AFB_present = B_TRUE;
	}

	unitp->num_fans_present++;

	/* initialize the envctrl bus controller */
	mutex_exit(&unitp->umutex);

	envctrl_init_bus(unitp);

	mutex_enter(&unitp->umutex);
	envctrl_ps_probe(unitp);

	unitp->timeout_id = 0;
	unitp->blink_timeout_id = 0;

	if (envctrl_numcpus > 1) {
		unitp->num_cpus_present = envctrl_numcpus;
	}
	/* XXX need to somehow probe for more cpu's */

	/*
	 * we need to init the fan kstats before the tempr_poll
	 */
	envctrl_add_kstats(unitp);
	envctrl_init_fan_kstats(unitp);
	envctrl_init_encl_kstats(unitp);
	if (unitp->activity_led_blink == B_TRUE) {
		unitp->present_led_state = B_FALSE;
		envctrl_led_blink();
	}
	mutex_exit(&unitp->umutex);


	envctrl_tempr_poll();

	/*
	 * interpose envctrl's abort sequence handler
	 */
	if (envctrl_handler) {
		abort_seq_handler = envctrl_abort_seq_handler;
	}

	ddi_report_dev(dip);

	return (DDI_SUCCESS);

remhardintr1:
	ddi_remove_intr(dip, (uint_t)1, unitp->ic_trap_cookie);
remhardintr:
	ddi_remove_intr(dip, (uint_t)0, unitp->ic_trap_cookie);

remlock:
	mutex_destroy(&unitp->umutex);

failed:
	if (unitp->ctlr_handle)
		ddi_regs_map_free(&unitp->ctlr_handle);

	cmn_err(CE_WARN, "envctrl_attach:failed.\n");

	return (DDI_FAILURE);

}

static int
envctrl_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance;
	register struct envctrlunit *unitp;

	instance = ddi_get_instance(dip);
	unitp = ddi_get_soft_state(envctrlsoft_statep, instance);

	switch (cmd) {
	case DDI_DETACH:
		if (envctrl_allow_detach) {

			if (unitp->psksp != NULL) {
				kstat_delete(unitp->psksp);
			}
			if (unitp->fanksp != NULL) {
				kstat_delete(unitp->fanksp);
			}
			if (unitp->enclksp != NULL) {
				kstat_delete(unitp->enclksp);
			}

			if (unitp->timeout_id != 0) {
				(void) untimeout(unitp->timeout_id);
				unitp->timeout_id = 0;
			}
			if (unitp->blink_timeout_id != 0) {
				(void) untimeout(unitp->blink_timeout_id);
				unitp->blink_timeout_id = 0;
			}

			ddi_remove_minor_node(dip, NULL);

			ddi_remove_intr(dip, (uint_t)0, unitp->ic_trap_cookie);
			ddi_remove_intr(dip, (uint_t)1, unitp->ic_trap_cookie);

			ddi_regs_map_free(&unitp->ctlr_handle);

			mutex_destroy(&unitp->umutex);

			return (DDI_SUCCESS);
		} else {
			return (DDI_FAILURE);
		}

	case DDI_SUSPEND:
		if (!(unitp = ddi_get_soft_state(envctrlsoft_statep, instance)))
		    return (DDI_FAILURE);
		mutex_enter(&unitp->umutex);
		if (unitp->suspended) {
			cmn_err(CE_WARN, "envctrl already suspended\n");
			mutex_exit(&unitp->umutex);
			return (DDI_FAILURE);
		}
		unitp->suspended = 1;
		mutex_exit(&unitp->umutex);
		return (DDI_SUCCESS);

	default:
		cmn_err(CE_WARN, "envctrl suspend general fault\n");
		return (DDI_FAILURE);
	}


}
int
envctrl_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result)
{
	dev_t	dev = (dev_t) arg;
	struct envctrlunit *unitp;
	int	instance, ret;

	instance = getminor(dev);

#ifdef lint
	dip = dip;
#endif


	switch (infocmd) {
		case DDI_INFO_DEVT2DEVINFO:
			unitp = (struct envctrlunit *)
			    ddi_get_soft_state(envctrlsoft_statep, instance);
			*result = unitp->dip;
			ret = DDI_SUCCESS;
			break;
		case DDI_INFO_DEVT2INSTANCE:
			*result = (void *)instance;
			ret = DDI_SUCCESS;
			break;
		default:
			ret = DDI_FAILURE;
			break;
	}

	return (ret);
}

static int
envctrl_open(queue_t *q, dev_t *dev, int flag, register sflag, cred_t *credp)
{
	struct envctrlunit *unitp;
	int status = 0;
	register int	instance;
	register struct stroptions *sop;
	mblk_t		*mop;

#ifdef lint
	flag = flag;
	sflag = sflag;
	credp = credp;
#endif
	instance = getminor(*dev);
	if (instance < 0)
		return (ENXIO);
	unitp = (struct envctrlunit *)
		    ddi_get_soft_state(envctrlsoft_statep, instance);

	if (unitp == NULL)
		return (ENXIO);

	mutex_enter(&unitp->umutex);

	if (flag & FWRITE) {
		if ((unitp->oflag & FWRITE)) {
			mutex_exit(&unitp->umutex);
			return (EBUSY);
		} else {
			unitp->oflag |= FWRITE;
		}
	}

	if (!(mop = allocb(sizeof (struct stroptions), BPRI_MED))) {
		return (EAGAIN);
	}

	q->q_ptr = WR(q)->q_ptr = (caddr_t) unitp;

	mop->b_datap->db_type = M_SETOPTS;
	mop->b_wptr += sizeof (struct stroptions);

	/*
	 * if device is open with O_NONBLOCK flag set, let read(2) return 0
	 * if no data waiting to be read.  Writes will block on flow control.
	 */
	sop = (struct stroptions *) mop->b_rptr;
	sop->so_flags = SO_HIWAT | SO_LOWAT | SO_NDELON;
	sop->so_hiwat = 512;
	sop->so_lowat = 256;

	/* enable the stream */
	qprocson(q);

	unitp->readq = RD(q);
	unitp->writeq = WR(q);
	unitp->msg = (mblk_t *)NULL;

	mutex_exit(&unitp->umutex);
	return (status);
}

static int
envctrl_close(queue_t *q, int flag, cred_t *cred_p)
{
	struct envctrlunit *unitp;

	unitp = (struct envctrlunit *) q->q_ptr;

#ifdef lint
	flag = flag;
	cred_p = cred_p;
#endif
	mutex_enter(&unitp->umutex);

	unitp->oflag = B_FALSE;
	unitp->current_mode = ENVCTRL_NORMAL_MODE;

	/* disable the stream */
	q->q_ptr = WR(q)->q_ptr = NULL;
	qprocsoff(q);

	mutex_exit(&unitp->umutex);
	return (DDI_SUCCESS);
}

/*
 * standard put procedure for envctrl
 */
static int
envctrl_wput(queue_t *q, mblk_t *mp)
{
	register struct msgb *mp1;
	struct envctrlunit *unitp;
	struct iocblk *iocp;
	struct copyresp *csp;
	struct envctrl_tda8444t_chip *fanspeed;
	struct envctrl_pcf8574_chip *ledchip;
	struct envctrl_pcf8591_chip *temp;
	struct copyreq *cqp;
	int cmd;

	unitp = (struct envctrlunit *) q->q_ptr;

	switch (DB_TYPE(mp)) {

	case M_DATA:

		while (mp) {
			DB_TYPE(mp) = M_DATA;
			mp1 = unlinkb(mp);
			mp->b_cont = NULL;
			if ((mp->b_wptr - mp->b_rptr) <= 0) {
				freemsg(mp);
			} else {
				(void) putq(q, mp);
			}
			mp = mp1;
		}

		break;

	case M_IOCTL:
	{
		iocp = (struct iocblk *)(void *)mp->b_rptr;
		cmd = iocp->ioc_cmd;
		switch (cmd) {
		case ENVCTRL_IOC_SETMODE:
			if (iocp->ioc_count == TRANSPARENT) {
				envctrl_copyin(q, mp,
				*(caddr_t *)(void *)mp->b_cont->b_rptr,
				    sizeof (u_char));
			}
			break;
		case ENVCTRL_IOC_RESETTMPR:
			/*
			 * For diags, cancell the curent temp poll
			 * and reset it for a new one.
			 */
			if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
				if (unitp->timeout_id != 0) {
					(void) untimeout(unitp->timeout_id);
					unitp->timeout_id = 0;
				}
				envctrl_tempr_poll();
				envctrl_ack_ioctl(q, mp);
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		case ENVCTRL_IOC_GETTEMP:
			if (iocp->ioc_count == TRANSPARENT) {
				envctrl_copyin(q, mp,
				    *(caddr_t *)(void *)mp->b_cont->b_rptr,
				    sizeof (struct envctrl_pcf8591_chip));
			}
			break;
		case ENVCTRL_IOC_SETTEMP:
			if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
				if (iocp->ioc_count == TRANSPARENT) {
					envctrl_copyin(q, mp,
					*(caddr_t *)(void *)mp->b_cont->b_rptr,
					    sizeof (u_char));
				}
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		case ENVCTRL_IOC_SETWDT:
			if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
				if (iocp->ioc_count == TRANSPARENT) {
					envctrl_copyin(q, mp,
					*(caddr_t *)(void *)mp->b_cont->b_rptr,
					    sizeof (u_char));
				}
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		case ENVCTRL_IOC_SETFAN:
			/*
			 * we must be in diag mode before we can
			 * set any fan speeds.
			 */
			if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
				if (iocp->ioc_count == TRANSPARENT) {
					envctrl_copyin(q, mp,
					*(caddr_t *)(void *)mp->b_cont->b_rptr,
					sizeof (struct envctrl_tda8444t_chip));
				}
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		case ENVCTRL_IOC_GETFAN:
			if (iocp->ioc_count == TRANSPARENT) {
				envctrl_copyin(q, mp,
				    *(caddr_t *)(void *)mp->b_cont->b_rptr,
				    sizeof (struct envctrl_tda8444t_chip));
			}
			break;
		case ENVCTRL_IOC_SETFSP:
			if (iocp->ioc_count == TRANSPARENT) {
				envctrl_copyin(q, mp,
				*(caddr_t *)(void *)mp->b_cont->b_rptr,
				    sizeof (u_char));
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		case ENVCTRL_IOC_SETDSKLED:
			if (iocp->ioc_count == TRANSPARENT) {
				envctrl_copyin(q, mp,
				*(caddr_t *)(void *)mp->b_cont->b_rptr,
				    sizeof (struct envctrl_pcf8574_chip));
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		case ENVCTRL_IOC_GETDSKLED:
			if (iocp->ioc_count == TRANSPARENT) {
				envctrl_copyin(q, mp,
				*(caddr_t *)(void *)mp->b_cont->b_rptr,
				    sizeof (struct envctrl_pcf8574_chip));
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		default:
			envctrl_nack_ioctl(q, mp, EINVAL);
			break;
		}

		break;

	}
	case (M_IOCDATA):
	{
		u_char *tempr, *wdval;
		long state;
		csp = (struct copyresp *)(void *)mp->b_rptr;
		cqp = (struct copyreq *)(void *)mp->b_rptr;

		cmd = csp->cp_cmd;
		state = (long)cqp->cq_private;

		switch (cmd) {
		case ENVCTRL_IOC_SETFAN:
			fanspeed = (struct envctrl_tda8444t_chip *)
					    (void *)mp->b_cont->b_rptr;
			mutex_enter(&unitp->umutex);
			if (envctrl_xmit(unitp, (caddr_t *)fanspeed,
			    fanspeed->type) == DDI_FAILURE) {
				mutex_exit(&unitp->umutex);
				envctrl_nack_ioctl(q, mp, EINVAL);
			} else {
				mutex_exit(&unitp->umutex);
				envctrl_ack_ioctl(q, mp);
			}
			break;
		case ENVCTRL_IOC_SETFSP:
			wdval = (u_char *)(void *)mp->b_cont->b_rptr;
			mutex_enter(&unitp->umutex);
			/*
			 * If a user is in normal mode and they try
			 * to set anything other than a disk fault or
			 * a gen fault it is an invalid operation.
			 * in diag mode we allow everything to be
			 * twiddled.
			 */
			if (unitp->current_mode == ENVCTRL_NORMAL_MODE) {
				if (*wdval & ~ENVCTRL_FSP_USRMASK) {
					mutex_exit(&unitp->umutex);
					envctrl_nack_ioctl(q, mp, EINVAL);
					break;
				}
			}
			envctrl_set_fsp(unitp, wdval);
			mutex_exit(&unitp->umutex);
			envctrl_ack_ioctl(q, mp);
			break;
		case ENVCTRL_IOC_SETDSKLED:
			ledchip = (struct envctrl_pcf8574_chip *)
					    (void *)mp->b_cont->b_rptr;
			mutex_enter(&unitp->umutex);
			if (envctrl_set_dskled(unitp, ledchip)) {
				envctrl_nack_ioctl(q, mp, EINVAL);
			} else {
				envctrl_ack_ioctl(q, mp);
			}
			mutex_exit(&unitp->umutex);
			break;
		case ENVCTRL_IOC_GETDSKLED:
			if (state  == -1) {
				envctrl_ack_ioctl(q, mp);
				break;
			}
			ledchip = (struct envctrl_pcf8574_chip *)
					    (void *)mp->b_cont->b_rptr;
			mutex_enter(&unitp->umutex);
			if (envctrl_get_dskled(unitp, ledchip)) {
				envctrl_nack_ioctl(q, mp, EINVAL);
			} else {
				envctrl_copyout(q, mp, (caddr_t)csp->cp_private,
				    sizeof (struct envctrl_pcf8574_chip));
			}
			mutex_exit(&unitp->umutex);
			break;
		case ENVCTRL_IOC_GETTEMP:
			/* Get the user buffer address */

			if (state  == -1) {
				envctrl_ack_ioctl(q, mp);
				break;
			}
			temp = (struct envctrl_pcf8591_chip *)
				    (void *)mp->b_cont->b_rptr;
			mutex_enter(&unitp->umutex);
			envctrl_recv(unitp, (caddr_t *)temp, PCF8591);
			mutex_exit(&unitp->umutex);
			envctrl_copyout(q, mp, (caddr_t)csp->cp_private,
			    sizeof (struct envctrl_pcf8591_chip));
			break;
		case ENVCTRL_IOC_GETFAN:
			/* Get the user buffer address */

			if (state  == -1) {
				envctrl_ack_ioctl(q, mp);
				break;
			}
			fanspeed = (struct envctrl_tda8444t_chip *)
				    (void *)mp->b_cont->b_rptr;
			mutex_enter(&unitp->umutex);
			envctrl_recv(unitp, (caddr_t *)fanspeed, TDA8444T);
			mutex_exit(&unitp->umutex);
			envctrl_copyout(q, mp, (caddr_t)csp->cp_private,
			    sizeof (struct envctrl_tda8444t_chip));
			break;
		case ENVCTRL_IOC_SETTEMP:
			tempr = (u_char *)(void *)mp->b_cont->b_rptr;
			if (*tempr < MIN_DIAG_TEMPR ||
			    *tempr > MAX_DIAG_TEMPR) {
				envctrl_nack_ioctl(q, mp, EINVAL);
			} else {
				mutex_enter(&unitp->umutex);
				envctrl_get_sys_temperatures(unitp, tempr);
				mutex_exit(&unitp->umutex);
				envctrl_ack_ioctl(q, mp);
			}
			break;
		case ENVCTRL_IOC_SETWDT:
			/* reset watchdog timeout period */
			wdval = (u_char *)(void *)mp->b_cont->b_rptr;
			if (*wdval < DIAG_MAX_TIMER_VAL ||
			    *wdval > MAX_CL_VAL) {
				envctrl_nack_ioctl(q, mp, EINVAL);
			} else {
				mutex_enter(&unitp->umutex);
				envctrl_reset_watchdog(unitp, wdval);
				mutex_exit(&unitp->umutex);
				envctrl_ack_ioctl(q, mp);
			}
			break;
		case ENVCTRL_IOC_SETMODE:
			/* Set mode */
			wdval = (u_char *)(void *)mp->b_cont->b_rptr;
			if (*wdval == ENVCTRL_DIAG_MODE || *wdval ==
			    ENVCTRL_NORMAL_MODE) {
				mutex_enter(&unitp->umutex);
				unitp->current_mode = *wdval;
				if (unitp->timeout_id != 0 &&
				    *wdval == ENVCTRL_DIAG_MODE) {
					(void) untimeout(unitp->timeout_id);
					unitp->timeout_id =
					    (timeout(envctrl_tempr_poll,
					    NULL, overtemp_timeout_hz));

				}
				if (*wdval == ENVCTRL_NORMAL_MODE) {
					envctrl_get_sys_temperatures(unitp,
					    (u_char *)NULL);
				}
				mutex_exit(&unitp->umutex);
				envctrl_ack_ioctl(q, mp);
			} else {
				envctrl_nack_ioctl(q, mp, EINVAL);
			}
			break;
		default:
			freemsg(mp);
			break;
		}

		break;
	}

	case M_FLUSH:
		envctrl_nack_ioctl(q, mp, EINVAL);
		freemsg(mp);
		break;

	default:
		freemsg(mp);
		break;
	}

	return (0);
}

uint_t
envctrl_bus_isr(caddr_t arg)
{
	struct envctrlunit *unitp = (struct envctrlunit *)(void *)arg;
	int ic = DDI_INTR_UNCLAIMED;

	mutex_enter(&unitp->umutex);

	/*
	 * NOT USED
	 */

	mutex_exit(&unitp->umutex);
	return (ic);
}

uint_t
envctrl_dev_isr(caddr_t arg)
{
	struct envctrlunit *unitp = (struct envctrlunit *)(void *)arg;
	u_char chip_addr, recv_data;
	int ic;

	ic = DDI_INTR_UNCLAIMED;

	mutex_enter(&unitp->umutex);

	/*
	 * First check to see if it is an interrupt for us by
	 * looking at the "ganged" interrrupt and vector
	 * according to the major type
	 * 0x70 is the addr of the ganged interrupt controller.
	 * Address map for the port byte read is as follows
	 * MSB
	 * -------------------------
	 * |  |  |  |  |  |  |  |  |
	 * -------------------------
	 *  P7 P6 P5 P4 P3 P2 P1 P0
	 * P0 = Power Supply 1 intr
	 * P1 = Power Supply 2 intr
	 * P2 = Power Supply 3 intr
	 * P3 = Dlfop enable for fan sped set
	 * P4 = ENVCTRL_ Fan Fail intr
	 * P5 =	Front Panel Interrupt
	 * P6 = Power Fail Detect Low.
	 * P7 = Enable Interrupts to system
	 */

	chip_addr = (PCF8574A_BASE_ADDR | ENVCTRL_PCF8574_DEV0 |
	PCF8574_READ_BIT);

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, chip_addr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* this should be salve addr */
	recv_data = envctrl_recv_byte(unitp, S0);
	if (recv_data != chip_addr && envctrl_debug_flags) {
		cmn_err(CE_WARN, "DEVISR FAILED received 0x%x\n",
			    recv_data);
	}

	/*
	 * The intr cond should get cleared now
	 * just by reading the chip that caused the
	 * intr.
	 * ports are normally high or 0x01 state for
	 * 8 bits. Read the ports and see who is in
	 * trouble.
	 */

	recv_data = envctrl_recv_byte(unitp, S0);

	/* Stop bus */
	envctrl_send_byte(unitp, S1, STOP);

	/*
	 * Port 0 = PS1 interrupt
	 * Port 1 = PS2 Interrupt
	 * Port 2 = PS3 Interrupt
	 * Port 3 = SPARE
	 * Port 4 = Fan Fail Intr
	 * Port 5 = Front Panle Module intr
	 * Port 6 = Keyswitch Intr
	 * Port 7 = ESINTR ENABLE ???
	 */

	if (!(recv_data & ENVCTRL_PCF8574_PORT0)) {
		envctrl_PS_intr_service(unitp, PS1);
		ic = DDI_INTR_CLAIMED;
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT1)) {
		envctrl_PS_intr_service(unitp, PS2);
		ic = DDI_INTR_CLAIMED;
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT2)) {
		envctrl_PS_intr_service(unitp, PS3);
		ic = DDI_INTR_CLAIMED;
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT3)) {
		ic = DDI_INTR_CLAIMED;
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT4)) {
		/*
		 * Check for a fan fail
		 * Single fan fail all Fans in that bank go to 100%
		 * Double fan fail report error, shutdown system
		 */
		envctrl_fan_fail_service(unitp);
		ic = DDI_INTR_CLAIMED;
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT5)) {
		ic = DDI_INTR_CLAIMED;
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT6)) {
		ic = DDI_INTR_CLAIMED;
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT7)) {
		ic = DDI_INTR_CLAIMED;
	}


	mutex_exit(&unitp->umutex);
	return (ic);

}

static void
envctrl_ack_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk  *iocbp;

	mp->b_datap->db_type = M_IOCACK;
	mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	};

	iocbp = (struct iocblk *) mp->b_rptr;
	iocbp->ioc_error = 0;
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;

	qreply(q, mp);

}

static void
envctrl_nack_ioctl(queue_t *q, mblk_t *mp, int err)
{
	struct iocblk  *iocbp;

	mp->b_datap->db_type = M_IOCNAK;
	mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
	iocbp = (struct iocblk *) mp->b_rptr;
	iocbp->ioc_error = err;

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	qreply(q, mp);
}

/*
 * Set up for a simple copyin of user data, reusing the existing message block.
 * Set the private data field to the user address.
 *
 * This routine supports single-level copyin.
 * More complex user data structures require a better state machine.
 */
static void
envctrl_copyin(queue_t *q, mblk_t *mp, caddr_t addr, uint_t len)
{
	struct copyreq *cqp;

	cqp = (struct copyreq *)(void *)mp->b_rptr;
	mp->b_wptr = mp->b_wptr + sizeof (struct copyreq);
	cqp->cq_addr = addr;
	cqp->cq_size = len;
	cqp->cq_private = (mblk_t *)(void *)addr;
	cqp->cq_flag = 0;
	if (mp->b_cont != NULL) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	mp->b_datap->db_type = M_COPYIN;
	qreply(q, mp);
}


/*
 * Set up for a simple copyout of user data, reusing the existing message block.
 * Set the private data field to -1, signifying the final processing state.
 * Assumes that the output data is already set up in mp->b_cont.
 *
 * This routine supports single-level copyout.
 * More complex user data structures require a better state machine.
 */
static void
envctrl_copyout(queue_t *q, mblk_t *mp, caddr_t addr, uint_t len)
{
	struct copyreq *cqp;

	cqp = (struct copyreq *)(void *)mp->b_rptr;
	mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
	cqp->cq_addr = addr;
	cqp->cq_size = len;
	cqp->cq_private = (mblk_t *)-1;
	cqp->cq_flag = 0;
	mp->b_datap->db_type = M_COPYOUT;
	qreply(q, mp);
}
static void
envctrl_init_bus(struct envctrlunit *unitp)
{

	u_char noval = NULL;

	mutex_enter(&unitp->umutex);
	/* Sets the Mode to 808x type bus */
	ddi_putb(unitp->ctlr_handle,
	    &unitp->bus_ctl_regs->s0, ENVCTRL_CHAR_ZERO);

	/* SET UP SLAVE ADDR XXX Required..send 0x80 */

	ddi_putb(unitp->ctlr_handle, &unitp->bus_ctl_regs->s1,
	ENVCTRL_BUS_INIT0);
	ddi_putb(unitp->ctlr_handle, &unitp->bus_ctl_regs->s0,
	ENVCTRL_BUS_INIT1);

	/* Set the clock now */
	ddi_putb(unitp->ctlr_handle,
	    &unitp->bus_ctl_regs->s1, ENVCTRL_BUS_CLOCK0);

	/* S0 is now S2  necause of the previous write to S1 */
	/* clock= 12MHz, SCL=90KHz */
	ddi_putb(unitp->ctlr_handle,
	    &unitp->bus_ctl_regs->s0, ENVCTRL_BUS_CLOCK1);

	/* Enable serial interface */
	ddi_putb(unitp->ctlr_handle,
	    &unitp->bus_ctl_regs->s1, ENVCTRL_BUS_ESI);

	envctrl_stop_clock(unitp);

	envctrl_reset_dflop(unitp);

	envctrl_enable_devintrs(unitp);

	unitp->current_mode = ENVCTRL_NORMAL_MODE;
	envctrl_reset_watchdog(unitp, &noval);

	mutex_exit(&unitp->umutex);
}

static int
envctrl_xmit(struct envctrlunit *unitp, caddr_t *data, int chip_type)
{

	struct envctrl_tda8444t_chip *fanspeed;
	struct envctrl_pcf8574_chip *ioport;
	u_char slave_addr, fan_number;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	switch (chip_type) {
	case TDA8444T:
		/*
		 * if we have failed fans then we should be blasting
		 * all fans we don't want to set them to anything
		 * other than max which should have already been done.
		 */
		if (unitp->num_fans_failed > 0 &&
		    unitp->current_mode == ENVCTRL_NORMAL_MODE)
			break;

		fanspeed = (struct envctrl_tda8444t_chip *)data;

		if (fanspeed->chip_num > ENVCTRL_FAN_ADDR_MAX ||
		    fanspeed->chip_num < ENVCTRL_FAN_ADDR_MIN) {
			return (DDI_FAILURE);
		}

		if (fanspeed->fan_num > ENVCTRL_PORT7 ||
		    fanspeed->fan_num < ENVCTRL_CPU_FANS) {
			return (DDI_FAILURE);
		}

		if (fanspeed->val < MIN_FAN_VAL ||
			fanspeed->val > MAX_FAN_VAL) {
			return (DDI_FAILURE);
		}

		slave_addr = (TDA8444T_BASE_ADDR | fanspeed->chip_num);
		fan_number = (NO_AUTO_PORT_INCR | fanspeed->fan_num);

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, slave_addr);

		/* STEP 3: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);


		/* STEP 7: Write first DATA to slave , fan number */
		envctrl_send_byte(unitp, S0, fan_number);

		/* STEP 9: Write second DATA to slave , fan speed */
		envctrl_send_byte(unitp, S0, fanspeed->val);

		/*
		 * Update the kstats.
		 */
		switch (fanspeed->fan_num) {
		case ENVCTRL_CPU_FANS:
			unitp->fan_kstats[ENVCTRL_FAN_TYPE_CPU].fanspeed =
			    fanspeed->val;
			break;
		case ENVCTRL_PS_FANS:
			unitp->fan_kstats[ENVCTRL_FAN_TYPE_PS].fanspeed =
			    fanspeed->val;
			break;
		case ENVCTRL_AFB_FANS:
			unitp->fan_kstats[ENVCTRL_FAN_TYPE_AFB].fanspeed =
			    fanspeed->val;
			break;
		default:
			break;
		}
		break;
	case PCF8574:
		ioport = (struct envctrl_pcf8574_chip *)data;
		if (ioport->chip_num > ENVCTRL_PCF8574_DEV7)
			return (DDI_FAILURE);

		if (ioport->type == PCF8574A) {
			slave_addr = (PCF8574A_BASE_ADDR | ioport->chip_num);
		} else {
			slave_addr = (PCF8574_BASE_ADDR | ioport->chip_num);
		}

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, slave_addr);

		/* STEP 3: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);

		/* STEP 9: Write second DATA to slave , fan speed */
		envctrl_send_byte(unitp, S0, ioport->val);

		/* STEP 10: End Transmission */
		envctrl_send_byte(unitp, S1, STOP);
		break;

	default:
		return (DDI_FAILURE);
	}

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);
	return (DDI_SUCCESS);
}

static void
envctrl_send_byte(struct envctrlunit *unitp, u_char *reg, u_char data)
{

	int x = 0;
	u_char csr;


	ASSERT(MUTEX_HELD(&unitp->umutex));

	/* STEP 4: POLL for PIN bit = 0 , 0 indicates OK */

	csr = ddi_getb(unitp->ctlr_handle, &unitp->bus_ctl_regs->s1);


	while ((csr & CSRS1_PIN) && x < 1000) {
		drv_usecwait(10);
		csr = ddi_getb(unitp->ctlr_handle, S1);
		x++;
	}

	x = 0;

	/* STEP 6: SLAVE ACKED ?? LRB == 0 */

	csr = ddi_getb(unitp->ctlr_handle, S1);

	if (((csr & CSRS1_LRB) != 0)) {
		if (envctrl_debug_flags) {
			cmn_err(CE_WARN,
			    "envctrl_send_byte: slave LRB ack 1 failed\n");
		}
		csr = CSRS1_PIN | CSRS1_ESO | CSRS1_STO | CSRS1_ACK;
		ddi_putb(unitp->ctlr_handle, S1, csr);
	}

	ddi_putb(unitp->ctlr_handle, reg, data);
	/* per app notes on envctrl */
	drv_usecwait(5);
}

static void
envctrl_recv(struct envctrlunit *unitp, caddr_t *data, int chip_type)
{

	struct envctrl_pcf8591_chip *temp;
	struct envctrl_pcf8574_chip *ioport;
	struct envctrl_tda8444t_chip *fanspeed;
	u_char slave_addr, recv_data, port, fan_num;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	switch (chip_type) {
	case PCF8591:
		temp = (struct envctrl_pcf8591_chip *)data;
		slave_addr = (PCF8591_BASE_ADDR |
		    PCF8591_READ_BIT | temp->chip_num);
		port = temp->sensor_num;

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, slave_addr);

		/* STEP 2: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);
		drv_usecwait(5);

		/* this should be salve addr */
		recv_data = envctrl_recv_byte(unitp, S0);
		if (recv_data != slave_addr && envctrl_debug_flags) {
			cmn_err(CE_WARN, "envctrl SLAVE read failed\n");
		}

		/* address port in chip */
		envctrl_send_byte(unitp, S0, port);

		/* this is not a typo 2 reads necessary */
		recv_data = envctrl_recv_byte(unitp, S0);
		recv_data = envctrl_recv_byte(unitp, S0);
		temp->temp_val = recv_data;
		break;
	case TDA8444T:
		fanspeed = (struct envctrl_tda8444t_chip *)data;

		if (fanspeed->chip_num > ENVCTRL_FAN_ADDR_MAX) {
			cmn_err(CE_WARN,
			"envctrl: recv fan dev 0x%x out of range\n",
			fanspeed->chip_num);
		}

		if (fanspeed->fan_num > ENVCTRL_PORT7)
			cmn_err(CE_WARN, "envctrl: fan val out of range\n");

		/*
		 * in order to read a specific port on the A/D
		 * controller. You have to address that port in
		 * write mode. then stop then re-address the
		 * chip in read mode and then the port should
		 * be pointing to the right fan port.
		 */
		slave_addr = (TDA8444T_BASE_ADDR | fanspeed->chip_num);
		fan_num = (NO_AUTO_PORT_INCR | fanspeed->fan_num);

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, slave_addr);

		/* STEP 2: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);
		drv_usecwait(5);

		/* STEP 7: Write first DATA to slave , fan number */
		envctrl_send_byte(unitp, S0, fan_num);

		/* STEP 10: End Transmission */
		envctrl_send_byte(unitp, S1, STOP);
		drv_usecwait(5);

		slave_addr |= TDA8444T_READ_BIT;

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, slave_addr);

		/* STEP 2: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);

		/* this should be salve addr */
		recv_data = envctrl_recv_byte(unitp, S0);
		if (recv_data != slave_addr && envctrl_debug_flags) {
			cmn_err(CE_WARN, "envctrl SLAVE read failed\n");
		}

		recv_data = envctrl_recv_byte(unitp, S0);

		fanspeed->val = envctrl_recv_byte(unitp, S0);
		break;
	case PCF8574:
		ioport = (struct envctrl_pcf8574_chip *)data;

		if (ioport->chip_num > ENVCTRL_PCF8574_DEV7)
			cmn_err(CE_WARN, "envctrl: dev out of range 0x%x\n",
ioport->chip_num);

		if (ioport->type == PCF8574A) {
			slave_addr = (PCF8574_READ_BIT | PCF8574A_BASE_ADDR |
			    ioport->chip_num);
		} else {
			slave_addr = (PCF8574_READ_BIT | PCF8574_BASE_ADDR |
			    ioport->chip_num);
		}

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, slave_addr);

		/* STEP 3: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);

		/* this should be salve addr */
		recv_data = envctrl_recv_byte(unitp, S0);
		if (recv_data != slave_addr && envctrl_debug_flags) {
			cmn_err(CE_WARN, "envctrl SLAVE read failed\n");
		}
		ioport->val = envctrl_recv_byte(unitp, S0);
		envctrl_send_byte(unitp, S1, STOP);
		/* End Transmission */
		envctrl_send_byte(unitp, S0, slave_addr);
		envctrl_send_byte(unitp, S1, STOP);
		envctrl_send_byte(unitp, S1, START);
		break;
	default:
		break;
	}

	/* End Transmission */
	envctrl_send_byte(unitp, S1, STOP);
}

static int
envctrl_recv_byte(struct envctrlunit *unitp, u_char *reg)
{

	int x = 0;
	u_char csr;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	/* STEP 4: POLL for PIN bit = 0 , 0 indicates OK */

	csr = ddi_getb(unitp->ctlr_handle, &unitp->bus_ctl_regs->s1);

	while ((csr & CSRS1_PIN) && x < 1000) {
		drv_usecwait(10);
		csr = ddi_getb(unitp->ctlr_handle, S1);
		x++;
	}

	x = 0;

	/* STEP 6: SLAVE ACKED ?? LRB == 0 */

	csr = ddi_getb(unitp->ctlr_handle, &unitp->bus_ctl_regs->s1);

	if (((csr & CSRS1_LRB) != 0)) {
		if (envctrl_debug_flags) {
			cmn_err(CE_WARN,
			    "envctrl_recv_byte: slave LRB ack 1 failed\n");
		}
		csr = CSRS1_PIN | CSRS1_ESO | CSRS1_STO | CSRS1_ACK;
		ddi_putb(unitp->ctlr_handle, S1, csr);
		return (-1);
	}

	return (ddi_getb(unitp->ctlr_handle, reg));
}

static int
envctrl_get_ps_temp(struct envctrlunit *unitp, u_char psaddr)
{
	u_char tempr, slave_addr, recv_data;
	int i;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	/*
	 * This routine algorithm is as follows,
	 * we first set the auto increment bit in the
	 * PCF8591 for the 4 input ports. Then we read
	 * each input port, this will indicate the
	 * RAW thermistor value within the supply
	 * We will return the highest value, of the
	 * 4 read values. This value will be used for
	 * setting the supply fanspeeds.
	 */

	tempr = 0;

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, psaddr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* send auto increment bit */
	envctrl_send_byte(unitp, S1, PCF8591_AUTO_INCR);

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);

	slave_addr = psaddr | PCF8591_READ_BIT;
	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, slave_addr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* this should be salve addr */
	recv_data = envctrl_recv_byte(unitp, S0);
	if (recv_data != slave_addr && envctrl_debug_flags) {
		cmn_err(CE_WARN, "envctrl PS TEMP read failed\n");
	}

	/* 4 is the hardwired number of ports on the PCF8591 */
	for (i = 0; i < PCF8591_MAX_PORTS; i++) {
		recv_data = envctrl_recv_byte(unitp, S0);
		if (recv_data > tempr) {
			tempr = recv_data;
		}
	}
	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);

	/*
	 * If we don't re-address the chip in write mode and
	 * send another start and stop the SDA line will remain
	 * low causing problems.
	 */

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, psaddr);
	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);
	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);
	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);
	return (tempr);
}

static int
envctrl_get_cpu_temp(struct envctrlunit *unitp, int cpunum)
{
	u_char tempr, slave_addr, recv_data, psaddr;
	int i;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	/*
	 * This routine takes in the number of the port that
	 * we want to read in the 8591. This should be the
	 * location of the COU thermistor for one of the 4
	 * cpu's. It will return the temperature in degrees C
	 * to the caller.
	 */

	tempr = 0;
	psaddr = ENVCTRL_CPU_PCF8591_ADDR;

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, psaddr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* send auto increment bit */
	envctrl_send_byte(unitp, S1, PCF8591_AUTO_INCR);

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);

	slave_addr = psaddr | PCF8591_READ_BIT;
	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, slave_addr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* this should be salve addr */
	recv_data = envctrl_recv_byte(unitp, S0);
	if (recv_data != slave_addr && envctrl_debug_flags) {
		cmn_err(CE_WARN, "envctrl PS TEMP read failed\n");
	}

	/*
	 * 4 is the hardwired number of ports on the PCF8591
	 * so cpunum shpuldn't ever exceed 4. If it does the read will
	 * just wrap around. e.g. 5 would be port 1.
	 */
	for (i = 0; i < cpunum; i++) {
		recv_data = envctrl_recv_byte(unitp, S0);
		if (recv_data > tempr) {
			tempr = recv_data;
		}
	}
	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);

	/*
	 * If we don't re-address the chip in write mode and
	 * send another start and stop the SDA line will remain
	 * low causing problems.
	 */

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, psaddr);
	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);
	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);
	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);
	return (tempr);
}

static int
envctrl_get_lm75_temp(struct envctrlunit *unitp)
{

	int k;
	ushort lmval;
	u_char tmp1;
	u_char tmp2;
	u_char slave_addr, recv_data;


	ASSERT(MUTEX_HELD(&unitp->umutex));

	slave_addr = LM75_BASE_ADDR | LM75_CONFIG_ADDRA | LM75_READ_BIT;

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, slave_addr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* this should be salve addr */
	recv_data = envctrl_recv_byte(unitp, S0);
	if (recv_data != slave_addr && envctrl_debug_flags) {
		cmn_err(CE_WARN, "envctrl LM75 TEMP read failed\n");
	}

	/* this is not a typo 2 reads necessary */
	tmp1 = envctrl_recv_byte(unitp, S0);
	tmp2 = envctrl_recv_byte(unitp, S0);

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);

	/*
	 * Store the forst 8 bits in the upper nibble of the
	 * short, then store the lower 8 bits in the lower nibble
	 * of the short, shift 7 to the right to get the 9 bit value
	 * that the lm75 is really sending.
	 */
	lmval = tmp1 << 8;
	lmval = (lmval | tmp2);
	lmval = (lmval >> 7);
	/*
	 * Check the 9th bit to see if it is a negative
	 * temperature. If so change into 2's compliment
	 * and divide by 2 since each value is equal to a
	 * half degree strp in degrees C
	 */
	if (lmval & LM75_COMP_MASK) {
		tmp1 = (lmval & LM75_COMP_MASK_UPPER);
		tmp1 = -tmp1;
		tmp1 = tmp1/2;
		k = 0 - tmp1;
	} else {
		k = lmval /2;
	}
		return (k);
}


static void
envctrl_tempr_poll(void)
{
	struct envctrlunit *unitp;

	unitp = (struct envctrlunit *)ddi_get_soft_state(envctrlsoft_statep, 0);

	mutex_enter(&unitp->umutex);

	unitp->current_mode = ENVCTRL_NORMAL_MODE;
	envctrl_get_sys_temperatures(unitp, (u_char *)NULL);

	/* now have this thread sleep for a while */
	unitp->timeout_id = (timeout(envctrl_tempr_poll,
	    NULL, overtemp_timeout_hz));

	mutex_exit(&unitp->umutex);
}

static void
envctrl_led_blink(void)
{
	struct envctrlunit *unitp;
	struct envctrl_pcf8574_chip fspchip;

	unitp = (struct envctrlunit *)ddi_get_soft_state(envctrlsoft_statep, 0);

	mutex_enter(&unitp->umutex);

	fspchip.type = PCF8574A;
	fspchip.chip_num = ENVCTRL_PCF8574_DEV6; /* 0x01 port 1 */
	envctrl_recv(unitp, (caddr_t *)&fspchip, PCF8574);

	if (unitp->present_led_state == B_TRUE) {
		/*
		 * Now we need to "or" in fault bits of the FSP
		 * module for the mass storage fault led.
		 * and set it.
		 */
		fspchip.val = (fspchip.val & ~(ENVCTRL_PCF8574_PORT4));
		unitp->present_led_state = B_FALSE;
	} else {
		fspchip.val = (fspchip.val | ENVCTRL_PCF8574_PORT4);
		unitp->present_led_state = B_TRUE;
	}

	envctrl_xmit(unitp, (caddr_t *)&fspchip, PCF8574);

	/* now have this thread sleep for a while */
	unitp->blink_timeout_id = (timeout(envctrl_led_blink,
	    NULL, blink_timeout_hz));

	mutex_exit(&unitp->umutex);
}

/* called with mutex held */
static void
envctrl_get_sys_temperatures(struct envctrlunit *unitp, u_char *diag_tempr)
{
	int temperature, tmptemp, cputemp;
	int i;
	struct envctrl_tda8444t_chip fan;
	u_char psaddr[] = {PSTEMP1, PSTEMP2, PSTEMP3, PSTEMP0};
	u_char noval = NULL;
	u_char fspval;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	fan.fan_num = ENVCTRL_CPU_FANS;
	fan.chip_num = ENVCTRL_TDA8444T_DEV7;

	tmptemp = 0;	/* Right init value ?? */

	/*
	 * THis routine is caled once every minute
	 * we wil re-se the watchdog timer each time
	 * we poll the temps. The watchdog timer is
	 * set up for 3 minutes. Should the kernel thread
	 * wedge, for some reason the watchdog will go off
	 * and blast the fans.
	 */

	if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
		unitp->current_mode = ENVCTRL_NORMAL_MODE;
		envctrl_reset_watchdog(unitp, &noval);
		unitp->current_mode = ENVCTRL_DIAG_MODE;
	} else {
		envctrl_reset_watchdog(unitp, &noval);
	}

	/*
	 * we need to reset the dflop to allow the fans to be
	 * set if the watchdog goes of and the kernel resumes
	 * resetting the dflop alos resets the device interrupts
	 * we need to reenable them also.
	 */
	envctrl_reset_dflop(unitp);

	envctrl_enable_devintrs(unitp);

	/*
	 * If we are in diag mode we allow the system to be
	 * faked out as to what the temperature is
	 * to see if the fans speed up.
	 */
	if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
		if (unitp->timeout_id != 0) {
		    (void) untimeout(unitp->timeout_id);
		}

		temperature = *diag_tempr;
		unitp->timeout_id = (timeout(envctrl_tempr_poll,
		    NULL, overtemp_timeout_hz));
	} else {
		temperature = envctrl_get_lm75_temp(unitp);
		/*
		 * Sometimes when we read the temp it comes back bogus
		 * to fix this we just need to reset the envctrl bus
		 */
		if (temperature == -100) {
			mutex_exit(&unitp->umutex);
			envctrl_init_bus(unitp);
			mutex_enter(&unitp->umutex);
			temperature = envctrl_get_lm75_temp(unitp);
		}
	}

	fspval = envctrl_get_fpm_status(unitp);

	if (temperature < 0) {
		fan.val = 1;		/* blast it is out of range */
	} else if (temperature > MAX_AMB_TEMP) {
		fan.val = 1;
		fspval |= (ENVCTRL_FSP_TEMP_ERR | ENVCTRL_FSP_GEN_ERR);
	} else {
		fan.val = fan_speed[temperature];
		fspval &= ~(ENVCTRL_FSP_TEMP_ERR | ENVCTRL_FSP_GEN_ERR);
	}
	envctrl_set_fsp(unitp, &fspval);

	/*
	 * Update temperature kstats. FSP kstats are updated in the
	 * set and get routine.
	 */

	unitp->fan_kstats[ENVCTRL_FAN_TYPE_CPU].fanspeed = fan.val;
	envctrl_mod_encl_kstats(unitp, ENVCTRL_ENCL_AMBTEMPR, INSTANCE_0,
	    temperature);

	envctrl_xmit(unitp, (caddr_t *)&fan, TDA8444T);

#ifdef DEBUG
	if (envctrl_afb_present) {
		fan.val = AFB_MAX;
	} else {
		fan.val = AFB_MIN;
	}
#else
	if (unitp->AFB_present) {
		fan.val = AFB_MAX;
	} else {
		fan.val = AFB_MIN;
	}
#endif
	unitp->fan_kstats[ENVCTRL_FAN_TYPE_AFB].fanspeed = fan.val;
	fan.fan_num = ENVCTRL_AFB_FANS;
	envctrl_xmit(unitp, (caddr_t *)&fan, TDA8444T);

	/*
	 * Now set the Powersupply fans
	 */

	temperature = 0;
	for (i = 0; i <= MAXPS; i++) {
		if (unitp->ps_present[i]) {
			tmptemp = envctrl_get_ps_temp(unitp, psaddr[i]);
			unitp->ps_kstats[i].ps_tempr = (u_char)tmptemp;
			if (tmptemp > temperature) {
				temperature = tmptemp;
			}
		}
	}

	for (i = 0; i < unitp->num_cpus_present; i++) {
		cputemp = envctrl_get_cpu_temp(unitp, i);
#ifdef lint 	/* XXX fix me for P1 hardware */
		cputemp = cputemp;
#endif
	}

	fan.fan_num = ENVCTRL_PS_FANS;
	fan.val = ps_temps[temperature];
	/*
	 * XXX add in error condition for ps overtemp
	 */
	unitp->fan_kstats[ENVCTRL_FAN_TYPE_PS].fanspeed = fan.val;
	envctrl_xmit(unitp, (caddr_t *)&fan, TDA8444T);
}

/* called with mutex held */
static void
envctrl_fan_fail_service(struct envctrlunit *unitp)
{
	u_char chip_addr, recv_data, fpmstat;
	struct envctrl_tda8444t_chip fan;
	u_char i;
	int fantype;
	int fanflt = 0;
	int fan_failed = 0;

	/*
	 * The fan fail sensor is located at address 0x70
	 * on the envctrl bus.
	 */

	ASSERT(MUTEX_HELD(&unitp->umutex));

	chip_addr = (PCF8574A_BASE_ADDR | ENVCTRL_PCF8574_DEV4 |
	PCF8574_READ_BIT);

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, chip_addr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* this should be salve addr */
	recv_data = envctrl_recv_byte(unitp, S0);
	if (recv_data != chip_addr && envctrl_debug_flags) {
		cmn_err(CE_WARN, "FAN FAILFAILED received 0x%x\n",
			recv_data);
	}

	/*
	 * The intr cond should get cleared now
	 * just by reading the chip that caused the
	 * intr.
	 * ports are normally high or 0x01 state for
	 * 8 bits. Read the ports and see who is in
	 * trouble.
	 */

	recv_data = envctrl_recv_byte(unitp, S0);

	/* Stop bus */
	envctrl_send_byte(unitp, S1, STOP);
	envctrl_send_byte(unitp, S1, STOP);


	/*
	 * If all fan ports are high (0xff) then we don't have any
	 * fan faults. Reset the kstats
	 */
	if (recv_data & 0xff) {
		unitp->fan_kstats[ENVCTRL_FAN_TYPE_PS].fans_ok = B_TRUE;
		unitp->fan_kstats[ENVCTRL_FAN_TYPE_CPU].fans_ok = B_TRUE;
		unitp->fan_kstats[ENVCTRL_FAN_TYPE_AFB].fans_ok = B_TRUE;
		unitp->fan_kstats[ENVCTRL_FAN_TYPE_PS].fanflt_num = B_TRUE;
		unitp->fan_kstats[ENVCTRL_FAN_TYPE_CPU].fanflt_num = B_TRUE;
		unitp->fan_kstats[ENVCTRL_FAN_TYPE_AFB].fanflt_num = B_TRUE;
	}

	fantype = ENVCTRL_FAN_TYPE_PS;

	if (!(recv_data & ENVCTRL_PCF8574_PORT0)) {
		fanflt = PS_FAN_1;
		fan_failed++;
	}
	if (!(recv_data & ENVCTRL_PCF8574_PORT1)) {
		fanflt = PS_FAN_2;
		fan_failed++;
	}
	if (!(recv_data & ENVCTRL_PCF8574_PORT2)) {
		fanflt = PS_FAN_3;
		fan_failed++;
	}

	if (fan_failed != 0) {
		unitp->fan_kstats[fantype].fans_ok = B_FALSE;
		unitp->fan_kstats[fantype].fanflt_num = fanflt;

		fpmstat = envctrl_get_fpm_status(unitp);
		fpmstat |= ENVCTRL_FSP_GEN_ERR;
		envctrl_set_fsp(unitp, &fpmstat);
	}

	fantype = ENVCTRL_FAN_TYPE_CPU;

	if (!(recv_data & ENVCTRL_PCF8574_PORT3)) {
		fanflt = CPU_FAN_1;
		fan_failed++;
	}
	if (!(recv_data & ENVCTRL_PCF8574_PORT4)) {
		fanflt = CPU_FAN_2;
		fan_failed++;
	}
	if (!(recv_data & ENVCTRL_PCF8574_PORT5)) {
		fanflt = CPU_FAN_3;
		fan_failed++;
	}
	if (!(recv_data & ENVCTRL_PCF8574_PORT6)) {
		fanflt = CPU_FAN_3;
		fan_failed++;
	}

	if (fanflt != 0) {
		unitp->fan_kstats[fantype].fans_ok = B_FALSE;
		unitp->fan_kstats[fantype].fanflt_num = fanflt;
		fpmstat = envctrl_get_fpm_status(unitp);
		fpmstat |= ENVCTRL_FSP_GEN_ERR;
		envctrl_set_fsp(unitp, &fpmstat);
	}

	if (!(recv_data & ENVCTRL_PCF8574_PORT7)) {
		/*
		 * If AFB is NOT present and its fan fails
		 * just log the error.
		 */
		if (unitp->AFB_present) {
			fan_failed++;
			unitp->fan_kstats[ENVCTRL_FAN_TYPE_AFB].fans_ok
			    = B_FALSE;
			unitp->fan_kstats[ENVCTRL_FAN_TYPE_AFB].fanflt_num =
			    AFB_FAN_1;
		}
	}


	/*
	 * If one fan fails then we want to set the fans to high
	 * If 2 fans fail then we will log the error and power off the
	 * system.
	 * If the AFB is present and it's fan fails then we Max out
	 * the fans on the cpu side. If the AFB isn't present and its
	 * fan fails then we do nothing but note the failure.
	 */

	if (envctrl_fan_debug) {
		fan_failed = envctrl_fan_debug;
	}

	if (fan_failed == 1 &&
	    unitp->current_mode == ENVCTRL_NORMAL_MODE) {
		fan.val = MAX_FAN_SPEED;
		fan.chip_num = TDA8444T_BASE_ADDR | ENVCTRL_TDA8444T_DEV7;

		for (i = ENVCTRL_CPU_FANS; i <= ENVCTRL_PORT7; i++) {
			fan.fan_num = i;
			envctrl_xmit(unitp, (caddr_t *)&fan, TDA8444T);
		}
		for (i = 0; i < unitp->num_fans_present; i++) {
			unitp->fan_kstats[i].fanspeed = MAX_FAN_SPEED;
		}
	} else if (fan_failed >= 2 &&
	    unitp->current_mode == ENVCTRL_NORMAL_MODE) {
		/*  poweroff the system */
		if (!(envctrl_power_off_overide)) {
			power_down("Multiple Fan Failure Shutdown");
		}
	}

	/*
	 * If the fans came back online somehow clear the error
	 */
	if (fan_failed == 0) {
		fpmstat = envctrl_get_fpm_status(unitp);
		fpmstat &= ~(ENVCTRL_FSP_GEN_ERR);
		envctrl_set_fsp(unitp, &fpmstat);
	}

	/*
	 * set this after we turn on the fans to blast because
	 * if the unitp number is > 0, we just return from the
	 * xmit routine.
	 */
	unitp->num_fans_failed = fan_failed;

}

/*
 * Check for power supply insertion and failure.
 * This is a bit tricky, because a power supply insertion will
 * trigger a load share interrupt as well as PS present in the
 * new supply. if we detect an insertion clear
 * interrupts, disable interrupts, wait for a couple of seconds
 * come back and see if the PSOK bit is set, PS_PRESENT is set
 * and the share fail interrupts are gone. If not this is a
 * real load share fail event.
 * Called with mutex held
 */

static void
envctrl_PS_intr_service(struct envctrlunit *unitp, u_char psaddr)
{
	u_char chip_addr, recv_data;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	chip_addr = (PCF8574A_BASE_ADDR | psaddr | PCF8574_READ_BIT);

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, chip_addr);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/* this should be salve addr */
	recv_data = envctrl_recv_byte(unitp, S0);
	if (recv_data != chip_addr && envctrl_debug_flags) {
		cmn_err(CE_WARN, "PS INTR received 0x%x\n",
			    recv_data);
	}

	/*
	 * The intr cond should get cleared now
	 * just by reading the chip that caused the
	 * intr.
	 * ports are normally high or 0x01 state for
	 * 8 bits. Read the ports and see who is in
	 * trouble.
	 */

	recv_data = envctrl_recv_byte(unitp, S0);

	/* Stop bus */
	envctrl_send_byte(unitp, S1, STOP);

	envctrl_ps_probe(unitp);

}

/* called with mutex held */
static void
envctrl_reset_dflop(struct envctrlunit *unitp)
{
	struct envctrl_pcf8574_chip initval;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	/*
	 * This initialization sequence allows a
	 * to change state to stop the fans from
	 * blastion upon poweron. If this isn't
	 * done the writes to the 8444 will not complete
	 * to the hardware because the dflop will
	 * be closed
	 */
	initval.chip_num = ENVCTRL_PCF8574_DEV0; /* 0x01 port 1 */
	initval.type = PCF8574A;

	initval.val = ENVCTRL_DFLOP_INIT0;
	envctrl_xmit(unitp, (caddr_t *)&initval, PCF8574);

	initval.val = ENVCTRL_DFLOP_INIT1;
	envctrl_xmit(unitp, (caddr_t *)&initval, PCF8574);
}

static void
envctrl_add_encl_kstats(struct envctrlunit *unitp, int type,
    int instance, u_char val)
{
	int i = 0;
	boolean_t inserted = B_FALSE;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	while (i < MAX_DEVS && inserted == B_FALSE) {
		if (unitp->encl_kstats[i].instance == I2C_NODEV) {
			unitp->encl_kstats[i].instance = instance;
			unitp->encl_kstats[i].type = type;
			unitp->encl_kstats[i].value = val;
			inserted = B_TRUE;
		}
		i++;
	}
	unitp->num_encl_present++;
}

/* called with mutex held */
static void
envctrl_enable_devintrs(struct envctrlunit *unitp)
{
	struct envctrl_pcf8574_chip initval;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	/*
	 * This initialization sequence allows a
	 * to change state to stop the fans from
	 * blastion upon poweron. If this isn't
	 * done the writes to the 8444 will not complete
	 * to the hardware because the dflop will
	 * be closed
	 */
	initval.chip_num = ENVCTRL_PCF8574_DEV0; /* 0x01 port 1 */
	initval.type = PCF8574A;

	initval.val = ENVCTRL_DEVINTR_INTI0;
	envctrl_xmit(unitp, (caddr_t *)&initval, PCF8574);

	/*
	 * set lowerbits all high p0 = PS1, p1 = PS2
	 * p2 = PS3 p4 = envctrl intr_ctrl
	 */
	initval.val = ENVCTRL_DEVINTR_INTI1;
	envctrl_xmit(unitp, (caddr_t *)&initval, PCF8574);
}

/* called with mutex held */
static void
envctrl_stop_clock(struct envctrlunit *unitp)
{

	/*
	 * This routine talks to the PCF8583 which
	 * is a clock calendar chip on the envctrl bus.
	 * We use this chip as a watchdog timer for the
	 * fan control. At reset this chip pulses the interrupt
	 * line every 1 second. We need to be able to shut
	 * this off.
	 */

	ASSERT(MUTEX_HELD(&unitp->umutex));

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, PCF8583_BASE_ADDR);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	envctrl_send_byte(unitp, S0, CLOCK_CSR_REG);
	envctrl_send_byte(unitp, S0, CLOCK_DISABLE);

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);


}

static void
envctrl_reset_watchdog(struct envctrlunit *unitp, u_char *wdval)
{

	u_char w, r;
	u_char res = 0;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	/* the clock MUST be stopped before we re-set it */
	envctrl_stop_clock(unitp);

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, PCF8583_BASE_ADDR);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	/*
	 * set up the alarm timer for 3 minutes
	 * start by setting reg 8 ALARM_CTRL_REG
	 * If we are in diag mode, we set the timer in
	 * seconds. Valid values are 40-99. The timer
	 * counts up to 99. 40 would be 59 seconds
	 */
	envctrl_send_byte(unitp, S0, CLOCK_ALARM_REG_A);
	if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
		if (unitp->timeout_id != 0) {
			(void) untimeout(unitp->timeout_id);
			unitp->timeout_id = 0;
			unitp->timeout_id = (timeout(envctrl_tempr_poll,
			    NULL, overtemp_timeout_hz));
		}
		envctrl_send_byte(unitp, S0, CLOCK_ENABLE_TIMER_S);
	} else {
		envctrl_send_byte(unitp, S0, CLOCK_ENABLE_TIMER);
	}

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);

	/*
	 * Now set up the alarm timer register it
	 * counts from 0-99 with an intr triggered
	 * when it gets to overflow.. or 99. It will
	 * also count from a pre-set value which is
	 * where we are seting from. We want a 3 minute fail
	 * safe so our value is 99-3 or 96.
	 * we are programming register 7 in the 8583.
	 */

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, PCF8583_BASE_ADDR);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	envctrl_send_byte(unitp, S0, ALARM_CTRL_REG);
	/*
	 * Allow the diagnostic to set the egg timer val.
	 * never allow it to be set greater than the default.
	 */
	if (unitp->current_mode == ENVCTRL_DIAG_MODE) {
		if (*wdval < DIAG_MAX_TIMER_VAL || *wdval > MAX_CL_VAL) {
			envctrl_send_byte(unitp, S0, EGG_TIMER_VAL);
		} else {

		w = *wdval/10;
		r = *wdval%10;

		res = res | r;
		res = (0x99 - (res | (w << 4)));
		envctrl_send_byte(unitp, S0, res);
		}
	} else {
		envctrl_send_byte(unitp, S0, EGG_TIMER_VAL);
	}

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);

	/*
	 * Now that we have set up.. it is time
	 * to re-start the clock in the CSR.
	 */

	/* STEP 1: Load Slave Address into S0 */
	envctrl_send_byte(unitp, S0, PCF8583_BASE_ADDR);

	/* STEP 2: Generate start condition 0xC5 */
	envctrl_send_byte(unitp, S1, START);

	envctrl_send_byte(unitp, S0, CLOCK_CSR_REG);
	envctrl_send_byte(unitp, S0, CLOCK_ENABLE);

	/* STEP 10: End Transmission */
	envctrl_send_byte(unitp, S1, STOP);
}

/* Called with unip mutex held */
static void
envctrl_ps_probe(struct envctrlunit *unitp)
{

	u_char chip_addr, recv_data, fpmstat;
	u_char psaddr[] = {PS1, PS2, PS3, PSTEMP0};
	int i;
	int ps_error = 0;


	ASSERT(MUTEX_HELD(&unitp->umutex));

	unitp->ps_present[0] = B_FALSE;
	unitp->num_ps_present = 0;

	for (i = 0; i <= MAXPS; i++) {
		unitp->ps_present[i] = B_FALSE;
		chip_addr = (psaddr[i] | PCF8574_READ_BIT);

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, chip_addr);

		/* STEP 2: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);

		/* this should be salve addr */
		recv_data = envctrl_recv_byte(unitp, S0);
		if (recv_data != chip_addr) {
			cmn_err(CE_WARN, "PS %d Probe failed\n", i);
			return;
		}
		recv_data = envctrl_recv_byte(unitp, S0);
		/* Stop bus */
		envctrl_send_byte(unitp, S1, STOP);
		/*
		 * Port 0 = PS Present
		 * Port 1 = PS Type
		 * Port 2 = PS Type
		 * Port 3 = PS TYpe
		 * Port 4 = DC Status
		 * Port 5 = Current Limit
		 * Port 6 = Current Share
		 * Port 7 = SPARE
		 */

		/*
		 * Port 0 = PS Present
		 * Port is pulled LOW "0" to indicate
		 * present.
		 */

		if (!(recv_data & ENVCTRL_PCF8574_PORT0)) {
			unitp->ps_present[i] = B_TRUE;
			/* update unit kstat array */
			unitp->ps_kstats[i].instance = i;
			unitp->ps_kstats[i].ps_tempr = ENVCTRL_INIT_TEMPR;
			++unitp->num_ps_present;
		}

		if (!(recv_data & ENVCTRL_PCF8574_PORT1)) {
			unitp->ps_kstats[i].ps_rating = ENVCTRL_PS_550;
		}
		if (!(recv_data & ENVCTRL_PCF8574_PORT2)) {
			unitp->ps_kstats[i].ps_rating = ENVCTRL_PS_650;
		}
		if (!(recv_data & ENVCTRL_PCF8574_PORT3)) {
			if (envctrl_debug_flags) {
				cmn_err(CE_WARN, "ERROR Envctrl PS Type 3\n");
			}
		}
		if (!(recv_data & ENVCTRL_PCF8574_PORT4)) {
			unitp->ps_kstats[i].ps_ok = B_FALSE;
			ps_error++;
		} else {
			unitp->ps_kstats[i].ps_ok = B_TRUE;
		}
		if (!(recv_data & ENVCTRL_PCF8574_PORT5)) {
			unitp->ps_kstats[i].limit_ok = B_FALSE;
			ps_error++;
		} else {
			unitp->ps_kstats[i].limit_ok = B_TRUE;
		}
		if (!(recv_data & ENVCTRL_PCF8574_PORT6)) {
			unitp->ps_kstats[i].curr_share_ok = B_FALSE;
			ps_error++;
		} else {
			unitp->ps_kstats[i].curr_share_ok = B_TRUE;
		}

		if (!(recv_data & ENVCTRL_PCF8574_PORT7)) {
			cmn_err(CE_WARN, "PS %d Shouln't interrupt\n", i);
			ps_error++;
		}
	}

	fpmstat = envctrl_get_fpm_status(unitp);
	if (ps_error) {
		fpmstat |= (ENVCTRL_FSP_PS_ERR | ENVCTRL_FSP_GEN_ERR);
	} else {
		fpmstat &= ~(ENVCTRL_FSP_PS_ERR | ENVCTRL_FSP_GEN_ERR);
	}
	envctrl_set_fsp(unitp, &fpmstat);

	/*
	 * We need to reset all of the fans etc when a supply is
	 * interrupted and added
	 */
	envctrl_get_sys_temperatures(unitp, (u_char *)NULL);
}

/*
 * consider key switch position when handling an abort sequence
 */
static void
envctrl_abort_seq_handler(char *msg)
{
	struct envctrlunit *unitp;
	u_char secure = 0;

	unitp = (struct envctrlunit *)ddi_get_soft_state(envctrlsoft_statep, 0);

	mutex_enter(&unitp->umutex);

	secure = envctrl_get_fpm_status(unitp);
	/*
	 * take the logical not because we are in hardware mode only
	 */

	if ((secure & ENVCTRL_FSP_KEYMASK) == ENVCTRL_FSP_KEYLOCKED) {
			cmn_err(CE_CONT,
			    "!envctrl: ignoring debug enter sequence\n");
	} else {
		if (envctrl_debug_flags) {
			cmn_err(CE_CONT, "!envctrl: allowing debug enter\n");
		}
		debug_enter(msg);
	}
	mutex_exit(&unitp->umutex);
}

/*
 * get the front Panel module LED and keyswitch status.
 * this part is addressed at 0x7C on the i2c bus.
 * called with mutex held
 */
static u_char
envctrl_get_fpm_status(struct envctrlunit *unitp)
{
	u_char chip_addr;
	u_char status = 0;

	ASSERT(MUTEX_HELD(&unitp->umutex));

		chip_addr = (PCF8574A_BASE_ADDR | ENVCTRL_PCF8574_DEV6 |
		PCF8574_READ_BIT);

		/* STEP 1: Load Slave Address into S0 */
		envctrl_send_byte(unitp, S0, chip_addr);

		/* STEP 2: Generate start condition 0xC5 */
		envctrl_send_byte(unitp, S1, START);

		/* this should be salve addr */
		status = envctrl_recv_byte(unitp, S0);
		if (status != chip_addr) {
			envctrl_send_byte(unitp, S1, STOP);
			envctrl_send_byte(unitp, S0, chip_addr);
			envctrl_send_byte(unitp, S1, STOP);
			envctrl_send_byte(unitp, S1, START);
			envctrl_send_byte(unitp, S1, STOP);
			status = 0;
			envctrl_mod_encl_kstats(unitp, ENVCTRL_ENCL_FSP,
			    INSTANCE_0, status);
			if (envctrl_debug_flags)
				cmn_err(CE_WARN, "FPM Probe failed\n");
			return (DDI_SUCCESS);
		}
		status = envctrl_recv_byte(unitp, S0);

		/*
		 * yet another place where a read can cause the
		 * the SDA line of the i2c bus to get stuck low.
		 * this funky sequence frees the SDA line.
		 */
		envctrl_send_byte(unitp, S1, STOP);
		envctrl_send_byte(unitp, S0, chip_addr);
		envctrl_send_byte(unitp, S1, STOP);
		envctrl_send_byte(unitp, S1, START);
		envctrl_send_byte(unitp, S1, STOP);
		status = ~status;
		envctrl_mod_encl_kstats(unitp, ENVCTRL_ENCL_FSP,
		    INSTANCE_0, status);

	return (status);
}

static void
envctrl_set_fsp(struct envctrlunit *unitp, u_char *val)
{
	struct envctrl_pcf8574_chip chip;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	chip.val = ENVCTRL_FSP_OFF; /* init all values to off */
	chip.chip_num = ENVCTRL_PCF8574_DEV6; /* 0x01 port 1 */
	chip.type = PCF8574A;

	/*
	 * strip off bits that are R/O
	 */
	chip.val = (~(ENVCTRL_FSP_KEYMASK | ENVCTRL_FSP_POMASK) & (*val));

	chip.val = ~chip.val;
	envctrl_xmit(unitp, (caddr_t *)&chip, PCF8574);

}

static int
envctrl_get_dskled(struct envctrlunit *unitp, struct envctrl_pcf8574_chip *chip)
{
	u_int oldtype;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	if (chip->chip_num > ENVCTRL_PCF8574_DEV2 ||
		chip->chip_num < ENVCTRL_PCF8574_DEV0 ||
		chip->type != ENVCTRL_ENCL_BACKPLANE4 &&
		chip->type != ENVCTRL_ENCL_BACKPLANE8) {
		return (DDI_FAILURE);
	}
	oldtype = chip->type;
	chip->type = PCF8574;
	envctrl_recv(unitp, (caddr_t *)chip, PCF8574);
	chip->type = oldtype;
	chip->val = ~chip->val;

	return (DDI_SUCCESS);
}
static int
envctrl_set_dskled(struct envctrlunit *unitp, struct envctrl_pcf8574_chip *chip)
{

	struct envctrl_pcf8574_chip fspchip;
	struct envctrl_pcf8574_chip backchip;
	int i, instance;
	int diskfault = 0;
	u_char controller_addr[] = {ENVCTRL_PCF8574_DEV0,
	    ENVCTRL_PCF8574_DEV2};

	/*
	 * We need to check the type of disk led being set. If it
	 * is a 4 slot backplane then the upper 4 bits (7, 6, 5, 4) are
	 * invalid.
	 */
	ASSERT(MUTEX_HELD(&unitp->umutex));


	if (chip->chip_num > ENVCTRL_PCF8574_DEV2 ||
		chip->chip_num < ENVCTRL_PCF8574_DEV0 ||
		chip->val > ENVCTRL_DISK8LED_ALLOFF ||
		chip->val < ENVCTRL_CHAR_ZERO) {
		return (DDI_FAILURE);
	}

	if (chip->type != ENVCTRL_ENCL_BACKPLANE4 &&
	    chip->type != ENVCTRL_ENCL_BACKPLANE8) {
		return (DDI_FAILURE);
	}

	/*
	 * setting all of the bits to 0x00 is telling us that
	 * the user wants to clear the fault condition. We need to now
	 * check all of the other controllwes LED states to make sure
	 * that there are no disk faults. If so then we can turn off
	 * the mass storage fault led.
	 */

	if (chip->val & ENVCTRL_DISK4LED_ALLOFF ||
		chip->val & ENVCTRL_DISK8LED_ALLOFF) {
		backchip.type = PCF8574;
		for (i = 0; i < MAX_TAZ_CONTROLLERS; i++) {
			backchip.chip_num = controller_addr[i];
			envctrl_recv(unitp, (caddr_t *)&backchip, PCF8574);
			if (backchip.val != ENVCTRL_CHAR_ZERO) {
				diskfault++;
			}
		}
	}

	fspchip.type = PCF8574A;
	fspchip.chip_num = ENVCTRL_PCF8574_DEV6; /* 0x01 port 1 */
	envctrl_recv(unitp, (caddr_t *)&fspchip, PCF8574);

	if (diskfault) {
		/*
		 * Now we need to "or" in fault bits of the FSP
		 * module for the mass storage fault led.
		 * and set it.
		 */
		fspchip.val = (fspchip.val &
		    ~(ENVCTRL_FSP_DISK_ERR | ENVCTRL_FSP_GEN_ERR));
	} else {
		fspchip.val = (fspchip.val |
		    (ENVCTRL_FSP_DISK_ERR | ENVCTRL_FSP_GEN_ERR));
	}
	fspchip.type = PCF8574A;
	fspchip.chip_num = ENVCTRL_PCF8574_DEV6; /* 0x01 port 1 */
	envctrl_xmit(unitp, (caddr_t *)&fspchip, PCF8574);

	for (i = 0; i < (sizeof (backaddrs) / sizeof (u_char)); i++) {
		if (chip->chip_num == backaddrs[i]) {
			instance =  i;
		}
	}

	switch (chip->type) {
	case ENVCTRL_ENCL_BACKPLANE4:
		envctrl_mod_encl_kstats(unitp, ENVCTRL_ENCL_BACKPLANE4,
		    instance, chip->val);
		break;
	case ENVCTRL_ENCL_BACKPLANE8:
		envctrl_mod_encl_kstats(unitp, ENVCTRL_ENCL_BACKPLANE8,
		    instance, chip->val);
		break;
	default:
		break;
	}
	chip->type = PCF8574;
	/*
	 * we take the ones compliment of the val passed in
	 * because the hardware thinks that a "low" or "0"
	 * is the way to indicate a fault. of course software
	 * knows that a 1 is a TRUE state or fault. ;-)
	 */
	chip->val = ~(chip->val);
	envctrl_xmit(unitp, (caddr_t *)chip, PCF8574);
	return (DDI_SUCCESS);
}

void
envctrl_add_kstats(struct envctrlunit *unitp)
{

	ASSERT(MUTEX_HELD(&unitp->umutex));

	if ((unitp->enclksp = kstat_create(ENVCTRL_MODULE_NAME, unitp->instance,
	    ENVCTRL_KSTAT_ENCL, "misc", KSTAT_TYPE_RAW,
	    sizeof (unitp->encl_kstats),
	    KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "envctrl%d: encl raw kstat_create failed",
			unitp->instance);
		return;
	}

	unitp->enclksp->ks_update = envctrl_encl_kstat_update;
	unitp->enclksp->ks_private = (void *)unitp;
	kstat_install(unitp->enclksp);


	if ((unitp->fanksp = kstat_create(ENVCTRL_MODULE_NAME, unitp->instance,
	    ENVCTRL_KSTAT_FANSTAT, "misc", KSTAT_TYPE_RAW,
	    sizeof (unitp->fan_kstats),
	    KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "envctrl%d: fans kstat_create failed",
			unitp->instance);
		return;
	}

	unitp->fanksp->ks_update = envctrl_fanstat_kstat_update;
	unitp->fanksp->ks_private = (void *)unitp;
	kstat_install(unitp->fanksp);

	if ((unitp->psksp = kstat_create(ENVCTRL_MODULE_NAME, unitp->instance,
	    ENVCTRL_KSTAT_PSNAME, "misc", KSTAT_TYPE_RAW,
	    sizeof (unitp->ps_kstats),
	    KSTAT_FLAG_PERSISTENT)) == NULL) {
		cmn_err(CE_WARN, "envctrl%d: ps name kstat_create failed",
			unitp->instance);
		return;
	}

	unitp->psksp->ks_update = envctrl_ps_kstat_update;
	unitp->psksp->ks_private = (void *)unitp;
	kstat_install(unitp->psksp);

}

int
envctrl_ps_kstat_update(kstat_t *ksp, int rw)
{
	struct envctrlunit *unitp;
	char *kstatp;



	unitp = (struct envctrlunit *)ksp->ks_private;

	mutex_enter(&unitp->umutex);
	ASSERT(MUTEX_HELD(&unitp->umutex));

	kstatp = (char *)ksp->ks_data;

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {

		unitp->psksp->ks_ndata = unitp->num_ps_present;
		bcopy((caddr_t)&unitp->ps_kstats, kstatp,
		    sizeof (unitp->ps_kstats));
	}
	mutex_exit(&unitp->umutex);
	return (DDI_SUCCESS);
}
int
envctrl_fanstat_kstat_update(kstat_t *ksp, int rw)
{
	struct envctrlunit *unitp;
	char *kstatp;

	kstatp = (char *)ksp->ks_data;
	unitp = (struct envctrlunit *)ksp->ks_private;

	mutex_enter(&unitp->umutex);
	ASSERT(MUTEX_HELD(&unitp->umutex));

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {
		unitp->fanksp->ks_ndata = unitp->num_fans_present;
		bcopy((caddr_t)unitp->fan_kstats, kstatp,
		    sizeof (unitp->fan_kstats));
	}
	mutex_exit(&unitp->umutex);
	return (DDI_SUCCESS);
}

int
envctrl_encl_kstat_update(kstat_t *ksp, int rw)
{
	struct envctrlunit *unitp;
	char *kstatp;


	kstatp = (char *)ksp->ks_data;
	unitp = (struct envctrlunit *)ksp->ks_private;

	mutex_enter(&unitp->umutex);
	ASSERT(MUTEX_HELD(&unitp->umutex));

	if (rw == KSTAT_WRITE) {
		return (EACCES);
	} else {

		unitp->enclksp->ks_ndata = unitp->num_encl_present;
		envctrl_get_fpm_status(unitp);
		/* XXX Need to ad disk updates too ??? */
		bcopy((caddr_t)unitp->encl_kstats, kstatp,
		    sizeof (unitp->encl_kstats));
	}
	mutex_exit(&unitp->umutex);
	return (DDI_SUCCESS);
}

/*
 * called with unitp lock held
 * type, fanspeed and fanflt will be set by the service routines
 */
static void
envctrl_init_fan_kstats(struct envctrlunit *unitp)
{
	int i;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	for (i = 0; i < unitp->num_fans_present; i++) {
		unitp->fan_kstats[i].instance = 0;
		unitp->fan_kstats[i].type = 0;
		unitp->fan_kstats[i].fans_ok = B_TRUE;
		unitp->fan_kstats[i].fanflt_num = B_FALSE;
		unitp->fan_kstats[i].fanspeed = B_FALSE;
	}

	unitp->fan_kstats[ENVCTRL_FAN_TYPE_PS].type = ENVCTRL_FAN_TYPE_PS;
	unitp->fan_kstats[ENVCTRL_FAN_TYPE_CPU].type = ENVCTRL_FAN_TYPE_CPU;
	unitp->fan_kstats[ENVCTRL_FAN_TYPE_AFB].type = ENVCTRL_FAN_TYPE_AFB;
}

static void
envctrl_init_encl_kstats(struct envctrlunit *unitp)
{

	int i;
	u_char val;
	struct envctrl_pcf8574_chip chip;
	int *reg_prop;
	u_int len = 0;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	for (i = 0; i < MAX_DEVS; i++) {
		unitp->encl_kstats[i].instance = I2C_NODEV;
	}

	/*
	 * add in kstats now
	 * We ALWAYS HAVE THE FOLLOWING
	 * 1. FSP
	 * 2. AMB TEMPR
	 * 3. (1) CPU TEMPR
	 * 4. (1) 4 slot disk backplane
	 * OPTIONAL
	 * 8 slot backplane
	 * more cpu's
	 */

	chip.type = PCF8574A;
	chip.chip_num = ENVCTRL_PCF8574_DEV6; /* 0x01 port 1 */
	envctrl_recv(unitp, (caddr_t *)&chip, PCF8574);

	envctrl_add_encl_kstats(unitp, ENVCTRL_ENCL_FSP, INSTANCE_0, chip.val);

	val = (u_short)envctrl_get_lm75_temp(unitp);
	envctrl_add_encl_kstats(unitp, ENVCTRL_ENCL_AMBTEMPR, INSTANCE_0, val);

	/* FIX ME FOR CPU's XXXX */
	envctrl_add_encl_kstats(unitp, ENVCTRL_ENCL_CPUTEMPR, INSTANCE_0, val);


	(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, unitp->dip,
		    DDI_PROP_DONTPASS, ENVCTRL_DISK_LEDS_PR, &reg_prop, &len);

	ASSERT(len != 0);
	chip.type = PCF8574;

	for (i = 0; i < len; i++) {
		chip.chip_num = backaddrs[i];
		if (reg_prop[i] == ENVCTRL_4SLOT_BACKPLANE) {
			envctrl_recv(unitp, (caddr_t *)&chip, PCF8574);
			envctrl_add_encl_kstats(unitp, ENVCTRL_ENCL_BACKPLANE4,
			    i, ~chip.val);
		}
		if (reg_prop[i] == ENVCTRL_8SLOT_BACKPLANE) {
			envctrl_recv(unitp, (caddr_t *)&chip, PCF8574);
			envctrl_add_encl_kstats(unitp, ENVCTRL_ENCL_BACKPLANE8,
			    i, ~chip.val);
		}
	}

}

static void
envctrl_mod_encl_kstats(struct envctrlunit *unitp, int type,
    int instance, u_char val)
{
	int i = 0;
	boolean_t inserted = B_FALSE;

	ASSERT(MUTEX_HELD(&unitp->umutex));

	while (i < MAX_DEVS && inserted == B_FALSE) {
		if (unitp->encl_kstats[i].instance == instance &&
		    unitp->encl_kstats[i].type == type) {
			unitp->encl_kstats[i].value = val;
			inserted = B_TRUE;
		}
		i++;
	}
}
