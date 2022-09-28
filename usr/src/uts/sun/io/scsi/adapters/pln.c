/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Copyright (c) 1995, by Cray Research, Inc.
 */

#pragma	ident	"@(#)pln.c  1.65	96/10/24 SMI"

/*
 * Pluto host adapter driver
 */

#include <sys/note.h>
#include <sys/scsi/scsi.h>
#include <sys/fc4/fc.h>
#include <sys/fc4/fcp.h>
#include <sys/fc4/fc_transport.h>
#include <sys/scsi/adapters/plndef.h>
#include <sys/scsi/targets/pln_ctlr.h>	/* for pln structures */
#include <sys/scsi/adapters/plnvar.h>
#include <sys/scsi/adapters/ssaisp.h>
#include <sys/varargs.h>



#ifdef	TRACE
#include <sys/vtrace.h>
#endif	/* TRACE */

/*
 * Local function prototypes
 */

static int pln_scsi_tgt_init(dev_info_t *, dev_info_t *,
	scsi_hba_tran_t *, struct scsi_device *);
static int pln_scsi_tgt_probe(struct scsi_device *, int (*)());
static void pln_scsi_tgt_free(dev_info_t *, dev_info_t *,
	scsi_hba_tran_t *, struct scsi_device *);
static	int pln_scsi_get_name(struct scsi_device *devp, char *name, int len);

static struct scsi_pkt *pln_scsi_init_pkt(struct scsi_address *ap,
	struct scsi_pkt *pkt, struct buf *bp, int cmdlen, int statuslen,
	int tgtlen, int flags, int (*callback)(), caddr_t arg);
static void pln_scsi_destroy_pkt(struct scsi_address *ap,
	struct scsi_pkt *pkt);
void pln_scsi_dmafree(struct scsi_address *ap, struct scsi_pkt *pkt);

static	int pln_get_int_prop(dev_info_t *dip, char *property, int *value);

static  int pln_initchild(dev_info_t *dip, dev_info_t *child_dip,
	scsi_hba_tran_t *hba_tran, pln_address_t *addr, int sleep_flag);
static	int pln_form_addr(dev_info_t *dip, pln_address_t *addr);

static	int pln_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static	int pln_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static	struct pln *pln_softstate_alloc(dev_info_t *dip, int sleep_flag);
static	void pln_softstate_free(dev_info_t *dip, struct pln *pln);
static	void pln_softstate_unlink(struct pln *);
static  int pln_start(struct scsi_address *ap, struct scsi_pkt *pkt);

static	int pln_local_start(struct pln *, struct pln_scsi_cmd *,
	pln_fc_pkt_t *);
static	int pln_dopoll(struct pln *, struct pln_fc_pkt *);
static	void pln_start_fcp(pln_fc_pkt_t *);
static	void pln_cmd_callback(struct fc_packet	*fpkt);
static	void pln_throttle(struct pln *);
static	void pln_throttle_start(struct pln *, int);
static	void pln_restart_one(struct pln *, struct pln_scsi_cmd *);
static	void pln_uc_callback(void *arg);
static	void pln_statec_callback(void *arg, fc_statec_t);
static	int pln_init_scsi_pkt(struct pln *pln, struct pln_scsi_cmd *sp);
static	int pln_prepare_fc_packet(struct pln *pln,
	struct pln_scsi_cmd *sp, struct pln_fc_pkt *);
static	void pln_fill_fc_pkt(fc_packet_t *);
static	int pln_prepare_short_pkt(struct pln *,
	struct pln_fc_pkt *, void (*)(struct fc_packet *), int);
static	void pln_fpacket_dispose_all(struct pln *pln, struct pln_disk *pd);
static	void pln_fpacket_dispose(struct pln *pln, pln_fc_pkt_t *fp);
static	int pln_prepare_cmd_dma_seg(struct pln *pln, pln_fc_pkt_t *fp,
	struct pln_scsi_cmd *sp);
static	int pln_prepare_data_dma_seg(pln_fc_pkt_t *fp, struct pln_scsi_cmd *sp);
static	int pln_execute_cmd(struct pln *pln, int cmd, int arg1,
	int arg2, caddr_t datap, int datalen, int sleep_flag);
static	int pln_private_cmd(struct pln *, int, struct pln_scsi_cmd *,
	int, int, caddr_t, int, long, int);
static	int pln_abort(struct scsi_address *ap, struct scsi_pkt *pkt);
static	int _pln_abort(struct pln *pln, struct scsi_address *ap,
	struct scsi_pkt *pkt);
static	int pln_reset(struct scsi_address *ap, int level);
static	int pln_getcap(struct scsi_address *ap, char *cap, int whom);
static	int pln_setcap(struct scsi_address *ap, char *cap,
	int value, int whom);
static	int pln_commoncap(struct scsi_address *ap, char *cap,
	int val, int tgtonly, int doset);
static	void pln_watch(caddr_t arg);
static	void pln_transport_offline(struct pln *, int, int);
static	void pln_offline_callback(struct fc_packet *);
static	void pln_transport_reset(struct pln *, int, int);
static	void pln_reset_callback(struct fc_packet *);
static	int pln_build_disk_state(struct pln *pln, int sleep_flag);
static	int pln_alloc_disk_state(struct pln *pln, int sleep_flag);
static	void pln_init_disk_state_mutexes(struct pln *pln);
static	void pln_free_disk_state(struct pln *pln);
static	void pln_destroy_disk_state_mutexes(struct pln *pln);
static	void pln_scsi_pktfree(struct scsi_pkt *);
static	int pln_cr_alloc(struct pln *, pln_fc_pkt_t *);
static	void pln_cr_free(struct pln *, pln_fc_pkt_t *, pln_cr_pool_t *,
			pln_cr_free_t *);
static	int pln_cr_pool_init(struct pln *, pln_cr_pool_t *);
static	void pln_disp_err(dev_info_t *, u_int, char *);
static void pln_get_fw_rev(char *, int, int *, int *);

#ifdef	PLNDEBUG
static	void pln_printf(dev_info_t *dip, const char *format, ...);
static	char *pln_cdb_str(char *s, u_char *cdb, int cdblen);
static	void pln_dump(dev_info_t *, char *msg, u_char *addr, int len);
#endif	/* PLNDEBUG */

/*
 * kmem cache constuctor and destructor
 */
static int pln_kmem_cache_constructor(void * buf, void *arg, int size);
static void pln_kmem_cache_destructor(void * buf, void *arg);


/*
 * Local static data
 */
static struct pln	*pln_softc		= NULL;
static kmutex_t		pln_softc_mutex;
static long		pln_watchdog_tick	= 0;
static int		pln_watchdog_id		= 0;
static u_long		pln_watchdog_time	= 1;
static u_long		pln_watchdog_init	= 0;
#ifndef	lint
static char		*pln_label		= "pln";
#endif
static int		pln_initiator_id	= PLN_INITIATOR_ID;

static int		pln_disable_timeouts	= 0;
static int		pln_online_timeout	= PLN_ONLINE_TIMEOUT;
static int		pln_en_online_timeout	= 1;
static char		pln_firmware_vers[] 	= "3.9";

/*
 * This variable must be set to 1 in /etc/system to enable
 * the suspend-resume/detach-attach feature.
 */
static int	pln_enable_detach_suspend	= 0;

#ifdef	PLNDEBUG
/*
 * PATCH this location to 0xffffffff to enable full
 * debugging.
 * The definition of each one of these mask bits is
 * in plnvar.h
 */
static u_int		plnflags		= 0x00000001;
static int		plndebug		= 0;
#endif	/* PLNDEBUG */

/*
 * Number of FCP command/response structures to allocate in attach()
 * (establishes one of the constraints on the maximum queue depth)
 */
static int		pln_fcp_elements	= PLN_CR_POOL_DEPTH;

/*
 * Externals to pln_ctlr, which is actually linked together
 * with pln to form the final driver module.
 */
extern struct cb_ops	pln_ctlr_cb_ops;

/*
 * pln_ctlr interface functions
 */
extern int	pln_ctlr_init(void);
extern void	pln_ctlr_fini(void);
extern int	pln_ctlr_attach(dev_info_t *dip, struct pln *pln);
extern int	pln_ctlr_detach(dev_info_t *dip);


/*
 * dev_ops
 */
static struct dev_ops pln_dev_ops = {
	DEVO_REV,			/* devo_rev, */
	0,				/* refcnt  */
	ddi_no_info,			/* info */
	nulldev,			/* identify */
	nulldev,			/* probe */
	pln_attach,			/* attach */
	pln_detach,			/* detach */
	nodev,				/* reset */
	&pln_ctlr_cb_ops,		/* cb_ops */
	NULL				/* no bus ops */
};

/*
 * Warlock directives
 *
 * "unshared" denotes fields that are written and read only when
 * a single thread is guaranteed to "own" the structure or variable
 */
_NOTE(SCHEME_PROTECTS_DATA("wr only by timer & attach", pln_watchdog_id))
_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_watchdog_time))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", pln_watchdog_tick))
_NOTE(SCHEME_PROTECTS_DATA("write only in attach", scsi_hba_tran))

_NOTE(MUTEX_PROTECTS_DATA(pln_softc_mutex, pln_softc))
_NOTE(MUTEX_PROTECTS_DATA(pln_softc_mutex, pln_watchdog_init))
_NOTE(SCHEME_PROTECTS_DATA("unshared", fc_packet))
_NOTE(SCHEME_PROTECTS_DATA("unshared", fcp_cmd))

_NOTE(SCHEME_PROTECTS_DATA("unshared", pln_scsi_cmd))
_NOTE(SCHEME_PROTECTS_DATA("unshared", scsi_extended_sense))
_NOTE(SCHEME_PROTECTS_DATA("unshared", scsi_arq_status))
_NOTE(SCHEME_PROTECTS_DATA("unshared", scsi_address))
_NOTE(SCHEME_PROTECTS_DATA("unshared", scsi_device))
_NOTE(SCHEME_PROTECTS_DATA("unshared", scsi_cdb))
_NOTE(SCHEME_PROTECTS_DATA("unshared", FC2_FRAME_HDR))

_NOTE(SCHEME_PROTECTS_DATA("unshared", scsi_pkt))
_NOTE(MUTEX_PROTECTS_DATA(pln_softc_mutex, pln::pln_next))
_NOTE(MUTEX_PROTECTS_DATA(pln_softc_mutex, pln::pln_ref_cnt))

_NOTE(SCHEME_PROTECTS_DATA("safe sharing", pln_address))

/*
 * Return an integer property only
 */
static int
pln_get_int_prop(
	dev_info_t		*dip,
	char			*property,
	int			*value)
{
	int			len;

	len = sizeof (int);
	return ((ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP,
		property, (caddr_t)value, &len)) == DDI_SUCCESS);
}


/*ARGSUSED*/
static int
pln_scsi_tgt_probe(
	struct scsi_device	*sd,
	int			(*callback)())
{
	int	rval;

	rval = scsi_hba_probe(sd, callback);

#ifdef PLN_DEBUG
	{
		char		*s;
		struct pln	*pln = SDEV2PLN(sd);

		switch (rval) {
		case SCSIPROBE_NOMEM:
			s = "scsi_probe_nomem";
			break;
		case SCSIPROBE_EXISTS:
			s = "scsi_probe_exists";
			break;
		case SCSIPROBE_NONCCS:
			s = "scsi_probe_nonccs";
			break;
		case SCSIPROBE_FAILURE:
			s = "scsi_probe_failure";
			break;
		case SCSIPROBE_BUSY:
			s = "scsi_probe_busy";
			break;
		case SCSIPROBE_NORESP:
			s = "scsi_probe_noresp";
			break;
		default:
			s = "???";
			break;
		}
		cmn_err(CE_CONT, "pln%d: %s target %d lun %d %s\n",
			ddi_get_instance(pln->pln_dip),
			ddi_get_name(sd->sd_dev),
			sd->sd_address.a_target,
			sd->sd_address.a_lun, s);
	}
#endif	/* PLN_DEBUG */

	return (rval);
}


/*ARGSUSED*/
static void
pln_scsi_tgt_free(
	dev_info_t		*hba_dip,
	dev_info_t		*tgt_dip,
	scsi_hba_tran_t		*hba_tran,
	struct scsi_device	*sd)
{
	pln_address_t		*pln_addr;
	struct pln_disk		*pd;
	struct pln		*pln;

#ifdef	PLN_DEBUG
	cmn_err(CE_CONT, "pln_scsi_tgt_free: %s%d %s%d <%d,%d>\n",
		ddi_get_name(hba_dip), ddi_get_instance(hba_dip),
		ddi_get_name(tgt_dip), ddi_get_instance(tgt_dip),
		targ, lun);
#endif	/* PLN_DEBUG */

	/*LINTED*/
	_NOTE(NO_COMPETING_THREADS_NOW);

	for (pln = pln_softc; pln && (pln->pln_dip != hba_dip);
		pln = pln->pln_next);

	pln_addr = hba_tran->tran_tgt_private;

	pd = pln->pln_ids[pln_addr->pln_port] + pln_addr->pln_target;
	pd->pd_state = PD_NOT_ATTACHED;

	kmem_free(pln_addr, sizeof (pln_address_t));
	hba_tran->tran_tgt_private = NULL;

	/*LINTED*/
	_NOTE(COMPETING_THREADS_NOW);
}

static int
pln_form_addr(dev_info_t *tgt_dip, pln_address_t *addr)
{
	int target, port;

	if (!pln_get_int_prop(tgt_dip, "target", &target)) {
		return (DDI_NOT_WELL_FORMED);
	}
	if (!pln_get_int_prop(tgt_dip, "port", &port)) {
		return (DDI_NOT_WELL_FORMED);
	}

	/*
	 * Set up addressing for this child
	 */
	addr->pln_entity = (u_short)PLN_ENTITY_DISK_SINGLE;
	addr->pln_port = (u_short)port;
	addr->pln_target = (u_short)target;
	addr->pln_reserved = 0;

	return (DDI_SUCCESS);
}


/*ARGSUSED*/
static int
pln_scsi_tgt_init(
	dev_info_t		*hba_dip,
	dev_info_t		*tgt_dip,
	scsi_hba_tran_t		*hba_tran,
	struct scsi_device	*sd)
{
	pln_address_t		addr;
	int			rval;

#ifdef PLN_DEBUG
	cmn_err(CE_CONT, "%s%d: %s%d <%d,%d>\n",
		ddi_get_name(hba_dip), ddi_get_instance(hba_dip),
		ddi_get_name(tgt_dip), ddi_get_instance(tgt_dip),
		sd->sd_address.a_target, sd->sd_address.a_lun);
#endif
	rval = pln_form_addr(tgt_dip, &addr);
	if (rval != DDI_SUCCESS) {
		return (rval);
	}

	return (pln_initchild(hba_dip, tgt_dip, hba_tran,
		&addr, KM_SLEEP));
}

/*ARGSUSED*/
static int
pln_scsi_get_name(struct scsi_device *devp, char *name, int len)
{
	pln_address_t	addr;

	/* rather arbitrary length check */
	if (len < 16) {
		return (0);
	}

	if (devp == SCSI_GET_INITIATOR_ID) {
		sprintf(name, "%x", pln_initiator_id);
		return (1);
	}

	if (pln_form_addr(devp->sd_dev, &addr) != DDI_SUCCESS) {
		return (0);
	}

	sprintf(name, "%d,%d", addr.pln_port, addr.pln_target);
	return (1);
}

/*
 * Function name : pln_initchild()
 *
 * Return Values : DDI_SUCCESS
 *		   DDI_FAILURE
 */
static int
pln_initchild(
	dev_info_t		*my_dip,
	dev_info_t		*child_dip,
	scsi_hba_tran_t		*hba_tran,
	pln_address_t		*addr,
	int			sleep_flag)
{
	struct pln		*pln;
	pln_address_t		*pln_addr;
	struct pln_disk		*pd;


	P_I_PRINTF((my_dip, "pln_initchild: pln dip 0x%x\n", my_dip));
	P_I_PRINTF((my_dip,
		"pln_initchild: child dip 0x%x\n", child_dip));

	mutex_enter(&pln_softc_mutex);

	for (pln = pln_softc; pln && (pln->pln_dip != my_dip);
		pln = pln->pln_next);

	mutex_exit(&pln_softc_mutex);

	if (!pln)
	    return (DDI_FAILURE);

	P_I_PRINTF((my_dip, "pln_initchild: pln structure at 0x%x\n", pln));

	P_I_PRINTF((my_dip, "pln_initchild: address 0x%x 0x%x 0x%x 0x%x\n",
		addr->pln_entity, addr->pln_port, addr->pln_target,
		addr->pln_reserved));

	/*
	 * Find the appropriate pln_disk structure for this child
	 */
	switch (addr->pln_entity) {
	case PLN_ENTITY_DISK_SINGLE:
		/*
		 * Child is an individual disk
		 */
		P_I_PRINTF((my_dip, "init child: Individual disk\n"));
		if (addr->pln_port >= pln->pln_nports ||
				addr->pln_target >= pln->pln_ntargets ||
					addr->pln_reserved != 0) {
			return (DDI_FAILURE);
		}
		pd = pln->pln_ids[addr->pln_port] + addr->pln_target;

		/* Check for duplicate calls to attach */
		if (pd->pd_state != PD_NOT_ATTACHED)
			return (DDI_FAILURE);

		pd->pd_state = PD_ATTACHED;
		break;

	default:
		P_E_PRINTF((my_dip, "init child: no such entity 0x%x\n",
			addr->pln_entity));
		return (DDI_FAILURE);
	}

	/*
	 * Allocate and initialize the address structure
	 */
	pln_addr = (pln_address_t *)
		kmem_zalloc(sizeof (pln_address_t), sleep_flag);
	if (pln_addr == NULL) {
		P_E_PRINTF((my_dip,
			"init child: pln_address alloc failed\n"));
		return (DDI_FAILURE);
	}
	P_I_PRINTF((my_dip,
		"init child: Allocated pln_addr struct at 0x%x size 0x%x\n",
		pln_addr, sizeof (pln_address_t)));
	bcopy((void *)addr, (void *)pln_addr, sizeof (pln_address_t));

	pd->pd_dip = child_dip;

	/*
	 * Set the target-private field of the transport
	 * structure to point to our extended address structure.
	 */
	hba_tran->tran_tgt_private = pln_addr;

	return (DDI_SUCCESS);
}



/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

char _depends_on[] = "misc/scsi";

