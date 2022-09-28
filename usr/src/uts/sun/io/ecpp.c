/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ecpp.c	2.37	96/10/15 SMI"

/*
 * IEEE 1284 Parallel Port Device Driver
 *
 * Todo:
 * timeout handling
 * abort handling
 * diag mode handling
 * error handling
 * postio
 * kstat
 * debug messages
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

#include <sys/ecppreg.h>	/* hw description */
#include <sys/ecppio.h>		/* ioctl description */
#include <sys/ecppvar.h>	/* driver description */
/*
* For debugging, allocate space for the trace buffer
*/
#if defined(POSTRACE)
struct postrace postrace_buffer[NPOSTRACE+1];
struct postrace *postrace_ptr;
int postrace_count;
#endif

#ifndef ECPP_DEBUG
#define	ECPP_DEBUG 0
#endif	/* ECPP_DEBUG */

#if	ECPP_DEBUG > 0
#define	ECPP_PRINT(level, args)	_STMT(if (ecpp_debug >= (level)) \
					cmn_err args; /* space */)
#else
#define	ECPP_PRINT(level, args)	/* nothing */
#endif	/* ECPP_DEBUG */
static	int ecpp_debug = ECPP_DEBUG;

/* driver entry point fn definitions */
static int 	ecpp_open(queue_t *, dev_t *, int, int, cred_t *);
static int	ecpp_close(queue_t *, int, cred_t *);
static uint_t 	ecpp_isr(caddr_t);

/* configuration entry point fn definitions */
static int 	ecpp_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	ecpp_attach(dev_info_t *, ddi_attach_cmd_t);
static int	ecpp_detach(dev_info_t *, ddi_detach_cmd_t);

/* Streams Routines */
static int	ecpp_wput(queue_t *, mblk_t *);
static int	ecpp_wsrv(queue_t *);
static void	ecpp_flush(struct ecppunit *, int);
static void	ecpp_start(queue_t *, mblk_t *, caddr_t, int);

/* ioctl handling */
static void	ecpp_putioc(queue_t *, mblk_t *);
static void	ecpp_srvioc(queue_t *, mblk_t *);
static void 	ecpp_ack_ioctl(queue_t *, mblk_t *);
static void 	ecpp_nack_ioctl(queue_t *, mblk_t *, int);
static void 	ecpp_copyin(queue_t *, mblk_t *, caddr_t, uint_t);
static void 	ecpp_copyout(queue_t *, mblk_t *, caddr_t, uint_t);

/* Sidewinder programming */
static void	ecpp_remove_reg_maps(struct ecppunit *);
static void	write_config_reg(struct ecppunit *, u_char, u_char);
static u_char	read_config_reg(struct ecppunit *, u_char);
static void	ecpp_init_interface(struct ecppunit *);
static void	set_chip_pio(struct ecppunit *);
static void	ecpp_xfer_timeout(struct ecppunit *);

/* IEEE 1284 states */
static void 	ecpp_default_negotiation(struct ecppunit *);
static int 	ecpp_mode_negotiation(struct ecppunit *, u_char);
static int	ecpp_terminate_phase(struct ecppunit *);
static int 	ecpp_idle_phase(struct ecppunit *);
static uint_t	ecpp_peripheral2host(struct ecppunit *pp);

static int	ecp_negotiation(struct ecppunit *);
static int	nibble_negotiation(struct ecppunit *);
static u_char	ecp_peripheral2host(struct ecppunit *pp);
static u_char	nibble_peripheral2host(struct ecppunit *pp);
static int	ecp_forward2reverse(struct ecppunit *);
static int	ecp_reverse2forward(struct ecppunit *);

static u_char	ecpp_get_error_status(u_char);

/* debugging functions */
static void	ecpp_error(dev_info_t *, char *, ...);

static void    *ecppsoft_statep;
static int	ecpp_burstsize = DCSR_BURST_1 | DCSR_BURST_0;

#define	DCSR_INIT_BITS  DCSR_INT_EN | DCSR_EN_CNT | DCSR_CSR_DRAIN \
		| ecpp_burstsize | DCSR_TCI_DIS | DCSR_EN_DMA

#define	DCSR_INIT_BITS2 DCSR_INT_EN | DCSR_EN_CNT | DCSR_CSR_DRAIN \
		| ecpp_burstsize | DCSR_EN_DMA

static struct ecpp_transfer_parms default_xfer_parms = {
	60,		/* write timeout 60 seconds */
	ECPP_CENTRONICS	/* supported mode */
};

#define	PDEBUG B_TRUE
#define	PDEBUG_ECR B_TRUE
#define	PDEBUG_CR B_TRUE

/*
 * Local declarations and variables
 * ecpp_def_timeout is meant for /etc/system
 * XXX HIWAT is huge because we get 88 byte mblks from
 * the lp subsystem or sh.. need to understand this better.
 * anything lower causes canput to fail in the wput routine.
 */

caddr_t dmablock;
#define	DMABUFSZ 1024 * 32
#define	ECPPHIWAT 1024 * 1000 * 5
#define	ECPPLOWAT 1024 * 16
int morebytes = 0;
int ecpp_def_timeout = ECPP_W_TIMEOUT_DEFAULT;

struct module_info ecppinfo = {
	/* id, name, min pkt siz, max pkt siz, hi water, low water */
	42, "ecpp", 0, DMABUFSZ, ECPPHIWAT, ECPPLOWAT
};

static struct qinit ecpp_rinit = {
	putq, NULL, ecpp_open, ecpp_close, NULL, &ecppinfo, NULL
};

static struct qinit ecpp_wint = {
	ecpp_wput, ecpp_wsrv, ecpp_open, ecpp_close, NULL, &ecppinfo, NULL
};

struct streamtab ecpp_str_info = {
	&ecpp_rinit, &ecpp_wint, NULL, NULL
};

static struct cb_ops ecpp_cb_ops = {
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
	&ecpp_str_info,	/* cb_stream */
	(int)(D_NEW | D_MP)	/* cb_flag */
};

/*
 * Declare ops vectors for auto configuration.
 */
struct dev_ops  ecpp_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	ecpp_getinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	ecpp_attach,		/* devo_attach */
	ecpp_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&ecpp_cb_ops,		/* devo_cb_ops */
	(struct bus_ops *)NULL,	/* devo_bus_ops */
	nulldev			/* devo_power */
};

extern struct mod_ops mod_driverops;

static struct modldrv ecppmodldrv = {
	&mod_driverops,		/* type of module - driver */
	"pport driver: ecpp 2.18 95/10/27",
	&ecpp_ops,
};

static struct modlinkage ecppmodlinkage = {
	MODREV_1,
	&ecppmodldrv,
	0
};

int
_init(void)
{
	register int    error;

	if ((error = mod_install(&ecppmodlinkage)) == 0) {
		ddi_soft_state_init(&ecppsoft_statep,
			sizeof (struct ecppunit), 1);
	}

	return (error);
}

int
_fini(void)
{
	register int    error;

	if ((error = mod_remove(&ecppmodlinkage)) == 0)
		ddi_soft_state_fini(&ecppsoft_statep);

	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&ecppmodlinkage, modinfop));
}

static int
ecpp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	register int	instance;
	char		name[16];
	register struct	ecppunit *pp;
	struct ddi_device_acc_attr attr;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;

	PTRACEINIT();		/* initialize tracing */

	instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		if (!(pp = ddi_get_soft_state(ecppsoft_statep, instance)))
			return (DDI_FAILURE);
		mutex_enter(&pp->umutex);
		if (!pp->suspended) {
			mutex_exit(&pp->umutex);
			return (DDI_FAILURE);
		}
		pp->suspended = 0;
		mutex_exit(&pp->umutex);
		ecpp_init_interface(pp);
		RESET_DMAC_CSR(pp);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(ecppsoft_statep, instance) != 0)
		goto failed;

	pp = ddi_get_soft_state(ecppsoft_statep, instance);

	if (ddi_regs_map_setup(dip, 1, (caddr_t *)&pp->c_reg, 0,
			sizeof (struct config_reg), &attr,
			&pp->c_handle) != DDI_SUCCESS) {
		PRNattach1("(%d):ecpp_attach failed to map c_reg\n",
			instance);
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}

	if (ddi_regs_map_setup(dip, 0, (caddr_t *)&pp->i_reg, 0,
			sizeof (struct info_reg), &attr, &pp->i_handle)
			!= DDI_SUCCESS) {
		PRNattach1("(%d):ecpp_attach failed to map i_reg\n",
			instance);
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}

	if (ddi_regs_map_setup(dip, 0, (caddr_t *)&pp->f_reg, 0x400,
			sizeof (struct fifo_reg), &attr, &pp->f_handle)
			!= DDI_SUCCESS) {
		PRNattach1("(%d):ecpp_attach failed to map f_reg\n",
			instance);
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}

	if (ddi_regs_map_setup(dip, 2, (caddr_t *)&pp->dmac, 0,
			sizeof (struct cheerio_dma_reg), &attr,
			&pp->d_handle) != DDI_SUCCESS) {
		PRNattach1("(%d):ecpp_attach failed to map dmac\n",
			instance);
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}
	pp->attr.dma_attr_version = DMA_ATTR_V0;
	pp->attr.dma_attr_addr_lo = 0x00000000ull;
	pp->attr.dma_attr_addr_hi = 0xfffffffeull;
	pp->attr.dma_attr_count_max = 0xffffff;
	pp->attr.dma_attr_align = 1;
	pp->attr.dma_attr_burstsizes = 0x74;
	pp->attr.dma_attr_minxfer = 1;
	pp->attr.dma_attr_maxxfer = 0xffff;
	pp->attr.dma_attr_seg = 0xffff;
	pp->attr.dma_attr_sgllen = 1;
	pp->attr.dma_attr_granular = 1;

	if (ddi_dma_alloc_handle(dip, &pp->attr, DDI_DMA_DONTWAIT,
			NULL, &pp->dma_handle) != DDI_SUCCESS) {
		goto failed;
	} else {
		pp->dip = dip;
		pp->msg = (mblk_t *)NULL;
	}

	/* add interrupts */

	if (ddi_get_iblock_cookie(dip, 0,
			&pp->ecpp_trap_cookie) != DDI_SUCCESS)  {
		PRNattach0("ddi_get_iblock_cookie FAILED \n");
		goto failed;
	}

	mutex_init(&pp->umutex, "ecpp mutex ", MUTEX_DRIVER,
		(void *)pp->ecpp_trap_cookie);

	cv_init(&pp->pport_cv, "ecpp port cv ", CV_DRIVER,
		&pp->ecpp_trap_cookie);

	if (ddi_add_intr(dip, 0, &pp->ecpp_trap_cookie, NULL, ecpp_isr,
			(caddr_t)pp) != DDI_SUCCESS) {
		PRNattach1("(%d):ecpp_attach failed to add hard intr\n",
			instance);
		goto remlock;
	}

	sprintf(name, "ecpp%d", instance);

	if (ddi_create_minor_node(dip, name, S_IFCHR, instance, NULL,
			NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(dip, NULL);
		goto remhardintr;
	}

	ddi_report_dev(dip);
	dmablock = (caddr_t)kmem_alloc(DMABUFSZ, KM_NOSLEEP);

	PRNattach1("(%d):ecpp_attach success.\n", instance);

	return (DDI_SUCCESS);

remhardintr:
	ddi_remove_intr(dip, (uint_t)0, pp->ecpp_trap_cookie);

remlock:
	mutex_destroy(&pp->umutex);
	cv_destroy(&pp->pport_cv);

failed:
	PRNattach0("ecpp_attach:failed.\n");
	ecpp_remove_reg_maps(pp);

	return (DDI_FAILURE);
}