static struct modldrv modldrv = {
	&mod_driverops,			/* This module is a driver */
	"SPARCstorage Array Driver 1.65", /* Name of the module. */
	&pln_dev_ops,			/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

_init(void)
{
	int	i;

	mutex_init(&pln_softc_mutex, "pln global mutex", MUTEX_DRIVER, NULL);


	if ((i = scsi_hba_init(&modlinkage)) != 0) {
		return (i);
	}
	if ((i = mod_install(&modlinkage)) != 0) {
		cmn_err(CE_CONT,
			"?pln _init: mod_install failed error=%d\n", i);
		scsi_hba_fini(&modlinkage);
		mutex_destroy(&pln_softc_mutex);
		return (i);
	}

	/*
	 * Initialize pln_ctlr
	 */
	return (pln_ctlr_init());
}

_fini(void)
{
	int	i;

	if ((i = mod_remove(&modlinkage)) != 0) {
		return (i);
	}
	/*
	 * Now that we've finished the mod_remove(), which invokes our
	 * _detach() entry point, it's safe to destroy the "pln_softc_mutex".
	 * The _detach() code flow clears up the pln global watchdog timer,
	 * which would otherwise have an outstanding entry in the kernel
	 * global callout table assosciated with "pln_softc_mutex".
	*/
	mutex_destroy(&pln_softc_mutex);

	scsi_hba_fini(&modlinkage);
	pln_ctlr_fini();

	return (i);
}

_info(
	struct modinfo	*modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


int pln_inquiry_timeout = 420;

static int
pln_attach(
	dev_info_t		*dip,
	ddi_attach_cmd_t	cmd)
{
	struct pln		*pln;
	struct fc_transport	*fc;
	struct pln_scsi_cmd	*sp;
	int			instance;
	char			buf[80];

	P_1_PRINTF((dip, "pln_attach:\n"));

	switch (cmd) {
	case DDI_ATTACH:
		P_PA_PRINTF((dip, "attaching instance 0x%x\n",
			ddi_get_instance(dip)));
		if ((pln = pln_softstate_alloc(dip, KM_SLEEP)) == NULL) {
		    P_E_PRINTF((dip, "pln_attach: softstate alloc failed\n"));
		    return (DDI_FAILURE);
		}

		/*
		 * Attach this instance of the hba
		 *
		 * We only do scsi_hba_attach in pln_attach not pln_probe
		 * because pointer to parents transport structure gets modified
		 * by scsi_hba_attach.
		 */
		if (scsi_hba_attach(dip, pln->pln_fc_tran->fc_dmalimp,
				pln->pln_tran, SCSI_HBA_TRAN_CLONE, NULL) !=
				DDI_SUCCESS) {
			P_E_PRINTF((dip, "attach: scsi_hba_attach failed\n"));
			pln_softstate_free(pln->pln_dip, pln);
			return (DDI_FAILURE);
		}
		/*
		* Construct the kmem_cache for pln_fc_pkt structures
		*/
		if (pln->pln_kmem_cache == NULL) {
			instance = ddi_get_instance(dip);
			sprintf(buf, "pln%d_cache", instance);
			pln->pln_kmem_cache = kmem_cache_create(buf,
				sizeof (struct pln_fc_pkt), 8,
				pln_kmem_cache_constructor,
				pln_kmem_cache_destructor, NULL,
				(void *)pln, NULL, 0);
			if (pln->pln_kmem_cache == NULL) {
				cmn_err(CE_WARN,
					" pln: cannot create kmem_cache");
				scsi_hba_detach(dip);
				pln_softstate_unlink(pln);
				pln_softstate_free(pln->pln_dip, pln);
				return (DDI_FAILURE);
			}
		}



		/*
		 * Link pln into the list of pln structures
		 */
		mutex_enter(&pln_softc_mutex);

		if (pln_softc == (struct pln *)NULL) {
			pln_softc = pln;
		} else {
			struct pln	*p = pln_softc;
			while (p->pln_next != NULL) {
				p = p->pln_next;
			}
			p->pln_next = pln;
		}
		pln->pln_next = (struct pln *)NULL;

		mutex_exit(&pln_softc_mutex);

		/*
		 * Get some iopb space for the fcp cmd/rsp packets
		 */
		if (!pln_cr_pool_init(pln, &pln->pln_cmd_pool))
			goto failure;

		/*
		 * Grab a couple of fc packet structures for error recovery
		 */
		if ((sp = (struct pln_scsi_cmd *)
			pln_scsi_init_pkt(&pln->pln_scsi_addr,
			(struct scsi_pkt *)NULL,
			(struct	buf *)NULL,
			sizeof (union scsi_cdb),
			sizeof (struct scsi_arq_status),
			NULL, NULL, SLEEP_FUNC, NULL)) == NULL) {
			goto failure;
		}
		pln->pkt_offline = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

		if ((sp = (struct pln_scsi_cmd *)
			pln_scsi_init_pkt(&pln->pln_scsi_addr,
			(struct scsi_pkt *)NULL,
			(struct	buf *)NULL,
			sizeof (union scsi_cdb),
			sizeof (struct scsi_arq_status),
			NULL, NULL, SLEEP_FUNC, NULL)) == NULL) {
			goto failure;
		}
		pln->pkt_reset = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

		/*
		 * Register an unsolicited cmd callback
		 */
		P_PA_PRINTF((dip, "pln_attach: uc register\n"));
		fc = pln->pln_fc_tran;
		pln->pln_uc_cookie = fc->fc_uc_register(fc->fc_cookie,
			TYPE_SCSI_FCP,
			pln_uc_callback, (void *) pln);
		if (pln->pln_uc_cookie == NULL)
			goto failure;

		/*
		 * Register the routine to handle port state changes
		 */
		pln->pln_statec_cookie = fc->fc_statec_register(fc->fc_cookie,
			pln_statec_callback, (void *) pln);
		if (pln->pln_statec_cookie == NULL)
			goto failure;

		/*
		 * Get the pluto configuration info.  This
		 * is used to build the pln_disk structure for each
		 * possible disk on the pluto.
		 */
		if (pln_build_disk_state(pln, KM_NOSLEEP))
			goto failure;


		P_1_PRINTF((dip, "pln_attach: OK\n\n"));

		/*
		 * Attach pln_ctlr
		 */
		if (pln_ctlr_attach(dip, pln) != DDI_SUCCESS)
			goto failure;

		/*
		 * Start off watchdog now we are fully initialized
		 */
		mutex_enter(&pln_softc_mutex);
		if (!pln_watchdog_init) {
			pln_watchdog_init = 1;
			mutex_exit(&pln_softc_mutex);

			pln_watchdog_tick = 3 * drv_usectohz((clock_t)1000000);
			pln_watchdog_id = timeout(pln_watch,
				(caddr_t)0, pln_watchdog_tick);
			P_PA_PRINTF((dip, "pln_attach: watchdog ok\n"));

			mutex_enter(&pln_softc_mutex);
		}

		/* Indicate we're attached */
		pln->pln_ref_cnt++;
		mutex_exit(&pln_softc_mutex);

		ddi_report_dev(dip);
		return (DDI_SUCCESS);
	case DDI_RESUME:
		mutex_enter(&pln_softc_mutex);
		for (pln = pln_softc; pln && (pln->pln_dip != dip);
			pln = pln->pln_next);
		mutex_exit(&pln_softc_mutex);

		if (!pln)
			return (DDI_FAILURE);

		mutex_enter(&pln->pln_state_mutex);
		pln->pln_state = PLN_STATE_ONLINE;
		mutex_exit(&pln->pln_state_mutex);
		return (DDI_SUCCESS);
	default:
		P_E_PRINTF((dip, "pln_attach: unknown cmd 0x%x\n", cmd));
		return (DDI_FAILURE);
	}

failure:
	scsi_hba_detach(dip);
	pln_softstate_unlink(pln);
	pln_softstate_free(pln->pln_dip, pln);
	return (DDI_FAILURE);
}


/*ARGSUSED*/
static int
pln_detach(
	dev_info_t		*dip,
	ddi_detach_cmd_t	cmd)
{
	struct pln		*pln;
	int			i;
	pln_fc_pkt_t		*fp;

	if (pln_enable_detach_suspend == 0)
		return (DDI_FAILURE);

	switch (cmd) {
	case DDI_SUSPEND:

		mutex_enter(&pln_softc_mutex);
		for (pln = pln_softc; pln && (pln->pln_dip != dip);
			pln = pln->pln_next);
		mutex_exit(&pln_softc_mutex);
		if (!pln)
			return (DDI_FAILURE);

		/*
		 * Before we go on,
		 * set our state to PLN_STATE_SUSPENDING.
		 * This action will cause pln_watch() to opt out of it's code
		 * flow very quickly, greatly increasing our success rate when
		 * attempting to DDI_SUSPEND on an active system.
		 */
		mutex_enter(&pln->pln_state_mutex);
		pln->pln_state |= PLN_STATE_SUSPENDING;
		mutex_exit(&pln->pln_state_mutex);

		/*
		 * Force the link offline to flush all commands
		 * from the hardware and put on on-hold queue.
		 */

		/*
		 * Hold off the timer
		 */
		pln->pln_timer =
			pln_watchdog_time + PLN_OFFLINE_TIMEOUT + 60;

		fp = pln->pkt_offline;
		mutex_enter(&pln->pln_state_mutex);
		if (fp->fp_state != FP_STATE_IDLE) {
		    mutex_exit(&pln->pln_state_mutex);
		    pln->pln_state &= ~PLN_STATE_SUSPENDING;
			return (DDI_FAILURE);
		}
		(void) pln_prepare_short_pkt(pln, fp,
			NULL, PLN_OFFLINE_TIMEOUT);
		fp->fp_pkt->fc_pkt_io_class = FC_CLASS_OFFLINE;
		fp->fp_state = FP_STATE_NOTIMEOUT;
		mutex_exit(&pln->pln_state_mutex);

		fp->fp_pkt->fc_pkt_flags |= FCFLAG_NOINTR;

		/*
		 * Our parent does the real work...
		 */
		if (pln->pln_fc_tran->fc_transport(fp->fp_pkt,
			FC_NOSLEEP) != FC_TRANSPORT_SUCCESS) {
			fp->fp_state = FP_STATE_IDLE;
			pln->pln_state &= ~PLN_STATE_SUSPENDING;
			return (DDI_FAILURE);
		}
		fp->fp_state = FP_STATE_IDLE;
		mutex_enter(&pln->pln_state_mutex);
		pln->pln_state = PLN_STATE_SUSPENDED;
		mutex_exit(&pln->pln_state_mutex);
		return (DDI_SUCCESS);

	case DDI_DETACH:
		P_PA_PRINTF((dip, "detaching instance %d\n",
			ddi_get_instance(dip)));

		/*
		 * Free all remaining memory allocated to this instance.
		 */
		if (pln_ctlr_detach(dip) != DDI_SUCCESS)
			return (DDI_FAILURE);

		/*
		 * Find the pln structure corresponding to this dip
		 */
		mutex_enter(&pln_softc_mutex);
		for (pln = pln_softc; pln && (pln->pln_dip != dip);
			pln = pln->pln_next);
		pln->pln_ref_cnt--;
		mutex_exit(&pln_softc_mutex);

		if (!pln)
		    return (DDI_FAILURE);

		/*
		 * Detach this instance of the hba
		*/
		scsi_hba_detach(dip);

		/*
		 * Unlink pln from the list of pln's.
		 */
		pln_softstate_unlink(pln);

		/*
		 * Kill off the watchdog if we're the last pln
		 */
		mutex_enter(&pln_softc_mutex);
		if (pln_softc == NULL) {
			pln_watchdog_init = 0;
			i = pln_watchdog_id;
			mutex_exit(&pln_softc_mutex);
			P_PA_PRINTF((dip, "pln_detach: untimeout\n"));
			(void) untimeout(i);
		} else {
			mutex_exit(&pln_softc_mutex);
		}

		P_PA_PRINTF((dip, "pln_detach: softstate free\n"));
		pln_softstate_free(dip, pln);

		P_PA_PRINTF((dip, "pln_detach: ok\n"));
		return (DDI_SUCCESS);

	/* FALLTHROUGH */
	default:
		return (DDI_FAILURE);
	}
}


/*
 * pln_softstate_alloc() - build the structures we'll use for
 * an instance of pln
 */
static struct pln *
pln_softstate_alloc(
	dev_info_t		*dip,
	int			sleep_flag)
{
	struct pln		*pln;
	char			name[32];
	struct pln_address	*addr;
	struct scsi_address	*saddr;
	scsi_hba_tran_t		*tran;


	/*
	 * Allocate softc information.
	 */
	pln = (struct pln *)kmem_zalloc(sizeof (struct pln), sleep_flag);
	if (pln == (struct pln *)NULL) {
		P_E_PRINTF((dip, "attach: pln alloc failed\n"));
		return (NULL);
	}


	pln->pln_fc_tran =
		(struct fc_transport *)ddi_get_driver_private(dip);

	sprintf(name, "pln%d mutex", ddi_get_instance(dip));
	mutex_init(&pln->pln_mutex, name, MUTEX_DRIVER, pln->pln_iblock);
	cv_init(&pln->pln_private_cv, "pln_private_cv",
		CV_DRIVER, NULL);

	/*
	 * Allocate and initialize transport structure
	 */
	tran = scsi_hba_tran_alloc(dip, 0);
	if (tran == NULL) {
		P_E_PRINTF((dip, "attach: hba_tran_alloc failed\n"));
		mutex_destroy(&pln->pln_mutex);
		cv_destroy(&pln->pln_private_cv);
		kmem_free(pln, sizeof (struct pln));
		return (NULL);
	}

	pln->pln_tran			= tran;
	pln->pln_dip			= dip;

	/*
	 * Set pointer to controller address space for internal commands
	 */
	tran->tran_hba_private		= pln;
	tran->tran_tgt_private		= &pln->pln_ctlr_addr;

	tran->tran_tgt_init		= pln_scsi_tgt_init;
	tran->tran_tgt_probe		= pln_scsi_tgt_probe;
	tran->tran_tgt_free		= pln_scsi_tgt_free;

	tran->tran_start		= pln_start;
	tran->tran_abort		= pln_abort;
	tran->tran_reset		= pln_reset;
	tran->tran_getcap		= pln_getcap;
	tran->tran_setcap		= pln_setcap;
	tran->tran_init_pkt		= pln_scsi_init_pkt;
	tran->tran_destroy_pkt		= pln_scsi_destroy_pkt;
	tran->tran_dmafree		= pln_scsi_dmafree;

	tran->tran_get_bus_addr		= pln_scsi_get_name;
	tran->tran_get_name		= pln_scsi_get_name;

	/*
	 * Allocate and initialize resources for pln:ctlr
	 */
	pln->pln_ctlr = (struct pln_disk *)
		kmem_zalloc(sizeof (struct pln_disk), sleep_flag);
	if (pln->pln_ctlr == NULL) {
		scsi_hba_tran_free(tran);
		cv_destroy(&pln->pln_private_cv);
		mutex_destroy(&pln->pln_mutex);
		P_E_PRINTF((dip, "attach: pln_disk alloc failed\n"));
		return (NULL);
	}
	pln->pln_locator = -1;
	pln->pln_ctlr->pd_dip = dip;

	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(pln->pln_state))
	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(pln->pln_maxcmds))

	/*
	 * Build the address of the pluto controller itself.
	 */
	addr = &pln->pln_ctlr_addr;
	addr->pln_entity = PLN_ENTITY_CONTROLLER;
	addr->pln_port = 0;
	addr->pln_target = 0;
	addr->pln_reserved = 0;
	saddr = &pln->pln_scsi_addr;
	saddr->a_hba_tran = pln->pln_tran;

	(void) sprintf(name, "pln%d ctlr", ddi_get_instance(dip));
	mutex_init(&pln->pln_ctlr->pd_pkt_inuse_mutex, name, MUTEX_DRIVER,
		pln->pln_iblock);
	/*
	 * initialize for throttling
	 */
	pln->pln_ncmd_ref = 0;
	pln->pln_maxcmds = PLN_MAX_CMDS;
	sprintf(name, "pln%d throttle", ddi_get_instance(dip));
	mutex_init(&pln->pln_throttle_mtx, name, MUTEX_DRIVER, pln->pln_iblock);
	sprintf(name, "pln%d state", ddi_get_instance(dip));
	mutex_init(&pln->pln_state_mutex, name, MUTEX_DRIVER, pln->pln_iblock);
	/*
	 * Initialize the pln device state
	 */
	pln->pln_state = PLN_STATE_ONLINE;
	pln->pln_en_online_timeout = pln_en_online_timeout;

	/*
	 * Initialize the fcp command/response pool mutex
	 */
	sprintf(name, "pln%d cmd/rsp pool", ddi_get_instance(dip));
	mutex_init(&pln->pln_cr_mutex, name, MUTEX_DRIVER, pln->pln_iblock);

	_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(pln->pln_state))
	_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(pln->pln_maxcmds))

	return (pln);
}


/*
 * pln_softstate_free() - release resources associated with a pln device
 */
/*ARGSUSED*/
static void
pln_softstate_free(
	dev_info_t		*dip,
	struct pln		*pln)
{
	struct fc_transport	*fc;
	pln_cr_pool_t		*cp;

	/*
	 * Delete callback routines
	 */
	fc = pln->pln_fc_tran;
	if (pln->pln_uc_cookie)
	    fc->fc_uc_unregister(fc->fc_cookie, pln->pln_uc_cookie);
	if (pln->pln_statec_cookie)
	    fc->fc_statec_unregister(fc->fc_cookie, pln->pln_statec_cookie);

	if (pln->pkt_offline)
	    pln_fpacket_dispose(pln, pln->pkt_offline);
	if (pln->pkt_reset)
	    pln_fpacket_dispose(pln, pln->pkt_reset);

	/*
	 * Free pln_ctlr and disk mutex and state
	 */
	pln_fpacket_dispose_all(pln, pln->pln_ctlr);
	mutex_destroy(&pln->pln_ctlr->pd_pkt_inuse_mutex);
	if (pln->pln_disk_mtx_init)
	    pln_destroy_disk_state_mutexes(pln);

	/*
	 * We free up the disk state here, after
	 * pln_destroy_disk_state_mutexes() has had
	 * a chance to wreak it's destruction
	 * (thereby avoiding NULLed out mutexes/pointers...)
	*/
	pln_free_disk_state(pln);

	kmem_free((void *) pln->pln_ctlr, sizeof (struct pln_disk));

	/*
	 * Get rid of the pools for fcp commands/responses
	 */
	cp = &pln->pln_cmd_pool;
	if (cp->cmd_handle)
	    ddi_dma_free(cp->cmd_handle);
		cp->cmd_handle = NULL;