static int
ecpp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance;
	register struct ecppunit *pp;

	instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_DETACH:
		break;

	case DDI_SUSPEND:
		if (!(pp = ddi_get_soft_state(ecppsoft_statep, instance)))
		    return (DDI_FAILURE);
		mutex_enter(&pp->umutex);
		if (pp->suspended) {
		    mutex_exit(&pp->umutex);
		    return (DDI_FAILURE);
		}
		pp->suspended = 1;
		mutex_exit(&pp->umutex);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	pp = ddi_get_soft_state(ecppsoft_statep, instance);

	if (pp->dma_handle != NULL)
		ddi_dma_free_handle(&pp->dma_handle);

	ddi_remove_minor_node(dip, NULL);

	ddi_remove_intr(dip, (uint_t)0, pp->ecpp_trap_cookie);

	cv_destroy(&pp->pport_cv);

	mutex_destroy(&pp->umutex);

	ecpp_remove_reg_maps(pp);

	kmem_free(dmablock, DMABUFSZ);

	return (DDI_SUCCESS);

}

int
ecpp_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t	dev = (dev_t)arg;
	struct ecppunit *pp;
	int	instance, ret;

#if defined(lint)
	dip = dip;
#endif
	instance = getminor(dev);

	switch (infocmd) {
		case DDI_INFO_DEVT2DEVINFO:
			pp = (struct ecppunit *)
				ddi_get_soft_state(ecppsoft_statep, instance);
			*result = pp->dip;
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

/*ARGSUSED2*/
static int
ecpp_open(queue_t *q, dev_t *dev, int flag, register sflag, cred_t *credp)
{
	struct ecppunit *pp;
	int		instance;
	register struct stroptions *sop;
	mblk_t		*mop;

	instance = getminor(*dev);
	if (instance < 0)
		return (ENXIO);
	pp = (struct ecppunit *)ddi_get_soft_state(ecppsoft_statep, instance);

	PTRACE(ecpp_open, 'OPEN', pp);

	if (pp == NULL)
		return (ENXIO);

	mutex_enter(&pp->umutex);

	if ((pp->oflag == TRUE)) {
		PRNopen0("ecpp open failed");
		mutex_exit(&pp->umutex);
		return (EBUSY);
	}

	pp->oflag = TRUE;

	mutex_exit(&pp->umutex);

	/* initialize state variables */
	pp->error_status = ECPP_NO_1284_ERR;
	pp->xfer_parms = default_xfer_parms;	/* structure assignment */

	pp->current_mode = ECPP_CENTRONICS;
	pp->current_phase = ECPP_PHASE_PO;
	pp->port = ECPP_PORT_DMA;
	pp->instance = instance;
	pp->xfer_parms.write_timeout = ecpp_def_timeout;
	pp->timeout_error = 0;
	pp->saved_dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	/* put chip in initial state */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		(ECR_mode_000 | ECPP_INTR_MASK |
		ECPP_INTR_SRV | ECPP_FIFO_EMPTY));


	ecpp_init_interface(pp);
	RESET_DMAC_CSR(pp);
	set_chip_pio(pp);
	ecpp_default_negotiation(pp);

	/* clear the state flag */
	pp->e_busy = ECPP_IDLE;

	/* check that phases are correct for corresponding mode */
	switch (pp->current_mode) {
	case ECPP_CENTRONICS:
	case ECPP_COMPAT_MODE:
		if (pp->current_phase != ECPP_PHASE_CMPT_FWD) {
			ecpp_error(pp->dip, "ecpp_open: compat bad");
		}
		break;
	case ECPP_NIBBLE_MODE:
		if (pp->current_phase != ECPP_PHASE_NIBT_REVIDLE) {
			ecpp_error(pp->dip, "ecpp_open: nibble bad");
		}
		break;
	case ECPP_ECP_MODE:
		if (pp->current_phase != ECPP_PHASE_ECP_FWD_IDLE) {
			ecpp_error(pp->dip, "ecpp_open: ecp bad");
		}
		break;
	default:
		ecpp_error(pp->dip, "ecpp_open: bad current_mode");
	}

	/* make sure we don't have the SRV bit set yet */
	OR_SET_BYTE_R(pp->f_handle, &pp->f_reg->ecr, ECPP_INTR_SRV);

	/* enable interrupts on the Pport */
	AND_SET_BYTE_R(pp->f_handle, &pp->f_reg->ecr, ~ECPP_INTR_MASK);

	if (!(mop = allocb(sizeof (struct stroptions), BPRI_MED))) {
		return (EAGAIN);
	}

	q->q_ptr = WR(q)->q_ptr = (caddr_t)pp;

	mop->b_datap->db_type = M_SETOPTS;
	mop->b_wptr += sizeof (struct stroptions);

	/*
	 * if device is open with O_NONBLOCK flag set, let read(2) return 0
	 * if no data waiting to be read.  Writes will block on flow control.
	 */
	sop = (struct stroptions *)mop->b_rptr;
	sop->so_flags = SO_HIWAT | SO_LOWAT | SO_NDELON;
	sop->so_hiwat = 512;
	sop->so_lowat = 256;

	/* enable the stream */
	qprocson(q);

	putnext(q, mop);

	pp->readq = RD(q);
	pp->writeq = WR(q);
	pp->msg = (mblk_t *)NULL;

	return (0);
}

/*ARGSUSED1*/
static int
ecpp_close(queue_t *q, int flag, cred_t *cred_p)
{
	struct ecppunit *pp;

	pp = (struct ecppunit *)q->q_ptr;
	PTRACE(ecpp_close, 'CLOS', get_dmac_csr(pp));
	PTRACE(ecpp_close, '0RCB', get_dmac_bcr(pp));

	/* wait till all output activity has ceased */

	mutex_enter(&pp->umutex);
	PTRACE(ecpp_close, 'ETYB', morebytes);
	while (pp->e_busy == ECPP_BUSY) {
		PTRACE(ecpp_close, 'YSUB', pp->e_busy);
		cv_wait(&pp->pport_cv, &pp->umutex);
	}
	qprocsoff(q);
	PTRACE(ecpp_close, 'EREH', pp->e_busy);

	untimeout(pp->timeout_id);
	/* set link to Compatible mode */
	switch (pp->current_mode) {
	case ECPP_CENTRONICS:
	case ECPP_COMPAT_MODE:
	case ECPP_DIAG_MODE:
		if (pp->current_phase != ECPP_PHASE_CMPT_FWD) {
			ecpp_error(pp->dip, "ecpp_close: compat bad");
		}
		break;
	case ECPP_NIBBLE_MODE:
		if (pp->current_phase != ECPP_PHASE_NIBT_REVIDLE) {
			ecpp_error(pp->dip, "ecpp_close: nibble bad");
		} else if (ecpp_terminate_phase(pp) == FAILURE) {
			ecpp_error(pp->dip,
				"ecpp_close: nibble ecpp_terminate bad");
			pp->current_mode = ECPP_FAILURE_MODE;
		}
		break;
	case ECPP_ECP_MODE:
		if (pp->current_phase != ECPP_PHASE_ECP_FWD_IDLE) {
			ecpp_error(pp->dip, "ecpp_close: ecp bad");
		} else if (ecpp_terminate_phase(pp) == FAILURE) {
			ecpp_error(pp->dip,
				"ecpp_close: ecp ecpp_terminate bad");
			pp->current_mode = ECPP_FAILURE_MODE;
		}
		break;
	default:
		ecpp_error(pp->dip, "ecpp_close: bad current_mode");
	}

	PTRACE(ecpp_close, 'ETYB', morebytes);
	pp->oflag = FALSE;
	q->q_ptr = WR(q)->q_ptr = NULL;
	pp->msg = (mblk_t *)NULL;

	mutex_exit(&pp->umutex);

	PTRACE(ecpp_close, 'SOLC', pp);
	return (0);
}

/*
 * standard put procedure for ecpp
 */
static int
ecpp_wput(queue_t *q, mblk_t *mp)
{
	register struct msgb *mp1;
	struct ecppunit *pp;

	PTRACE(ecpp_wput, 'DATA', mp);
	pp = (struct ecppunit *)q->q_ptr;

	switch (DB_TYPE(mp)) {

	case M_DATA:
		while (mp) {
			PTRACE(ecpp_wput, 'mp  ', (mp->b_wptr - mp->b_rptr));
			if ((mp->b_wptr - mp->b_rptr) <= 0) {
				freemsg(mp);
				mp = NULL;
			} else {
				if (canput(q)) {
					mp1 = unlinkb(mp);
					mp->b_cont = NULL;
					(void) putq(q, mp);
					mp = mp1;
				} else {
					cmn_err(CE_WARN, "ecpp wput err\n");
					return (ENOSR);
				}
			}
		}

		break;

	case M_CTL:

		if ((mp->b_wptr - mp->b_rptr) <= 0) {
			freemsg(mp);
		} else {
			(void) putq(q, mp);
		}

		break;

	case M_IOCTL:
		PTRACE(ecpp_wput, 'IOCT', M_IOCTL);

		ecpp_putioc(q, mp);

		break;

	case M_IOCDATA:
	{
		struct copyresp *csp;

		csp = (struct copyresp *)mp->b_rptr;

		PTRACE(ecpp_wput, 'IOCD', M_IOCDATA);

		/*
		 * If copy request failed, quit now
		 */
		if (csp->cp_rval != 0) {
			freemsg(mp);
			return (0);
		}

		switch (csp->cp_cmd) {
		case ECPPIOC_SETPARMS:
		case ECPPIOC_SETREGS:
		case ECPPIOC_SETPORT:
		case ECPPIOC_SETDATA:
			/*
			 * need to retrieve and use the data, but if the
			 * device is busy, wait.
			 */
			(void) putq(q, mp);
			break;

		case ECPPIOC_GETPARMS:
		case ECPPIOC_GETREGS:
		case ECPPIOC_GETPORT:
		case ECPPIOC_GETDATA:
		case BPPIOC_GETERR:
		case BPPIOC_TESTIO:
			/* data transfered to user space okay */
			ecpp_ack_ioctl(q, mp);
			break;
		default:
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		break;
	}

	case M_FLUSH:
		switch (*(mp->b_rptr)) {
			case FLUSHRW:
				ecpp_flush(pp, (FREAD | FWRITE));
				*(mp->b_rptr) = FLUSHR;
				qreply(q, mp);
				break;
			case FLUSHR:
				ecpp_flush(pp, FREAD);
				qreply(q, mp);
				break;
			case FLUSHW:
				ecpp_flush(pp, FWRITE);
				freemsg(mp);
				break;

			default:
			break;
		}
		break;

	default:
		ecpp_error(pp->dip, "ecpp_wput: bad messagetype 0x%X\n",
		    DB_TYPE(mp));
		freemsg(mp);
		break;
	}

	return (0);
}

static u_char
ecpp_get_error_status(u_char status)
{
	register u_char pin_status = 0;


	if (!(status & ECPP_nERR)) {
		pin_status |= BPP_ERR_ERR;
	}

	if (status & ECPP_PE) {
		pin_status |= BPP_PE_ERR;
	}

	if (!(status & ECPP_SLCT)) {
		pin_status |= (BPP_ERR_ERR | BPP_SLCT_ERR);
	}

	if (!(status & ECPP_nBUSY)) {
		pin_status |= (BPP_SLCT_ERR);
	}

	return (pin_status);
}

/*
 * ioctl handler for output PUT procedure.
 */
static void
ecpp_putioc(queue_t *q, mblk_t *mp)
{
	struct iocblk	*iocbp;
	struct ecppunit *pp;

	pp = (struct ecppunit *)q->q_ptr;

	iocbp = (struct iocblk *)mp->b_rptr;

	PTRACE(ecpp_putioc, 'iocb', iocbp);

	/* I_STR ioctls are invalid */
	if (iocbp->ioc_count != TRANSPARENT) {
		ecpp_nack_ioctl(q, mp, EINVAL);
		return;
	}

	PTRACE(ecpp_putioc, 'bcio', iocbp->ioc_cmd);
	switch (iocbp->ioc_cmd) {
	case ECPPIOC_SETPARMS:
	{
	PTRACE(ecpp_putioc, 'Acio', iocbp->ioc_cmd);
		ecpp_copyin(q, mp, *(caddr_t *)(void *)mp->b_cont->b_rptr,
			sizeof (struct ecpp_transfer_parms));
		break;
	}
	case ECPPIOC_GETPARMS:
	{
		caddr_t uaddr;
		struct ecpp_transfer_parms *xfer_parms;

		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

	PTRACE(ecpp_putioc, 'Bcio', iocbp->ioc_cmd);
		freemsg(mp->b_cont);

		mp->b_cont =
			allocb(sizeof (struct ecpp_transfer_parms), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		xfer_parms = (struct ecpp_transfer_parms *)mp->b_cont->b_rptr;
		xfer_parms->write_timeout = pp->xfer_parms.write_timeout;
		xfer_parms->mode = pp->xfer_parms.mode = pp->current_mode;

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
			sizeof (struct ecpp_transfer_parms);

		ecpp_copyout(q, mp, uaddr,
			sizeof (struct ecpp_transfer_parms));

		break;
	}
	case ECPPIOC_SETREGS:
	{
	PTRACE(ecpp_putioc, 'Ccio', iocbp->ioc_cmd);
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		ecpp_copyin(q, mp, *(caddr_t *)(void *)mp->b_cont->b_rptr,
			sizeof (struct ecpp_regs));
		break;
	}
	case ECPPIOC_GETREGS:
	{
		caddr_t uaddr;
		struct ecpp_regs *rg;

	PTRACE(ecpp_putioc, 'Dcio', iocbp->ioc_cmd);
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

		freemsg(mp->b_cont);

		mp->b_cont = allocb(sizeof (struct ecpp_regs), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		rg = (struct ecpp_regs *)mp->b_cont->b_rptr;

		mutex_enter(&pp->umutex);

		rg->dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		rg->dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);

		mutex_exit(&pp->umutex);

		rg->dsr |= 0x07;  /* bits 0,1,2 must be 1 */
		rg->dcr |= 0xf0;  /* bits 4 - 7 must be 1 */

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
			sizeof (struct ecpp_regs);

		ecpp_copyout(q, mp, uaddr, sizeof (struct ecpp_regs));

		break;
	}

	case ECPPIOC_SETPORT:
	case ECPPIOC_SETDATA:
	{
	PTRACE(ecpp_putioc, 'Ecio', iocbp->ioc_cmd);
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/*
		 * each of the commands fetches a byte quantity.
		 */
		ecpp_copyin(q, mp, *(caddr_t *)(void *)mp->b_cont->b_rptr,
			sizeof (u_char));
		break;
	}

	case ECPPIOC_GETDATA:
	case ECPPIOC_GETPORT:
	{
		caddr_t uaddr;
		u_char *port;

	PTRACE(ecpp_putioc, 'Fcio', iocbp->ioc_cmd);
		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}


		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

		freemsg(mp->b_cont);

		mp->b_cont = allocb(sizeof (u_char), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		port = (u_char *)mp->b_cont->b_rptr;

		if (iocbp->ioc_cmd == ECPPIOC_GETPORT)
			*port = pp->port;

		else if (iocbp->ioc_cmd == ECPPIOC_GETDATA) {
			mutex_enter(&pp->umutex);
			while (pp->e_busy == ECPP_BUSY) {
				PTRACE(ecpp_putioc, 'YSUB', pp->e_busy);
				cv_wait(&pp->pport_cv, &pp->umutex);
			}
			switch (pp->port) {
			case ECPP_PORT_PIO:
				*port = PP_GETB(pp->i_handle,
					&pp->i_reg->ir.datar);
				PTRACE(ecpp_putioc, 'PIO ', *port);
				break;
			case ECPP_PORT_TDMA:
				*port = PP_GETB(pp->f_handle,
					&pp->f_reg->fr.tfifo);
				PTRACE(ecpp_putioc, 'TDMA', *port);
				break;
			default:
				ecpp_nack_ioctl(q, mp, EINVAL);
				break;
			}
			mutex_exit(&pp->umutex);
		}

		else {
			ecpp_error(pp->dip, "wierd command");
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
			sizeof (u_char);

		ecpp_copyout(q, mp, uaddr, sizeof (u_char));

		break;
	}

	case BPPIOC_GETERR:
	{
		caddr_t uaddr;
		struct bpp_error_status *bpp_status;

	PTRACE(ecpp_putioc, 'Gcio', iocbp->ioc_cmd);
		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

		freemsg(mp->b_cont);

		mp->b_cont =
			allocb(sizeof (struct bpp_error_status), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		bpp_status = (struct bpp_error_status *)mp->b_cont->b_rptr;
		bpp_status->timeout_occurred = pp->timeout_error;
		bpp_status->bus_error = 0;	/* not used */
		bpp_status->pin_status = ecpp_get_error_status(pp->saved_dsr);

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (struct bpp_error_status);

		ecpp_copyout(q, mp, uaddr, sizeof (struct bpp_error_status));

		break;
	}

	case BPPIOC_TESTIO:
	{
	PTRACE(ecpp_putioc, 'Hcio', iocbp->ioc_cmd);
		if (!((pp->current_mode == ECPP_CENTRONICS) ||
				(pp->current_mode == ECPP_COMPAT_MODE))) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		mutex_enter(&pp->umutex);

		pp->saved_dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

		if ((pp->saved_dsr & ECPP_PE) ||
		    !(pp->saved_dsr & ECPP_SLCT) ||
		    !(pp->saved_dsr & ECPP_nERR)) {
			ecpp_nack_ioctl(q, mp, EIO);
		} else {
			ecpp_ack_ioctl(q, mp);
		}

		mutex_exit(&pp->umutex);

		break;
	}

	default:
	PTRACE(ecpp_putioc, 'Icio', iocbp->ioc_cmd);
		ecpp_nack_ioctl(q, mp, EINVAL);
		break;
	}
}

static int
ecpp_wsrv(queue_t *q)
{
	struct ecppunit *pp;
	register struct msgb *mp, *mp1;
	register struct msgb *omp;
	int len, total_len, starting;
	caddr_t my_dmablock;

	pp = (struct ecppunit *)q->q_ptr;

	PTRACE(ecpp_wsrv, 'WSRV', pp->e_busy);

	starting = 0;
	/*
	 * if channel is actively doing work, wait till completed
	 */
	if (pp->e_busy == ECPP_BUSY && get_dmac_bcr(pp) != 0) {
		pp->service = TRUE;
		PTRACE(ecpp_wsrv, 'NTER', pp->e_busy);
		PTRACE(ecpp_wsrv, ' RCB', get_dmac_bcr(pp));
		return (0);
	}

	len = total_len = 0;
	my_dmablock = dmablock;

	/*
	 * The following While loop is implemented to gather the
	 * many small writes that the lp subsystem makes and
	 * compile them into one large dma transfer. The len and
	 * total_len variables are a running count of the number of
	 * bytes that have been gathered. They are bcopied to the
	 * dmabuf buffer. my_dmabuf is a pointer that gets incremented
	 * each time we add len to the buffer. the pp->e_busy state
	 * flag is set to E_BUSY as soon as we start gathering packets
	 * because if not there is a possibility that we could get to
	 * to the close routine asynchronously and free up some of the
	 * data that we are currently transferring.
	 */

	while (mp = getq(q)) {

		switch (DB_TYPE(mp)) {

		case M_DATA:
			len = mp->b_wptr - mp->b_rptr;

			if (len >= DMABUFSZ) {
				pp->e_busy = ECPP_BUSY;
				starting ++;
				ecpp_start(q, mp, (caddr_t)mp->b_rptr, len);
				return (1);
			}

			if (total_len + len <= DMABUFSZ) {
				pp->e_busy = ECPP_BUSY;
				PTRACE(ecpp_wsrv, 'NELW', len);
				bcopy((void *)mp->b_rptr,
				    (void *)my_dmablock, len);
				my_dmablock += len;
				PTRACE(ecpp_wsrv, 'BAMD', dmablock);
				PTRACE(ecpp_wsrv, 'RDAM', my_dmablock);
				total_len += len;
				starting++;
				freemsg(mp);
				omp = (mblk_t *)NULL;
				break;
			} else {
				PTRACE(ecpp_wsrv, 'QBUP', dmablock);
				putbq(q, mp);
				pp->e_busy = ECPP_BUSY;
				starting++;
				ecpp_start(q, omp, dmablock, total_len);
				return (1);
			}


		case M_IOCTL:
			ecpp_putioc(q, mp);

			break;

		case M_IOCDATA:
		{
			struct copyresp *csp;

			csp = (struct copyresp *)mp->b_rptr;

			/*
			 * If copy request failed, quit now
			 */
			if (csp->cp_rval != 0) {
				freemsg(mp);
				break;
			}

			switch (csp->cp_cmd) {
			case ECPPIOC_SETPARMS:
			case ECPPIOC_SETREGS:
			case ECPPIOC_SETPORT:
			case ECPPIOC_SETDATA:

				ecpp_srvioc(q, mp);
				break;
			default:
				ecpp_nack_ioctl(q, mp, EINVAL);
				break;
			}

			break;
		}
		case M_CTL:

			ecpp_peripheral2host(pp);
			/*
			 * We are done if there are no more mblocks
			 * We got here from the queue of the M_CTL
			 * in the interrupt routine.
			 */
			mp1 = getq(pp->writeq);
			if (mp1 == NULL) {
				cv_signal(&pp->pport_cv);
			} else {
				putbq(pp->writeq, mp1);
			}

			break;
		default:
			printf("ecpp: should never get here\n");
			freemsg(mp);
			break;
		}
	}


	if (starting == 0) {
		if (ecpp_idle_phase(pp) == FAILURE) {
			pp->error_status = ECPP_1284_ERR;
		}
	}

	if (total_len != 0) {
		pp->e_busy = ECPP_BUSY;
		ecpp_start(q, omp, dmablock, total_len);
		return (1);
	}

	return (1);
}

/*
 * Ioctl processor for queued ioctl data transfer messages.
 */
static void
ecpp_srvioc(queue_t *q, mblk_t *mp)
{
	struct iocblk	*iocbp;
	struct ecppunit *pp;

	iocbp = (struct iocblk *)mp->b_rptr;
	pp = (struct ecppunit *)q->q_ptr;

	PTRACE(ecpp_srvioc, 'SRVI', pp);

	switch (iocbp->ioc_cmd) {
	case ECPPIOC_SETPARMS:
	{
		struct ecpp_transfer_parms *xferp;

		xferp = (struct ecpp_transfer_parms *)mp->b_cont->b_rptr;


		if (xferp->write_timeout <= 0 ||
				xferp->write_timeout >= ECPP_MAX_TIMEOUT) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		if (!((xferp->mode == ECPP_CENTRONICS) ||
			(xferp->mode == ECPP_COMPAT_MODE) ||
			(xferp->mode == ECPP_NIBBLE_MODE) ||
			(xferp->mode == ECPP_ECP_MODE) ||
			(xferp->mode == ECPP_DIAG_MODE))) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		pp->xfer_parms = *xferp;

		if (ecpp_mode_negotiation(pp, pp->xfer_parms.mode) == FAILURE) {
			ecpp_nack_ioctl(q, mp, EPROTONOSUPPORT);
		    }
		else
			ecpp_ack_ioctl(q, mp);

		if (pp->current_mode != ECPP_DIAG_MODE)
		    pp->port = ECPP_PORT_DMA;
		else
		    pp->port = ECPP_PORT_PIO;

		pp->xfer_parms.mode = pp->current_mode;

		break;
	}

	case ECPPIOC_SETREGS:
	{
		struct ecpp_regs *rg;
		u_char dcr;

		rg = (struct ecpp_regs *)mp->b_cont->b_rptr;

		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/* bits 4-7 must be 1 or return EINVAL */
		if ((rg->dcr & 0xf0) != 0xf0) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/* get the old dcr */
		dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
		/* get the new dcr */
		dcr = (dcr & 0xf0) | (rg->dcr & 0x0f);

		PP_PUTB(pp->i_handle, &pp->i_reg->dcr, dcr);
		ecpp_ack_ioctl(q, mp);
		break;
	}
	case ECPPIOC_SETPORT:
	{
		u_char *port;

		port = (u_char *)mp->b_cont->b_rptr;

		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		PTRACE(ecpp_srvioc, 'SETP', *port);
		switch (*port) {
		case ECPP_PORT_PIO:
			/* put superio into PIO mode */
			PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECR_mode_001 | ECPP_INTR_MASK
				| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));
			pp->port = *port;
			PTRACE(ecpp_srvioc, ' OIP', pp);
			ecpp_ack_ioctl(q, mp);
			break;

		case ECPP_PORT_TDMA:
			/* change to mode 110 */
			PP_PUTB(pp->f_handle, & pp->f_reg->ecr,
				(ECPP_DMA_ENABLE | ECPP_INTR_MASK
				| ECR_mode_110 | ECPP_FIFO_EMPTY));
			pp->port = *port;
			PTRACE(ecpp_srvioc, 'AMDT', pp);
			ecpp_ack_ioctl(q, mp);
			break;
		default:
			ecpp_nack_ioctl(q, mp, EINVAL);
		}

		break;
	}
	case ECPPIOC_SETDATA:
	{
		u_char *data;

		data = (u_char *)mp->b_cont->b_rptr;

		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		switch (pp->port) {
		case ECPP_PORT_PIO:
			PP_PUTB(pp->i_handle, &pp->i_reg->ir.datar, *data);
			PTRACE(ecpp_srvioc, '1OIP', pp);
			ecpp_ack_ioctl(q, mp);
			break;
		case ECPP_PORT_TDMA:
			PP_PUTB(pp->f_handle, &pp->f_reg->fr.tfifo, *data);
			PTRACE(ecpp_srvioc, 'amdt', pp);
			ecpp_ack_ioctl(q, mp);
			break;
		default:
			ecpp_nack_ioctl(q, mp, EINVAL);
		}

		break;
	}

	default:		/* unexpected ioctl type */
		ecpp_nack_ioctl(q, mp, EINVAL);
		break;
	}

}

static void
ecpp_flush(struct ecppunit *pp, int cmd)
{
	queue_t		*q;

	PTRACE(ecpp_flush, 'Flus', cmd);
	PTRACE(ecpp_flush, ' RCB', get_dmac_bcr(pp));
	if (cmd & FWRITE) {
		q = pp->writeq;
		/* Discard all messages on the output queue. */
		flushq(q, FLUSHDATA);

		/* wait for the current data transfer to finish */
		if (pp->e_busy != ECPP_IDLE) {
			pp->service = 1;
			cv_wait_sig(&pp->pport_cv, &pp->umutex);
		}
	}
}

static void
ecpp_start(queue_t *q, mblk_t *mp, caddr_t addr, int len)
{
	struct ecppunit *pp;
	int		cond;
	u_char ecr;

	/* MUST be called with mutex held */

	pp = (struct ecppunit *)q->q_ptr;


	ASSERT(pp->e_busy == ECPP_BUSY);

	PTRACE(ecpp_start, 'GNEL', len);
	PTRACE(ecpp_start, 'RDDA', addr);
	cond = ddi_dma_addr_bind_handle(pp->dma_handle, NULL, addr, len,
		DDI_DMA_WRITE, DDI_DMA_DONTWAIT, NULL, &pp->dma_cookie,
		&pp->dma_cookie_count);

	switch (cond) {
	case DDI_DMA_MAPPED:
		break;
	case DDI_DMA_PARTIAL_MAP:
		PRNfwdx0("ecpp_start:DDI_DMA_PARTIAL_MAP:");
		return;
	case DDI_DMA_NORESOURCES:
		PRNfwdx0("ecpp_start:DDI_DMA_NORESOURCES: failure.");
		return;
	case DDI_DMA_NOMAPPING:
		PRNfwdx0("ecpp_start:DDI_DMA_NOMAPPING: failure.");
		return;
	case DDI_DMA_TOOBIG:
		PRNfwdx0("ecpp_start:DDI_DMA_TOOBIG: failure.");
		return;
	case DDI_DMA_INUSE:
		PRNfwdx0("ecpp_start:DDI_DMA_INUSE: failure.");
		return;
	default:
		PRNfwdx0("ddi_dma_addr_bind_handle:unknown return:failure.");
	}

	mutex_enter(&pp->umutex);

	/* put dmac in a known state */
	RESET_DMAC_CSR(pp);

	PTRACE(ecpp_start, 'mode', pp->current_mode);

	switch (pp->current_mode) {
	case ECPP_CENTRONICS:
	case ECPP_COMPAT_MODE:
		/* now put the host interface in the correct mode */
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
		ecpp_error(pp->dip, "ecpp_start ecr %x\n", ecr);

		PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
			(ECR_mode_010 | ECPP_DMA_ENABLE));

		pp->current_phase = ECPP_PHASE_CMPT_FWD;
		break;
	case ECPP_DIAG_MODE:
		PP_PUTB(pp->f_handle, & pp->f_reg->ecr,
			(ECPP_DMA_ENABLE | ECPP_INTR_MASK
			| ECR_mode_110 | ECPP_FIFO_EMPTY));
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
		PTRACE(ecpp_start, 'FIFO', ecr);
		break;
	case ECPP_NIBBLE_MODE:
		if (pp->current_phase != ECPP_PHASE_NIBT_REVIDLE)
			ecpp_error(pp->dip, "ecpp_start: nibble problem\n");
		else
			ecpp_terminate_phase(pp);
		/* no put the host interface in the correct mode */
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
		ecpp_error(pp->dip, "ecpp_start ecr %x\n", ecr);
		PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
			(ECR_mode_010 | ECPP_DMA_ENABLE));

		PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
			ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN);

		pp->current_phase = ECPP_PHASE_CMPT_FWD;

		break;
	case ECPP_ECP_MODE:
		switch (pp->current_phase) {

		case ECPP_PHASE_ECP_FWD_IDLE:
			break;
		case ECPP_PHASE_ECP_REV_IDLE:
			ecp_reverse2forward(pp);
			break;
		default:
			ecpp_error(pp->dip, "ecpp_start: ecp problem\n");
			break;
		}

		PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
			(ECR_mode_011 | ECPP_DMA_ENABLE));

		break;
	}

	PTRACE(ecpp_start, 'EZIS', pp->dma_cookie.dmac_size);
	set_dmac_bcr(pp, pp->dma_cookie.dmac_size);
	set_dmac_acr(pp, pp->dma_cookie.dmac_address);

	set_dmac_csr(pp, DCSR_INIT_BITS2);	/* read from memory to device */

	pp->timeout_id = timeout(ecpp_xfer_timeout, (caddr_t)pp,
		pp->xfer_parms.write_timeout * drv_usectohz(1000000));

	if (pp->current_mode == ECPP_DIAG_MODE) {
		cv_signal(&pp->pport_cv);
	}

	pp->msg = mp;
	mutex_exit(&pp->umutex);
}