	if (cp->cmd_base)
	    ddi_iopb_free(cp->cmd_base);
		cp->cmd_base = NULL;

	if (cp->rsp_handle)
	    ddi_dma_free(cp->rsp_handle);
		cp->rsp_handle = NULL;

	if (cp->rsp_base)
	    ddi_iopb_free(cp->rsp_base);
		cp->rsp_base = NULL;

	kmem_cache_destroy(pln->pln_kmem_cache);
	/*
	 * Free mutexes/condition variables
	 */
	mutex_destroy(&pln->pln_mutex);
	mutex_destroy(&pln->pln_throttle_mtx);
	mutex_destroy(&pln->pln_state_mutex);
	cv_destroy(&pln->pln_private_cv);
	mutex_destroy(&pln->pln_cr_mutex);

	/*
	 * Free the pln structure itself
	 */
	kmem_free((caddr_t)pln, sizeof (struct pln));
}
/*
 * Delete a pln instance from the list of controllers
 */
static void
pln_softstate_unlink(
	struct pln		*pln)
{
	int i = 0;

	mutex_enter(&pln_softc_mutex);

	/*
	 * If someone is looking at this structure now, we'll spin until
	 * they are done
	 */
	while (pln->pln_ref_cnt) {
	    mutex_exit(&pln_softc_mutex);

	    while (pln->pln_ref_cnt)
		i++;

	    mutex_enter(&pln_softc_mutex);
	}

	if (pln == pln_softc) {
		pln_softc = pln->pln_next;
	} else {
		struct pln	*p = pln_softc;
		struct pln	*v = NULL;
		while (p != pln) {
			ASSERT(p != NULL);
			v = p;
			p = p->pln_next;
		}
		ASSERT(v != NULL);
		v->pln_next = p->pln_next;
	}

	mutex_exit(&pln_softc_mutex);
}


/*
 * Called by target driver to start a command
 */
static int
pln_start(
	struct scsi_address		*ap,
	struct scsi_pkt			*pkt)
{
	struct pln			*pln;
	int				rval, i;
	struct pln_fc_pkt		*fp;

	struct pln_scsi_cmd	*sp = (struct pln_scsi_cmd *)pkt;
	/*
	 * Get the pln instance and fc4 address out of the pkt
	 */
	pln = ADDR2PLN(ap);
	/*
	 * Is the link dead?  If so, fail the command immediately,
	 * so that it doesn't take so long for commands to fail
	 * (otherwise detected at the poll rate of pln_watch() or pln_dopoll())
	 */
	if (pln->pln_state & PLN_STATE_LINK_DOWN) {
		return (TRAN_FATAL_ERROR);
	}

	fp = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

	ASSERT(&fp->fp_scsi_cmd == sp);
	ASSERT(fp->fp_pln == pln);
	ASSERT(fp->fp_state == FP_STATE_IDLE);

	fp->fp_retry_cnt = PLN_NRETRIES;

	/*
	 * Basic packet initialization and sanity checking
	 */
	if ((rval = pln_init_scsi_pkt(pln, sp)) != TRAN_ACCEPT) {
		return (rval);
	}
	/*
	 * Get FCP cmd/rsp packets (iopbs)
	 */
	i = (sp->cmd_pkt.pkt_flags & FLAG_NOINTR);
	fp->fp_cr_callback = (i) ? NULL : pln_start_fcp;
	/*
	 * There are 3 return values possible from cr_alloc
	 * Here we fall through if we get an fcp structure from
	 * the free list.
	 */
	if (fp->fp_cmd == NULL) {
		if (!(rval = pln_cr_alloc(pln, fp))) {
			if (i)
				return (TRAN_BADPKT);
			return (TRAN_ACCEPT);
		} else if (rval < 0)
			return (TRAN_FATAL_ERROR);
	}
	/*
	 * Build the resources necessary to transport the pkt
	 */
	rval = pln_prepare_fc_packet(pln, sp, fp);
	if (rval == TRAN_ACCEPT) {
		rval = pln_local_start(pln, sp, fp);
	}

	return (rval);
}


/*
 * Start a command.  This can be called either to start the command initially,
 * or to retry in case of link errors.
 *
 */
/* ARGSUSED */
static int
pln_local_start(
	struct pln		*pln,
	struct pln_scsi_cmd	*sp,
	struct pln_fc_pkt	*fp)
{
	int			pkt_time;
	struct fc_transport	*fc = pln->pln_fc_tran;

	P_X_PRINTF((pln->pln_dip, "pln_local_start:\n"));

	ASSERT(sp->cmd_fc_pkt);

	/*
	 * Get the pln_fc_packet
	 */
	fp = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

	ASSERT(&fp->fp_scsi_cmd == sp);
	ASSERT(fp->fp_pln == pln);
	ASSERT(fp->fp_state == FP_STATE_IDLE);

	/* XXX -- add R_A_TOV to the timeout value */
	pkt_time = fp->fp_scsi_cmd.cmd_pkt.pkt_time;

	fp->fp_timeout = (pkt_time) ?
			    pln_watchdog_time + pkt_time + PLN_TIMEOUT_PAD
			    : 0;


	/*
	 * Polled command treatment
	 */
	if (sp->cmd_pkt.pkt_flags & FLAG_NOINTR) {
	    return (pln_dopoll(pln, fp));
	}


	mutex_enter(&pln->pln_throttle_mtx);

	if ((pln->pln_state == PLN_STATE_ONLINE) &&
		(!pln->pln_throttle_flag) &&
		((pln->pln_ncmd_ref - pln->pln_maxcmds) < 0)) {
	    fp->fp_state = FP_STATE_ISSUED;
	    pln->pln_ncmd_ref++;
	} else {
	/*
	 * Throttling...
	 * Later, when looking for fp's that are in the "on hold" state,
	 * we'll scan the list of commands only if the pd_onhold_flag
	 * is set.  To avoid missing an "on hold" fp, then, we need
	 * make sure we set the flag *after* marking the state.
	 */
	    fp->fp_state = FP_STATE_ONHOLD;
	    fp->fp_pd->pd_onhold_flag = 1;
	    pln->pln_throttle_flag = 1;
	    mutex_exit(&pln->pln_throttle_mtx);
	    return (TRAN_ACCEPT);
	}

	mutex_exit(&pln->pln_throttle_mtx);

	/*
	 * Run the command
	 */
	switch (fc->fc_transport(fp->fp_pkt, FC_SLEEP)) {

		case FC_TRANSPORT_SUCCESS:
			return (TRAN_ACCEPT);

		case FC_TRANSPORT_QFULL:
		case FC_TRANSPORT_UNAVAIL:
			fp->fp_state = FP_STATE_ONHOLD;
			fp->fp_pd->pd_onhold_flag = 1;
			mutex_enter(&pln->pln_throttle_mtx);
			pln->pln_throttle_flag = 1;
			mutex_exit(&pln->pln_throttle_mtx);
			return (TRAN_ACCEPT);

		case FC_TRANSPORT_FAILURE:
		case FC_TRANSPORT_TIMEOUT:
			fp->fp_state = FP_STATE_IDLE;
			mutex_enter(&pln->pln_throttle_mtx);
			pln->pln_ncmd_ref--;
			mutex_exit(&pln->pln_throttle_mtx);
			return (TRAN_BADPKT);

		default:
			pln_disp_err(pln->pln_dip, CE_PANIC,
				"Invalid transport status\n");
		_NOTE(NOT_REACHED);
		/* NOTREACHED */
	}

}

/*
 * Run a command in polled mode
 */
static int
pln_dopoll(
	struct pln		*pln,
	struct pln_fc_pkt	*fp)
{
	struct fc_transport	*fc = pln->pln_fc_tran;
	int			timer;
	int			timeout_flag;

	ASSERT(fp->fp_state != FP_STATE_ISSUED);

	for (;;) {
	    mutex_enter(&pln->pln_throttle_mtx);
	    pln->pln_ncmd_ref++;
	    mutex_exit(&pln->pln_throttle_mtx);
	    timeout_flag = 0;
	    fp->fp_state = FP_STATE_ISSUED;

	    switch (fc->fc_transport(fp->fp_pkt, FC_SLEEP)) {

		case FC_TRANSPORT_SUCCESS:
		    pln_cmd_callback(fp->fp_pkt);
		    if (fp->fp_state == FP_STATE_IDLE)
			return (TRAN_ACCEPT);
		    if ((fp->fp_state == FP_STATE_PRETRY) &&
			(fp->fp_pkt->fc_pkt_status != FC_STATUS_ERR_OFFLINE)) {
			pln_transport_offline(pln, PLN_STATE_ONLINE, 1);
		    }
		    fp->fp_state = FP_STATE_IDLE;

		/* FALLTHROUGH */
		case FC_TRANSPORT_QFULL:
		case FC_TRANSPORT_UNAVAIL:
		    mutex_enter(&pln->pln_throttle_mtx);
		    pln->pln_ncmd_ref--;
		    mutex_exit(&pln->pln_throttle_mtx);
		    break;

		case FC_TRANSPORT_TIMEOUT:
		    pln_transport_offline(pln, PLN_STATE_ONLINE, 1);
		    timeout_flag = 1;
		    mutex_enter(&pln->pln_throttle_mtx);
		    pln->pln_ncmd_ref--;
		    mutex_exit(&pln->pln_throttle_mtx);
		    break;

		case FC_TRANSPORT_FAILURE:
		    mutex_enter(&pln->pln_throttle_mtx);
		    pln->pln_ncmd_ref--;
		    mutex_exit(&pln->pln_throttle_mtx);
		    return (TRAN_BADPKT);

		default:
		    pln_disp_err(pln->pln_dip, CE_PANIC,
			    "Invalid transport status\n");
		_NOTE(NOT_REACHED);
		/* NOTREACHED */
	    }

	/*
	 * Error recovery loop
	 */
	    timer = (PLN_ONLINE_TIMEOUT * 1000000) / PLN_POLL_DELAY;
	    do {
		drv_usecwait(PLN_POLL_DELAY);
		fc->fc_interface_poll(fc->fc_cookie);

		/* Check for a timeout waiting for an online */
		if (pln->pln_en_online_timeout && (--timer <= 0)) {
		    fp->fp_state = FP_STATE_ISSUED;
		    mutex_enter(&pln->pln_throttle_mtx);
		    pln->pln_ncmd_ref++;
		    mutex_exit(&pln->pln_throttle_mtx);
		    fp->fp_pkt->fc_pkt_status = (timeout_flag) ?
				FC_STATUS_TIMEOUT : FC_STATUS_ERR_OFFLINE;
		    fp->fp_retry_cnt = 1;
		    pln_cmd_callback(fp->fp_pkt);
		    return (TRAN_ACCEPT);
		}
	    } while (!(fp->fp_pkt->fc_pkt_flags & FCFLAG_COMPLETE) ||
			!(pln->pln_state & PLN_STATE_ONLINE));
	}
}

/*
 * Start a command after having waited for FCP cmd/response pkts
 */
static void
pln_start_fcp(
	pln_fc_pkt_t		*fp)
{
	struct pln_scsi_cmd	*sp;
	struct pln		*pln;
	int			failure = 0;

	/*
	 * Build the resources necessary to transport the pkt
	 */
	sp = &fp->fp_scsi_cmd;
	pln = fp->fp_pln;
	if (pln_prepare_fc_packet(pln, sp, fp) != TRAN_ACCEPT)
	    failure++;

	if (!failure) {
	    if (pln_local_start(pln, sp, fp) != TRAN_ACCEPT)
		failure++;
	}

	if (failure) {

	    /* Failed to start, fake up some status information */
	    sp->cmd_pkt.pkt_state = 0;
	    sp->cmd_pkt.pkt_statistics = 0;
	    sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;

	    if (sp->cmd_pkt.pkt_comp) {
		(*sp->cmd_pkt.pkt_comp)((struct scsi_pkt *)sp);
	    }
	}
}

/*
 * Command completion callback
 */
static void
pln_cmd_callback(
	struct fc_packet	*fpkt)
{
	struct pln_fc_pkt	*fp;
	struct pln		*pln;
	struct pln_scsi_cmd	*sp;
	struct fcp_rsp		*rsp;
	struct scsi_arq_status	*arq;
	struct fcp_scsi_bus_err *bep;
	int			i;
	caddr_t			msg1 = NULL;
	char			msg[80];
	char			msgbuild[80];
	int			new_state = FP_STATE_IDLE;

	fp = (struct pln_fc_pkt *)fpkt->fc_pkt_private;
	sp = &fp->fp_scsi_cmd;
	pln = fp->fp_pln;

	ASSERT(fp == (struct pln_fc_pkt *)sp->cmd_fc_pkt);

	ASSERT(fp->fp_state == FP_STATE_ISSUED ||
	    fp->fp_state == FP_STATE_TIMED_OUT);

	P_X_PRINTF((pln->pln_dip,
		"pln_cmd_callback:  Transport status=0x%x  statistics=0x%x\n",
		fpkt->fc_pkt_status, fpkt->fc_pkt_statistics));