static void
ecpp_ack_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk  *iocbp;

	mp->b_datap->db_type = M_IOCACK;
	mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	};

	iocbp = (struct iocblk *)mp->b_rptr;
	iocbp->ioc_error = 0;
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;

	qreply(q, mp);

}

static void
ecpp_nack_ioctl(queue_t *q, mblk_t *mp, int err)
{
	struct iocblk  *iocbp;

	mp->b_datap->db_type = M_IOCNAK;
	mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
	iocbp = (struct iocblk *)mp->b_rptr;
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
ecpp_copyin(queue_t *q, mblk_t *mp, caddr_t addr, uint_t len)
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
ecpp_copyout(queue_t *q, mblk_t *mp, caddr_t addr, uint_t len)
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
ecpp_remove_reg_maps(struct ecppunit *pp)
{
	if (pp->c_handle) ddi_regs_map_free(&pp->c_handle);
	if (pp->i_handle) ddi_regs_map_free(&pp->i_handle);
	if (pp->f_handle) ddi_regs_map_free(&pp->f_handle);
	if (pp->d_handle) ddi_regs_map_free(&pp->d_handle);
}

uint_t
ecpp_isr(caddr_t arg)
{
	struct ecppunit *pp = (struct ecppunit *)(void *)arg;
	mblk_t		*mp;
	int ic;
	u_int dcsr;
	u_char dsr;

	ic = DDI_INTR_UNCLAIMED;

	mutex_enter(&pp->umutex);

restart:
	dcsr = get_dmac_csr(pp);

	PTRACE(ecpp_isr, 'Hint', dcsr);

	if (dcsr & DCSR_INT_PEND) {  /* interrupt is for this device */
		if (dcsr & DCSR_ERR_PEND) {

			/* we are expecting a data transfer interrupt */
			ASSERT(pp->e_busy == ECPP_BUSY);

			/*
			 * some kind of DMA error.  Abort transfer and retry.
			 * on the third retry failure, give up.
			 */

			PTRACE(ecpp_isr, 'HECR', dcsr);

			if ((get_dmac_bcr(pp)) != 0) {
				mutex_exit(&pp->umutex);
				ecpp_error(pp->dip,
					"interrupt with bcr != 0");
				mutex_enter(&pp->umutex);
			}

			RESET_DMAC_CSR(pp);

			ddi_dma_unbind_handle(pp->dma_handle);

			if (pp->msg != NULL) {
				freemsg(pp->msg);
				pp->msg = (mblk_t *)NULL;
			}

			ic = DDI_INTR_CLAIMED;

			pp->service = FALSE;
			pp->e_busy = ECPP_IDLE;

			pp->about_to_untimeout = 1;
			mutex_exit(&pp->umutex);

			untimeout(pp->timeout_id);

			mutex_enter(&pp->umutex);
			pp->about_to_untimeout = 0;
			mutex_exit(&pp->umutex);

			ecpp_error(pp->dip, "transfer count error");

			return (ic);
		}

		if (dcsr & DCSR_TC) {

			PTRACE(ecpp_isr, 'HTCR', dcsr);
			/*
			 * read of the dsr is necessary to stop
			 * interrupt not serviced messages on the
			 * console.
			 */
			dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
			PTRACE(ecpp_isr, '0RSD', dsr);

			/* we are expecting a data transfer interrupt */
			ASSERT(pp->e_busy == ECPP_BUSY);

			/* disable DMA (don't mask other interrupts) */

			if (pp->current_mode == ECPP_DIAG_MODE) {
			    AND_SET_BYTE_R(pp->f_handle, &pp->f_reg->ecr,
				    ~ECPP_DMA_ENABLE);
			} else {
				if (PP_GETB(pp->f_handle,
				    &pp->f_reg->ecr) & ECPP_INTR_SRV) {
					OR_SET_BYTE_R(pp->f_handle,
					    &pp->f_reg->ecr, (ECPP_INTR_SRV));
				}
			}

			/* ACK and disable the TC interrupt */
			OR_SET_LONG_R(pp->d_handle, &pp->dmac->csr,
				DCSR_TC | DCSR_TCI_DIS);

			/* disable DMA and byte counter */
			AND_SET_LONG_R(pp->d_handle, &pp->dmac->csr,
				~(DCSR_EN_DMA | DCSR_EN_CNT));

			ASSERT(--pp->dma_cookie_count == 0);

			ddi_dma_unbind_handle(pp->dma_handle);

			if (pp->msg != NULL) {
				freemsg(pp->msg);
				pp->msg = (mblk_t *)NULL;
			}

			pp->about_to_untimeout = 1;
			mutex_exit(&pp->umutex);

			untimeout(pp->timeout_id);

			mutex_enter(&pp->umutex);
			pp->about_to_untimeout = 0;
			mutex_exit(&pp->umutex);


			pp->service = FALSE;

			qenable(pp->writeq);

			/*
			 * We are done if there are no more mblocks
			 */
			mp = getq(pp->writeq);
			if (mp == NULL) {
				PTRACE(ecpp_isr, 'ENOD', pp->e_busy);
				pp->e_busy = ECPP_IDLE;
				cv_signal(&pp->pport_cv);
			} else {
				putbq(pp->writeq, mp);
			}
			pp->e_busy = ECPP_IDLE;
			ic = DDI_INTR_CLAIMED;

			return (ic);
		}

		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

		/* does peripheral need attention? */
		if ((dsr & ECPP_nERR) == 0) {
			PTRACE(ecpp_isr, 'HTSR', dcsr);
			PTRACE(ecpp_wsrv, ' RCB', get_dmac_bcr(pp));


			/*
			 * dma breaks in ecp mode if the next three lines
			 * are not there.
			 */
			drv_usecwait(20);	/* 20us */
			PTRACE(ecpp_wsrv, '1RCB', get_dmac_bcr(pp));
			/*
			 * mask the interrupt so that it can be
			 * handled later.
			 */
			OR_SET_BYTE_R(pp->f_handle,
				&pp->f_reg->ecr, ECPP_INTR_MASK);

			/*
			 * dma breaks in ecp mode if the next three lines
			 * are not there.
			 */
			drv_usecwait(6);	/* 6us */

			if (pp->current_mode == ECPP_ECP_MODE) {
				PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				    (ECPP_INTR_MASK | ECR_mode_011 |
				    ECPP_DMA_ENABLE));
			} else {
				PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				    (ECPP_INTR_MASK | ECR_mode_001 |
				    ECPP_DMA_ENABLE));
			}

			set_dmac_csr(pp, DCSR_INIT_BITS2);

			mutex_exit(&pp->umutex);

			mp = allocb(sizeof (int), BPRI_MED);

			if (mp == NULL) {
				ecpp_error(pp->dip,
					"lost backchannel\n");
			} else {
					mp->b_datap->db_type = M_CTL;
				*(int *)mp->b_rptr = ECPP_BACKCHANNEL;
					mp->b_wptr = mp->b_rptr + sizeof (int);
				if ((pp->oflag == TRUE)) {
					putbq(pp->writeq, mp);
					qenable(pp->writeq);
				}
			}

			ic = DDI_INTR_CLAIMED;

			return (ic);
		}

		printf("ecpp_isr: IP, but interrupt not for us\n");

		mutex_exit(&pp->umutex);
	} else {
		printf("ecpp_isr: interrupt not for us\n");

		mutex_exit(&pp->umutex);

	}

	return (ic);
}

/*VARARGS*/
static void
ecpp_error(dev_info_t *dip, char *fmt, ...)
{
	static	long	last;
	static	char	*lastfmt;
	char		msg_buffer[255];
	va_list	ap;

	/*
	 * Don't print same error message too often.
	 */
	if ((last == (hrestime.tv_sec & ~1)) && (lastfmt == fmt)) {
		return;
	}
	last = hrestime.tv_sec & ~1;
	lastfmt = fmt;


	va_start(ap, fmt);
	vsprintf(msg_buffer, fmt, ap);
	if (ecpp_debug) {
		cmn_err(CE_CONT, "%s%d: %s", ddi_get_name(dip),
					ddi_get_instance(dip),
					msg_buffer);
	}
	va_end(ap);
}

static void
ecpp_xfer_timeout(struct ecppunit *pp)
{

	PTRACE(ecpp_xfer_timeout, ' POT', get_dmac_bcr(pp));
	mutex_enter(&pp->umutex);

	if (pp->about_to_untimeout) {
		mutex_exit(&pp->umutex);
		return;
	}

	AND_SET_BYTE_R(pp->f_handle, &pp->f_reg->ecr,
			~ECPP_DMA_ENABLE);

	AND_SET_BYTE_R(pp->f_handle, &pp->f_reg->ecr,
		~ECPP_INTR_SRV);

	RESET_DMAC_CSR(pp);

	if (ddi_dma_unbind_handle(pp->dma_handle) != DDI_SUCCESS)
		ecpp_error(pp->dip, "xfer_timeout: unbind FAILURE \n");
	else
		ecpp_error(pp->dip, "xfer_timeout: unbind OK.\n");

	if (pp->msg != NULL) {
		putbq(pp->writeq, pp->msg);
	}

	ASSERT(--pp->dma_cookie_count == 0);

	OR_SET_BYTE_R(pp->f_handle, &pp->f_reg->ecr,
			ECPP_DMA_ENABLE);

	/* mark the error status structure */
	pp->timeout_error = 1;
	pp->service = FALSE;
	pp->e_busy = ECPP_IDLE;

	ecpp_flush(pp, FWRITE);
	qenable(pp->writeq);
	cv_signal(&pp->pport_cv);

	mutex_exit(&pp->umutex);

}