	/*
	 * Decode fc transport (SOC) status
	 * and map into reason codes
	 *
	 * If Transport ok then use scsi status to
	 * update scsi packet information
	 */
	switch (fpkt->fc_pkt_status) {
	case FC_STATUS_OK:
		/*
		 * At least command came back from SOC OK
		 * May have had a transport error in SSA
		 * or command may have failed in the disk
		 * in which case we should have scsi sense data.
		 */

		/* Default to command completed normally */
		sp->cmd_pkt.pkt_reason = CMD_CMPLT;

		i = ddi_dma_sync(pln->pln_cmd_pool.rsp_handle,
			(caddr_t)fp->fp_rsp - pln->pln_cmd_pool.rsp_base,
			(u_int)(sizeof (struct pln_rsp)),
			DDI_DMA_SYNC_FORKERNEL);
		if (i != DDI_SUCCESS) {
			P_E_PRINTF((pln->pln_dip,
				"ddi_dma_sync failed (rsp)\n"));
			sp->cmd_pkt.pkt_reason = CMD_STS_OVR;
			break;
		}

		/*
		 * Ptr to the FCP response area
		 */
		rsp = (struct fcp_rsp *)fp->fp_rsp;

		/*
		 * Update the command status
		 */

		/*
		 * Default to all OK which is what we report
		 * unless there was a problem.
		 */
		sp->cmd_pkt.pkt_state = STATE_GOT_BUS |
			STATE_GOT_TARGET |
			STATE_SENT_CMD |
			STATE_GOT_STATUS;
		if (sp->cmd_flags & P_CFLAG_DMAVALID) {
			sp->cmd_pkt.pkt_state |= STATE_XFERRED_DATA;
			/* For consistent memory reads, sync the data */
			if ((sp->cmd_flags & P_CFLAG_CONSISTENT) &&
				(!(sp->cmd_flags & P_CFLAG_DMAWRITE)))
				(void) ddi_dma_sync(sp->cmd_dmahandle, 0, 0,
						DDI_DMA_SYNC_FORKERNEL);
		}

		/*
		 * Check to see if we got a status byte but
		 * no Request Sense information.
		 * Can happen on busy or reservation conflict.
		 *
		 * Do it the old way for 1093 until we prove
		 * new way works on 1093!!!!
		 *
		 *
		 */
		if (sp->cmd_pkt.pkt_scbp && ((*(sp->cmd_pkt.pkt_scbp) =
			rsp->fcp_u.fcp_status.scsi_status) != STATUS_GOOD)) {
			    if (!rsp->fcp_u.fcp_status.rsp_len_set &&
			    !rsp->fcp_u.fcp_status.sense_len_set) {
				sp->cmd_pkt.pkt_state &= ~STATE_XFERRED_DATA;
				sp->cmd_pkt.pkt_resid = sp->cmd_dmacount;
				break;
			    }
		}

		/*
		 * Zero the pkt_statistics field
		 */
		sp->cmd_pkt.pkt_statistics = 0;

		/*
		 * Update the transfer resid, if appropriate
		 */
		sp->cmd_pkt.pkt_resid = 0;
		if (rsp->fcp_u.fcp_status.resid_len_set) {
			sp->cmd_pkt.pkt_resid = rsp->fcp_resid;
			P_X_PRINTF((pln->pln_dip,
			"All data NOT transfered: resid: 0x%x\n",
				rsp->fcp_resid));
		}

		/*
		 * Check to see if the SCSI command failed.
		 *
		 * If it did then update the request sense info
		 * and state.
		 *
		 * The target driver should always enable automatic
		 * request sense when interfacing to a Pluto...
		 */

		/*
		 * First see if we got a transport
		 * error in the SSA.  If so, we set the corresponding
		 * fields in the scsi_pkt.
		 */
		if (rsp->fcp_u.fcp_status.rsp_len_set) {

		/*
		 * Transport information
		 */
		    bep = (struct fcp_scsi_bus_err *)
			    (&rsp->fcp_response_len +
			    1 +
			    rsp->fcp_sense_len);
		    sp->cmd_pkt.pkt_state = (bep->isp_state_flags >> 8);
		    switch (bep->rsp_info_type) {
		    case FCP_RSP_SCSI_BUS_ERR:
			sp->cmd_pkt.pkt_reason = bep->isp_status;
			sp->cmd_pkt.pkt_statistics = bep->isp_stat_flags;
			/*
			 * Print scsi bus errors only if target was
			 * selected.
			 */
			if ((sp->cmd_pkt.pkt_state & STATE_GOT_TARGET)) {
				if ((bep->isp_status >= CMD_CMPLT) &&
				    (bep->isp_status <= CMD_UNX_BUS_FREE)) {
					msg1 = scsi_rname(bep->isp_status);
				} else {
					msg1 = "FCP_RSP_CMD: UNKNOWN";
				}
			}
			break;
		    case FCP_RSP_SCSI_PORT_ERR:
			sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
			msg1 = "FCP_RSP_SCSI_PORT_ERR";
			break;
		    case FCP_RSP_SOC_ERR:
			sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
			msg1 = "FCP_RSP_SOC_ERR";
			break;
		    default:
			sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
			msg1 = "Response type: UNKNOWN";
			break;
		    }
		}

		/*
		 * See if we got a SCSI error with sense data
		 */
		if (rsp->fcp_u.fcp_status.sense_len_set) {
		    u_char rqlen = min(rsp->fcp_sense_len,
			    sizeof (struct scsi_extended_sense));
		    caddr_t sense = (caddr_t)rsp +
			    sizeof (struct fcp_rsp);
		    if (sp->cmd_pkt.pkt_scbp)
			*(sp->cmd_pkt.pkt_scbp) = STATUS_CHECK;
#ifdef	PLNDEBUG
		    if (plndebug) {
			sprintf(msg, "Request Sense Info: len=0x%x\n", rqlen);
			pln_disp_err(fp->fp_pd->pd_dip, CE_NOTE, msg);
			pln_dump(fp->fp_pd->pd_dip, "sense data: ",
					(u_char *)sense, rqlen);
		    }
#endif	PLNDEBUG
		    if ((sp->cmd_senselen >= sizeof (struct scsi_arq_status)) &&
				(sp->cmd_pkt.pkt_scbp)) {
			/*
			 * Automatic Request Sense enabled.
			 */
			sp->cmd_pkt.pkt_state |= STATE_ARQ_DONE;

			arq = (struct scsi_arq_status *)
			    sp->cmd_pkt.pkt_scbp;
			/*
			 * copy out sense information
			 */
			bcopy(sense, (caddr_t)&arq->sts_sensedata,
				rqlen);
			arq->sts_rqpkt_resid =
				sizeof (struct scsi_extended_sense) -
					rqlen;
			/*
			 * Set up the flags for the auto request sense
			 * command like we really did it even though
			 * we didn't.
			 */
			*((u_char *)&arq->sts_rqpkt_status) = STATUS_GOOD;
			arq->sts_rqpkt_reason = 0;
			arq->sts_rqpkt_statistics = 0;
			arq->sts_rqpkt_state = STATE_GOT_BUS |
			STATE_GOT_TARGET |
			STATE_SENT_CMD |
			STATE_GOT_STATUS |
			STATE_ARQ_DONE |
			STATE_XFERRED_DATA;

		    }
		}
		P_X_PRINTF((pln->pln_dip,
			"pln_cmd_callback: pkt_state: 0x%x\n",
			sp->cmd_pkt.pkt_state));
		break;

	case FC_STATUS_ERR_OFFLINE:
		/* Note that we've received an offline response */
		mutex_enter(&pln->pln_state_mutex);
		if (pln->pln_state == PLN_STATE_ONLINE)
		    pln->pln_state |= PLN_STATE_OFFLINE_RSP;

		mutex_exit(&pln->pln_state_mutex);

		if (fp->fp_state == FP_STATE_TIMED_OUT) {
		/*
		 * this packet, all by itself, timed out.
		 * There is no reason to retry it
		 */
		    pln_address_t *pln_addr =
			(pln_address_t *)
			(fp->fp_scsi_cmd.cmd_pkt.pkt_address
			    .a_hba_tran->tran_tgt_private);
		    u_short port = pln_addr->un.pln_addr.pa_port;
		    u_short target = pln_addr->un.pln_addr.pa_target;

		    sprintf(msgbuild, "COMMAND TIMED OUT: port %d target %d",
			port, target);
		    msg1 = msgbuild;
		    sp->cmd_pkt.pkt_reason = CMD_TIMEOUT;
		} else if (--fp->fp_retry_cnt > 0) {

		    if (sp->cmd_pkt.pkt_flags & FLAG_NOINTR)
			new_state = FP_STATE_PRETRY;

		    /* Wait for a state change to online */
		    else
			new_state = FP_STATE_ONHOLD;
		} else {
		    msg1 = "Fibre Channel Offline";
		    sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		}
		break;

	case FC_STATUS_MAX_XCHG_EXCEEDED:
		if (sp->cmd_pkt.pkt_flags & FLAG_NOINTR)
		    new_state = FP_STATE_PTHROT;

		else {
		    pln_throttle(pln);
		    new_state = FP_STATE_ONHOLD;
		}
		break;

	case FC_STATUS_P_RJT:
		if (!fpkt->fc_frame_resp) {
		    msg1 = "Received P_RJT status, but no header";
		} else if (
		((aFC2_RJT_PARAM *)&fpkt->fc_frame_resp->ro)->rjt_reason ==
			CANT_ESTABLISH_EXCHANGE) {

		    if (sp->cmd_pkt.pkt_flags & FLAG_NOINTR)
			new_state = FP_STATE_PTHROT;

		    /* Need to throttle... */
		    else {
			pln_throttle(pln);
			new_state = FP_STATE_ONHOLD;
		    }
		    break;
		} else {
		    msg1 = "Fibre Channel P_RJT";
		}
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_TIMEOUT:
		if (--fp->fp_retry_cnt > 0) {

		    if (sp->cmd_pkt.pkt_flags & FLAG_NOINTR)
			new_state = FP_STATE_PRETRY;

		    else {
			pln_transport_offline(pln, PLN_STATE_ONLINE, 0);
			new_state = FP_STATE_ONHOLD;
		    }
		} else {
		    msg1 = "Fibre Channel Timeout";
		    sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		}
		break;

	case FC_STATUS_ERR_OVERRUN:
		msg1 = "CMD_DATA_OVR";
		sp->cmd_pkt.pkt_reason = CMD_DATA_OVR;
		break;

	case FC_STATUS_P_BSY:
		msg1 = "Fibre Channel P_BSY";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_UNKNOWN_CQ_TYPE:
		msg1 = "Unknown CQ type";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_BAD_SEG_CNT:
		msg1 = "Bad SEG CNT";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_BAD_XID:
		msg1 = "Fibre Channel Invalid X_ID";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_XCHG_BUSY:
		msg1 = "Fibre Channel Exchange Busy";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_INSUFFICIENT_CQES:
		msg1 = "Insufficient CQEs";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_ALLOC_FAIL:
		msg1 = "ALLOC FAIL";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_BAD_SID:
		msg1 = "Fibre Channel Invalid S_ID";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_NO_SEQ_INIT:
		msg1 = "Fibre Channel Seq Init Error";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	case FC_STATUS_ONLINE_TIMEOUT:
		msg1 = "Fibre Channel Online Timeout";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;

	default:
		msg1 = "Unknown FC Status";
		sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;
		break;
	}


	/*
	 * msg1 will be non-NULL if we've detected some sort of error
	 * We use CE_NOTE instead of CE_WARN since CE_WARN calls ddi_pathname
	 * which calls ASSERT(DEV_OPS_HELD()) which can fail if the dev_ops
	 * refcnt is zero.
	 */
	if (msg1) {
	    sprintf(msg, "!Transport error:  %s", msg1);
	    pln_disp_err(fp->fp_pd->pd_dip, CE_NOTE, msg);

	}


	mutex_enter(&pln->pln_throttle_mtx);
	pln->pln_ncmd_ref--;
	mutex_exit(&pln->pln_throttle_mtx);

	/*
	 * Update the command state.  We must do this before checking
	 * the fp_timeout_flag, since pln_watch could be setting this
	 * flag at the same time as we get here.
	 */
	fp->fp_state = new_state;

	/*
	 * Check to see if we're waiting for the queue to empty following
	 * a timeout detection.
	 */
	if (fp->fp_timeout_flag) {
	    mutex_enter(&pln->pln_state_mutex);

	    if (fp->fp_timeout_flag) {
		fp->fp_timeout_flag = 0;

		if (--(pln->pln_timeout_count) == 0) {
		    pln->pln_state &= ~PLN_STATE_TIMEOUT;
		    if (pln->pln_state == PLN_STATE_ONLINE)
			P_W_PRINTF((pln->pln_dip,
			    "pln timeout recovery not required.\n"));
		}
	    }

	    mutex_exit(&pln->pln_state_mutex);
	}

	if (new_state == FP_STATE_IDLE) {
	    if (sp->cmd_pkt.pkt_comp) {
		(*sp->cmd_pkt.pkt_comp)((struct scsi_pkt *)sp);
	    }
	} else if (new_state == FP_STATE_ONHOLD) {
	    fp->fp_pd->pd_onhold_flag = 1;
	    mutex_enter(&pln->pln_throttle_mtx);
	    pln->pln_throttle_flag = 1;
	    mutex_exit(&pln->pln_throttle_mtx);
	}

	/*
	 * Try to start any "throttled" commands
	 */
	if (pln->pln_throttle_flag &&
		(pln->pln_state == PLN_STATE_ONLINE) &&
		((pln->pln_maxcmds - pln->pln_ncmd_ref) >
		PLN_THROTTLE_START)) {
	    pln_throttle_start(pln, 0);
	}
}

/*
 * Establish a new command throttle
 *
 * We basically just go into the share mode to only use 1/2 the
 * available exchanges the SOC can handle. This gets bumped
 * back up every second by pln_watch.
 *
 */
static void
pln_throttle(struct pln *pln)
{
	mutex_enter(&pln->pln_throttle_mtx);
	    pln->pln_throttle_cnt++;
	    pln->pln_maxcmds = PLN_MAX_CMDS/2;
	mutex_exit(&pln->pln_throttle_mtx);
}

/*
 * Try to start any "throttled" commands
 *
 * Hopefully, most calls to this routine will cause an already-built
 * list of "throttled" commands per pln_disk (the pd_onhold_head linkage)
 * to be scanned, looking for commands to start.  When this list is
 * exhausted, we need to scan through the "inuse" list of all pln_disk
 * structures, constructing a new "on hold" list.
 *
 * When adjusting the throttle up we only issue a limited number of
 * commands.
 *
 */
static void
pln_throttle_start(struct pln *pln, int throttle_up_flag)
{
	struct pln_disk		*pd;
	pln_fc_pkt_t		*fp,
				*fpn;
	int			j;
	int			build_flag = 0;
	int			throttle_up_count = 0;

	mutex_enter(&pln->pln_throttle_mtx);
	if (!pln->pln_throttle_flag) {
	    mutex_exit(&pln->pln_throttle_mtx);
	    return;
	}

	pln->pln_throttle_flag = 0;
	pd = pln->cur_throttle;
	j = 1;

	/*
	 * The outer loop looks through the already-built lists
	 * of "on hold" commands
	 */
	while (((pln->pln_maxcmds - pln->pln_ncmd_ref) > 0) &&
	    ((!throttle_up_flag) ||
		(throttle_up_count < PLN_THROTTLE_UP_CNT))) {
	    if (pd == pln->cur_throttle) {
		if (!j) {

		/*
		 * This inner loop scans through all pln_disks
		 * to build new "on hold" command lists.
		 * Note that the "on hold" list for all pln_disks
		 * is protected by the pln_throttle_mtx.
		 */
		    if (!build_flag) {
			pd = pln->pln_ids[0];
			do {
			    if (pd->pd_onhold_flag) {
				pd->pd_onhold_flag = 0;
				fpn = NULL;
				mutex_enter(&pd->pd_pkt_inuse_mutex);
				for (fp = pd->pd_inuse_head; fp;
					fp = fp->fp_next) {
				    if (fp->fp_state == FP_STATE_ONHOLD) {
					j++;
					fp->fp_onhold = NULL;
					if (!fpn) {
					    fpn = fp;
					    pd->pd_onhold_head = fp;
					} else {
					    fpn->fp_onhold = fp;
					    fpn = fp;
					}
				    }
				}
				mutex_exit(&pd->pd_pkt_inuse_mutex);
			    }
			} while ((pd = pd->pd_next) != pln->pln_ids[0]);

			pd = pln->cur_throttle;
			build_flag = 1;
		    }

		    if (!j) {
			mutex_exit(&pln->pln_throttle_mtx);
			return;
		    }
		}
		j = 0;
	    }

	/*
	 * If there's a throttled command for this device,
	 * start it.
	 */
	    if ((fp = pd->pd_onhold_head) != NULL) {
		if (throttle_up_flag)
			throttle_up_count++;
		j++;
		pd->pd_onhold_head = fp->fp_onhold;
		fp->fp_state = FP_STATE_IDLE;
		mutex_exit(&pln->pln_throttle_mtx);
		pln_restart_one(pln, &fp->fp_scsi_cmd);
		mutex_enter(&pln->pln_throttle_mtx);
	    }
	    pd = pd->pd_next;
	}

	pln->pln_throttle_flag = 1;

	pln->cur_throttle = pd;

	mutex_exit(&pln->pln_throttle_mtx);
}


/*
 * Restart a single command.
 *
 * We use this routine to resume command processing after throttling of
 * the command or to retry after link error detection.
 */
static void
pln_restart_one(
	struct pln	*pln,
	struct pln_scsi_cmd	*sp)
{
	pln_fc_pkt_t	*fp;

	fp = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

	if (pln_local_start(pln, sp, fp) != TRAN_ACCEPT) {

	/*
	 * Give up if we've encountered a hard transport
	 * failure...
	 */
	    sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;

	    fp->fp_state = FP_STATE_IDLE;
	    if (sp->cmd_pkt.pkt_comp) {
		(*sp->cmd_pkt.pkt_comp)((struct scsi_pkt *)sp);
	    }
	}
}

/*
 * Interface state changes are communicated back to us through
 * this routine
 */
static void
pln_statec_callback(
	void		*arg,
	fc_statec_t	msg)
{
	struct pln	*pln = (struct pln *)arg;
	pln_fc_pkt_t	*fp;
	struct pln	*p;
	struct pln_disk	*pd;

	/*
	 * Make sure we're still attached
	 */
	mutex_enter(&pln_softc_mutex);
	for (p = pln_softc; p; p = p->pln_next)
	    if (p == pln) break;

	/* If we're not completely attached, forget it */
	if (!pln->pln_ref_cnt) {
	    mutex_exit(&pln_softc_mutex);
	    return;
	}

	mutex_exit(&pln_softc_mutex);

	if (p != pln)
	    return;

	P_UC_PRINTF((pln->pln_dip, "pln: state change callback\n"));

	mutex_enter(&pln->pln_state_mutex);

	switch (msg) {
	    case FC_STATE_ONLINE:
		if (pln->pln_state == PLN_STATE_ONLINE) {
		    mutex_exit(&pln->pln_state_mutex);
		    return;
		}

		/*
		 * We're transitioning from offline to online, so
		 * reissue all commands in the "on hold" state
		 */
		/*
		 * First reset the watchdog timer
		 * to prevent it from timing out
		 * before we get PLN_STATE_ONLINE set
		 */
		pln->pln_timer = pln_watchdog_time + pln_online_timeout;
		pln->pln_state = PLN_STATE_ONLINE;
		mutex_exit(&pln->pln_state_mutex);

		pln_throttle_start(pln, 0);

		return;

	    case FC_STATE_OFFLINE:
		/*
		 * The link went offline.  Set the timer so we
		 * can time out transitions back to an online
		 * state.
		 */
		pln->pln_timer = pln_watchdog_time + pln_online_timeout;
		pln->pln_state = PLN_STATE_OFFLINE;
		mutex_exit(&pln->pln_state_mutex);
		break;

	    case FC_STATE_RESET:
		/*
		 * Start the online timer before setting the state
		 */
		pln->pln_timer = pln_watchdog_time + pln_online_timeout;
		pln->pln_state |= PLN_STATE_RESET | PLN_STATE_OFFLINE;

		mutex_exit(&pln->pln_state_mutex);

		/*
		 * We assume the lower level has taken care of passing
		 * any completed commands back to us before returning
		 * this status.  Thus, we'll mark everything on the
		 * "in use" list as "idle", and
		 * wait for an online state change.
		 */
		pd = pln->pln_disk_list;
		do {
		    mutex_enter(&pd->pd_pkt_inuse_mutex);

		    for (fp = pd->pd_inuse_head; fp; fp = fp->fp_next)
			if (fp->fp_state == FP_STATE_ISSUED) {
			    fp->fp_state = FP_STATE_ONHOLD;
			    pd->pd_onhold_flag = 1;
			    if (!pln->pln_throttle_flag) {
				mutex_enter(&pln->pln_throttle_mtx);
				pln->pln_throttle_flag = 1;
				mutex_exit(&pln->pln_throttle_mtx);
			    }
			    if (fp->fp_timeout_flag) {
				fp->fp_timeout_flag = 0;
				mutex_enter(&pln->pln_state_mutex);
				pln->pln_timeout_count--;
				mutex_exit(&pln->pln_state_mutex);
			    }
			}

		    mutex_exit(&pd->pd_pkt_inuse_mutex);

		} while ((pd = pd->pd_next) != pln->pln_disk_list);

		/*
		 * Reset state change indications from the lower level
		 * must guarantee that all cmds, including the reset,
		 * have been returned, and that any commands not yet
		 * returned are lost in the hardware.
		 */
		/* Say no commands issued */
		mutex_enter(&pln->pln_throttle_mtx);
		pln->pln_ncmd_ref = 0;
		mutex_exit(&pln->pln_throttle_mtx);

		pln->pkt_offline->fp_state = FP_STATE_IDLE;
		break;

	    default:
		mutex_exit(&pln->pln_state_mutex);
		pln_disp_err(pln->pln_dip, CE_WARN,
			    "Unknown state change\n");
		break;

	}

}

/*
 * Asynchronous callback
 */
static void
pln_uc_callback(
	void		*arg)
{
	struct pln	*pln = (struct pln *)arg;

	P_UC_PRINTF((pln->pln_dip, "pln: unsolicited callback\n"));
}


/*
 * Initialize scsi packet and do some sanity checks
 * before starting a command.
 */
static int
pln_init_scsi_pkt(
	struct pln	*pln,
	struct pln_scsi_cmd	*sp)
{

	P_X_PRINTF((pln->pln_dip, "pln_init_scsi_pkt: scsi_cmd: 0x%x\n",
		sp));

	/*
	 * Clear out SCSI cmd LUN.
	 * We dont support LUN's.
	 */
	sp->cmd_pkt.pkt_cdbp[1] &= 0x1f;

#ifdef	PLNDEBUG
	if (plnflags & P_S_FLAG) {
		char	cdb[128];
		if (sp->cmd_flags & P_CFLAG_DMAVALID) {
			pln_printf(pln->pln_dip,
				"cdb=%s %s 0x%x\n",
				pln_cdb_str(cdb, sp->cmd_pkt.pkt_cdbp,
				sp->cmd_cdblen),
				(sp->cmd_flags & P_CFLAG_DMAWRITE) ?
				"write" : "read", sp->cmd_dmacount);
		} else {
			pln_printf(pln->pln_dip,
				"cdb=%s\n",
				pln_cdb_str(cdb, sp->cmd_pkt.pkt_cdbp,
				sp->cmd_cdblen));
		}
		pln_printf(pln->pln_dip,
		"pkt 0x%x timeout 0x%x flags 0x%x status len 0x%x\n",
		sp->cmd_pkt,
		sp->cmd_pkt.pkt_time,
		sp->cmd_flags,
		sp->cmd_senselen);
	}
#endif	/* PLNDEBUG */