/*
 * 1284 utility routines
 */
static u_char
read_config_reg(struct ecppunit *pp, u_char reg_num)
{
	u_char retval;

	mutex_enter(&pp->umutex);

	PP_PUTB(pp->c_handle, &pp->c_reg->index, reg_num);
	retval = PP_GETB(pp->c_handle, &pp->c_reg->data);

	mutex_exit(&pp->umutex);

	return (retval);
}

static void
write_config_reg(struct ecppunit *pp, u_char reg_num, u_char val)
{
	mutex_enter(&pp->umutex);

	PP_PUTB(pp->c_handle, &pp->c_reg->index, reg_num);
	PP_PUTB(pp->c_handle, &pp->c_reg->data, val);

	/*
	 * second write to this register is needed.  the register behaves as
	 * a fifo.  the first value written goes to the data register.  the
	 * second write pushes the initial value to the register indexed.
	 */

	PP_PUTB(pp->c_handle, &pp->c_reg->data, val);

	mutex_exit(&pp->umutex);
}

static void
ecpp_init_interface(struct ecppunit *pp)
{
	u_char	fer, far, ptr, fcr, pcr, pmc, dcr, ecr, dsr;
	int ptimeout;

	pp->current_phase = ECPP_PHASE_INIT;

	/*
	 * the first three configuration registers are set on POR
	 * via jumpers on the board.  We simply check them here anyway.
	 * the pport is put in compatibility mode.
	 */

	/* enable device (default == 0x0f) */
	fer = read_config_reg(pp, FER);
	if ((fer & 0x1) != 0x01)
		ecpp_error(pp->dip, "pport not enabled %x\n", fer);

	/* set address to 0x3bc (it should be set to LPT1, default == 0x11) */
	far = read_config_reg(pp, FAR);
	if ((far & 0x3) != 0x01)
		ecpp_error(pp->dip, "pport at bad address %x\n", far);

	/* set compatiblity (extended) mode and IRQ 5 (default == 0x80) */
	ptr = read_config_reg(pp, PTR);
	if ((ptr & 0x80) != 0x80)
		ecpp_error(pp->dip, "pport not in compat mode %x\n", ptr);

	/* check that float parallel port pins are driven (default == 0x00) */
	fcr = read_config_reg(pp, FCR);
	if (fcr != 0x00)
		ecpp_error(pp->dip, "pport fcr not correct %x\n", fcr);
	write_config_reg(pp, FCR, 0x00);

	/* enable ECP DMA capability */
	pmc = read_config_reg(pp, PMC);
	if (pmc != 0x08)
		ecpp_error(pp->dip, "pport pmc not correct %x\n", pmc);
	write_config_reg(pp, PMC, 0x08);

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if ((dcr & ~ECPP_DCR_SET) != ECPP_nINIT)
		ecpp_error(pp->dip, "pport dcr not correct %x\n", dcr);

	/* clear bits 3-0 in CTR (aka DCR) prior to enabling ECP mode */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	/* enable ECP mode, level intr (note that DCR bits 3-0 == 0x0) */
	pcr = read_config_reg(pp, PCR);
	if ((pcr & 0x04) != 0x04)
		ecpp_error(pp->dip, "pport pcr not correct %x\n", pcr);
	write_config_reg(pp, PCR, 0x14);

	/* at this point pport is in ECP mode, check if in reset state */
	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
	if (ecr != (ECPP_INTR_MASK | ECPP_INTR_SRV | ECPP_FIFO_EMPTY))
		ecpp_error(pp->dip, "pport ecr not correct %x\n", ecr);

	/* put chip in initial state */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		(ECR_mode_001 | ECPP_INTR_MASK |
		ECPP_INTR_SRV | ECPP_FIFO_EMPTY));

	/*
	 * put interface into compatibility mode idle phase by toggling
	 * the nInit and nSelectIn lines
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_SLCTIN);

	drv_usecwait(1);	/* 1us */

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN);

	/* check that device thinks that it has been selected */

	ptimeout = 10;
	do {
		ptimeout--;
		/* ten seconds */
		delay(1 * drv_usectohz(1000000));
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		PTRACE(ecpp_init_interface, 'nBSY', dsr);
	} while (((dsr & ECPP_nBUSY) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip, "chip_init: timeout dsr %x\n", dsr);
	}

	if ((dsr & (ECPP_nBUSY | ECPP_nACK | ECPP_SLCT | ECPP_nERR)) !=
			(ECPP_nBUSY | ECPP_nACK | ECPP_SLCT | ECPP_nERR))
		ecpp_error(pp->dip, "ecpp_init: device not selected %x\n", dsr);

}

/*
 * put the chip in mode zero (forward direction only).  Note that a switch
 * to mode zero must from some of the other modes (010 and 011) must only
 * occur after the FIFO has emptied.
 */
static void
set_chip_pio(struct ecppunit *pp)
{
	u_char	dcr;

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if ((dcr & ~ECPP_DCR_SET) != (ECPP_nINIT | ECPP_SLCTIN))
		ecpp_error(pp->dip, "chip_pio dcr not correct %x\n", dcr);

	/* in mode 0, the DCR direction bit is forced to zero */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	/* we are in centronic mode */
	pp->current_mode = ECPP_CENTRONICS;

	/* in compatible mode with no data transfer in progress */
	pp->current_phase = ECPP_PHASE_CMPT_IDLE;
}

static int
ecp_negotiation(struct ecppunit *pp)
{
	u_char datar, dsr, dcr;
	int ptimeout;

	/* Ecp negotiation */

	/* XXX Failing noe check that we are in idle phase */
#ifdef FCS
	ASSERT(pp->current_phase == ECPP_PHASE_CMPT_IDLE);
#endif
	ecpp_terminate_phase(pp);
	drv_usecwait(1000);

	/* enter negotiation phase */
	pp->current_phase = ECPP_PHASE_NEGO;

	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	/* in mode 0, the DCR direction bit is forced to zero */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if ((dcr & ~ECPP_DCR_SET) != (ECPP_nINIT | ECPP_SLCTIN))
		ecpp_error(pp->dip, "ecp_nego: dcr not correct %x\n", dcr);

	/* Event 0: host sets 0x10 on data lines */
	PP_PUTB(pp->i_handle, &pp->i_reg->ir.datar, 0x10);

	datar = PP_GETB(pp->i_handle, &pp->i_reg->ir.datar);

	/*
	 * we can read back the contents of the latched data register since
	 * the port is in extended mode
	 */
	if (datar != 0x10)
		ecpp_error(pp->dip, "pport datar not correct %x\n", datar);

	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	/* Event 1: host deassert nSelectin and assert nAutoFd */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/*
	 * Event 2: peripheral asserts nAck, deasserts nFault, asserts
	 * select, asserts Perror, wait for max 35ms.
	 */


	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		dsr &= (ECPP_nACK | ECPP_PE | ECPP_SLCT | ECPP_nERR);
	} while (dsr != ((ECPP_PE | ECPP_SLCT | ECPP_nERR)) && ptimeout);

	if (ptimeout == 0) {
		/*
		 * ECPP_nERR should be high if peripheral rejects mode,
		 * this isn't a 1284 device.
		 */
		ecpp_error(pp->dip,
			"ecp_negotiation: failed event 2 %x\n", dsr);

		pp->current_mode = ECPP_CENTRONICS;

		/* put chip back into compatibility mode */
		set_chip_pio(pp);

		return (FAILURE);
	}

	/*
	 * Event 3: hosts assert nStrobe, latching extensibility value into
	 * peripherals input latch.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX | ECPP_STB);

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/*
	 * Event 4: hosts deasserts nStrobe and nAutoFd to acknowledge that
	 * it has recognized an 1284 compatible perripheral.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	/*
	 * Event 5 & 6: peripheral deasserts Perror, deasserts Busy,
	 * asserts select if it support ECP mode, then sets nAck,
	 * wait max 35ms
	 */

	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if ((ptimeout == 0) || ((dsr & ECPP_PE) != 0)) {
		/*
		 * something seriously wrong, should just reset the
		 * interface and go back to compatible mode.
		 */
		ecpp_error(pp->dip,
			"ecp_negotiation: timeout event 6 %x\n", dsr);

		pp->current_mode = ECPP_CENTRONICS;

		/* put chip back into compatibility mode */
		set_chip_pio(pp);

		return (FAILURE);
	}

	/* ECPP_SLCT should be low if peripheral rejects mode */
	if ((dsr & ECPP_SLCT) == 0) {
		ecpp_error(pp->dip,
			"ecp_negotiation: mode rejected %x\n", dsr);

		/* terminate the mode */
		ecpp_terminate_phase(pp);

		return (FAILURE);
	}

	/* successful negotiation into ECP mode */
	pp->current_mode = ECPP_ECP_MODE;

	/* execute setup phase */
	pp->current_phase = ECPP_PHASE_ECP_SETUP;

	drv_usecwait(1);

	/* Event 30: host asserts nAutoFd */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/* Event 31: peripheral set Perror, wait max 35ms */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_PE) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"ecp_negotiation: failed event 30 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		return (FAILURE);
	}

	/* interface is now in forward idle phase */
	pp->current_phase = ECPP_PHASE_ECP_FWD_IDLE;

	/* deassert nAutoFd */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	return (SUCCESS);
}