	/*
	 * Initialize the command
	 */
	sp->cmd_pkt.pkt_reason = CMD_CMPLT;
	sp->cmd_pkt.pkt_state = 0;
	sp->cmd_pkt.pkt_statistics = 0;

	if (sp->cmd_flags & P_CFLAG_DMAVALID) {
		sp->cmd_pkt.pkt_resid = sp->cmd_dmacount;
	} else {
		sp->cmd_pkt.pkt_resid = 0;
	}

	/*
	 * Check for an out-of-limits cdb length
	 */
	if (sp->cmd_cdblen > FCP_CDB_SIZE) {
		P_E_PRINTF((pln->pln_dip,
			"cdb size %d exceeds maximum %d\n",
			sp->cmd_cdblen, FCP_CDB_SIZE));
		return (TRAN_BADPKT);
	}

	/*
	 * the scsa spec states that it is an error to have no
	 * completion function when FLAG_NOINTR is not set
	 */
	if ((sp->cmd_pkt.pkt_comp == NULL) &&
			((sp->cmd_pkt.pkt_flags & FLAG_NOINTR) == 0)) {
		P_E_PRINTF((pln->pln_dip, "intr packet with pkt_comp == 0\n"));
		return (TRAN_BADPKT);
	}

	/*
	 * We don't allow negative command timeouts
	 */
	if (sp->cmd_pkt.pkt_time < (long)NULL) {
		P_E_PRINTF((pln->pln_dip, "Invalid cmd timeout\n"));
		return (TRAN_BADPKT);
	}

	return (TRAN_ACCEPT);
}


/*
 * Prepare an fc_transport pkt for the command
 */
static int
pln_prepare_fc_packet(
	struct pln		*pln,
	struct pln_scsi_cmd	*sp,
	struct pln_fc_pkt	*fp)
{
	fc_packet_t		*fpkt;

	fpkt = fp->fp_pkt;

	/*
	 * Initialize the cmd data segment
	 */
	if (pln_prepare_cmd_dma_seg(pln, fp, sp) == 0) {
		P_E_PRINTF((pln->pln_dip, "cmd alloc failed\n"));
		return (TRAN_BUSY);
	}

	/*
	 * Initialize the response data segment
	 */
	fpkt->fc_pkt_rsp = &fp->fp_rspseg;

	/*
	 * Initialize the data packets segments, if
	 * this command involves data transfer.
	 */
	if (sp->cmd_flags & P_CFLAG_DMAVALID) {
		if (pln_prepare_data_dma_seg(fp, sp) == 0) {
			P_E_PRINTF((pln->pln_dip, "data dma_seg failed\n"));
			return (TRAN_BUSY);
		}
		fpkt->fc_pkt_io_class = (sp->cmd_flags & P_CFLAG_DMAWRITE) ?
			FC_CLASS_IO_WRITE : FC_CLASS_IO_READ;
	} else {
		fpkt->fc_pkt_datap = NULL;
		fpkt->fc_pkt_io_class = FC_CLASS_SIMPLE;
	}

	fpkt->fc_pkt_flags = 0;
	fpkt->fc_pkt_comp = pln_cmd_callback;
	/*
	 * Initialize other fields of the packet
	 */
	fpkt->fc_pkt_cookie = (pln->pln_fc_tran)->fc_cookie;
	fpkt->fc_pkt_timeout = sp->cmd_pkt.pkt_time;

	/* pass flag to transport routine */
	if (sp->cmd_pkt.pkt_flags & FLAG_NOINTR) {
		fpkt->fc_pkt_flags |= FCFLAG_NOINTR;	/* poll for intr */
		fpkt->fc_pkt_comp = NULL;
	}

	return (TRAN_ACCEPT);
}

/*
 * Prepare an fc_transport pkt for a command that involves no SCSI command
 */
static int
pln_prepare_short_pkt(
	struct pln		*pln,
	struct pln_fc_pkt	*fp,
	void			(*callback)(struct fc_packet *),
	int			cmd_timeout)
{
	fc_packet_t		*fpkt;
	fc_frame_header_t	*hp;

	fpkt = fp->fp_pkt;

	/*
	 * Initialize other fields of the packet
	 */
	fpkt->fc_pkt_cookie = (pln->pln_fc_tran)->fc_cookie;
	fpkt->fc_pkt_comp = callback;
	fpkt->fc_pkt_timeout = cmd_timeout;
	fpkt->fc_pkt_io_devdata = TYPE_SCSI_FCP;
	fpkt->fc_pkt_status = 0;
	fpkt->fc_pkt_statistics = 0;
	fpkt->fc_pkt_flags = 0;

	/*
	 * Fill in the fields of the command's FC header
	 */
	hp = fpkt->fc_frame_cmd;
	hp->r_ctl = R_CTL_COMMAND;
	hp->type = TYPE_SCSI_FCP;
	hp->f_ctl = F_CTL_FIRST_SEQ | F_CTL_SEQ_INITIATIVE;
	hp->seq_id = 0;
	hp->df_ctl = 0;
	hp->seq_cnt = 0;
	hp->ox_id = 0xffff;
	hp->rx_id = 0xffff;
	hp->ro = 0;

	return (TRAN_ACCEPT);
}


/*
 *
 * Dispose of all cached pln_fc_pkt resources for a pln_disk
 */
static void
pln_fpacket_dispose_all(
	struct pln		*pln,
	struct pln_disk		*pd)
{
	struct pln_fc_pkt	*fp;
	struct pln_fc_pkt	*fp2;

	P_RD_PRINTF((pln->pln_dip, "pln_fpacket_dispose_all\n"));

	mutex_enter(&pd->pd_pkt_inuse_mutex);

	fp = pd->pd_pkt_pool;
	while (fp != NULL) {
		fp2 = fp->fp_next;
		pln_fpacket_dispose(pln, fp);
		fp = fp2;
	}

	pd->pd_pkt_pool = NULL;

	mutex_exit(&pd->pd_pkt_inuse_mutex);
}


/*
 * Dispose of a pln_fc_pkt and all associated resources.
 */
static void
pln_fpacket_dispose(
	struct pln		*pln,
	struct pln_fc_pkt	*fp)
{
	struct fc_transport	*fc = pln->pln_fc_tran;

	P_RD_PRINTF((pln->pln_dip, "pln_fpacket_dispose\n"));

	/*
	 * Free the lower level's fc_packet
	 */
	if (fp->fp_pkt) {
		fc->fc_pkt_free(fc->fc_cookie, fp->fp_pkt);
		fp->fp_pkt = NULL;
	}

	/*
	 * Free the pln_fc_pkt itself
	 */
	pln_scsi_pktfree((struct scsi_pkt *)&fp->fp_scsi_cmd);
}


/*
 * Fill in various fields in the fcp command packet before sending
 * off a new command
 */
static int
pln_prepare_cmd_dma_seg(
	struct pln		*pln,
	struct pln_fc_pkt	*fp,
	struct pln_scsi_cmd	*sp)
{
	fc_packet_t		*fpkt = fp->fp_pkt;
	struct fcp_cmd		*cmd;
	fcp_ent_addr_t		*f0;

	P_RA_PRINTF((pln->pln_dip, "pln_prepare_cmd_dma_seg\n"));

	ASSERT(fp->fp_cmd != NULL);

	cmd = fp->fp_cmd;

	/*
	 * Zero everything in preparation to build the command
	 */
	bzero((caddr_t)cmd, sizeof (struct fcp_cmd));

	/*
	 * Prepare the entity address
	 */
	f0 =
	(fcp_ent_addr_t *)sp->cmd_pkt.pkt_address.a_hba_tran->tran_tgt_private;
	cmd->fcp_ent_addr.ent_addr_0 = f0->ent_addr_0;
	cmd->fcp_ent_addr.ent_addr_1 = f0->ent_addr_1;
	cmd->fcp_ent_addr.ent_addr_2 = f0->ent_addr_2;
	cmd->fcp_ent_addr.ent_addr_3 = f0->ent_addr_3;

	/*
	 * Prepare the SCSI control options
	 */
	if (sp->cmd_flags & P_CFLAG_DMAVALID) {
		if (sp->cmd_flags & P_CFLAG_DMAWRITE) {
			cmd->fcp_cntl.cntl_read_data = 0;
			cmd->fcp_cntl.cntl_write_data = 1;
		} else {
			cmd->fcp_cntl.cntl_read_data = 1;
			cmd->fcp_cntl.cntl_write_data = 0;
		}
	} else {
		cmd->fcp_cntl.cntl_read_data = 0;
		cmd->fcp_cntl.cntl_write_data = 0;
	}
	cmd->fcp_cntl.cntl_reset = 0;
	/* set up the Tagged Queuing type */
	if (sp->cmd_pkt.pkt_flags & FLAG_TAGMASK) {
		if (sp->cmd_pkt.pkt_flags & FLAG_STAG)
			cmd->fcp_cntl.cntl_qtype = FCP_QTYPE_SIMPLE;
		else if (sp->cmd_pkt.pkt_flags & FLAG_HTAG)
			cmd->fcp_cntl.cntl_qtype = FCP_QTYPE_HEAD_OF_Q;
		else
			cmd->fcp_cntl.cntl_qtype = FCP_QTYPE_ORDERED;
	} else {
		cmd->fcp_cntl.cntl_qtype = FCP_QTYPE_UNTAGGED;
	}

	/*
	 * Total transfer length
	 */
	cmd->fcp_data_len = (sp->cmd_flags & P_CFLAG_DMAVALID) ?
		sp->cmd_dmacount : 0;

	/*
	 * Copy the SCSI command over to fc packet
	 */
	ASSERT(sp->cmd_cdblen <= FCP_CDB_SIZE);
	bcopy((caddr_t)sp->cmd_pkt.pkt_cdbp, (caddr_t)cmd->fcp_cdb,
		sp->cmd_cdblen);

	/*
	 * Set the command dma segment in the fc_transport structure
	 */
	fpkt->fc_pkt_cmd = &fp->fp_cmdseg;

	/*
	 * Sync the cmd segment
	 */
	if (ddi_dma_sync(pln->pln_cmd_pool.cmd_handle,
		(caddr_t)fp->fp_cmd - pln->pln_cmd_pool.cmd_base,
		sizeof (struct fcp_cmd), DDI_DMA_SYNC_FORDEV) ==
		    DDI_FAILURE)
		return (0);

	return (1);
}


/*
 * Do some setup so that our parent can figure out where the
 * data is that we're to operate upon
 */
static int
pln_prepare_data_dma_seg(
	struct pln_fc_pkt	*fp,
	struct pln_scsi_cmd	*sp)
{
	fc_packet_t		*fpkt = fp->fp_pkt;

	ASSERT(sp->cmd_flags & P_CFLAG_DMAVALID);

	/*
	 * Initialize list of data dma_segs: only
	 * one segment, and null-terminate the list.
	 */
	fp->fp_datasegs[0] = &fp->fp_dataseg;
	fp->fp_datasegs[1] = NULL;

	/*
	 * Set up the data dma_seg in the fc_packet to
	 * point to our list of data segments.
	 */
	fpkt->fc_pkt_datap = &fp->fp_datasegs[0];

	/*
	 * Set up the data dma segment
	 */

	fp->fp_dataseg.fc_count = sp->cmd_dmacookie.dmac_size;
	fp->fp_dataseg.fc_base = sp->cmd_dmacookie.dmac_address;

	if (sp->cmd_flags & P_CFLAG_CONSISTENT) {
		if (ddi_dma_sync(sp->cmd_dmahandle, 0, 0, DDI_DMA_SYNC_FORDEV)
				!= DDI_SUCCESS)
			return (0);
	}

	return (1);
}


/*
 * Execute a command on the pluto controller with retries if necessary.
 * return 0 if failure, 1 if successful.
 */
static int
pln_execute_cmd(
	struct pln	*pln,
	int		cmd,
	int		arg1,
	int		arg2,
	caddr_t		datap,
	int		datalen,
	int		sleep_flag)
{
	int			i;
	struct scsi_arq_status	*status;
	struct pln_scsi_cmd	*sp;
	int			rval = 0;
	int			sts;
	int			retry;

	/*
	 * Allocate space for the scsi_cmd.
	 */
	if ((sp = (struct pln_scsi_cmd *)pln_scsi_init_pkt(&pln->pln_scsi_addr,
			(struct scsi_pkt *)NULL,
			(struct	buf *)NULL,
			sizeof (union scsi_cdb),
			sizeof (struct scsi_arq_status), NULL, NULL,
			(sleep_flag == KM_SLEEP) ? SLEEP_FUNC : NULL_FUNC,
			NULL)) == NULL) {
		P_E_PRINTF((pln->pln_dip,
			"pln_private_cmd: cmd alloc failed\n"));
		return (0);
	}

	status = (struct scsi_arq_status *)sp->cmd_pkt.pkt_scbp;

	/*
	 * Execute the command
	 */
	for (retry = 0; retry < PLN_NRETRIES; retry++) {

		i = pln_private_cmd(pln, cmd, sp, arg1, arg2, datap,
			datalen, PLN_INTERNAL_CMD_TIMEOUT, sleep_flag);

		/*
		 * If the command simply failed, we give up
		 */
		if (i != 0) {
			P_E_PRINTF((pln->pln_dip,
				"pln_execute_cmd: failed %d\n", i));
			goto done;
		}

		/*
		 * Check the status, figure out what next
		 */
		sts = *((u_char *)&status->sts_status);
		switch (sts & STATUS_MASK) {
		case STATUS_GOOD:
			rval = 1;
			goto done;
		case STATUS_CHECK:
			P_PC_PRINTF((pln->pln_dip, "status: check\n"));
#ifdef	PLNDEBUG
			if (plnflags & P_PC_FLAG) {
				pln_dump(pln->pln_dip, "sense data: ",
					(u_char *)&status->sts_sensedata,
					sizeof (struct scsi_extended_sense) -
						status->sts_rqpkt_resid);
			}
#endif	PLNDEBUG
			break;
		case STATUS_BUSY:
			P_PC_PRINTF((pln->pln_dip, "status: busy\n"));
			break;
		}
	}

done:
	/*
	 * Free the memory we've allocated.
	 */
	pln_scsi_pktfree((struct scsi_pkt *)sp);

	return (rval);
}



/*
 * Transport a private cmd to the pluto controller.
 * Return 0 for success, or non-zero error indication
 */
static int
pln_private_cmd(
	struct pln			*pln,
	int				cmd,
	struct pln_scsi_cmd		*sp,
	int				arg1,
	int				arg2,
	caddr_t				datap,
	int				datalen,
	long				nticks,
	int				sleep_flag)
{
	int				rval = 0;
	int				bound = 0;
	struct scsi_pkt			*pkt;
	union scsi_cdb			*cdb;
	int				i = 0;
	pln_fc_pkt_t			*fp;
	struct	fc_transport		*fc;

	pkt = &sp->cmd_pkt;

	/* Set the number of retries */
	((struct pln_fc_pkt *)sp->cmd_fc_pkt)->fp_retry_cnt = PLN_NRETRIES;

	/*
	 * Misc
	 */
	pkt->pkt_comp = NULL;
	pkt->pkt_time = nticks;

	/* Run all internal commands in polling mode */
	pkt->pkt_flags = FLAG_NOINTR;

	/*
	 * Build the cdb
	 */
	cdb = (union scsi_cdb *)sp->cmd_pkt.pkt_cdbp;
	switch (cmd) {

	/*
	 * Build a Test Unit Ready cmd
	 */
	case SCMD_TEST_UNIT_READY:
		ASSERT(datalen == 0);
		sp->cmd_cdblen = CDB_GROUP0;
		cdb->scc_cmd = (u_char) cmd;
		break;

	/*
	 * Build an Inquiry cmd
	 */
	case SCMD_INQUIRY:
		ASSERT(datalen > 0);
		sp->cmd_cdblen = CDB_GROUP0;
		cdb->scc_cmd = (u_char) cmd;
		FORMG0COUNT(cdb, (u_char) datalen);
		break;

	/*
	 * Build a Group0 Mode Sense cmd.
	 * arg1 is the mode sense page number, arg2 is the
	 * page control (current, saved, etc.)
	 */
	case SCMD_MODE_SENSE:
		ASSERT(datalen > 0);
		sp->cmd_cdblen = CDB_GROUP0;
		cdb->scc_cmd = (u_char) cmd;
		FORMG0COUNT(cdb, (u_char) datalen);
		cdb->cdb_opaque[2] = arg1 | arg2;
		break;

	/*
	 * Build a Group1 Mode Sense cmd.
	 * arg1 is the mode sense page number, arg2 is the
	 * page control (current, saved, etc.)
	 */
	case SCMD_MODE_SENSE | SCMD_MS_GROUP1:
		ASSERT(datalen > 0);
		sp->cmd_cdblen = CDB_GROUP1;
		cdb->scc_cmd = (u_char) cmd;
		FORMG1COUNT(cdb, datalen);
		cdb->cdb_opaque[2] = arg1 | arg2;
		break;

	default:
		P_E_PRINTF((pln->pln_dip,
			"pln: no such private cmd 0x%x\n", cmd));
		return (0);
	}

	/*
	 * Clear flags in preparation for what we really need
	 */
	sp->cmd_flags &= ~(P_CFLAG_DMAWRITE | P_CFLAG_DMAVALID);
	fp = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

	/*
	 * Allocate dvma resources for the data.
	 * Note we only handle read transfers.
	 */
	if (datalen > 0) {
		uint count;
		if (sp->cmd_dmahandle == (ddi_dma_handle_t)NULL) {
			fc = pln->pln_fc_tran;
			if ((rval = ddi_dma_alloc_handle(pln->pln_dip,
				fc->fc_dma_attrp, DDI_DMA_DONTWAIT, NULL,
				&sp->cmd_dmahandle)) != DDI_SUCCESS) {
				goto failed;
			}
		}

		ASSERT(datap != NULL);
		i = ddi_dma_addr_bind_handle(sp->cmd_dmahandle, NULL,
			datap, datalen, DDI_DMA_READ,
			(sleep_flag == KM_SLEEP) ?
				DDI_DMA_SLEEP : DDI_DMA_DONTWAIT,
			NULL, &sp->cmd_dmacookie, &count);
		switch (i) {
		case DDI_DMA_MAPPED:
			bound = 1;
			break;
		case DDI_DMA_NORESOURCES:
			P_E_PRINTF((pln->pln_dip,
				"pc ddi_dma_setup: no resources\n"));
			rval = ENOMEM;
			goto failed;
		case DDI_DMA_PARTIAL_MAP:
		case DDI_DMA_NOMAPPING:
		case DDI_DMA_TOOBIG:
			P_E_PRINTF((pln->pln_dip,
				"pc ddi_dma_setup: 0x%x\n", i));
			rval = ENOMEM;
			goto failed;
		default:
			P_E_PRINTF((pln->pln_dip,
				"pc ddi_dma_setup: 0x%x\n", i));
			rval = ENOMEM;
			goto failed;
		}
		ASSERT(count == 1);
		sp->cmd_flags |= P_CFLAG_DMAVALID;
		sp->cmd_dmacount = datalen;
	} else {
		sp->cmd_dmacount = 0;
		ASSERT(datap == NULL);
	}

	/*
	 * Allocate the FCP cmd/response pkts (iopbs)
	 */
	fp = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

	/*
	 * set fp_cr_callback to NULL because we are passing a
	 * NOINTR packet.
	 */
	fp->fp_cr_callback = NULL;
	if (fp->fp_cmd == NULL) {
		if (pln_cr_alloc(pln, fp) <= 0) {
			rval = -1;
			goto failed;
		}
	}

	/*
	 * 'Transport' the command...
	 */
	if ((pln_init_scsi_pkt(pln, sp) != TRAN_ACCEPT) ||
			(pln_prepare_fc_packet(pln, sp, fp) !=
				TRAN_ACCEPT)) {
		goto failed;
	}
	if (pln_local_start(pln, sp, fp) != TRAN_ACCEPT) {
		rval = EINVAL;
	}

failed:
	/*
	 * Free the data dma segment, if we allocated one
	 */
	if (datalen > 0 && i == DDI_DMA_MAPPED) {
		ASSERT(datap != NULL);
		P_PC_PRINTF((pln->pln_dip, "pc ddi_dma_free\n"));
		if (bound)
			ddi_dma_unbind_handle(sp->cmd_dmahandle);
	}

	return (rval);
}