static int
nibble_negotiation(struct ecppunit *pp)
{
	u_char datar, dsr, dcr;
	int ptimeout;

	/* Nibble mode negoatiation */

	PTRACE(nibble_negotiation, 'NiNe', pp->current_phase);
	/* check that we are in idle phase */
#ifdef FCS
	ASSERT(pp->current_phase == ECPP_PHASE_CMPT_IDLE);
#endif
	ecpp_terminate_phase(pp);
	drv_usecwait(1000);

	/* enter negotiation phase */
	pp->current_phase = ECPP_PHASE_NEGO;

	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if ((dcr & ~ECPP_DCR_SET) != (ECPP_nINIT | ECPP_SLCTIN))
		ecpp_error(pp->dip, "ecp_nego: dcr not correct %x\n", dcr);
	/* Event 0: host sets 0x00 on data lines */
	PP_PUTB(pp->i_handle, &pp->i_reg->ir.datar, 0x00);

	datar = PP_GETB(pp->i_handle, &pp->i_reg->ir.datar);

	/*
	 * we can read back the contents of the latched data register since
	 * the port is in extended mode
	 */
	if (datar != 0x00)
		ecpp_error(pp->dip, "pport datar not correct %x\n", datar);

	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	/* Event 1: host deassert nSelectin and assert nAutoFd */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/*
	 * Event 2: peripheral asserts nAck, deasserts nFault, asserts
	 * select, asserts Perror
	 */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		dsr &= (ECPP_nACK | ECPP_PE | ECPP_SLCT | ECPP_nERR);
	} while (dsr != ((ECPP_PE | ECPP_SLCT | ECPP_nERR)) && ptimeout);

	if (ptimeout == 0) {
		/*
		 * ECPP_nERR should be high if peripheral rejects mode,
		 * this isn't a 1284 device
		 */
		ecpp_error(pp->dip,
			"nibble_negotiation: failed event 2 %x\n", dsr);

		pp->current_mode = ECPP_CENTRONICS;

		/* put chip back into compatibility mode */
		set_chip_pio(pp);

		return (FAILURE);
	}

	/*
	 * Event 3: hosts assert nStrobe, latching extensibility value into
	 * peripherals input latch.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX | ECPP_STB);

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/*
	 * Event 4: hosts asserts nStrobe and nAutoFD to acknowledge that
	 * it has recognized an 1284 compatible perripheral.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	/*
	 * Event 5 & 6: peripheral deasserts Perror, deasserts Busy,
	 * asserts select if it support ECP mode, then sets nAck,
	 * wait max 35ms
	 */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if ((ptimeout == 0) || ((dsr & ECPP_PE) != 0)) {
		/*
		 * something seriously wrong, should just reset the
		 * interface and go back to compatible mode.
		 */
		ecpp_error(pp->dip,
		    "ecp_negotiation: timeout event 6 %x\n", dsr);
		pp->current_mode = ECPP_CENTRONICS;
		/* put chip back into compatibility mode */
		set_chip_pio(pp);
		return (FAILURE);
	}

	/* successful negotiation into Nibble mode */
	pp->current_mode = ECPP_NIBBLE_MODE;

	/*
	 * Event 14: Host tristates data bus, peripheral asserts nERR
	 * if data available, usually the status bits (7-0) and requires
	 * two reads since only nibbles are transfered.
	 */
	while (((dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr)) &
			ECPP_nERR) == 0) {
		(void) nibble_peripheral2host(pp);
	}

	pp->current_phase = ECPP_PHASE_NIBT_REVIDLE;

	return (SUCCESS);
}

/*
 * Upon conclusion of this routine, ecr mode = 010.
 * If successful, current_mode = ECPP_CENTRONICS, if not ECPP_FAILURE_MODE.
 */
static int
ecpp_terminate_phase(struct ecppunit *pp)
{
	u_char dsr;
	int ptimeout;

	PTRACE(ecpp_terminate_phase, 'tmod', pp->current_mode);

	if (((pp->current_mode == ECPP_NIBBLE_MODE) &&
			(pp->current_phase == ECPP_PHASE_NIBT_REVIDLE)) ||
		((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_FWD_IDLE)) ||
		((pp->current_mode == ECPP_CENTRONICS) &&
			(pp->current_phase == ECPP_PHASE_NEGO))) {
		pp->current_phase = ECPP_PHASE_TERM;
	} else
		ecpp_error(pp->dip, "ecpp_terminate_phase: why?\n");

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* superio in mode 0, the DCR direction bit is forced to zero */
	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

	/*
	 * This line is needed to make the diag program work
	 * needs investigation as to why we must go to
	 * mode 001 to terminate a phase KML XXX
	 */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	/* Event 22: hosts asserts select, deasserts nStrobe and nAutoFd. */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN);

	/* Event 23: peripheral deasserts nFault and nBusy */

	/* Event 24: peripheral asserts nAck */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_nACK) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"termination: failed event 24 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		return (FAILURE);
	}

	/* check some of the Event 23 flags */
	if ((dsr & (ECPP_nBUSY | ECPP_nERR)) != ECPP_nERR) {
		ecpp_error(pp->dip,
			"termination: failed event 23 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		return (FAILURE);
	}

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* Event 25: hosts asserts select and nAutoFd. */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN | ECPP_AFX);

	/* Event 26: the peripheral puts itself in compatible mode */

	/* Event 27: peripheral deasserts nACK */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"termination: failed event 27 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		return (FAILURE);
	}

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* Event 28: hosts deasserts nAutoFd. */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN);

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* we are in centronic mode */

	pp->current_mode = ECPP_COMPAT_MODE;

	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	/* in compatible mode with no data transfer in progress */
	pp->current_phase = ECPP_PHASE_CMPT_IDLE;

	return (SUCCESS);
}

static u_char
ecp_peripheral2host(struct ecppunit *pp)
{

	u_char ecr;
	u_char byte;
	mblk_t		*mp;

	if ((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_REV_IDLE))
		ecpp_error(pp->dip, "ecp_peripheral2host okay\n");

	/*
	 * device interrupts should be disabled by the caller and the mode
	 * should be 001, the DCR direction bit is forced to read
	 */

	AND_SET_BYTE_R(pp->i_handle,
			&pp->i_reg->dcr, ~ECPP_INTR_EN);

	AND_SET_BYTE_R(pp->i_handle, &pp->i_reg->dcr, ~ECPP_AFX);

	/* put superio into ECP mode with reverse transfer */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECR_mode_011 | ECPP_INTR_SRV | ECPP_INTR_MASK);

	pp->current_phase = ECPP_PHASE_ECP_REV_XFER;

	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);

	/*
	 * Event 42: Host tristates data bus, peripheral asserts nERR
	 * if data available, usually the status bits (7-0) and requires
	 * two reads since only nibbles are transfered.
	 */

	/* continue reading bytes from the peripheral */
	while ((ecr & ECPP_FIFO_EMPTY) == 0) {
		byte = PP_GETB(pp->f_handle, &pp->f_reg->fr.dfifo);
			PTRACE(ecp_peripheral2host, 'BYTE', byte);

		if ((mp = allocb(sizeof (u_char), BPRI_MED)) == NULL) {
			ecpp_error(pp->dip,
				"ecpp_periph2host: allocb FAILURE.\n");
			return (FAILURE);
		}

		if (canputnext(pp->readq) == 1) {
			mp->b_datap->db_type = M_DATA;
			*(u_char *)mp->b_rptr = byte;
			mp->b_wptr = mp->b_rptr + sizeof (u_char);
			putnext(pp->readq, mp);
		}
		drv_usecwait(100);
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
	}

	/* put superio into PIO mode */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECR_mode_011 | ECPP_INTR_SRV | ECPP_INTR_MASK);

	pp->current_phase = ECPP_PHASE_ECP_REV_IDLE;

	return (SUCCESS);
}

static u_char
nibble_peripheral2host(struct ecppunit *pp)
{
	u_char dsr;
	u_char byte = 0;
	int ptimeout;

	/* peripheral wants to send, so set DCR in Rev direction */
	OR_SET_BYTE_R(pp->i_handle, &pp->i_reg->dcr, ECPP_REV_DIR);

	/* superio in mode 1, the DCR direction bit is forced to read */
	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

	/* Event 7: host asserts nAutoFd to move to read 1st nibble */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/* Event 8: peripheral puts data on the status lines */

	/* Event 9: peripheral asserts nAck, data available */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_nACK) && ptimeout);

	if (ptimeout <= 1) {
		ecpp_error(pp->dip,
			"nibble_negotiation: failed event 9 %x\n", dsr);

		/* XX should reset interface */
	}

	if (dsr & ECPP_nERR)  byte = 0x01;
	if (dsr & ECPP_SLCT)  byte |= 0x02;
	if (dsr & ECPP_PE)    byte |= 0x04;
	if (~dsr & ECPP_nBUSY)byte |= 0x08;

	/* Event 10: host deasserts nAutoFd to grab data */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT);

	/* Event 11: peripheral deasserts nAck to finish handshake */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"nibble_negotiation: failed event 11 %x\n",
			dsr);

		/* XX should reset interface */
	}

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* Event 12: host asserts nAutoFd to move to read 2nd nibble */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/* Event 8: peripheral puts data on the status lines */

	/* Event 9: peripheral asserts nAck, data available */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_nACK) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"nibble_negotiation: failed event 9 %x\n", dsr);

		/* XX should reset interface */
	}

	if (dsr & ECPP_nERR)	byte |= 0x10;
	if (dsr & ECPP_SLCT)	byte |= 0x20;
	if (dsr & ECPP_PE)	byte |= 0x40;
	if (~dsr & ECPP_nBUSY)	byte |= 0x80;

	/* Event 10: host deasserts nAutoFd to grab data */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT);

	/* Event 13: peripheral deasserts nERR - no more data */

	/* Event 11: peripheral deasserts nAck to finish handshake */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_nACK) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"nibble_negotiation: failed event 11 %x\n",
			dsr);

		/* XX should reset interface */
	}

	return (byte);
}

/*
 * process data transfers requested by the peripheral
 */
static uint_t
ecpp_peripheral2host(struct ecppunit *pp)
{
	u_char dsr;
	u_char		byte;
	mblk_t		*mp;

	if (pp->current_mode == ECPP_DIAG_MODE)
		return (SUCCESS);

	switch (pp->backchannel) {
	case ECPP_CENTRONICS:
		return (SUCCESS);
	case ECPP_NIBBLE_MODE:
		if (pp->current_mode == ECPP_NIBBLE_MODE)
			return (SUCCESS);
		else {
			if (pp->current_mode != ECPP_COMPAT_MODE)
				return (FAILURE);

			ecpp_mode_negotiation(pp, ECPP_NIBBLE_MODE);
		}

		/*
		 * Event 14: Host tristates data bus, peripheral asserts nERR
		 * if data available, usually the status bits (7-0) and requires
		 * two reads since only nibbles are transfered.
		 */
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		while ((dsr & ECPP_nERR) == 0) {
			byte = nibble_peripheral2host(pp);
			if ((mp = allocb(sizeof (u_char), BPRI_MED)) == NULL) {
				ecpp_error(pp->dip,
					"ecpp_periph2host: allocb FAILURE.\n");
				return (FAILURE);
			}

			if (canputnext(pp->readq) == 1) {
				mp->b_datap->db_type = M_DATA;
				*(u_char *)mp->b_rptr = byte;
				mp->b_wptr = mp->b_rptr + sizeof (u_char);
				putnext(pp->readq, mp);
			}
			dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		}
		break;
	case ECPP_ECP_MODE:
		switch (pp->current_phase) {
		case ECPP_PHASE_ECP_REV_IDLE:
			break;
		case ECPP_PHASE_ECP_FWD_IDLE:
			if (ecp_forward2reverse(pp) == SUCCESS)
				break;
			else
				return (FAILURE);
		default:
			ecpp_error(pp->dip, "ecpp_periph2host: ECP phase");
			break;
		}

		return (ecp_peripheral2host(pp));

	default:
		ecpp_error(pp->dip, "ecpp_peripheraltohost: illegal back");
		return (FAILURE);
	}
	return (SUCCESS);
}