/*
 * Called by target driver to abort a command
 */
static int
pln_abort(
	struct scsi_address	*ap,
	struct scsi_pkt		*pkt)
{
	struct pln		*pln = ADDR2PLN(ap);
	int			rval;

	P_A_PRINTF((pln->pln_dip, "pln_abort\n"));

	mutex_enter(&pln->pln_mutex);
	rval =	_pln_abort(pln, ap, pkt);

	mutex_exit(&pln->pln_mutex);

	return (rval);
}

/*
 * Internal abort command handling
 */
/*ARGSUSED*/
static int
_pln_abort(
	struct pln		*pln,
	struct scsi_address	*ap,
	struct scsi_pkt		*pkt)
{
	P_A_PRINTF((pln->pln_dip, "pln_abort\n"));

	ASSERT(MUTEX_HELD(&pln->pln_mutex));
	return (0);
}


/*
 * Called by target driver to reset bus
 */
static int
pln_reset(
	struct scsi_address	*ap,
	int			level)
{
	struct pln		*pln = ADDR2PLN(ap);

	P_R_PRINTF((pln->pln_dip, "pln_reset: %d\n", level));

	switch (level) {
		/* to do */
	}

	return (0);
}


/*
 * Get capability
 */
static int
pln_getcap(
	struct scsi_address	*ap,
	char			*cap,
	int			whom)
{
	return (pln_commoncap(ap, cap, 0, whom, 0));
}


/*
 * Set capability
 */
static int
pln_setcap(
	struct scsi_address	*ap,
	char			*cap,
	int			value,
	int			whom)
{
	return (pln_commoncap(ap, cap, value, whom, 1));
}


/*
 * The core of capability handling.
 *
 * XXX - clean this up!
 */
static int
pln_commoncap(
	struct scsi_address	*ap,
	char			*cap,
	int			val,
	int			tgtonly,
	int			doset)
{
	struct pln		*pln = ADDR2PLN(ap);
	int			cidx;
	int			rval = 0;

	P_C_PRINTF((pln->pln_dip,
	    "%s capability: %s value=%d\n",
	    doset ? "Set" : "Get", cap, val));

	mutex_enter(&pln->pln_mutex);

	if ((tgtonly != 0 && tgtonly != 1) || cap == (char *)0) {
		goto exit;
	}
	cidx = scsi_hba_lookup_capstr(cap);

	if (cidx < 0) {
		P_C_PRINTF((pln->pln_dip,
			"capability not defined: %s\n", cap));
		rval = CAP_UNDEFINED;
	} else if (doset && (val == 0 || val == 1)) {
		/*
		 * At present, we can only set binary (0/1) values
		 */

		P_C_PRINTF((pln->pln_dip,
			"capability %s set to %d\n", cap, val));

		switch (cidx) {
		case SCSI_CAP_DMA_MAX:
		case SCSI_CAP_MSG_OUT:
		case SCSI_CAP_PARITY:
		case SCSI_CAP_INITIATOR_ID:
		case SCSI_CAP_DISCONNECT:
		case SCSI_CAP_SYNCHRONOUS:

			/*
			 * None of these are settable via
			 * the capability interface.
			 */
			break;

		case SCSI_CAP_TAGGED_QING:
			rval = 1;
			break;

		case SCSI_CAP_ARQ:
			/*
			 * We ALWAYS do automatic Request Sense
			 */
			rval = 1;
			break;

		case SCSI_CAP_WIDE_XFER:
		case SCSI_CAP_UNTAGGED_QING:
		default:
			rval = CAP_UNDEFINED;
			break;
		}

	} else if (doset == 0) {
		switch (cidx) {
		case SCSI_CAP_DMA_MAX:
			break;
		case SCSI_CAP_MSG_OUT:
			rval = 1;
			break;
		case SCSI_CAP_DISCONNECT:
			break;
		case SCSI_CAP_SYNCHRONOUS:
			break;
		case SCSI_CAP_PARITY:
			if (scsi_options & SCSI_OPTIONS_PARITY)
				rval = 1;
			break;
		case SCSI_CAP_INITIATOR_ID:
			rval = pln_initiator_id;
			break;
		case SCSI_CAP_TAGGED_QING:
			rval = 1;
			break;
		case SCSI_CAP_UNTAGGED_QING:
			rval = 1;
			break;
		case SCSI_CAP_ARQ:
			rval = 1;
			break;
		case SCSI_CAP_INTERCONNECT_TYPE:
			rval = INTERCONNECT_SSA;
			break;
		default:
			rval = CAP_UNDEFINED;
			break;
		}
		P_C_PRINTF((pln->pln_dip,
			"capability %s is %d\n", cap, rval));
	} else {
		P_C_PRINTF((pln->pln_dip,
			"capability: cannot set %s to %d\n", cap, val));
	}
exit:

	mutex_exit(&pln->pln_mutex);
	return (rval);
}


/*
 * pln_watch() - timer routine used to check for command timeouts, etc.
 */
/*ARGSUSED*/
static void
pln_watch(
	caddr_t		arg)
{
	struct pln		*pln;
	pln_fc_pkt_t		*fp;
	fc_packet_t		*fcpkt;
	struct pln_disk		*pd;
	pln_cr_pool_t		*cp;
	int			throttle_up = 0;

	/* This is our current time... */
	pln_watchdog_time += 3;

	/*
	 * Slowly adjust the throttle positions back up to full throttle.
	 * The reason we don't go up to PLN_MAX_CMDS all at once is we
	 * don't want to issue a bunch of commands that
	 * get rejected as FC_STATUS_P_RJT or FC_STATUS_MAX_XCHG_EXCEEDED.
	 *
	 */
	mutex_enter(&pln_softc_mutex);
	for (pln = pln_softc; pln; pln = pln->pln_next) {

	/*
	 * Don't process this one if attach() isn't complete
	 * Also, opt out of here as soon as possible if we're in the midst of
	 * an attempt to DDI_SUSPEND.
	 */
	    if (!pln->pln_ref_cnt || (pln->pln_state & PLN_STATE_SUSPENDED) ||
		(pln->pln_state & PLN_STATE_SUSPENDING)) {
			continue;
	    }

	    mutex_exit(&pln_softc_mutex);

	    mutex_enter(&pln->pln_throttle_mtx);
	    throttle_up = 0;
	    if ((pln->pln_throttle_cnt == 0) &&
		(pln->pln_maxcmds < PLN_MAX_CMDS)) {
		if ((pln->pln_maxcmds += PLN_THROTTLE_SWING) >
			PLN_MAX_CMDS) {
			pln->pln_maxcmds = PLN_MAX_CMDS;
		}
		throttle_up++;
	    }
	    pln->pln_throttle_cnt = 0;
	    mutex_exit(&pln->pln_throttle_mtx);

	    /* Try to start any "throttled" commands */
	    if (pln->pln_throttle_flag &&
		    (pln->pln_state == PLN_STATE_ONLINE) &&
		    ((pln->pln_maxcmds - pln->pln_ncmd_ref) > 0)) {
		pln_throttle_start(pln, throttle_up);
	    }
	    mutex_enter(&pln_softc_mutex);
	}
	mutex_exit(&pln_softc_mutex);


	/*
	 * Search through the queues of all devices, looking for timeouts...
	 */
	mutex_enter(&pln_softc_mutex);
	for (pln = pln_softc; pln; pln = pln->pln_next) {

	    /* Don't process this one if attach() isn't complete */
	    if (!pln->pln_ref_cnt || (pln->pln_state & PLN_STATE_SUSPENDED))
		continue;
	    mutex_exit(&pln_softc_mutex);

	    if ((pln->pln_state & ~PLN_STATE_OFFLINE_RSP) == PLN_STATE_ONLINE) {

		pd = pln->pln_disk_list;
		do {

		    mutex_enter(&pd->pd_pkt_inuse_mutex);

		    for (fp = pd->pd_inuse_head; fp; fp = fp->fp_next) {
			if ((fp->fp_state == FP_STATE_ISSUED) &&
			    (fp->fp_timeout != 0) &&
			    (fp->fp_timeout < pln_watchdog_time)) {

			/*
			 * Process this command's timeout.
			 * By setting the command's timeout flag
			 * and incrementing the pln_disk's timeout
			 * counter under mutex protection, we can
			 * easily check to see if all timed out commands
			 * have actually completed in pln_cmd_callback.
			 */
			    mutex_enter(&pln->pln_state_mutex);

			    fp->fp_timeout_flag = 1;
			    pln->pln_timeout_count++;

			/*
			 * Cover the race with pln_cmd_callback.
			 * We must do this check *after* setting
			 * the timeout flag.
			 */
			    if (fp->fp_state != FP_STATE_ISSUED) {
				fp->fp_timeout_flag = 0;
				pln->pln_timeout_count--;
			    } else {

				/*
				 * Put ourselves in the first level of timeout
				 * recovery.  In this state, we don't
				 * issue additional commands to the lower
				 * levels, hoping any commands that timed out
				 * will complete before we need to take
				 * more drastic measures.
				 */
				if ((pln->pln_state & ~PLN_STATE_OFFLINE_RSP) ==
					PLN_STATE_ONLINE) {
				    P_W_PRINTF((pln->pln_dip,
					"pln command timeout!\n"));
				    pln->pln_state |= PLN_STATE_TIMEOUT;
				    pln->pln_timer = pln_watchdog_time +
							PLN_TIMEOUT_RECOVERY;
				    P_W_PRINTF((pln->pln_dip,
					"timeout value for cmd = %d\n",
					fp->fp_scsi_cmd.cmd_pkt.pkt_time));
				/*
				 * if this is the only command in
				 * progress arrange that this
				 * command not be retried
				 */
				    if (pln->pln_ncmd_ref == 1)
					fp->fp_state = FP_STATE_TIMED_OUT;
				}
			    }
			    mutex_exit(&pln->pln_state_mutex);
			}
		    }
		    mutex_exit(&pd->pd_pkt_inuse_mutex);
		} while ((pd = pd->pd_next) != pln->pln_disk_list);

	    } else if (pln->pln_state & PLN_STATE_TIMEOUT) {

		if (pln->pln_timer < pln_watchdog_time) {

		/*
		 * Our first level of timeout recovery failed.
		 * Force the link offline to flush all commands
		 * from the hardware, so that we may try
		 * them again.
		 */
		    pln_disp_err(pln->pln_dip, CE_WARN,
			    "Timeout recovery being invoked...\n");

		    if (pln_disable_timeouts) {
			pln_disp_err(pln->pln_dip, CE_WARN,
			    "Timeout recovery disabled!\n");
			mutex_enter(&pln->pln_state_mutex);
			pln->pln_state |= PLN_STATE_DO_RESET;
			pln->pln_state &= ~PLN_STATE_TIMEOUT;
			mutex_exit(&pln->pln_state_mutex);
		    } else
			pln_transport_offline(pln, PLN_STATE_TIMEOUT, 0);
		}

	    } else if (pln->pln_state & PLN_STATE_DO_OFFLINE) {

		/*
		 * Now we're really in trouble.  The offline timeout
		 * recovery didn't work, so let's try to reset the
		 * hardware.
		 */
		if (pln->pln_timer < pln_watchdog_time) {
		    pln_disp_err(pln->pln_dip, CE_WARN,
			"Timeout recovery failed, resetting\n");
		    pln_transport_reset(pln, PLN_STATE_DO_OFFLINE, 0);
		}

	    } else if (pln->pln_state & PLN_STATE_OFFLINE) {

		/*
		 * Online timeouts are enabled only if
		 * pln->pln_en_online_timeout is nonzero
		 */
		if (pln->pln_en_online_timeout &&
			(pln->pln_timer < pln_watchdog_time)) {
		    mutex_enter(&pln->pln_state_mutex);
		    pln->pln_state |= PLN_STATE_LINK_DOWN;
		    mutex_exit(&pln->pln_state_mutex);

		    /* blow away waiters list */
		    mutex_enter(&pln->pln_cr_mutex);
		    cp = &pln->pln_cmd_pool;
		    fp = cp->waiters_head;
		    cp->waiters_head = cp->waiters_tail = NULL;
		    mutex_exit(&pln->pln_cr_mutex);

		    while (fp != NULL) {
			struct pln_scsi_cmd	*sp;

			sp = &fp->fp_scsi_cmd;
			/* Failed to start, fake up some status information */
			sp->cmd_pkt.pkt_state = 0;
			sp->cmd_pkt.pkt_statistics = 0;
			sp->cmd_pkt.pkt_reason = CMD_TRAN_ERR;

			fp = fp->fp_cr_next;
			if (sp->cmd_pkt.pkt_comp) {
				(*sp->cmd_pkt.pkt_comp)((struct scsi_pkt *)sp);
			}
		    }

		    pd = pln->pln_disk_list;
		    do {

			/*
			 * pln_fc_pkt transitions out of the FP_STATE_ONHOLD
			 * state are protected by the pln_throttle_mutex.
			 * We also grab the pd_pkt_inuse_mutex so that
			 * we may safely traverse the list.
			 */
			mutex_enter(&pln->pln_throttle_mtx);
			mutex_enter(&pd->pd_pkt_inuse_mutex);

			/*
			 * Capture all packets in the "on hold" state
			 * so that we may fail them.  We can't just leave
			 * them in the "on hold" state while doing
			 * the command completions, because they may
			 * be returned to the in use list in the "on hold"
			 * list if an upper layer should decide to retry.
			 *
			 * Also blast the "onhold" list anchor for the pd, so
			 * that the throttling routines don't try to start
			 * these commands either.
			 */
			for (fp = pd->pd_inuse_head; fp; fp = fp->fp_next)
			    if (fp->fp_state == FP_STATE_ONHOLD)
				fp->fp_state = FP_STATE_OFFLINE;

			pd->pd_onhold_head = NULL;

			/*
			 * Spin through the list of commands, calling
			 * their completion routines.
			 */
			fp = pd->pd_inuse_head;
			while (fp) {
			    if (fp->fp_state == FP_STATE_OFFLINE) {

				/*
				 * Fake up some fields to make it look like
				 * this command was processed by our parent
				 */
				fp->fp_state = FP_STATE_ISSUED;
				pln->pln_ncmd_ref++;
				fcpkt = fp->fp_pkt;
				fcpkt->fc_pkt_status = FC_STATUS_ERR_OFFLINE;
				fp->fp_retry_cnt = 1;

				/*
				 * Give up the mutexes to avoid a potential
				 * deadlock in the completion routine
				 */
				mutex_exit(&pd->pd_pkt_inuse_mutex);
				mutex_exit(&pln->pln_throttle_mtx);

				if (fcpkt->fc_pkt_comp)
				    (*fcpkt->fc_pkt_comp)(fcpkt);

				mutex_enter(&pln->pln_throttle_mtx);
				mutex_enter(&pd->pd_pkt_inuse_mutex);

				/*
				 * We have to start from the top since
				 * we gave up the mutex.  Good thing
				 * this isn't a performance-sensitive path.
				 */
				fp = pd->pd_inuse_head;
				continue;
			    }
			    fp = fp->fp_next;
			}

			mutex_exit(&pd->pd_pkt_inuse_mutex);
			mutex_exit(&pln->pln_throttle_mtx);
		    } while ((pd = pd->pd_next) != pln->pln_disk_list);
		}
	    }
	    mutex_enter(&pln_softc_mutex);
	}
	mutex_exit(&pln_softc_mutex);

	/* Start the timer again... */
	pln_watchdog_id = timeout(pln_watch, (caddr_t)0, pln_watchdog_tick);
}

/*
 * Force the interface offline
 */
static void
pln_transport_offline(
	struct pln	*pln,
	int		state_flag,
	int		poll)
{
	pln_fc_pkt_t	*fp;


	fp = pln->pkt_offline;

	mutex_enter(&pln->pln_state_mutex);

	if (!(pln->pln_state & state_flag) || (fp->fp_state != FP_STATE_IDLE)) {
	    mutex_exit(&pln->pln_state_mutex);
	    return;
	}

	pln->pln_timer = pln_watchdog_time + PLN_OFFLINE_TIMEOUT;
	pln->pln_state &= ~PLN_STATE_TIMEOUT;
	pln->pln_state |= PLN_STATE_DO_OFFLINE;

	(void) pln_prepare_short_pkt(pln, fp,
		(poll) ? NULL : pln_offline_callback, PLN_OFFLINE_TIMEOUT);
	fp->fp_pkt->fc_pkt_io_class = FC_CLASS_OFFLINE;

	fp->fp_state = FP_STATE_NOTIMEOUT;
	mutex_exit(&pln->pln_state_mutex);

	if (poll)
	    fp->fp_pkt->fc_pkt_flags |= FCFLAG_NOINTR;

	/*
	 * Our parent does the real work...
	 */
	if (pln->pln_fc_tran->fc_transport(fp->fp_pkt,
		FC_NOSLEEP) != FC_TRANSPORT_SUCCESS) {
	    fp->fp_state = FP_STATE_IDLE;
	    pln_transport_reset(pln, PLN_STATE_DO_OFFLINE, poll);
	    return;
	} else if (poll) {
	    fp->fp_state = FP_STATE_IDLE;
	}
}

/*
 * pln_offline_callback() - routine called when a request to send the soc
 * offline has completed
 */
static void
pln_offline_callback(
	struct fc_packet	*fpkt)
{
	pln_fc_pkt_t		*fp = (pln_fc_pkt_t *)fpkt->fc_pkt_private;
	struct pln		*pln = fp->fp_pln;


	mutex_enter(&pln->pln_state_mutex);
	fp->fp_state = FP_STATE_IDLE;
	pln->pln_state &= ~PLN_STATE_DO_OFFLINE;
	mutex_exit(&pln->pln_state_mutex);
}

/*
 * Reset the transport interface
 */
static void
pln_transport_reset(
	struct pln	*pln,
	int		state_flag,
	int		poll)
{
	pln_fc_pkt_t	*fp;

	fp = pln->pkt_reset;


	mutex_enter(&pln->pln_state_mutex);

	if ((fp->fp_state != FP_STATE_IDLE) || !(pln->pln_state & state_flag)) {
	    mutex_exit(&pln->pln_state_mutex);
	    return;
	}

	(void) pln_prepare_short_pkt(pln, fp,
		(poll) ? NULL : pln_reset_callback, 0);

	pln->pln_state |= PLN_STATE_DO_RESET;
	pln->pln_state &= ~PLN_STATE_DO_OFFLINE;
	fp->fp_state = FP_STATE_NOTIMEOUT;

	mutex_exit(&pln->pln_state_mutex);

	if (poll)
	    fp->fp_pkt->fc_pkt_flags |= FCFLAG_NOINTR;

	if ((*pln->pln_fc_tran->fc_reset)(fp->fp_pkt) == 0) {
		pln_disp_err(pln->pln_dip, CE_WARN, "reset recovery failed\n");
		fp->fp_state = FP_STATE_IDLE;
	} else if (poll) {
	    fp->fp_state = FP_STATE_IDLE;
	}
}

/*
 * Callback routine used after resetting the transport interface
 */
static void
pln_reset_callback(
	struct fc_packet	*fpkt)
{
	pln_fc_pkt_t		*fp = (pln_fc_pkt_t *)fpkt->fc_pkt_private;


	fp->fp_state = FP_STATE_IDLE;
}



/*
 * Set up the disk state info according to the configuration
 * of the pluto.
 * The Inquiry command is used to get the configuration in
 * case the pluto is reserved.
 *
 * Also set some properties based on the firmware revision level.
 *
 * Return 0 if ok, 1 if failure.
 */
static int
pln_build_disk_state(
	struct pln		*pln,
	int			sleep_flag)
{
	struct p_inquiry	*inquiry = NULL;
	u_char		n_ports, n_tgts;
	int		i;
	int		rval = 1;
	int		priority_res = 1;
	int		fast_wrt = 1;
	int		rev_num = 0;
	int		sub_num = 0;
	int		pln_rev = 0;
	int		pln_srev = 0;

	P_C_PRINTF((pln->pln_dip, "pln_build_disk_state:\n"));

	if ((inquiry = (struct p_inquiry *)
		    kmem_zalloc(sizeof (struct p_inquiry),
		    sleep_flag)) == NULL) {
		return (rval);
	}
	i = pln_execute_cmd(pln, SCMD_INQUIRY, 0, 0,
		(caddr_t)inquiry, sizeof (struct p_inquiry),
		sleep_flag);
	if (i == 0) {
		P_E_PRINTF((pln->pln_dip,
			"pln_build_disk_state:Inquiry failed\n"));
		goto done;
	}

	/*
	 * get Controller firmware revision and sub_revision
	 */

	pln_get_fw_rev(inquiry->inq_firmware_rev,
		sizeof (inquiry->inq_firmware_rev), &rev_num, &sub_num);

	/*
	 * Now get the current firmware revision for comaparision. The
	 * current firmware revusion is the level of ssafirmware when
	 * the driver was built. We print a message asking the user to
	 * upgrade if the firmware revision on the controller is less
	 * than that required by the driver.
	 */

	pln_get_fw_rev(pln_firmware_vers,
		strlen(pln_firmware_vers), &pln_rev, &pln_srev);

	/*
	 * First check to see if # ports and Targets are 0
	 */
	if ((inquiry->inq_ports == 0) || (inquiry->inq_tgts == 0)) {

		cmn_err(CE_NOTE,
		"pln%d: Old SSA firmware has been detected"
		" (Ver:%d.%d) : Expected (Ver:%d.%d) - Please upgrade\n",
		ddi_get_instance(pln->pln_dip), rev_num, sub_num,
			pln_rev, pln_srev);

		/*
		 * Make sure we really are talking to an SSA
		 *
		 * If we are then use default number of ports
		 * and targets.
		 * If not then fail.
		 */
		if (strncmp(inquiry->inq_pid, "SSA", 3) != 0) {
			P_E_PRINTF((pln->pln_dip, "Device not SSA\n"));
		    goto done;
		}
		/*
		 * Default number of ports and targets
		 * to match SSA100.
		 */
		n_ports = 6;
		n_tgts = 5;
	} else {
		n_ports = inquiry->inq_ports;
		n_tgts = inquiry->inq_tgts;
	}

	/*
	 * This is where we look at the firmware revision and
	 * set the appropriate properties that our child (ssd)
	 * uses.
	 *
	 * This requires the inq_firmware_rev field in the Inquiry
	 * to be in the format x.xs or x.xx in ascii, which I have been
	 * assured it will be. (x = ascii 0-9 and s = space).
	 *
	 * Set priority-reserve and fast-writes properties if
	 * firmware rev >= 1.9.
	 * Bug 1214756: Check the firmware version with what we think
	 * it should be. If not, print a warning.
	 */

	if ((rev_num > 1) || ((rev_num == 1) && (sub_num >= 9))) {
		(void) ddi_prop_create(DDI_DEV_T_NONE,
			pln->pln_dip,
			DDI_PROP_CANSLEEP,
			"priority-reserve",
			(caddr_t)&priority_res,
			sizeof (priority_res));
		(void) ddi_prop_create(DDI_DEV_T_NONE,
			pln->pln_dip,
			DDI_PROP_CANSLEEP,
			"fast-writes",
			(caddr_t)&fast_wrt,
			sizeof (fast_wrt));
	}

	if ((rev_num < pln_rev) ||
	    ((rev_num == pln_rev) && (sub_num < pln_srev))) {
		cmn_err(CE_NOTE,
		"pln%d: Old SSA firmware has been detected"
		" (Ver:%d.%d) : Expected (Ver:%d.%d) - Please upgrade\n",
		ddi_get_instance(pln->pln_dip), rev_num, sub_num,
			pln_rev, pln_srev);
	}

	pln->pln_nports = n_ports;
	pln->pln_ntargets = n_tgts;

	if (pln_alloc_disk_state(pln, sleep_flag) == 0) {
		pln_free_disk_state(pln);
		goto done;
	}
	pln_init_disk_state_mutexes(pln);
	rval = 0;
done:
	if (inquiry) {
		kmem_free((char *)inquiry, sizeof (struct p_inquiry));
	}

	return (rval);
}

static void
pln_get_fw_rev(char *fw_str, int str_len, int *rev_num, int *sub_num)
{
	int i = 0;
	char c;
	int	rev = 0;
	int	subrev = 0;

	while ((c = *(fw_str + i)) != '.' && i < str_len) {
		if (c >= '0' && c <= '9') {
			rev = (rev * 10) + (c - '0');
		}
		i++;
	}

	while ((c = *(fw_str + i)) != '\0' && i < str_len) {
		if (c >= '0' && c <= '9') {
			subrev = (subrev * 10) + (c - '0');
		}
		i++;
	}
	*rev_num = rev;
	*sub_num = subrev;
}

/*
 * Allocate state and initialize mutexes for the full set
 * of disks reported by the pluto.
 */
static int
pln_alloc_disk_state(
	struct pln		*pln,
	int			sleep_flag)
{
	u_short			port;
	u_short			target;
	struct pln_disk		*pd,
				*pd_prev,
				*pd_first = NULL;

	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(pln->cur_throttle))
	ASSERT(pln->pln_nports != 0);
	ASSERT(pln->pln_ntargets != 0);
	ASSERT(pln->pln_ids == NULL);
	ASSERT(pln->pln_ctlr != NULL);


	/*
	 * Allocate pln_disk structures for individual disks
	 */
	pln->pln_ids = (struct pln_disk **)
		kmem_zalloc(sizeof (struct pln_disk *) * pln->pln_nports,
			sleep_flag);
	if (pln->pln_ids == NULL) {
		return (0);
	}

	for (port = 0; port < pln->pln_nports; port++) {
		pln->pln_ids[port] = pd = (struct pln_disk *)kmem_zalloc(
			sizeof (struct pln_disk) * pln->pln_ntargets,
				sleep_flag);
		if (pd == NULL) {
			return (0);
		}
		if (!pd_first) {
			pd_first = pd;
			pd_prev = pd;
		} else {
			pd_prev->pd_next = pd;
			pd_prev = pd;
		}
		for (target = 1; target < pln->pln_ntargets; target++) {
			pd = pd_prev + 1;
			pd_prev->pd_next = pd;
			pd_prev = pd;
		}
	}

	pd->pd_next = pln->pln_ctlr;
	pln->pln_ctlr->pd_next = pd_first;
	pln->pln_disk_list = pd_first;
	pln->cur_throttle = pd_first;

	_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(pln->cur_throttle))

	return (1);
}



/*
 * Initialize mutexes for disk state
 */
static void
pln_init_disk_state_mutexes(
	struct pln		*pln)
{
	u_short			port;
	u_short			tgt;
	struct pln_disk		*pd;
	char			name[256];
	int			instance;
	instance = ddi_get_instance(pln->pln_dip);

	/*
	 * Initialize mutexes for individual disks
	 */
	for (port = 0; port < pln->pln_nports; port++) {
		pd = pln->pln_ids[port];
		for (tgt = 0; tgt < pln->pln_ntargets; tgt++, pd++) {
			(void) sprintf(name, "pln%d port%d target%d",
				instance, port, tgt);
			mutex_init(&pd->pd_pkt_inuse_mutex, name, MUTEX_DRIVER,
				pln->pln_iblock);
		}
	}

	pln->pln_disk_mtx_init = 1;
}


/*
 * Destroy all disk state mutexes.  This assumes that
 * a full allocation of the disk state was successful.
 */
static void
pln_destroy_disk_state_mutexes(
	struct pln		*pln)
{
	u_short			port;
	u_short			tgt;
	struct pln_disk		*pd;

	ASSERT(pln->pln_nports != 0);
	ASSERT(pln->pln_ntargets != 0);

	/*
	 * Free individual disk mutexes
	 */
	for (port = 0; port < pln->pln_nports; port++) {
		pd = pln->pln_ids[port];
		for (tgt = 0; tgt < pln->pln_ntargets; tgt++, pd++) {
			mutex_destroy(&pd->pd_pkt_inuse_mutex);
		}
	}

	/*
	 * Leave "pln->nports" & "pln->ntargets" alone,
	 * we need them to free up disk states that were allocated
	 * by "pln_alloc_disk_state()" later, in "pln_free_disk_state()".
	*/
	pln->pln_disk_mtx_init = 0;
}


/*
 * Free up all individual disk state allocated.
 * Note that this is structured so as to be able to free
 * up a partially completed allocation.
 */
static void
pln_free_disk_state(
	struct pln		*pln)
{
	u_short			port;
	struct pln_disk		*pd;

	/*
	 * Free the lists of command packets
	 */
	pd = pln->pln_disk_list;
	if (pd)
		do {
			pln_fpacket_dispose_all(pln, pd);
		} while ((pd = pd->pd_next) != pln->pln_disk_list);

	/*
	 * Free individual disk state
	 */
	if (pln->pln_ids != NULL) {
		for (port = 0; port < pln->pln_nports; port++) {
			if ((pd = pln->pln_ids[port]) != NULL) {
				kmem_free((void *) pd,
					sizeof (struct pln_disk) *
						pln->pln_ntargets);
			}
		}
		kmem_free((void *) pln->pln_ids,
			sizeof (struct pln_disk *) * pln->pln_nports);
		pln->pln_ids = NULL;
	}
	/*
	 * Now we can "clean up global info".
	 */
	pln->pln_nports = 0;
	pln->pln_ntargets = 0;
}

/*ARGSUSED*/
static void
pln_scsi_destroy_pkt(
	struct scsi_address	*ap,
	struct scsi_pkt		*pkt)
{
	pln_scsi_dmafree(ap, pkt);
	pln_scsi_pktfree(pkt);
}




/*ARGSUSED*/
void
pln_scsi_dmafree(
	struct scsi_address	*ap,
	struct scsi_pkt		*pkt)
{
	register struct	pln_scsi_cmd *cmd = (struct pln_scsi_cmd *)pkt;

	if (cmd->cmd_flags & P_CFLAG_DMAVALID) {
		/*
		 * Free the mapping.
		 */
		ddi_dma_unbind_handle(cmd->cmd_dmahandle);
		cmd->cmd_flags ^= P_CFLAG_DMAVALID;
	}
}

/*ARGSUSED*/
static struct scsi_pkt *
pln_scsi_init_pkt(
	struct scsi_address	*ap,
	struct scsi_pkt		*pkt,
	struct buf		*bp,
	int			cmdlen,
	int			statuslen,
	int			tgtlen,
	int			flags,
	int			(*callback)(),
	caddr_t			arg)
{
	int kf;
	register struct pln_scsi_cmd 	*sp;
	register struct pln		*pln = ADDR2PLN(ap);
	pln_address_t	*pln_ap =
			(pln_address_t *)ap->a_hba_tran->tran_tgt_private;
	register struct pln_disk 	*pd;
	struct scsi_pkt			*new_pkt = NULL;
	struct	pln_scsi_cmd 		*cmd;
	int				rval;
	u_int				dma_flags;
	struct pln_fc_pkt		*fp;

	/*
	 * Check if the requested values are within limits
	 */

	if ((cmdlen > sizeof (union scsi_cdb)) ||
		(statuslen > sizeof (struct scsi_arq_status)) ||
		(tgtlen > PLN_TGT_PRIV_LEN)) {
			return (NULL);
	}

	/*
	 * Allocate a pkt
	 */
	if (!pkt) {

		/*
		 * Get a ptr to the disk-specific structure
		 */
		switch (pln_ap->pln_entity) {
		    case PLN_ENTITY_CONTROLLER:
			if (pln_ap->pln_port != 0 || pln_ap->pln_target != 0 ||
					pln_ap->pln_reserved != 0) {
				return (NULL);
			}
			pd = pln->pln_ctlr;
			break;
		    case PLN_ENTITY_DISK_SINGLE:
			if (pln_ap->pln_port >= pln->pln_nports ||
				pln_ap->pln_target >= pln->pln_ntargets ||
						pln_ap->pln_reserved != 0) {
				return (NULL);
			}
			pd = pln->pln_ids[pln_ap->pln_port] +
				pln_ap->pln_target;
			break;
		    default:
			return (NULL);
		}

		kf = (callback == SLEEP_FUNC) ? KM_SLEEP: KM_NOSLEEP;

		/*
		 * Get a pln_fc_pkt, all threaded together with the
		 * necessary stuff
		 */
		fp = kmem_cache_alloc(pln->pln_kmem_cache, kf);
		if (fp == (struct pln_fc_pkt *)NULL) {
			return (NULL);
		}
		fp->fp_pd = pd;
		fp->fp_state = FP_STATE_IDLE;
		fp->fp_timeout = 0;
		fp->fp_timeout_flag = 0;
		fp->fp_next = NULL;

		sp = &fp->fp_scsi_cmd;

		/*
		 * Set up various fields in the structures
		 * for the next I/O operation.  The pln_fc_pkt, scsi_pkt,
		 * fc_packet, etc. are all already threaded together.
		 *
		 */
		sp->cmd_flags = 0;
		sp->cmd_next = NULL;
		sp->cmd_cdblen = (u_char) cmdlen;
		sp->cmd_senselen = (u_char)statuslen;
		sp->cmd_tgtlen = (u_char) tgtlen;
		sp->cmd_pkt.pkt_address.a_hba_tran = pln->pln_tran;
		sp->cmd_pkt.pkt_ha_private = sp;
		sp->cmd_fc_pkt = (caddr_t)fp;
		sp->cmd_pkt.pkt_cdbp = (opaque_t)&sp->cmd_cdb_un;
		bzero(sp->cmd_pkt.pkt_cdbp, sizeof (union scsi_cdb));
		sp->cmd_pkt.pkt_scbp = (opaque_t)&sp->cmd_scsi_scb;
		sp->cmd_pkt.pkt_private = (opaque_t)&sp->cmd_tgtprivate[0];
		bzero(sp->cmd_pkt.pkt_private, PLN_TGT_PRIV_LEN);
		pkt = new_pkt = (struct scsi_pkt *)sp;
		mutex_enter(&pd->pd_pkt_inuse_mutex);
		if ((fp->fp_prev = pd->pd_inuse_tail) != NULL)
			pd->pd_inuse_tail->fp_next = fp;
		else
			pd->pd_inuse_head = fp;
		pd->pd_inuse_tail = fp;
		mutex_exit(&pd->pd_pkt_inuse_mutex);
	}

	/*
	 * Set up dma info
	 */
	if (bp && (bp->b_bcount != 0)) {
		u_int	dmacookie_count;

		cmd = (struct pln_scsi_cmd *)pkt;
		fp = (struct pln_fc_pkt *)cmd->cmd_fc_pkt;

		/*
		 * clear any stale flags
		 */
		cmd->cmd_flags &= ~(P_CFLAG_DMAWRITE | P_CFLAG_DMAVALID);

		/*
		 * Get the host adapter's dev_info pointer
		 */
		if (bp->b_flags & B_READ) {
			dma_flags = DDI_DMA_READ;
			cmd->cmd_flags |= P_CFLAG_DMAVALID;
		} else {
			cmd->cmd_flags |= P_CFLAG_DMAWRITE | P_CFLAG_DMAVALID;
			dma_flags = DDI_DMA_WRITE;
		}
		if (flags & PKT_CONSISTENT) {
			dma_flags |= DDI_DMA_CONSISTENT;
			cmd->cmd_flags |= P_CFLAG_CONSISTENT;
		}
		ASSERT(cmd->cmd_dmahandle != NULL);
		rval = ddi_dma_buf_bind_handle(cmd->cmd_dmahandle,
			bp, dma_flags, callback,
			arg, &cmd->cmd_dmacookie, &dmacookie_count);

dma_failure:
		if (rval && rval != DDI_DMA_MAPPED) {
			switch (rval) {
			case DDI_DMA_NORESOURCES:
				ASSERT(bp->b_error == 0);
				break;
			case DDI_DMA_PARTIAL_MAP:
				cmn_err(CE_PANIC, "ddi_dma_buf_setup "
				"returned DDI_DMA_PARTIAL_MAP\n");
				break;
			case DDI_DMA_NOMAPPING:
				bp->b_error = EFAULT;
				bp->b_flags |= B_ERROR;
				break;
			case DDI_DMA_TOOBIG:
				bp->b_error = EFBIG;	/* ??? */
				bp->b_flags |= B_ERROR;
				break;
			default:
				bioerror(bp, EINVAL);
				break;
			}
			cmd->cmd_flags &= ~P_CFLAG_DMAVALID;
			if (new_pkt) {
				pln_scsi_pktfree(new_pkt);
			}
			return ((struct scsi_pkt *)NULL);
		}
		ASSERT(dmacookie_count == 1);
		cmd->cmd_dmacount = bp->b_bcount;
	}

	P_T_PRINTF(((dev_info_t *)(((struct pln *)
		ap->a_hba_tran->tran_hba_private)->pln_dip),
		"pln_scsi_init_pkt: pkt 0x%x\n", pkt));
	return (pkt);
}