static int
ecp_forward2reverse(struct ecppunit *pp)
{
	u_char dsr, ecr;
	int ptimeout;

	/*
	 * device interrupts should be disabled by the caller, the ecr
	 * should be mode 001
	 */
	if ((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_FWD_IDLE))
		ecpp_error(pp->dip, "ecp_forward2reverse okay\n");

	/*
	 * In theory the fifo should always be empty at this point
	 * however, in doing active printing with periodic queries
	 * to the printer for reverse transfers, the fifo is sometimes
	 * full at this point. Other times in diag mode we have seen
	 * hangs without having an escape counter. The values of
	 * 10000 and 50 usec wait have seemed to make printing more
	 * reliable. Further investigation may be needed here XXXX
	 */

	ptimeout = 10000;
	do {
		ptimeout--;
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
		drv_usecwait(100);
	} while (((ecr & ECPP_FIFO_EMPTY) == 0) && ptimeout);

	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);


	/*
	 * Must be in Forward idle state. This line
	 * sets forward idle,
	 * set data lines in high impedance and does
	 * Event 38: host asserts nAutoFd to grab data
	 */

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	drv_usecwait(3);	/* Tp(ecp) = 0.5us */

	/* Event 39: assert nINIT */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_AFX);

	/* Event 40: peripheral deasserts PERR. */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_PE) && ptimeout);

	if (ptimeout == 0) {
		/*
		 * ECPP_PE should be low
		 */
		ecpp_error(pp->dip,
			"ecp_forward2reverse: failed event 40 %x\n", dsr);

		/* XX should do error recovery here */
		return (FAILURE);
	}

	OR_SET_BYTE_R(pp->i_handle, &pp->i_reg->dcr, ECPP_REV_DIR);

	pp->current_phase = ECPP_PHASE_ECP_REV_IDLE;

	return (SUCCESS);
}

static int
ecp_reverse2forward(struct ecppunit *pp)
{
	u_char dsr, ecr;
	int ptimeout;

	if ((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_REV_IDLE))
		ecpp_error(pp->dip, "ecp_reverse2forward okay\n");


	/*
	 * National spec says that we must do the following
	 * Make sure that FIFO is empty before we switch modes
	 * make sure that we are in mode 001 before we change
	 * direction
	 */

	ptimeout = 1000;
	do {
		ptimeout--;
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
	} while (((ecr & ECPP_FIFO_EMPTY) == 1) && ptimeout);

	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

	/* Event 47: deassert nInit */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	/* Event 48: peripheral deasserts nAck */

	/* Event 49: peripheral asserts PERR. */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_PE) == 0) && ptimeout);

	if ((ptimeout == 0) || ((dsr & ECPP_nACK) == 0)) {
		/*
		 * ECPP_nACK should be high
		 */
		ecpp_error(pp->dip,
			"ecp_reverse2forward: failed event 49 %x\n", dsr);

		/* XX should do error recovery here */
		return (FAILURE);
	}

	pp->current_phase = ECPP_PHASE_ECP_FWD_IDLE;

	return (SUCCESS);
}

/*
 * Upon conclusion of ecpp_default_negotiation(), backchannel must be set
 * to one of the following values:
 *   ECPP_CENTRONICS - if peripheral does not support 1284
 *   ECPP_ECP_MODE - if peripheral supports 1284 ECP mode.
 *   ECPP_NIBBLE_MODE - if peripheral is only 1284 compatible.
 *
 * Any other values for backchannel are illegal.
 *
 * ecpp_default_negotiation() always returns SUCCESS.
 *
 * The calling routine must hold the mutex.
 * The calling routine must ensure the port is ~BUSY before it
 * calls this routine then mark it as BUSY.
 */
static void
ecpp_default_negotiation(struct ecppunit *pp)
{

	if (ecpp_mode_negotiation(pp, ECPP_ECP_MODE) == SUCCESS) {
		if (pp->current_mode == ECPP_ECP_MODE) {
			pp->backchannel = ECPP_ECP_MODE;
		} else
			ecpp_error(pp->dip, "ecpp_mode_negotiation: unknown");
	} else if (ecpp_mode_negotiation(pp, ECPP_NIBBLE_MODE) == SUCCESS) {
		if (pp->current_mode == ECPP_NIBBLE_MODE) {
			pp->backchannel = ECPP_NIBBLE_MODE;
		} else
			ecpp_error(pp->dip, "ecpp_mode_negotiation: unknown");
	} else {
		/*
		 * for some reason the 1284 device didn't negotiate
		 * into nibble mode.  Almost a fatal error.
		 */
		PTRACE(ecpp_default_negotiation, 'cmod', ECPP_CENTRONICS);
		if (pp->current_mode == ECPP_CENTRONICS) {
			pp->backchannel = ECPP_CENTRONICS;
		} else
			ecpp_error(pp->dip, "ecpp_mode_negotiation: unknown");
	}
}

static int
ecpp_mode_negotiation(struct ecppunit *pp, u_char newmode)
{

	int rval = 0;
	int current_mode;

	PTRACE(ecpp_mode_negotiation, 'NEGO', pp->current_mode);
	if (pp->current_mode == newmode)
		return (SUCCESS);

	switch (newmode) {
	case ECPP_CENTRONICS:
		switch (pp->current_mode) {
		case ECPP_COMPAT_MODE:
		case ECPP_FAILURE_MODE:
		case ECPP_DIAG_MODE:
			pp->current_mode = ECPP_CENTRONICS;
			return (SUCCESS);
		case ECPP_ECP_MODE:
		case ECPP_NIBBLE_MODE:
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			return (SUCCESS);
		}

		if (ecpp_terminate_phase(pp)) {
		    pp->current_mode = newmode;
		    return (SUCCESS);
		}
		else
		    return (FAILURE);

	case ECPP_COMPAT_MODE:
		switch (pp->current_mode) {
		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
		case ECPP_DIAG_MODE:
			pp->current_mode = ECPP_COMPAT_MODE;
			return (SUCCESS);
		case ECPP_ECP_MODE:
		case ECPP_NIBBLE_MODE:
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			return (SUCCESS);
		}

		if (ecpp_terminate_phase(pp)) {
		    pp->current_mode = newmode;
		    return (SUCCESS);
		}
		else
		    return (FAILURE);

	case ECPP_NIBBLE_MODE:
		switch (pp->current_mode) {
		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
		case ECPP_COMPAT_MODE:
		case ECPP_DIAG_MODE:
			break;
		case ECPP_ECP_MODE:
			if (ecpp_terminate_phase(pp) == FAILURE) {
				ecpp_error(pp->dip, "unknown current mode %x\n",
					pp->current_mode);
				/* XX put channel into compatible mode */
			}
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			break;
		}
		rval = nibble_negotiation(pp);
		/*
		 * This delay is necessary for the TI Microlaser Pro 600
		 * printer. It needs to "settle" down before we try to
		 * send dma at it. If this isn't here the printer
		 * just wedges.
		 */
		delay(1 * drv_usectohz(1000000));
		return (rval);

	case ECPP_ECP_MODE:
		current_mode = pp->current_mode;
		switch (pp->current_mode) {
		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
		case ECPP_COMPAT_MODE:
			break;
		case ECPP_NIBBLE_MODE:
			if (ecpp_terminate_phase(pp) == FAILURE) {
				ecpp_error(pp->dip, "unknown current mode %x\n",
					pp->current_mode);
				/* XX put channel into compatible mode */
			}
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			break;
		}

		rval = ecp_negotiation(pp);

		if (rval != SUCCESS) {
			if (current_mode != ECPP_ECP_MODE) {
				ecpp_mode_negotiation(pp, current_mode);
			}
		}
		/*
		 * This delay is necessary for the TI Microlaser Pro 600
		 * printer. It needs to "settle" down before we try to
		 * send dma at it. If this isn't here the printer
		 * just wedges.
		 */
		delay(1 * drv_usectohz(1000000));
		return (rval);

	case ECPP_DIAG_MODE:
		switch (pp->current_mode) {
		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
		case ECPP_COMPAT_MODE:
		case ECPP_DIAG_MODE:
			break;
		case ECPP_NIBBLE_MODE:
		case ECPP_ECP_MODE:
			if (ecpp_terminate_phase(pp) == FAILURE) {
				ecpp_error(pp->dip, "unknown current mode %x\n",
					pp->current_mode);
				/* XX put channel into compatible mode */
			}
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			break;
		}
		/* put superio into PIO mode */
		PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
			(ECR_mode_001 | ECPP_INTR_MASK
			| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));

		pp->current_mode = ECPP_DIAG_MODE;
		return (SUCCESS);

	default:
		ecpp_error(pp->dip,
			"ecpp_mode_negotiation: mode %d not supported.",
			newmode);
		return (FAILURE);
	}
}

static int
ecpp_idle_phase(struct ecppunit *pp)
{

	PTRACE(ecpp_idle_phase, 'eldI', pp->backchannel);

	if (pp->current_mode == ECPP_DIAG_MODE)
		return (SUCCESS);

	switch (pp->backchannel) {
	case ECPP_CENTRONICS:
		return (SUCCESS);
	case ECPP_NIBBLE_MODE:
		if (pp->current_mode == ECPP_NIBBLE_MODE)
			return (SUCCESS);
		else {
			if (pp->current_mode != ECPP_COMPAT_MODE)
			    return (FAILURE);

			return (ecpp_mode_negotiation(pp, pp->current_mode));
		}
	case ECPP_ECP_MODE:
		switch (pp->current_phase) {
		case ECPP_PHASE_ECP_FWD_IDLE:
		case ECPP_PHASE_ECP_REV_IDLE:
		    PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
			    ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

			return (SUCCESS);
		default:
			ecpp_error(pp->dip, "ecpp_idle_phase: ECP phase");
			return (FAILURE);
		}
	default:
		ecpp_error(pp->dip, "ecpp_idle_phase: illegal back");
		return (FAILURE);
	}
}