/*
 * Function name : pln_scsi_pktfree()
 *
 * Return Values : none
 * Description	 : return pkt to the free pool
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static void
pln_scsi_pktfree(struct scsi_pkt *pkt)
{
	struct pln_scsi_cmd *sp = (struct pln_scsi_cmd *)pkt;
	register pln_cr_pool_t *cp;
	register struct pln_fc_pkt *fp;
	register struct pln_disk *pd;
	register struct pln *pln = (struct pln *)PKT2PLN(pkt);

	fp = (struct pln_fc_pkt *)sp->cmd_fc_pkt;

	ASSERT(fp->fp_state == FP_STATE_IDLE);
	ASSERT(!fp->fp_timeout_flag);

	/*
	 * If the packet is already free, we have a problem
	 */
	if (sp->cmd_flags & P_CFLAG_FREE) {
			pln_disp_err(pln->pln_dip, CE_PANIC,
			    "pln_scsi_pktfree: freeing free packet");
			_NOTE(NOT_REACHED);
			/* NOTREACHED */
	}
	sp->cmd_flags = P_CFLAG_FREE;


	/* Stick it on the "free" list */
	fp->fp_state = FP_STATE_FREE;
	pd = fp->fp_pd;

	mutex_enter(&pd->pd_pkt_inuse_mutex);
	if (fp->fp_next)
		fp->fp_next->fp_prev = fp->fp_prev;
	else
		pd->pd_inuse_tail = fp->fp_prev;
	if (fp->fp_prev)
		fp->fp_prev->fp_next = fp->fp_next;
	else
		pd->pd_inuse_head = fp->fp_next;
	mutex_exit(&pd->pd_pkt_inuse_mutex);

	cp = &pln->pln_cmd_pool;
	if (fp->fp_cmd) {
		mutex_enter(&pln->pln_cr_mutex);
		if (cp->free == NULL) {
			pln_cr_free(pln, fp, cp,
					(pln_cr_free_t *)fp->fp_cmd);
		} else {
			mutex_exit(&pln->pln_cr_mutex);
		}
	}
	kmem_cache_free(pln->pln_kmem_cache, (void *)fp);

}
static int
pln_kmem_cache_constructor(void *buf, void *arg, int kmflags)
/*ARGSUSED*/
{
	struct fc_transport		*fc;
	struct pln			*pln = (struct pln *)arg;
	struct pln_fc_pkt		*fp = buf;
	struct pln_scsi_cmd		*sp = &fp->fp_scsi_cmd;
	fc_packet_t			*fpkt;
	int  (*callback)(caddr_t) = (kmflags & KM_SLEEP) ? DDI_DMA_SLEEP:
					DDI_DMA_DONTWAIT;

	bzero((caddr_t)fp, sizeof (struct pln_fc_pkt));

	fc = pln->pln_fc_tran;

	fpkt = fp->fp_pkt = fc->fc_pkt_alloc(fc->fc_cookie,
		(kmflags == KM_SLEEP) ? FC_SLEEP : FC_NOSLEEP);
	if (!fpkt) {
		return (-1);
	}

	pln_fill_fc_pkt(fpkt);
	fpkt->fc_pkt_private = (void *)fp;
	fp->fp_fc = fc;
	fp->fp_pln = pln;

	if (ddi_dma_alloc_handle(pln->pln_dip,
	    fc->fc_dma_attrp, callback, NULL,
	    &sp->cmd_dmahandle) != DDI_SUCCESS)  {
		fc->fc_pkt_free(fc->fc_cookie, fp->fp_pkt);
		return (-1);
	}

	return (0);
}

static void
pln_kmem_cache_destructor(void *buf, void *arg)
/*ARGSUSED*/
{
	struct pln_scsi_cmd	*sp;
	struct pln_fc_pkt	*fp = buf;
	struct pln		*pln = NULL;
	struct fc_transport	*fc;

	sp = &fp->fp_scsi_cmd;
	if (fp->fp_pkt) {
		fc = fp->fp_fc;
		fc->fc_pkt_free(fc->fc_cookie, fp->fp_pkt);
	}
	if (fp->fp_cmd) {
		pln = fp->fp_pln;
		mutex_enter(&pln->pln_cr_mutex);
		pln_cr_free(pln, fp, &pln->pln_cmd_pool,
				(pln_cr_free_t *)fp->fp_cmd);
	}
	if (sp->cmd_dmahandle) {
		ddi_dma_free_handle(&sp->cmd_dmahandle);
	}
}

/*
 * Fill the fc_pkt with data that is used all the time. The command specific
 * fields will be filled in pln_prepare_fc_packet()
 */
void
pln_fill_fc_pkt(fc_packet_t *fpkt)
{
	fc_frame_header_t	*hp;

	fpkt->fc_pkt_io_devdata = TYPE_SCSI_FCP;
	fpkt->fc_pkt_status = 0;
	fpkt->fc_pkt_statistics = 0;

	/*
	 * Fill in the fields of the command's FC header
	 */
	hp = fpkt->fc_frame_cmd;
	hp->r_ctl = R_CTL_COMMAND;
	hp->type = TYPE_SCSI_FCP;
	hp->f_ctl = F_CTL_FIRST_SEQ | F_CTL_SEQ_INITIATIVE;
	hp->seq_id = 0;
	hp->df_ctl = 0;
	hp->seq_cnt = 0;
	hp->ox_id = 0xffff;
	hp->rx_id = 0xffff;
	hp->ro = 0;
}

/*
 * A routine to allocate an fcp command or response structure and
 * the associated dvma mapping
 *
 * Return Values : -1   Could not queue cmd or allocate structure
 *		    1   fcp structure allocated
 *		    0   request queued
 *
 */
static int
pln_cr_alloc(
	struct pln	*pln,
	pln_fc_pkt_t	*fp)
{
	pln_cr_pool_t			*cp = &pln->pln_cmd_pool;
	pln_cr_free_t			*pp;
	pln_fc_pkt_t			*fp_free;
	int				rval;

	mutex_enter(&pln->pln_cr_mutex);

	/*
	 * If the free list is empty, queue up the request
	 */
	if ((pp = cp->free) == NULL) {
		if (fp->fp_cr_callback) {
			if ((fp_free = cp->waiters_tail) != NULL) {
				fp_free->fp_cr_next = fp;
			} else {
				cp->waiters_head = fp;
			}
			cp->waiters_tail = fp;
			fp->fp_cr_next = NULL;
			rval = 0;
		} else {
			rval = -1;
		}
		mutex_exit(&pln->pln_cr_mutex);

		return (rval);
	}

	/*
	 * Grab the first thing from the free list
	 */
	cp->free = pp->next;
	mutex_exit(&pln->pln_cr_mutex);

	/*
	 * Fill in the fields in the pln_fc_pkt for the fcp cmd/rsp
	 */
	fp->fp_cmd = (struct fcp_cmd *)pp;
	fp->fp_rsp = (struct pln_rsp *)pp->rsp;
	fp->fp_cmdseg.fc_count = sizeof (struct fcp_cmd);
	fp->fp_cmdseg.fc_base = pp->cmd_dmac;
	fp->fp_rspseg.fc_count = sizeof (struct pln_rsp);
	fp->fp_rspseg.fc_base = pp->rsp_dmac;

	return (1);
}

/*
 * Free an fcp command or response buffer
 */
static void
pln_cr_free(
	struct pln	*pln,
	pln_fc_pkt_t	*fp,
	pln_cr_pool_t	*cp,
	pln_cr_free_t	*pp)
{
	pln_fc_pkt_t			*fp_wait;

	_NOTE(LOCK_RELEASED_AS_SIDE_EFFECT(&pln->pln_cr_mutex));

	/* Fill in the free element's fields */
	pp->rsp = (caddr_t)fp->fp_rsp;
	pp->cmd_dmac = fp->fp_cmdseg.fc_base;
	pp->rsp_dmac = fp->fp_rspseg.fc_base;

	/* For freeing-free-iopb detection... */
	fp->fp_cmd = NULL;
	fp->fp_rsp = NULL;

	/*
	 * If someone is waiting for a cmd/rsp, then give this one to them.
	 * Otherwise, add it to the free list.
	 */
	if ((fp_wait = cp->waiters_head) != NULL) {

		if ((cp->waiters_head = fp_wait->fp_cr_next) == NULL)
			cp->waiters_tail = NULL;

		mutex_exit(&pln->pln_cr_mutex);

		fp_wait->fp_cmd = (struct fcp_cmd *)pp;
		fp_wait->fp_rsp = (struct pln_rsp *)pp->rsp;
		fp_wait->fp_cmdseg.fc_count = sizeof (struct fcp_cmd);
		fp_wait->fp_cmdseg.fc_base = pp->cmd_dmac;
		fp_wait->fp_rspseg.fc_count = sizeof (struct pln_rsp);
		fp_wait->fp_rspseg.fc_base = pp->rsp_dmac;

		ASSERT(fp_wait->fp_cr_callback);

		(*fp_wait->fp_cr_callback)(fp_wait);

		return;

	} else {
		pp->next = cp->free;
		cp->free = pp;
	}
	mutex_exit(&pln->pln_cr_mutex);
}

/*
 * Allocate IOPB and DVMA space for FCP command/responses
 * Should only be called from user context.
 *
 * We allocate a static amount of this stuff at attach() time, since
 * it's too expensive to allocate/free space from the system-wide pool
 * for each command, and, as of this writing, ddi_iopb_alloc() will
 * always cv_wait() when it runs out, so we can't even get an
 * affirmative indication back when the system is out of iopb space.
 */
static int
pln_cr_pool_init(
	struct pln	*pln,
	pln_cr_pool_t	*cp)
{
	int			cmd_buf_size,
				rsp_buf_size;
	pln_cr_free_t		*cptr;
	caddr_t			dptr,
				eptr;
	struct fc_transport	*fc = pln->pln_fc_tran;
	ddi_dma_cookie_t		cookie;

	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(cptr->next))
	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(cp->free))

	cmd_buf_size = sizeof (struct fcp_cmd) * pln_fcp_elements;
	rsp_buf_size = sizeof (struct pln_rsp) * pln_fcp_elements;

	/*
	 * Get a piece of memory in which to put commands
	 */
	if (ddi_iopb_alloc(pln->pln_dip, fc->fc_dmalimp, cmd_buf_size,
		(caddr_t *)&cp->cmd_base) != DDI_SUCCESS) {
	    cp->cmd_base = NULL;
	    goto fail;
	}

	/*
	 * Allocate dma resources for the payload
	 */
	if (ddi_dma_addr_setup(pln->pln_dip, (struct as *)NULL,
		cp->cmd_base, cmd_buf_size,
		DDI_DMA_WRITE | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT, NULL,
		fc->fc_dmalimp, &cp->cmd_handle) !=
			DDI_DMA_MAPPED) {
	    cp->cmd_handle = NULL;
	    goto fail;
	}

	/*
	 * Get a piece of memory in which to put responses
	 */
	if (ddi_iopb_alloc(pln->pln_dip, fc->fc_dmalimp, rsp_buf_size,
		(caddr_t *)&cp->rsp_base) != DDI_SUCCESS) {
	    cp->rsp_base = NULL;
	    goto fail;
	}

	/*
	 * Allocate dma resources for the payload
	 */
	if (ddi_dma_addr_setup(pln->pln_dip, (struct as *)NULL,
		cp->rsp_base, rsp_buf_size,
		DDI_DMA_READ | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT, NULL,
		fc->fc_dmalimp, &cp->rsp_handle) !=
			DDI_DMA_MAPPED) {
	    cp->rsp_handle = NULL;
	    goto fail;
	}

	/*
	 * Generate a (cmd/rsp structure) free list
	 */
	dptr = cp->cmd_base;
	eptr = cp->rsp_base;

	cp->free = (pln_cr_free_t *)cp->cmd_base;

	while (dptr <=
		(cp->cmd_base + cmd_buf_size - sizeof (struct fcp_cmd))) {
	    cptr = (pln_cr_free_t *)dptr;
	    dptr += sizeof (struct fcp_cmd);

	    cptr->next = (pln_cr_free_t *)dptr;
	    cptr->rsp = eptr;

	    /* Get the dvma cookies for this piece */
	    if (ddi_dma_htoc(cp->cmd_handle,
		    (off_t)((caddr_t)cptr - cp->cmd_base), &cookie) !=
		    DDI_SUCCESS) {
		goto fail;
	    }
	    cptr->cmd_dmac = cookie.dmac_address;

	    if (ddi_dma_htoc(cp->rsp_handle,
		    (off_t)(eptr - cp->rsp_base), &cookie) !=
		    DDI_SUCCESS) {
		goto fail;
	    }
	    cptr->rsp_dmac = cookie.dmac_address;

	    eptr += sizeof (struct pln_rsp);
	}

	/* terminate the list */
	cptr->next = NULL;
	cp->waiters_head = NULL;
	cp->waiters_tail = NULL;


	_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(cptr->next))
	_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(cp->free))

	return (1);

fail:
	if (cp->cmd_handle)
	    ddi_dma_free(cp->cmd_handle);

	if (cp->cmd_base)
	    ddi_iopb_free(cp->cmd_base);

	if (cp->rsp_handle)
	    ddi_dma_free(cp->rsp_handle);

	if (cp->rsp_base)
	    ddi_iopb_free(cp->rsp_base);

	cp->free = NULL;
	cp->cmd_base = NULL;
	cp->rsp_base = NULL;
	cp->cmd_handle = NULL;
	cp->rsp_handle = NULL;

	return (0);
}


/*
 * A common routine used to display error messages
 *
 * NOTE:  this routine should be called only when processing commands,
 *	  and not for unsolicited messages if no commands are queued
 *	  (to avoid a dev_ops.devo_revcnt usage count assertion inside
 *	  of scsi_log())
 */
static void
pln_disp_err(
	dev_info_t	*dip,
	u_int		level,
	char		*msg)
{
	scsi_log(dip, ddi_get_name(dip), level, msg);
}

#ifdef	PLNDEBUG
static void
pln_printf(
	dev_info_t	*dip,
	const char	*format,
			...)
{
	va_list		ap;
	char		buf[256];

	sprintf(buf, "%s%d: ", ddi_get_name(dip), ddi_get_instance(dip));

	va_start(ap, format);
	vsprintf(&buf[strlen(buf)], format, ap);
	va_end(ap);

	if (plndebug) {
		scsi_log(NULL, "", CE_CONT, buf);
	}
}
#endif	/* PLNDEBUG */


#ifdef	PLNDEBUG
static char *
pln_cdb_str(
	char		*s,
	u_char		*cdb,
	int		cdblen)
{
	static char	hex[] = "0123456789abcdef";
	char		*p;
	int		i;

	p = s;
	*p++ = '[';
	for (i = 0; i < cdblen; i++, cdb++) {
		*p++ = hex[(*cdb >> 4) & 0x0f];
		*p++ = hex[*cdb & 0x0f];
		*p++ = (i == cdblen-1) ? ']' : ' ';
	}
	*p = 0;

	return (s);
}


/*
 * Dump data in hex for debugging
 */
static void
pln_dump(dev_info_t	*dip,
	char	*msg,
	u_char	*addr,
	int	len)
{
	static char	hex[] = "0123456789abcdef";
	char		buf[256];
	char		*p;
	int		i;
	int		n;
	int		msglen = -1;

	while (len > 0) {
		p = buf;
		if (msglen == -1) {
			msglen = strlen(msg);
			while (*msg) {
				*p++ = *msg++;
			}
		} else {
			for (i = 0; i < msglen; i++) {
				*p++ = ' ';
			}
		}
		n = min(len, 16);
		for (i = 0; i < n; i++, addr++) {
			*p++ = hex[(*addr >> 4) & 0x0f];
			*p++ = hex[*addr & 0x0f];
			*p++ = ' ';
		}
		*p++ = '\n';
		*p = 0;

		scsi_log(dip, ddi_get_name(dip), CE_CONT, buf);
		len -= n;
	}
}

#endif	/* PLNDEBUG */

#ifdef __lock_lint
void
dummy_warlock()
{
	pln_offline_callback(NULL);
	pln_reset_callback(NULL);
	pln_scsi_dmafree(NULL, NULL);
	pln_abort(NULL, NULL);
	pln_commoncap(NULL, NULL, 0, 0, 0);
	pln_getcap(NULL, NULL, 0);
	pln_reset(NULL, 0);
	pln_scsi_destroy_pkt(NULL, NULL);
	pln_scsi_get_name(NULL, NULL, 0);
	pln_scsi_tgt_free(NULL, NULL, NULL, NULL);
	pln_setcap(NULL, NULL, 0, 0);
	pln_start(NULL, NULL);
	pln_statec_callback(NULL, 0);
	pln_uc_callback(NULL);
	pln_kmem_cache_constructor(NULL, NULL, 0);
	pln_kmem_cache_destructor(NULL, NULL);
}
#endif
