/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)glm.c	1.61	96/10/18 SMI"

/*
 * glm - Symbios 53c825 and 53c875 SCSI Processor HBA driver.
 */
#if defined(lint) && !defined(DEBUG)
#define	DEBUG 1
#define	GLM_DEBUG
#endif

/*
 * standard header files.
 */
#include <sys/note.h>
#include <sys/scsi/scsi.h>
#include <sys/pci.h>

/*
 * private header files.
 */
#include <sys/scsi/adapters/glmvar.h>
#include <sys/scsi/adapters/glmreg.h>
#include <sys/scsi/adapters/reset_notify.h>

#if defined(GLM_DEBUG)
static ulong_t	glm_debug_flags = 0x0;
#endif	/* defined(GLM_DEBUG) */

/*
 * Use one copy of scripts for memory scripts.  Point all hba's that
 * use memory scripts to this one location.
 */
static uint_t glm_scripts[NSS_FUNCS];
static uint_t glm_do_list_end;
static uint_t glm_di_list_end;

extern uchar_t	scsi_cdb_size[];

static struct glm *glm_head, *glm_tail;
static int glm_scsi_watchdog_tick;
static long glm_tick;

/*
 * tunables
 */
static uchar_t	glm_default_offset = GLM_875_OFFSET;
static uint_t	glm_selection_timeout = NB_STIME0_204;
static uchar_t	glm_use_scripts_prefetch = 1;

/*
 * Include the output of the NASM program. NASM is a program
 * which takes the scr.ss file and turns it into a series of
 * C data arrays and initializers.
 */
#include <sys/scsi/adapters/scr.out>

static ddi_acc_handle_t glm_script_acc_handle;
static ddi_dma_handle_t glm_script_dma_handle;

static size_t glm_script_size = sizeof (SCRIPT);

/*
 * warlock directives
 */
_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", scsi_pkt \
	glm_scsi_cmd NcrTableIndirect buf))

/*
 * autoconfiguration data and routines.
 */
static int glm_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int glm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int glm_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static void glm_table_init(glm_t *glm, glm_unit_t *unit, ulong_t tbl_addr,
    int target, int lun);

static int glm_config_space_init(struct glm *glm);
static int glm_hba_init(glm_t *glm);
static void glm_hba_fini(glm_t *glm);
static int glm_script_alloc(glm_t *glm);
static void glm_script_free(struct glm *glm);
static void glm_cfg_fini(glm_t *glm_blkp);
static int glm_script_offset(int func);
static int glm_memory_script_init(glm_t *glm);
static void glm_script_fini(struct glm *glm);

/*
 * SCSA function prototypes with some helper functions for DMA.
 */
static int glm_scsi_start(struct scsi_address *ap, struct scsi_pkt *pkt);
static int glm_scsi_reset(struct scsi_address *ap, int level);
static int glm_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pktp);
static int glm_capchk(char *cap, int tgtonly, int *cidxp);
static int glm_scsi_getcap(struct scsi_address *ap, char *cap, int tgtonly);
static int glm_scsi_setcap(struct scsi_address *ap, char *cap, int value,
    int tgtonly);
static struct scsi_pkt *glm_dmaget(struct scsi_pkt *pktp, struct buf *bp,
    int (*callback)(), caddr_t arg);
static struct scsi_pkt *glm_scsi_impl_dmaget(struct scsi_pkt *pkt,
    struct buf *bp, int (*callback)(), caddr_t callback_arg,
	ddi_dma_attr_t *dmaattrp);
static void glm_scsi_dmafree(struct scsi_address *ap, struct scsi_pkt *pktp);
static struct scsi_pkt *glm_scsi_init_pkt(struct scsi_address *ap,
    struct scsi_pkt *in_pkt, struct buf *bp, int cmdlen, int statuslen,
	int tgtlen, int flags, int (*callback)(), caddr_t arg);

static void glm_scsi_sync_pkt(struct scsi_address *ap, struct scsi_pkt *pktp);
static void glm_scsi_destroy_pkt(struct scsi_address *ap,
    struct scsi_pkt *pkt);
static int glm_scsi_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd);
static int glm_scsi_tgt_probe(struct scsi_device *sd,
    int (*callback)());
static void glm_scsi_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd);
static int glm_scsi_reset_notify(struct scsi_address *ap, int flag,
    void (*callback)(caddr_t), caddr_t arg);

/*
 * internal function prototypes.
 */
static void glm_sg_setup(struct glm_unit *unit, ncmd_t *cmdp);
static void glm_sg_update(glm_unit_t *unit, uchar_t index, ulong_t remain);
static ulong_t glm_sg_residual(glm_unit_t *unit);
static void glm_queue_pkt(glm_t *glm, glm_unit_t *unit, ncmd_t *cmd);
static int glm_abort_ccb(struct scsi_address *ap, glm_t *glm,
    glm_unit_t *unit);
static int glm_send_dev_reset(struct scsi_address *ap, glm_t *glm);
static int glm_do_scsi_reset(struct scsi_address *ap, int level);
static int glm_abort_cmd(struct glm *glm, struct glm_unit *unit,
    struct glm_scsi_cmd *cmd);
static void glm_chkstatus(glm_t *glm, glm_unit_t *unit,
    struct glm_scsi_cmd *cmd);
static void glm_pollret(glm_t *glm, ncmd_t *poll_cmdp);
static void glm_flush_hba(glm_t *glm, uchar_t flush_all, uchar_t pkt_reason,
    u_long pkt_state, u_long pkt_statistics);
static void glm_flush_target(glm_t *glm, ushort target, uchar_t flush_all,
    uchar_t pkt_reason, u_long pkt_state, u_long pkt_statistics);
static void glm_flush_lun(glm_t *glm, glm_unit_t *unit, uchar_t flush_all,
    uchar_t pkt_reason, u_long pkt_state, u_long pkt_statistics);
static void glm_set_done(glm_t *glm, glm_unit_t *unit, ncmd_t *cmdp,
    uchar_t pkt_reason, u_long pkt_state, u_long pkt_statistics);
static void glm_process_intr(glm_t *glm, uchar_t istat);
static ulong_t glm_decide(glm_t *glm, ulong_t action);
static ulong_t glm_ccb_decide(glm_t *glm, glm_unit_t *unit, ulong_t action);
static int glm_wait_intr(glm_t *glm);

static void glm_watch(caddr_t arg);
static void glm_watchsubr(register struct glm *glm);
static void glm_cmd_timeout(struct glm *glm, struct glm_unit *unit);
static void glm_sync_wide_backoff(struct glm *glm, struct glm_unit *unit);
static void glm_force_renegotiation(struct glm *glm, int target);

static uint_t glm_intr(caddr_t arg);
static void glm_start_next(struct glm *glm);
static void glm_wait_for_reselect(glm_t *glm, ulong_t action);
static void glm_restart_current(glm_t *glm, ulong_t action);
static void glm_restart_hba(glm_t *glm, ulong_t action);
static void glm_queue_target(glm_t *glm, glm_unit_t *unit);
static glm_unit_t *glm_get_target(glm_t *glm);
static ulong_t glm_check_intcode(glm_t *glm, glm_unit_t *unit, ulong_t action);
static ulong_t glm_parity_check(uchar_t phase);
static void glm_addfq(glm_t	*glm, glm_unit_t *unit);
static void glm_addbq(glm_t	*glm, glm_unit_t *unit);
static glm_unit_t *glm_rmq(glm_t *glm);
static void glm_doneq_add(glm_t *glm, ncmd_t *cmdp);
static ncmd_t *glm_doneq_rm(glm_t *glm);
static void glm_waitq_add(glm_unit_t *unit, ncmd_t *cmdp);
static void glm_waitq_add_lifo(glm_unit_t *unit, ncmd_t *cmdp);
static ncmd_t *glm_waitq_rm(glm_unit_t *unit);
static void glm_waitq_delete(glm_unit_t *unit, ncmd_t *cmdp);

static void glm_syncio_state(glm_t *glm, glm_unit_t *unit, uchar_t state,
    uchar_t sxfer, uchar_t sscfX10);
static void glm_syncio_disable(glm_t *glm);
static void glm_syncio_reset_target(glm_t *glm, int target);
static void glm_syncio_reset(glm_t *glm, glm_unit_t *unit);
static void glm_syncio_msg_init(glm_t *glm, glm_unit_t *unit);
static int glm_syncio_enable(glm_t *glm, glm_unit_t *unit);
static int glm_syncio_respond(glm_t *glm, glm_unit_t *unit);
static ulong_t glm_syncio_decide(glm_t *glm, glm_unit_t *unit, ulong_t action);

static int glm_max_sync_divisor(glm_t *glm, int syncioperiod,
    uchar_t *sxferp, uchar_t *sscfX10p);
static int glm_period_round(glm_t *glm, int syncioperiod);
static void glm_max_sync_rate_init(glm_t *glm);

static void glm_make_wdtr(struct glm *glm, struct glm_unit *unit, uchar_t wide);
static void glm_set_wide_scntl3(struct glm *glm, struct glm_unit *unit,
    uchar_t width);

static void glm_update_props(struct glm *glm, int tgt);
static void glm_update_this_prop(struct glm *glm, char *property, int value);

static void glm_log(struct glm *glm, int level, char *fmt, ...);

static void	glm53c810_reset(glm_t *glm);
static void	glm53c810_init(glm_t *glm);
static void	glm53c810_enable(glm_t *glm);
static void	glm53c810_disable(glm_t *glm);
static uchar_t	glm53c810_get_istat(glm_t *glm);
static void	glm53c810_halt(glm_t *glm);
static void	glm53c810_set_sigp(glm_t *glm);
static void	glm53c810_reset_sigp(glm_t *glm);
static ulong_t	glm53c810_get_intcode(glm_t *glm);
static void	glm53c810_check_error(glm_unit_t *unit, struct scsi_pkt *pktp);
static ulong_t	glm53c810_dma_status(glm_t *glm);
static ulong_t	glm53c810_scsi_status(glm_t *glm);
static int	glm53c810_save_byte_count(glm_t *glm, glm_unit_t *unit);
static int	glm53c810_get_target(struct glm *glm, uchar_t *tp);
static void	glm53c810_set_syncio(glm_t *glm, glm_unit_t *unit);
static void	glm53c810_setup_script(glm_t *glm, glm_unit_t *unit, int resel);
static void	glm53c810_start_script(glm_t *glm, int script);
static void	glm53c810_bus_reset(glm_t *glm);

static nops_t	glm53c810_nops = {
	"53c825",
	glm53c810_reset,
	glm53c810_init,
	glm53c810_enable,
	glm53c810_disable,
	glm53c810_get_istat,
	glm53c810_halt,
	glm53c810_set_sigp,
	glm53c810_reset_sigp,
	glm53c810_get_intcode,
	glm53c810_check_error,
	glm53c810_dma_status,
	glm53c810_scsi_status,
	glm53c810_save_byte_count,
	glm53c810_get_target,
	glm53c810_setup_script,
	glm53c810_start_script,
	glm53c810_set_syncio,
	glm53c810_bus_reset
};


static ddi_dma_attr_t glm_dma_attrs = {
	DMA_ATTR_V0,	/* attribute layout version		*/
	0x0ull,		/* address low - should be 0 (longlong)	*/
	0xffffffffull,	/* address high - 32-bit max range	*/
	0x00ffffffull,	/* count max - max DMA object size 	*/
	4,		/* allocation alignment requirements	*/
	0x78,		/* burstsizes - binary encoded values	*/
	1,		/* minxfer - gran. of DMA engine 	*/
	0x00ffffffull,	/* maxxfer - gran. of DMA engine 	*/
	0xffffffffull,	/* max segment size (DMA boundary) 	*/
	GLM_MAX_DMA_SEGS, /* scatter/gather list length		*/
	512,		/* granularity - device transfer size	*/
	0		/* flags, set to 0			*/
};

static ddi_device_acc_attr_t dev_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};

static struct dev_ops glm_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	glm_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	glm_attach,		/* attach */
	glm_detach,		/* detach */
	nodev,			/* reset */
	(struct	cb_ops *)0,	/* driver operations */
	NULL,			/* bus operations */
	ddi_power		/* power */
};

char _depends_on[] = "misc/scsi";

static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module. This one is a driver */
	"GLM SCSI HBA Driver 1.61.",   /* Name of the module. */
	&glm_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

/*
 * Local static data
 */
static kmutex_t glm_global_mutex;
static int glm_global_init = 0;
static void *glm_state;		/* soft	state ptr */

/*
 * Notes:
 *	- scsi_hba_init(9F) initializes SCSI HBA modules
 *	- must call scsi_hba_fini(9F) if modload() fails
 */

int
_init(void)
{
	int	status;
	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	status = ddi_soft_state_init(&glm_state, sizeof (struct glm),
		GLM_INITIAL_SOFT_SPACE);

	if (status != 0) {
		return (status);
	}

	if ((status = scsi_hba_init(&modlinkage)) != 0) {
		return (status);
	}

	mutex_init(&glm_global_mutex, "GLM Global Mutex",
		MUTEX_DRIVER, (void *)NULL);

	if ((status = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&glm_global_mutex);
		scsi_hba_fini(&modlinkage);
	}

	return (status);
}

/*
 * Notes:
 *	- scsi_hba_fini(9F) uninitializes SCSI HBA modules
 */
int
_fini(void)
{
	int	  status;
	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	if ((status = mod_remove(&modlinkage)) == 0) {
		mutex_destroy(&glm_global_mutex);
		ddi_soft_state_fini(&glm_state);
		scsi_hba_fini(&modlinkage);
	}
	return (status);
}

/*
 * The loadable-module _info(9E) entry point
 */
int
_info(struct modinfo *modinfop)
{
	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
glm_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	return (DDI_FAILURE);
}

/*
 * Notes:
 * 	Set up all device state and allocate data structures,
 *	mutexes, condition variables, etc. for device operation.
 *	Add interrupts needed.
 *	Return DDI_SUCCESS if device is ready, else return DDI_FAILURE.
 */
static int
glm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	register glm_t		*glm = NULL;
	scsi_hba_tran_t		*hba_tran = NULL;
	char			*prop_template = "target%d-scsi-options";
	char			prop_str[32];
	int			instance, i, id;
	char			buf[64];

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		hba_tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		if (!hba_tran) {
			return (DDI_FAILURE);
		}

		glm = TRAN2GLM(hba_tran);

		if (!glm) {
			return (DDI_FAILURE);
		}

		/*
		 * Reset hardware and softc to "no outstanding commands"
		 * Note	that a check condition can result on first command
		 * to a	target.
		 */
		mutex_enter(&glm->g_mutex);

		/*
		 * glm_config_space_init will re-enable the correct
		 * values in config space.
		 */
		if (glm_config_space_init(glm) == FALSE) {
			mutex_exit(&glm->g_mutex);
			return (DDI_FAILURE);
		}

		if (glm_script_alloc(glm) != DDI_SUCCESS) {
			mutex_exit(&glm->g_mutex);
			return (DDI_FAILURE);
		}

		glm->g_suspended = 0;

		/*
		 * reset/init the chip and enable the interrupts
		 * and the interrupt handler
		 */
		GLM_RESET(glm);
		GLM_INIT(glm);
		GLM_ENABLE_INTR(glm);

		glm_syncio_reset(glm, NULL);
		glm->g_wide_enabled = glm->g_wide_known = 0;

		mutex_exit(&glm->g_mutex);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);

	}

	instance = ddi_get_instance(dip);

	/*
	 * Allocate softc information.
	 */
	if (ddi_soft_state_zalloc(glm_state, instance) != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "glm%d: cannot allocate soft state", instance);
		return (DDI_FAILURE);
	}

	glm = (struct glm *)ddi_get_soft_state(glm_state, instance);

	if (glm == NULL) {
		ddi_soft_state_free(glm_state, instance);
		return (DDI_FAILURE);
	}

	/* Allocate a transport structure */
	hba_tran = scsi_hba_tran_alloc(dip, SCSI_HBA_CANSLEEP);

	if (hba_tran == NULL) {
		cmn_err(CE_WARN, "glm: scsi_hba_tran_alloc failed");
		ddi_soft_state_free(glm_state, instance);
		return (DDI_FAILURE);
	}

	glm->g_dip = dip;
	glm->g_tran = hba_tran;
	glm->g_instance = instance;

	/*
	 * set host ID
	 */
	glm->g_glmid = DEFAULT_HOSTID;
	id = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "initiator-id", -1);
	if (id == -1) {
		id = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
		    "scsi-initiator-id", -1);
	}
	if (id != DEFAULT_HOSTID && id >= 0 && id < NTARGETS_WIDE) {
		glm_log(glm, CE_CONT, "?initiator SCSI ID now %d\n", id);
		glm->g_glmid = (uchar_t)id;
	}

	glm->g_ops = &glm53c810_nops;

	/*
	 * Setup configuration space
	 */
	if (glm_config_space_init(glm) == FALSE) {
		scsi_hba_tran_free(hba_tran);
		return (DDI_FAILURE);
	}

	/*
	 * map in the GLM's operating registers.
	 */
	if (ddi_regs_map_setup(dip, MEM_SPACE, &glm->g_devaddr,
	    0, 0, &dev_attr, &glm->g_datap) != DDI_SUCCESS) {
		ddi_soft_state_free(glm_state, instance);
		scsi_hba_tran_free(hba_tran);
		return (DDI_FAILURE);
	}

	if (glm_script_alloc(glm) != DDI_SUCCESS) {
		glm_cfg_fini(glm);
		scsi_hba_tran_free(hba_tran);
		ddi_soft_state_free(glm_state, instance);
		return (DDI_FAILURE);
	}

	if (glm_hba_init(glm) == DDI_FAILURE) {
		glm_script_free(glm);
		glm_cfg_fini(glm);
		scsi_hba_tran_free(hba_tran);
		ddi_soft_state_free(glm_state, instance);
		return (DDI_FAILURE);
	}

	/*
	 * Get iblock_cookie to initialize mutexes used in the
	 * interrupt handler.
	 */
	if (ddi_get_iblock_cookie(dip, 0,
	    &glm->g_iblock) != DDI_SUCCESS) {
		glm_hba_fini(glm);
		glm_script_free(glm);
		glm_cfg_fini(glm);
		scsi_hba_tran_free(hba_tran);
		ddi_soft_state_free(glm_state, instance);
		return (DDI_FAILURE);
	}

	sprintf(buf, "glm%d_mutex", instance);
	mutex_init(&glm->g_mutex, buf, MUTEX_DRIVER, glm->g_iblock);

	/*
	 * Now register the interrupt handler.
	 */
	if (ddi_add_intr(dip, 0, &glm->g_iblock,
	    (ddi_idevice_cookie_t *)0, glm_intr, (caddr_t)glm)) {
		mutex_destroy(&glm->g_mutex);
		glm_hba_fini(glm);
		glm_script_free(glm);
		glm_cfg_fini(glm);
		scsi_hba_tran_free(hba_tran);
		ddi_soft_state_free(glm_state, instance);
		return (DDI_FAILURE);
	}

	/*
	 * initialize SCSI HBA transport structure
	 */
	hba_tran->tran_hba_private	= glm;
	hba_tran->tran_tgt_private	= NULL;

	hba_tran->tran_tgt_init		= glm_scsi_tgt_init;
	hba_tran->tran_tgt_probe	= glm_scsi_tgt_probe;
	hba_tran->tran_tgt_free		= glm_scsi_tgt_free;

	hba_tran->tran_start 		= glm_scsi_start;
	hba_tran->tran_reset		= glm_scsi_reset;
	hba_tran->tran_abort		= glm_scsi_abort;
	hba_tran->tran_getcap		= glm_scsi_getcap;
	hba_tran->tran_setcap		= glm_scsi_setcap;
	hba_tran->tran_init_pkt		= glm_scsi_init_pkt;
	hba_tran->tran_destroy_pkt	= glm_scsi_destroy_pkt;

	hba_tran->tran_dmafree		= glm_scsi_dmafree;
	hba_tran->tran_sync_pkt		= glm_scsi_sync_pkt;
	hba_tran->tran_reset_notify	= glm_scsi_reset_notify;
	hba_tran->tran_get_bus_addr	= NULL;
	hba_tran->tran_get_name		= NULL;

	/*
	 * disable wide for all targets
	 * (will be enabled by target driver if required)
	 * sync is enabled by default
	 */
	glm->g_nowide =	glm->g_notag = ALL_TARGETS;

	if (scsi_hba_attach_setup(dip, &glm_dma_attrs,
	    hba_tran, 0) != DDI_SUCCESS) {
		ddi_remove_intr(dip, 0, glm->g_iblock);
		mutex_destroy(&glm->g_mutex);
		glm_hba_fini(glm);
		glm_script_free(glm);
		glm_cfg_fini(glm);
		scsi_hba_tran_free(hba_tran);
		ddi_soft_state_free(glm_state, instance);
		return (DDI_FAILURE);
	}

	/*
	 * if scsi-options property exists, use it.
	 */
	glm->g_scsi_options = ddi_prop_get_int(DDI_DEV_T_ANY,
	    dip, 0, "scsi-options", DEFAULT_SCSI_OPTIONS);

	if ((glm->g_scsi_options & SCSI_OPTIONS_SYNC) == 0) {
		glm_syncio_disable(glm);
	}

	if ((glm->g_scsi_options & SCSI_OPTIONS_WIDE) == 0) {
		glm->g_nowide = ALL_TARGETS;
	}

	/*
	 * Initialize mask of deferred property updates
	 */
	glm->g_props_update = 0;

	/*
	 * if target<n>-scsi-options property exists, use it;
	 * otherwise use the g_scsi_options
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		(void) sprintf(prop_str, prop_template, i);
		glm->g_target_scsi_options[i] = ddi_prop_get_int(
			DDI_DEV_T_ANY, dip, 0, prop_str, -1);

		if (glm->g_target_scsi_options[i] != -1) {
			glm_log(glm, CE_NOTE, "?target%x-scsi-options=0x%x\n",
			    i, glm->g_target_scsi_options[i]);
			glm->g_target_scsi_options_defined |= (1 << i);
		} else {
			glm->g_target_scsi_options[i] = glm->g_scsi_options;
		}
		if (((glm->g_target_scsi_options[i] &
		    SCSI_OPTIONS_DR) == 0) &&
		    (glm->g_target_scsi_options[i] & SCSI_OPTIONS_TAG)) {
			glm->g_target_scsi_options[i] &= ~SCSI_OPTIONS_TAG;
		}

		/*
		 * glm driver only support FAST20 on Rev 2 or greater parts.
		 * This is the first chip with the SCLK doubler, so a 40 Mhz
		 * clock to the SCLK pin can be doubled to 80 Mhz internally.
		 *
		 * Otherwise, we disable FAST20 for all targets.
		 */
		if (glm->g_devid != GLM_53c875 ||
		    (glm->g_devid == GLM_53c875 && glm->g_revid < REV2)) {
			glm->g_target_scsi_options[i] &= ~SCSI_OPTIONS_FAST20;
		}
	}

	glm->g_scsi_tag_age_limit =
	    ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "scsi-tag-age-limit",
		DEFAULT_TAG_AGE_LIMIT);

	glm->g_scsi_reset_delay	= ddi_prop_get_int(DDI_DEV_T_ANY,
	    dip, 0, "scsi-reset-delay",	DEFAULT_RESET_DELAY);

	/*
	 * used for glm_watch
	 */
	if (glm_head == NULL) {
		glm_head = glm;
	} else {
		glm_tail->g_next = glm;
	}
	glm_tail = glm;

	if (glm_scsi_watchdog_tick == 0) {
		glm_scsi_watchdog_tick = ddi_prop_get_int(DDI_DEV_T_ANY,
		    dip, 0, "scsi-watchdog-tick", DEFAULT_WD_TICK);

		glm_tick = drv_usectohz((clock_t)
		    glm_scsi_watchdog_tick * 1000000);

		timeout(glm_watch, (caddr_t)0, glm_tick);
	}

	/*
	 * reset and initilize the chip.
	 */
	GLM_RESET(glm);
	GLM_INIT(glm);

	/*
	 * create power	management property
	 * All components are created idle.
	 */
	if (pm_create_components(dip, 1) == DDI_SUCCESS) {
		pm_set_normal_power(dip, 0, 1);
	} else {
		return (DDI_FAILURE);
	}

	/* Print message of HBA present */
	ddi_report_dev(dip);

	/* enable the interrupts and the interrupt handler */
	GLM_ENABLE_INTR(glm);

	return (DDI_SUCCESS);
}

/*
 * detach(9E).  Remove all device allocations and system resources;
 * disable device interrupts.
 * Return DDI_SUCCESS if done; DDI_FAILURE if there's a problem.
 */
static int
glm_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	register glm_t	*glm;
	scsi_hba_tran_t *tran;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	switch (cmd) {
	case DDI_DETACH:
		/*
		 * Be sure to call scsi_hba_detach(dip), if
		 * this is ever implemented.
		 */
		return (DDI_FAILURE);

	case DDI_SUSPEND:
	case DDI_PM_SUSPEND:
		tran = (scsi_hba_tran_t *)ddi_get_driver_private(devi);
		if (!tran) {
			return (DDI_SUCCESS);
		}
		glm = TRAN2GLM(tran);
		if (!glm) {
			return (DDI_SUCCESS);
		}

		if (cmd != DDI_SUSPEND) {
			glm->g_suspended = 1;
		}

		mutex_enter(&glm->g_mutex);

		GLM_BUS_RESET(glm);

		glm->g_wide_known = glm->g_wide_enabled = 0;
		glm_syncio_reset(glm, NULL);

		/* Disable HBA interrupts in hardware */
		GLM_DISABLE_INTR(glm);

		mutex_exit(&glm->g_mutex);

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
	/* NOTREACHED */
}


#define	ClrSetBits(reg, clr, set) \
	ddi_put8(glm->g_datap, (uint8_t *)(reg), \
		((ddi_get8(glm->g_datap, (uint8_t *)(reg)) & ~(clr)) | (set)))

/*
 * Clear the DMA FIFO pointers
 */
#define	CLEAR_DMA(glm) \
	ddi_put8((glm)->g_datap, (uint8_t *)((glm)->g_devaddr + NREG_CTEST3), \
	    ddi_get8((glm)->g_datap, \
	    (uint8_t *)((glm)->g_devaddr + NREG_CTEST3)) | NB_CTEST3_CLF)

/*
 * Clear the SCSI FIFO pointers
 */
#define	CLEAR_SCSI_FIFO(glm) \
	ddi_put8((glm)->g_datap, (uint8_t *)((glm)->g_devaddr + NREG_STEST3), \
	    ddi_get8((glm)->g_datap, \
	    (uint8_t *)((glm)->g_devaddr + NREG_STEST3)) | NB_STEST3_CSF)

/*
 * Reset SCSI Offset
 */
#define	RESET_SCSI_OFFSET(glm) \
	ddi_put8((glm)->g_datap, (uint8_t *)((glm)->g_devaddr + NREG_STEST2), \
	    ddi_get8((glm)->g_datap, \
	    (uint8_t *)((glm)->g_devaddr + NREG_STEST2)) | NB_STEST2_ROF)

/*
 * Initialize configuration space and figure out which
 * chip and revison of the chip the glm driver is using.
 */
static int
glm_config_space_init(struct glm *glm)
{
	ushort_t cmdreg;

	/*
	 * map in configuration space.
	 */
	if (ddi_regs_map_setup(glm->g_dip, CONFIG_SPACE, &glm->g_conf_addr,
	    0, 0, &dev_attr, &glm->g_conf_handle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "glm%d: cannot map configuration space.",
		    glm->g_instance);
		return (FALSE);
	}

	/*
	 * Get the chip device id:
	 *	3 - 53c825
	 *	f - 53c875
	 */
	glm->g_devid = ddi_get16(glm->g_conf_handle,
			(uint16_t *)(glm->g_conf_addr + PCI_CONF_DEVID));

	/*
	 * Get the chip revision.
	 */
	glm->g_revid = ddi_get8(glm->g_conf_handle,
			(uint8_t *)(glm->g_conf_addr + PCI_CONF_REVID));

	/*
	 * Each chip has different capabilities, disable certain
	 * features depending on which chip is found.
	 */
	if (glm->g_devid == GLM_53c825) {
		glm->g_ops->glm_chip = "53c825";
		glm->g_sync_offset = GLM_825_OFFSET;
		glm_log(glm, CE_CONT, "?Rev. %d Symbios 53c825 found.\n",
		    glm->g_revid);
	} else if (glm->g_devid == GLM_53c875) {
		glm->g_ops->glm_chip = "53c875";
		glm->g_sync_offset = glm_default_offset;
		switch (glm->g_revid) {
		case REV1:
			/*
			 * disable scripts prefetching for rev. 1 875 chips.
			 */
			glm_use_scripts_prefetch = 0;
			break;
		default:
			break;
		}
		glm_log(glm, CE_CONT, "?Rev. %d Symbios 53c875 found.\n",
		    glm->g_revid);

		/*
		 * Now locate the address of the SCRIPTS ram.  This
		 * address offset is needed by the SCRIPTS processor.
		 */
		glm->g_ram_base_addr = ddi_get32(glm->g_conf_handle,
		    (uint32_t *)(glm->g_conf_addr + PCI_CONF_BASE2));
	} else {
		/*
		 * Free the configuration registers and fail.
		 */
		ddi_regs_map_free(&glm->g_conf_handle);
		return (FALSE);
	}

	/*
	 * Set the command register to the needed values.
	 */
	cmdreg = ddi_get16(glm->g_conf_handle,
			(uint16_t *)(glm->g_conf_addr + PCI_CONF_COMM));
	cmdreg |= (PCI_COMM_ME | PCI_COMM_SERR_ENABLE |
			PCI_COMM_PARITY_DETECT | PCI_COMM_MAE);
	cmdreg &= ~PCI_COMM_IO;
	ddi_put16(glm->g_conf_handle,
			(uint16_t *)(glm->g_conf_addr + PCI_CONF_COMM), cmdreg);

	/*
	 * Set the latency timer to 0x40 as specified by the upa -> pci
	 * bridge chip design team.  This may be done by the sparc pci
	 * bus nexus driver, but the driver should make sure the latency
	 * timer is correct for performance reasons.
	 */
	ddi_put8(glm->g_conf_handle,
		(uint8_t *)(glm->g_conf_addr + PCI_CONF_LATENCY_TIMER),
		GLM_LATENCY_TIMER);

	/*
	 * Free the configuration space mapping, no longer needed.
	 */
	ddi_regs_map_free(&glm->g_conf_handle);
	return (TRUE);
}

/*
 * Initialize the Table Indirect pointers for each target, lun
 */
static void
glm_table_init(glm_t *glm, glm_unit_t *unit, ulong_t tbl_addr,
	int target, int lun)
{
	struct glm_dsa *dsap;
	ddi_acc_handle_t accessp;

	/* clear the table */
	dsap = unit->nt_dsap;
	bzero((caddr_t)dsap, sizeof (*dsap));

	unit->nt_dma_attr = glm_dma_attrs;
	unit->nt_dsa_addr = tbl_addr;
	unit->nt_target = (ushort)target;
	unit->nt_lun = (ushort)lun;
	unit->nt_state = NPT_STATE_DONE;
	unit->nt_waitqtail = &unit->nt_waitq;

	/* initialize the sharable data structure between host and hba */
	/* perform all byte assignments */
	dsap->nt_selectparm.nt_sdid = (uchar_t)target;
	dsap->nt_selectparm.nt_scntl3 = glm->g_scntl3;
	dsap->nt_msgoutbuf[0] = (MSG_IDENTIFY | lun);

	accessp = unit->nt_accessp;
	/* perform multi-bytes assignments */

	ddi_put32(accessp, (uint32_t *)&dsap->nt_cmd.count,
	    sizeof (dsap->nt_cdb));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_cmd.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_cdb) - (ulong_t)dsap));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_sendmsg.count,
	    sizeof (dsap->nt_msgoutbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_sendmsg.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_msgoutbuf) - (ulong_t)dsap));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_rcvmsg.count,
	    sizeof (dsap->nt_msginbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_rcvmsg.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_msginbuf) - (ulong_t)dsap));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_status.count,
	    sizeof (dsap->nt_statbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_status.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_statbuf) - (ulong_t)dsap));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_extmsg.count,
	    sizeof (dsap->nt_extmsgbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_extmsg.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_extmsgbuf) - (ulong_t)dsap));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_syncin.count,
	    sizeof (dsap->nt_syncibuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_syncin.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_syncibuf) - (ulong_t)dsap));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_errmsg.count,
	    sizeof (dsap->nt_errmsgbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_errmsg.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_errmsgbuf) - (ulong_t)dsap));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_widein.count,
	    sizeof (dsap->nt_wideibuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_widein.address,
	    EFF_ADDR(tbl_addr, (ulong_t)&(dsap->nt_wideibuf) - (ulong_t)dsap));
}


/*
 * glm_hba_init()
 *
 *	Set up this HBA's copy of the SCRIPT and initialize
 *	each of it's target/luns.
 */
static int
glm_hba_init(glm_t *glm)
{
	glm_unit_t *unit;
	u_int alloc_len;
	ddi_dma_attr_t unit_dma_attrs;
	u_int ncookie;
	struct glm_dsa *dsap;
	ddi_dma_cookie_t cookie;
	ddi_dma_handle_t dma_handle;
	ddi_acc_handle_t accessp;

	unit = kmem_zalloc(sizeof (glm_unit_t), KM_SLEEP);
	if (unit == NULL) {
		return (DDI_FAILURE);
	}

	glm->g_state = NSTATE_IDLE;

	/*
	 * Initialize the empty FIFO completion queue
	 */
	glm->g_donetail = &glm->g_doneq;

	/*
	 * Set syncio for hba to be reject, i.e. we never send a
	 * sdtr or wdtr to ourself.
	 */
	glm->g_syncstate[glm->g_glmid] = NSYNC_SDTR_REJECT;

	/*
	 * dynamically create a customized dma attribute structure
	 * that describes the GLM's per-target structures.
	 */
	unit_dma_attrs.dma_attr_version	= DMA_ATTR_V0;
	unit_dma_attrs.dma_attr_addr_lo	= 0x0ull;
	unit_dma_attrs.dma_attr_addr_hi	= 0xffffffffull;
	unit_dma_attrs.dma_attr_count_max = 0x00ffffffull;
	unit_dma_attrs.dma_attr_align = 4;
	unit_dma_attrs.dma_attr_burstsizes = 0x78;
	unit_dma_attrs.dma_attr_minxfer	= 1;
	unit_dma_attrs.dma_attr_maxxfer	= 0x00ffffffull;
	unit_dma_attrs.dma_attr_seg = 0xffffffffull;
	unit_dma_attrs.dma_attr_sgllen = 1;
	unit_dma_attrs.dma_attr_granular = sizeof (struct glm_dsa);
	unit_dma_attrs.dma_attr_flags = 0;

	/*
	 * allocate a per-target structure upon demand,
	 * in a platform-independent manner.
	 */
	if (ddi_dma_alloc_handle(glm->g_dip, &unit_dma_attrs,
	    DDI_DMA_DONTWAIT, NULL, &dma_handle)
		!= DDI_SUCCESS) {
		kmem_free(unit, sizeof (glm_unit_t));
		return (DDI_FAILURE);
	}

	if (ddi_dma_mem_alloc(dma_handle, sizeof (struct glm_dsa),
	    &dev_attr, DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
	    NULL, (caddr_t *)&dsap, &alloc_len, &accessp) != DDI_SUCCESS) {
		ddi_dma_free_handle(&dma_handle);
		kmem_free(unit, sizeof (glm_unit_t));
		return (DDI_FAILURE);
	}

	if (ddi_dma_addr_bind_handle(dma_handle, NULL, (caddr_t)dsap,
	    alloc_len, DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
	    NULL, &cookie, &ncookie) != DDI_DMA_MAPPED) {
		(void) ddi_dma_mem_free(&accessp);
		ddi_dma_free_handle(&dma_handle);
		kmem_free(unit, sizeof (glm_unit_t));
		return (DDI_FAILURE);
	}

	/*
	 * save ptr to HBA's per-target structure which is used by the
	 * SCRIPT while waiting for reconnections after disconnects
	 */
	NTL2UNITP(glm, glm->g_glmid, 0) = unit;
	glm->g_hbap = unit;

	unit->nt_dsap = dsap;
	unit->nt_dma_p = dma_handle;
	unit->nt_accessp = accessp;

	glm_table_init(glm, unit, cookie.dmac_address,
		glm->g_glmid, 0);

	return (DDI_SUCCESS);
}

static void
glm_hba_fini(glm_t *glm)
{
	glm_unit_t *unit;

	unit = glm->g_hbap;
	(void) ddi_dma_unbind_handle(unit->nt_dma_p);
	(void) ddi_dma_mem_free(&unit->nt_accessp);
	ddi_dma_free_handle(&unit->nt_dma_p);
	kmem_free(unit, sizeof (glm_unit_t));
	glm->g_hbap = NULL;
}

static int
glm_script_alloc(glm_t *glm)
{
	int k;

	/*
	 * If the glm is on a 875, download the script to the onboard
	 * 4k scripts ram.  Otherwise, use memory scripts.
	 *
	 * In the case of memory scripts, use only one copy.  Point all
	 * memory based glm's to this copy of memory scripts.
	 */
	switch (glm->g_devid) {
	case GLM_53c875:
		/*
		 * Now map in the 4k SCRIPTS RAM for use by the CPU/driver.
		 */
		if (ddi_regs_map_setup(glm->g_dip, BASE_REG2,
		    &glm->g_scripts_ram, 0, 4096, &dev_attr,
					&glm->g_ram_handle) != DDI_SUCCESS) {
			    return (DDI_FAILURE);
		}

		/*
		 * The reset bit in the ISTAT register can not be set
		 * if we want to write to the 4k scripts ram.
		 */
		ddi_put8(glm->g_datap,
		    (uint8_t *)(glm->g_devaddr + NREG_ISTAT), ~NB_ISTAT_SRST);

		/*
		 * Copy the scripts code into the local 4k RAM.
		 */
		ddi_rep_put32(glm->g_ram_handle, (uint32_t *)SCRIPT,
		    (uint32_t *)glm->g_scripts_ram, (glm_script_size >> 2),
			DDI_DEV_AUTOINCR);

		/*
		 * Free the 4k SRAM mapping.
		 */
		ddi_regs_map_free(&glm->g_ram_handle);

		/*
		 * These are the script entry offsets.
		 */
		for (k = 0; k < NSS_FUNCS; k++)
			glm->g_glm_scripts[k] =
			    (glm->g_ram_base_addr + glm_script_offset(k));

		glm->g_do_list_end = (glm->g_ram_base_addr + Ent_do_list_end);
		glm->g_di_list_end = (glm->g_ram_base_addr + Ent_di_list_end);

		break;

	case GLM_53c825:
		/*
		 * Memory scripts are initialized once.
		 */
		if (!glm_global_init) {
			if (glm_memory_script_init(glm) == FALSE) {
				return (DDI_FAILURE);
			}
		}

		/*
		 * Point this hba to the memory scripts.
		 */
		for (k = 0; k < NSS_FUNCS; k++)
			glm->g_glm_scripts[k] = glm_scripts[k];

		glm->g_do_list_end = glm_do_list_end;
		glm->g_di_list_end = glm_di_list_end;

		glm_global_init++;

		break;
	}

	return (DDI_SUCCESS);
}

static void
glm_script_free(struct glm *glm)
{
	mutex_enter(&glm_global_mutex);
	glm_global_init--;
	if (glm_global_init == 0)
		glm_script_fini(glm);
	mutex_exit(&glm_global_mutex);
}

static void
glm_cfg_fini(glm_t *glm)
{
	ddi_regs_map_free(&glm->g_datap);
	glm->g_ops = NULL;
}

/*
 * Offsets of SCRIPT routines.
 */
static int
glm_script_offset(int func)
{
	switch (func) {
	case NSS_STARTUP:	/* select a target and start a request */
		return (Ent_start_up);
	case NSS_CONTINUE:	/* continue with current target (no select) */
		return (Ent_continue);
	case NSS_WAIT4RESELECT:	/* wait for reselect */
		return (Ent_resel_m);
	case NSS_CLEAR_ACK:
		return (Ent_clear_ack);
	case NSS_EXT_MSG_OUT:
		return (Ent_ext_msg_out);
	case NSS_ERR_MSG:
		return (Ent_errmsg);
	case NSS_BUS_DEV_RESET:
		return (Ent_dev_reset);
	case NSS_ABORT:
		return (Ent_abort);
	default:
		return (0);
	}
	/*NOTREACHED*/
}

/*
 * glm_memory_script_init()
 */
static int
glm_memory_script_init(glm_t *glm)
{
	caddr_t		memp;
	int		func;
	u_int		alloc_len;
	u_int		ncookie;
	ddi_dma_cookie_t cookie;
	ddi_dma_attr_t	script_dma_attrs;

	/*
	 * dynamically create a customized dma attribute structure
	 * that describes the GLM's per-target structures.
	 */
	script_dma_attrs.dma_attr_version	= DMA_ATTR_V0;
	script_dma_attrs.dma_attr_addr_lo	= 0x0ull;
	script_dma_attrs.dma_attr_addr_hi	= 0xffffffffull;
	script_dma_attrs.dma_attr_count_max	= 0x00ffffffull;
	script_dma_attrs.dma_attr_align		= 4;
	script_dma_attrs.dma_attr_burstsizes	= 0x78;
	script_dma_attrs.dma_attr_minxfer	= 1;
	script_dma_attrs.dma_attr_maxxfer	= 0x00ffffffull;
	script_dma_attrs.dma_attr_seg		= 0xffffffffull;
	script_dma_attrs.dma_attr_sgllen	= 1;
	script_dma_attrs.dma_attr_granular	= glm_script_size;
	script_dma_attrs.dma_attr_flags		= 0;

	if (ddi_dma_alloc_handle(glm->g_dip, &script_dma_attrs,
	    DDI_DMA_DONTWAIT, NULL, &glm_script_dma_handle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "glm: unable to allocate dma handle.");
		return (FALSE);
	}

	if (ddi_dma_mem_alloc(glm_script_dma_handle, glm_script_size,
	    &dev_attr, DDI_DMA_STREAMING, DDI_DMA_DONTWAIT,
	    NULL, &memp, &alloc_len, &glm_script_acc_handle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "glm: unable to allocate script memory.");
		return (FALSE);
	}

	if (ddi_dma_addr_bind_handle(glm_script_dma_handle, NULL, memp,
	    alloc_len, DDI_DMA_READ | DDI_DMA_STREAMING, DDI_DMA_DONTWAIT, NULL,
	    &cookie, &ncookie) != DDI_DMA_MAPPED) {
		cmn_err(CE_WARN, "glm: unable to allocate script DMA.");
		return (FALSE);
	}

	/* copy the script into the buffer we just allocated */
	ddi_rep_put32(glm_script_acc_handle, (uint32_t *)SCRIPT,
	    (uint32_t *)memp, glm_script_size >> 2, DDI_DEV_AUTOINCR);

	for (func = 0; func < NSS_FUNCS; func++)
		glm_scripts[func] =
		    cookie.dmac_address + glm_script_offset(func);

	glm_do_list_end = (cookie.dmac_address + Ent_do_list_end);

	glm_di_list_end = (cookie.dmac_address + Ent_di_list_end);

	return (TRUE);
}


/*
 * Free the script buffer
 */
static void
glm_script_fini(struct glm *glm)
{
	/*
	 * If we are using memory scripts, free the memory.
	 */
	if (glm->g_devid == GLM_53c825) {
		(void) ddi_dma_unbind_handle(glm_script_dma_handle);
		(void) ddi_dma_mem_free(&glm_script_acc_handle);
		ddi_dma_free_handle(&glm_script_dma_handle);
	}
}

/*
 * prepare the pkt:
 * the pkt may have been resubmitted or just reused so
 * initialize some fields and do some checks.
 */
static int
glm_prepare_pkt(register struct glm *glm, register struct glm_scsi_cmd *cmd)
{
	register struct scsi_pkt *pkt = CMD2PKT(cmd);
	register uchar_t cdbsize;

	pkt->pkt_reason = CMD_CMPLT;
	*(pkt->pkt_scbp) = 0;

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		pkt->pkt_resid = cmd->cmd_dmacount;

		/*
		 * consistent packets need to be sync'ed first
		 * (only for data going out)
		 */
		if (cmd->cmd_flags & (CFLAG_CMDIOPB | CFLAG_DMASEND)) {
			(void) ddi_dma_sync(cmd->cmd_dmahandle, 0, 0,
			    DDI_DMA_SYNC_FORDEV);
		}
	}

	/*
	 * Lookup the cdb size.  If can't figure out how big it is
	 * (e.g. 6, 10, or 12 bytes), use the cdb buffer size and punt.
	 * If the cdb buffer is bigger than 12-bytes, reject the pkt.
	 */
	if ((cdbsize =
		scsi_cdb_size[CDB_GROUPID((char)*pkt->pkt_cdbp)]) == 0) {
			if ((cdbsize = cmd->cmd_cdblen) > 12) {
				return (TRAN_BADPKT);
			}
	}
	cmd->cmd_cdblen = cdbsize;

	if ((pkt->pkt_comp == NULL) &&
	    ((pkt->pkt_flags & FLAG_NOINTR) == 0)) {
		NDBG1(("intr packet with pkt_comp == 0\n"));
		return (TRAN_BADPKT);
	}

	if ((glm->g_target_scsi_options[Tgt(pkt)] & SCSI_OPTIONS_DR) == 0) {
		pkt->pkt_flags |= FLAG_NODISCON;
	}

	cmd->cmd_flags |= (cmd->cmd_flags & ~CFLAG_TRANFLAG) |
		CFLAG_PREPARED | CFLAG_IN_TRANSPORT;

	cmd->cmd_type = NRQ_NORMAL_CMD;

	return (TRAN_ACCEPT);
}

/*
 * SCSA	Interface functions
 *
 * Visible to the external world via the transport structure.
 */

/*
 * Notes:
 *	- transport the command to the addressed SCSI target/lun device
 *	- normal operation is to schedule the command to be transported,
 *	  and return TRAN_ACCEPT if this is successful.
 *	- if NO_INTR, tran_start must poll device for command completion
 */
/*ARGSUSED*/
static int
glm_scsi_start(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	glm_t *glm = PKT2GLM(pkt);
	glm_unit_t *unit;
	struct glm_scsi_cmd *cmd = PKT2CMD(pkt);
	register int rval;

	NDBG12(("glm_scsi_start\n"));

	/*
	 * prepare the pkt before taking mutex.
	 */
	rval = glm_prepare_pkt(glm, cmd);
	if (rval != TRAN_ACCEPT) {
		return (rval);
	}

	/*
	 * Send the command to target/lun, however your HBA requires it.
	 * If busy, return TRAN_BUSY; if there's some other formatting error
	 * in the packet, return TRAN_BADPKT; otherwise, fall through to the
	 * return of TRAN_ACCEPT.
	 *
	 * Remember that access to shared resources, including the glm_t
	 * data structure and the HBA hardware registers, must be protected
	 * with mutexes, here and everywhere.
	 *
	 * Also remember that at interrupt time, you'll get an argument
	 * to the interrupt handler which is a pointer to your glm_t
	 * structure; you'll have to remember which commands are outstanding
	 * and which scsi_pkt is the currently-running command so the
	 * interrupt handler can refer to the pkt to set completion
	 * status, call the target driver back through pkt_comp, etc.
	 */

	mutex_enter(&glm->g_mutex);

	unit = PKT2GLMUNITP(pkt);
	glm_queue_pkt(glm, unit, cmd);

	/*
	 * if NO_INTR flag set, tran_start(9E) must poll
	 * device for command completion.
	 */
	if (pkt->pkt_flags & FLAG_NOINTR) {
		glm_pollret(glm, cmd);
	}

	mutex_exit(&glm->g_mutex);

	NDBG12(("glm_scsi_start: okay\n"));
	return (rval);
}

static int
glm_do_scsi_reset(struct scsi_address *ap, int level)
{
	glm_t *glm = ADDR2GLM(ap);
	glm_unit_t *unit;
	int rval = FALSE;

	NDBG22(("glm_do_scsi_reset\n"));

	switch (level) {

	case RESET_ALL:
		/*
		 * Reset the SCSI bus, kill all commands in progress
		 * (remove them from lists, etc.)  Make sure you
		 * wait the specified time for the reset to settle,
		 * if your hardware doesn't do that for you somehow.
		 */
		GLM_BUS_RESET(glm);
		rval = TRUE;
		break;

	case RESET_TARGET:
		/*
		 * Issue a Bus Device Reset message to the target/lun
		 * specified in ap;
		 */

		if ((unit = ADDR2GLMUNITP(ap)) == NULL) {
			return (FALSE);
		}

		if (unit->nt_state == NPT_STATE_ACTIVE) {
			/* can't do device reset while device is active */
			return (FALSE);
		}

		rval = glm_send_dev_reset(ap, glm);
		break;
	}
	return (rval);
}

/*
 * Notes:
 *	- RESET_ALL:	reset the SCSI bus
 *	- RESET_TARGET:	reset the target specified in scsi_address
 */
static int
glm_scsi_reset(struct scsi_address *ap, int level)
{
	glm_t *glm = ADDR2GLM(ap);
	int rval;

	NDBG22(("glm_scsi_reset\n"));

	mutex_enter(&glm->g_mutex);
	ASSERT(!glm->g_suspended);
	rval = glm_do_scsi_reset(ap, level);
	mutex_exit(&glm->g_mutex);
	return (rval);
}

/*
 * Notes:
 *	- if pkt is not NULL, abort just that command
 *	- if pkt is NULL, abort all outstanding commands for target
 */
static int
glm_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pktp)
{
	glm_t	*glm = ADDR2GLM(ap);
	glm_unit_t	*unit;
	ncmd_t	*cmdp;
	int	rc;

	NDBG23(("glm_scsi_abort\n"));

	/*
	 * Abort the command pktp on the target/lun in ap.  If pktp is
	 * NULL, abort all outstanding commands on that target/lun.
	 * If you can abort them, return 1, else return 0.
	 * Each packet that's aborted should be sent back to the target
	 * driver through the callback routine, with pkt_reason set to
	 * CMD_ABORTED.
	 */

	/*
	 * abort cmd pktp on HBA hardware; clean out of outstanding
	 * command lists, etc.
	 */
	mutex_enter(&glm->g_mutex);	/* can't abort if it's active */

	if (pktp != NULL) {
		/* abort the specified packet */
		unit = PKT2GLMUNITP(pktp);
		cmdp = PKT2CMD(pktp);

		if (cmdp->cmd_queued) {
			glm_waitq_delete(unit, cmdp);
			glm_set_done(glm, unit, cmdp, CMD_ABORTED,
							0, STAT_ABORTED);
			mutex_exit(&glm->g_mutex);
			return (TRUE);
		}
		if (unit->nt_state == NPT_STATE_DONE ||
		    unit->nt_state == NPT_STATE_ACTIVE) {
			/* If it's done then it's probably already on */
			/* the done queue. If it's active we can't abort. */
			mutex_exit(&glm->g_mutex);
			return (FALSE);
		}
		/* try to abort the target's current request */
		rc = glm_abort_ccb(ap, glm, unit);
	} else {
		/* Abort all the packets for a particular LUN */
		unit = ADDR2GLMUNITP(ap);

		if (unit->nt_state == NPT_STATE_DONE ||
		    unit->nt_state == NPT_STATE_ACTIVE) {
			/* If it's done then it's probably already on */
			/* the done queue. If it's active we can't abort. */
			mutex_exit(&glm->g_mutex);
			return (FALSE);
		}

		if (unit->nt_state == NPT_STATE_QUEUED) {
			/* it's the currently active ccb on this target but */
			/* the ccb hasn't been started yet */
			glm_set_done(glm, unit, unit->nt_ncmdp, CMD_ABORTED,
			    STATE_GOT_TARGET, STAT_ABORTED);
			unit->nt_ncmdp = NULL;
		} else {
			NDBG30(("glm_abort_ccb: disconnected\n"));
			glm_flush_lun(glm, unit, FALSE, CMD_ABORTED,
			    (STATE_GOT_BUS | STATE_GOT_TARGET), STAT_ABORTED);
		}

		/* try to abort the target's current request */
		if (glm_abort_ccb(ap, glm, unit) == FALSE) {
			mutex_exit(&glm->g_mutex);
			return (FALSE);
		}

		/* abort the queued requests */
		while ((cmdp = glm_waitq_rm(unit)) != NULL) {
			NDBG23(("glm_abort: cmdp=0x%x\n", cmdp));
			glm_set_done(glm, unit, cmdp, CMD_ABORTED,
			    0, STAT_ABORTED);
		}
		rc = TRUE;
	}
	mutex_exit(&glm->g_mutex);
	return (rc);
}

/*
 * (*tran_getcap).  Get the capability named, and return its value.
 */
static int
glm_scsi_getcap(struct scsi_address *ap, char *cap, int tgtonly)
{
	register struct glm *glm = ADDR2GLM(ap);
	int ckey;
	int rval = FALSE;

	mutex_enter(&glm->g_mutex);

	NDBG23(("glm_scsi_getcap: %s\n", cap));

	if ((glm_capchk(cap, tgtonly, &ckey)) != TRUE) {
		mutex_exit(&glm->g_mutex);
		return (UNDEFINED);
	}

	switch (ckey) {
	case SCSI_CAP_ARQ:
		if (tgtonly && ADDR2GLMUNITP(ap)->nt_arq) {
			rval = TRUE;
		}
		break;
	case SCSI_CAP_TAGGED_QING:
		if (tgtonly && ADDR2GLMUNITP(ap)->nt_tagque) {
			rval = TRUE;
		}
		break;
	case SCSI_CAP_RESET_NOTIFICATION:
		rval = TRUE;
		break;
	default:
		rval = UNDEFINED;
		break;
	}
	mutex_exit(&glm->g_mutex);
	return (rval);
}

/*
 * (*tran_setcap).  Set the capability named to the value given.
 */
static int
glm_scsi_setcap(struct scsi_address *ap, char *cap, int value, int tgtonly)
{
	register struct glm *glm = ADDR2GLM(ap);
	int	ckey;
	register int target = ap->a_target;
	register ushort_t tshift = (1<<target);
	int rval = FALSE;

	mutex_enter(&glm->g_mutex);

	NDBG23(("glm_scsi_setcap: %s %d\n", cap, value));

	if ((glm_capchk(cap, tgtonly, &ckey)) != TRUE) {
		mutex_exit(&glm->g_mutex);
		return (UNDEFINED);
	}

	switch (ckey) {
	case SCSI_CAP_DMA_MAX:
	case SCSI_CAP_MSG_OUT:
	case SCSI_CAP_PARITY:
	case SCSI_CAP_INITIATOR_ID:
	case SCSI_CAP_LINKED_CMDS:
	case SCSI_CAP_UNTAGGED_QING:
	case SCSI_CAP_RESET_NOTIFICATION:
		/*
		 * None of these are settable via
		 * the capability interface.
		 */
		break;
	case SCSI_CAP_WIDE_XFER:
		if (value) {
			if (glm->g_target_scsi_options[target] &
			    SCSI_OPTIONS_WIDE) {
				glm->g_nowide &= ~tshift;
			}
		} else {
			glm->g_nowide |= tshift;
		}
		glm->g_wide_known &= ~tshift;
		glm->g_wide_enabled &= ~tshift;
		rval = TRUE;
		break;
	default:
		rval = UNDEFINED;
		break;
	}
	mutex_exit(&glm->g_mutex);
	return (rval);
}

/*
 * property management
 * glm_update_props:
 * create/update sync/wide/TQ/scsi-options properties for this target
 */
static void
glm_update_props(struct glm *glm, int tgt)
{
	char property[32];
	int wide_enabled;
	uint_t xfer_rate = 0;
	struct glm_unit *unit = glm->g_units[TL2INDEX(tgt, 0)];

	wide_enabled = ((glm->g_nowide & (1<<tgt)) == 0);

	if ((unit->nt_dsap->nt_selectparm.nt_sxfer & 0x1f) != 0) {
		xfer_rate = ((1000 * 1000)/glm->g_minperiod[tgt]);
		xfer_rate *= ((wide_enabled)? 2 : 1);
	}

	(void) sprintf(property, "target%x-sync-speed", tgt);
	glm_update_this_prop(glm, property, xfer_rate);

	(void) sprintf(property, "target%x-wide", tgt);
	glm_update_this_prop(glm, property, wide_enabled);

	(void) sprintf(property, "target%x-TQ", tgt);
	glm_update_this_prop(glm, property, 0);
}

static void
glm_update_this_prop(struct glm *glm, char *property, int value)
{
	dev_info_t *dip = glm->g_dip;

	if (ddi_prop_update_int(DDI_DEV_T_NONE, dip,
	    property, value) != DDI_PROP_SUCCESS) {
		NDBG1(("cannot modify/create %s property.\n", property));
	}
}

/*
 * (*tran_dmaget).  DMA resource allocation.  This version assumes your
 * HBA has some sort of bus-mastering or onboard DMA capability, with a
 * scatter-gather list of length GLM_MAX_DMA_SEGS, as given in the
 * ddi_dma_attr_t structure and passed to scsi_impl_dmaget.
 */
static struct scsi_pkt *
glm_dmaget(struct scsi_pkt *pkt, struct buf *bp, int (*callback)(),
    caddr_t arg)
{
	register struct glm_scsi_cmd *cmd = PKT2CMD(pkt);
	int cnt;
	glmti_t *dmap;		/* ptr to the S/G list */

	/*
	 * Set up DMA memory and position to the next DMA segment.
	 * Information will be in scsi_cmd on return; most usefully,
	 * in cmd->cmd_dmaseg.
	 */
	if (!glm_scsi_impl_dmaget(pkt, bp, callback, arg,
		&glm_dma_attrs))
		return (NULL);

	/*
	 * Always use scatter-gather transfer
	 * Use the loop below to store physical addresses of
	 * DMA segments, from the DMA cookies, into your HBA's
	 * scatter-gather list.
	 */
	dmap = cmd->cmd_sg;

	ASSERT(cmd->cmd_cookie.dmac_size != 0);

	/*
	 * store the first segment into the S/G list
	 */
	dmap->count = (ulong_t)cmd->cmd_cookie.dmac_size;
	dmap->address = (caddr_t)cmd->cmd_cookie.dmac_address;

	cmd->cmd_dmacount += cmd->cmd_cookie.dmac_size;

	/*
	 * We already stored the first DMA scatter gather segment,
	 * start at 1 if we need to store more.
	 */
	for (cnt = 1; cnt < cmd->cmd_cookiec; cnt++) {
		/*
		 * Get next DMA cookie
		 */
		ddi_dma_nextcookie(cmd->cmd_dmahandle, &cmd->cmd_cookie);
		dmap++;

		cmd->cmd_dmacount += cmd->cmd_cookie.dmac_size;

		/*
		 * store the segment parms into the S/G list
		 */
		dmap->count = (ulong_t)cmd->cmd_cookie.dmac_size;
		dmap->address = (caddr_t)cmd->cmd_cookie.dmac_address;
	}
	NDBG16(("glm_dmaget: cmd_dmacount=%d.\n", cmd->cmd_dmacount));

	return (pkt);
}


/*
 * If this pkt is doing data movement, allocate a handle and
 * bind the buffer to that handle.
 */
static struct scsi_pkt *
glm_scsi_impl_dmaget(struct scsi_pkt *pkt, struct buf *bp,
    int (*callback)(), caddr_t callback_arg, ddi_dma_attr_t *dmaattrp)
{
	register struct glm_scsi_cmd *cmd = PKT2CMD(pkt);
	dev_info_t *dip = PKT2TRAN(pkt)->tran_hba_dip;
	int dma_flags;
	int rval;

	if (!cmd->cmd_dmahandle) {
		if ((rval = ddi_dma_alloc_handle(dip, dmaattrp, callback,
		    callback_arg, &cmd->cmd_dmahandle)) != DDI_SUCCESS) {
			goto dma_failure;
		}

		ASSERT(cmd->cmd_dmahandle != NULL);

		if (bp->b_flags & B_READ) {
			dma_flags = DDI_DMA_READ;
			cmd->cmd_flags &= ~CFLAG_DMASEND;
		} else {
			dma_flags = DDI_DMA_WRITE;
			cmd->cmd_flags |= CFLAG_DMASEND;
		}

		if (cmd->cmd_flags & CFLAG_CMDIOPB)
			dma_flags |= DDI_DMA_CONSISTENT;

		if (cmd->cmd_flags & CFLAG_DMA_PARTIAL)
			dma_flags |= DDI_DMA_PARTIAL;

		rval = ddi_dma_buf_bind_handle(cmd->cmd_dmahandle, bp,
				dma_flags, callback, callback_arg,
				&cmd->cmd_cookie, &cmd->cmd_cookiec);

dma_failure:
		if (rval && (rval != DDI_DMA_MAPPED)) {
			switch (rval) {
			case DDI_DMA_NORESOURCES:
				bioerror(bp, 0);
				break;
			case DDI_DMA_BADATTR:
			case DDI_DMA_NOMAPPING:
				bioerror(bp, EFAULT);
				break;
			case DDI_DMA_TOOBIG:
			default:
				bioerror(bp, EINVAL);
				break;
			}
			cmd->cmd_flags &= ~CFLAG_DMAVALID;
			return ((struct scsi_pkt *)NULL);
		}
		cmd->cmd_flags |= CFLAG_DMAVALID;
		ASSERT(cmd->cmd_cookiec > 0);
	}
	return (pkt);
}

/*
 * tran_dmafree(9E) - deallocate DMA resources allocated for command
 */
/*ARGSUSED*/
static void
glm_scsi_dmafree(register struct scsi_address *ap,
    register struct scsi_pkt *pktp)
{
	register struct glm_scsi_cmd *cmd = PKT2CMD(pktp);

	/* Free the mapping. */
	if (cmd->cmd_dmahandle) {
		ASSERT(cmd->cmd_flags & CFLAG_DMAVALID);
		(void) ddi_dma_unbind_handle(cmd->cmd_dmahandle);
		(void) ddi_dma_free_handle(&cmd->cmd_dmahandle);
		cmd->cmd_flags ^= CFLAG_DMAVALID;
		cmd->cmd_dmahandle = NULL;
	}
}

/*
 * tran_init_pkt(9E) - allocate scsi_pkt(9S) for command
 *
 * One of three possibilities:
 *	- allocate scsi_pkt
 *	- allocate scsi_pkt and DMA resources
 *	- allocate DMA resources to an already-allocated pkt
 */
static struct scsi_pkt *
glm_scsi_init_pkt(struct scsi_address *ap, struct scsi_pkt *in_pkt,
    struct buf *bp, int cmdlen, int statuslen, int tgtlen, int flags,
    int (*callback)(), caddr_t arg)
{
	register struct scsi_pkt *pkt;
	register struct glm_scsi_cmd *cmd;
	struct glm *glm;

	ASSERT(callback == NULL_FUNC || callback == SLEEP_FUNC);

	/*
	 * Allocate the new packet.
	 */
	if (in_pkt == NULL) {
		glm = ADDR2GLM(ap);
		pkt = scsi_hba_pkt_alloc(glm->g_dip, ap, cmdlen, statuslen,
			tgtlen, sizeof (ncmd_t), callback, arg);

		if (pkt == NULL)
			return (NULL);

		cmd = PKT2CMD(pkt);
		cmd->cmd_pkt	= (struct scsi_pkt *)pkt;
		cmd->cmd_cdblen = (uchar_t)cmdlen;
		pkt->pkt_scbp	= (opaque_t)&cmd->cmd_scb;

		if (flags & PKT_CONSISTENT)
			cmd->cmd_flags |= CFLAG_CMDIOPB;

		if (flags & PKT_DMA_PARTIAL)
			cmd->cmd_flags |= CFLAG_DMA_PARTIAL;

	} else {
		pkt = in_pkt;
	}

	/*
	 * Set up dma info
	 */
	if (bp && bp->b_bcount != 0 &&
		(cmd->cmd_flags & CFLAG_DMAVALID) == 0) {
		if (glm_dmaget(pkt, bp, callback, arg) == NULL) {
			if (!in_pkt)
				scsi_hba_pkt_free(ap, pkt);
			return (NULL);
		}
	}
	return (pkt);
}

/*
 * tran_sync_pkt(9E) - explicit DMA synchronization
 */
/*ARGSUSED*/
static void
glm_scsi_sync_pkt(struct scsi_address *ap, register struct scsi_pkt *pktp)
{
	register struct glm_scsi_cmd *cmdp = PKT2CMD(pktp);

	if (cmdp->cmd_dmahandle) {
		(void) ddi_dma_sync(cmdp->cmd_dmahandle, 0, 0,
		    (cmdp->cmd_flags & CFLAG_DMASEND) ?
		    DDI_DMA_SYNC_FORDEV : DDI_DMA_SYNC_FORCPU);
	}
}

/*
 * tran_destroy_pkt(9E) - scsi_pkt(9s) deallocation
 *
 * Notes:
 *	- also frees DMA resources if allocated
 *	- implicit DMA synchonization
 */
static void
glm_scsi_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	register struct glm_scsi_cmd *cmd = PKT2CMD(pkt);

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		/*
		 * Free the mapping.
		 */
		glm_scsi_dmafree(ap, pkt);
	}
	scsi_hba_pkt_free(ap, pkt);
}

/*
 * tran_tgt_init(9E) - target device instance initialization
 */
/*ARGSUSED*/
static int
glm_scsi_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	/*
	 * At this point, the scsi_device structure already exists
	 * and has been initialized.
	 *
	 * Use this function to allocate target-private data structures,
	 * if needed by this HBA.  Add revised flow-control and queue
	 * properties for child here, if desired and if you can tell they
	 * support tagged queueing by now.
	 */

	glm_t			*glm;
	glm_unit_t		*unit;
	int			targ;
	int			lun;
	u_int			alloc_len;
	ddi_dma_attr_t		unit_dma_attrs;
	ddi_dma_handle_t	dma_handle;
	ddi_acc_handle_t	accessp;
	ddi_dma_cookie_t	cookie;
	u_int			ncookie;
	struct glm_dsa		*dsap;

	glm = SDEV2GLM(sd);

	mutex_enter(&glm->g_mutex);

	targ = sd->sd_address.a_target;
	lun = sd->sd_address.a_lun;

	if (targ < 0 || targ >= NTARGETS_WIDE ||
	    lun < 0 || lun >= NLUNS_PER_TARGET) {
		NDBG0(("%s%d: %s%d bad address <%d,%d>\n",
			ddi_get_name(hba_dip), ddi_get_instance(hba_dip),
			ddi_get_name(tgt_dip), ddi_get_instance(tgt_dip),
			targ, lun));
		mutex_exit(&glm->g_mutex);
		return (DDI_FAILURE);
	}

	/*
	 * Has this target already been initialized?
	 */
	if ((unit = NTL2UNITP(glm, targ, lun)) != NULL) {
		unit->nt_refcnt++;
		mutex_exit(&glm->g_mutex);
		return (DDI_SUCCESS);
	}

	/*
	 * dynamically create a customized dma attribute structure
	 * that describes the GLM's per-target structures.
	 */
	unit_dma_attrs.dma_attr_version		= DMA_ATTR_V0;
	unit_dma_attrs.dma_attr_addr_lo		= 0x0ull;
	unit_dma_attrs.dma_attr_addr_hi		= 0xffffffffull;
	unit_dma_attrs.dma_attr_count_max	= 0x00ffffffull;
	unit_dma_attrs.dma_attr_align		= 4;
	unit_dma_attrs.dma_attr_burstsizes	= 0x78;
	unit_dma_attrs.dma_attr_minxfer		= 1;
	unit_dma_attrs.dma_attr_maxxfer		= 0x00ffffffull;
	unit_dma_attrs.dma_attr_seg		= 0xffffffffull;
	unit_dma_attrs.dma_attr_sgllen		= 1;
	unit_dma_attrs.dma_attr_granular	= sizeof (struct glm_dsa);
	unit_dma_attrs.dma_attr_flags		= 0;

	/*
	 * allocate a per-target structure upon demand,
	 * in a platform-independent manner.
	 */
	if (ddi_dma_alloc_handle(glm->g_dip, &unit_dma_attrs,
	    DDI_DMA_SLEEP, NULL, &dma_handle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "(%d,%d): unable to allocate dma handle.",
		    targ, lun);
		mutex_exit(&glm->g_mutex);
		return (DDI_FAILURE);
	}

	if (ddi_dma_mem_alloc(dma_handle, sizeof (struct glm_dsa),
	    &dev_attr, DDI_DMA_CONSISTENT, DDI_DMA_SLEEP,
	    NULL, (caddr_t *)&dsap, &alloc_len, &accessp) != DDI_SUCCESS) {
		ddi_dma_free_handle(&dma_handle);
		cmn_err(CE_WARN,
		    "(%d,%d): unable to allocate per-target structure.",
			targ, lun);
		mutex_exit(&glm->g_mutex);
		return (DDI_FAILURE);
	}

	if (ddi_dma_addr_bind_handle(dma_handle, NULL, (caddr_t)dsap,
	    alloc_len, DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP,
	    NULL, &cookie, &ncookie) != DDI_DMA_MAPPED) {
		(void) ddi_dma_mem_free(&accessp);
		ddi_dma_free_handle(&dma_handle);
		cmn_err(CE_WARN, "(%d,%d): unable to bind DMA resources.",
		    targ, lun);
		mutex_exit(&glm->g_mutex);
		return (DDI_FAILURE);
	}

	unit = kmem_zalloc(sizeof (glm_unit_t), KM_SLEEP);
	ASSERT(unit != NULL);
	unit->nt_refcnt++;

	/* store pointer to per-target structure in HBA's array */
	NTL2UNITP(glm, targ, lun) = unit;

	unit->nt_dsap = dsap;
	unit->nt_dma_p = dma_handle;
	unit->nt_accessp = accessp;

	glm_table_init(glm, unit, cookie.dmac_address, targ, lun);

	mutex_exit(&glm->g_mutex);
	return (DDI_SUCCESS);
}

/*
 * tran_tgt_probe(9E) - target device probing
 */
static int
glm_scsi_tgt_probe(struct scsi_device *sd, int (*callback)())
{
	dev_info_t dip = ddi_get_parent(sd->sd_dev);
	int rval = SCSIPROBE_FAILURE;
	scsi_hba_tran_t *tran;
	struct glm *glm;
	int tgt = sd->sd_address.a_target;

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	ASSERT(tran != NULL);
	glm = TRAN2GLM(tran);

	/*
	 * renegotiate because not all targets will return a
	 * check condition on inquiry
	 */
	mutex_enter(&glm->g_mutex);
	glm_force_renegotiation(glm, tgt);
	mutex_exit(&glm->g_mutex);
	rval = scsi_hba_probe(sd, callback);

	/*
	 * the scsi-options precedence is:
	 *	target-scsi-options		highest
	 *	device-type-scsi-options
	 *	per bus scsi-options
	 *	global scsi-options		lowest
	 */
	mutex_enter(&glm->g_mutex);
	if ((rval == SCSIPROBE_EXISTS) &&
	    ((glm->g_target_scsi_options_defined & (1 << tgt)) == 0)) {
		int options;

		options = scsi_get_device_type_scsi_options(dip, sd, -1);
		if (options != -1) {
			glm->g_target_scsi_options[tgt] = options;
			glm_log(glm, CE_NOTE,
				"?target%x-scsi-options = 0x%x\n", tgt,
				glm->g_target_scsi_options[tgt]);
			glm_force_renegotiation(glm, tgt);
		}
	}
	mutex_exit(&glm->g_mutex);

	return (rval);
}

/*
 * tran_tgt_free(9E) - target device instance deallocation
 */
/*ARGSUSED*/
static void
glm_scsi_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	glm_t		*glm	= TRAN2GLM(hba_tran);
	int		targ	= sd->sd_address.a_target;
	int		lun	= sd->sd_address.a_lun;
	glm_unit_t *unit;

	mutex_enter(&glm->g_mutex);

	unit = NTL2UNITP(glm, targ, lun);
	ASSERT(unit && unit->nt_refcnt > 0);

	/*
	 * Decrement reference count to per-target info, and
	 * if we're finished with this target, release the
	 * per-target info.
	 */
	if (--(unit->nt_refcnt) == 0) {
		(void) ddi_dma_unbind_handle(unit->nt_dma_p);
		(void) ddi_dma_mem_free(&unit->nt_accessp);
		ddi_dma_free_handle(&unit->nt_dma_p);
		kmem_free(unit, sizeof (glm_unit_t));
		NTL2UNITP(glm, targ, lun) = NULL;
	}
	mutex_exit(&glm->g_mutex);
}

static int
glm_scsi_reset_notify(struct scsi_address *ap, int flag,
	void (*callback)(caddr_t), caddr_t arg)
{
	struct glm *glm = ADDR2GLM(ap);

	return (scsi_hba_reset_notify_setup(ap, flag, callback, arg,
		&glm->g_mutex, &glm->g_reset_notify_listf));
}

/*
 * Utility routine for glm_ifsetcap/ifgetcap
 */
/*ARGSUSED*/
static int
glm_capchk(char *cap, int tgtonly, int *cidxp)
{
	NDBG23(("glm_capchk\n"));

	if (!cap)
		return (FALSE);

	*cidxp = scsi_hba_lookup_capstr(cap);
	return (TRUE);
}

/*
 * set up the target's Scatter/Gather DMA list
 */
static void
glm_sg_setup(struct glm_unit *unit, ncmd_t *cmd)
{
	struct glm_dsa *dsap = unit->nt_dsap;
	uint_t cookiec;

	/*
	 * Save the number of entries in the DMA Scatter/Gather list
	 */
	dsap->nt_curdp.nd_left = cmd->cmd_cookiec;

	cookiec = cmd->cmd_cookiec;
	while (cookiec--) {
		(void) ddi_rep_put32(unit->nt_accessp,
		    (uint32_t *)&cmd->cmd_sg[cookiec],
			(uint32_t *)&dsap->nt_curdp.nd_data[cookiec],
			    2, DDI_DEV_AUTOINCR);
	}

	/*
	 * Save the data pointers for disconnects/reselects.
	 */
	unit->nt_savedp = dsap->nt_curdp;
	unit->nt_savedp.nd_left = cmd->cmd_cookiec;
}

/*
 * Save the scatter/gather current-index and number-completed
 * values so when the target reconnects we can restart the
 * data in/out move instruction at the proper point. Also, if the
 * disconnect happened within a segment there's a fixup value
 * for the partially completed data in/out move instruction.
 */
static void
glm_sg_update(glm_unit_t *unit, uchar_t index, ulong_t remain)
{
	glmti_t	*sgp;
	struct glm_dsa *dsap = unit->nt_dsap;

	/*
	 * Record the number of segments left to do.
	 */
	unit->nt_savedp.nd_left =
		dsap->nt_curdp.nd_left = index;

	/*
	 * If interrupted between segments then don't adjust S/G table
	 */
	if (remain == 0) {
		/*
		 * Must have just completed the current segment when
		 * the interrupt occurred, restart at the next segment.
		 */
		dsap->nt_curdp.nd_left--;
		unit->nt_savedp.nd_left--;
		return;
	}

	/*
	 * index is zero based, so to index into the
	 * scatter/gather list subtract one.
	 */
	index = GLM_MAX_DMA_SEGS - index;

	/*
	 * Fixup the Table Indirect entry for this segment.
	 */
	sgp = &dsap->nt_curdp.nd_data[index];

	sgp->address += (sgp->count - remain);
	sgp->count = remain;

	NDBG17(("glm_sg_update: remain=%d\n", remain));
	NDBG17(("Total number of bytes to transfer was %d\n", sgp->count));
	NDBG17(("at address=0x%x\n", sgp->address));
}

/*
 * Determine if the command completed with any bytes leftover
 * in the Scatter/Gather DMA list.
 */
static ulong_t
glm_sg_residual(glm_unit_t *unit)
{
	glmti_t	*sgp;
	ulong_t	 residual = 0;
	int	 index;

	/*
	 * Get the current index into the sg table.
	 */
	index = GLM_MAX_DMA_SEGS - unit->nt_dsap->nt_curdp.nd_left;

	NDBG17(("glm_sg_residual: index=%d\n", index));

	sgp = &unit->nt_dsap->nt_curdp.nd_data[index];

	for (; index < GLM_MAX_DMA_SEGS; index++, sgp++)
		residual += sgp->count;

	NDBG17(("glm_sg_residual: residual=%d\n", residual));

	return (residual);
}

static void
glm_queue_pkt(glm_t *glm, glm_unit_t *unit, ncmd_t *cmd)
{
	struct scsi_pkt *pkt = CMD2PKT(cmd);

	NDBG17(("glm_queue_pkt\n"));

	/*
	 * Add this pkt to the target's work queue
	 */
	if (pkt->pkt_flags & FLAG_HEAD) {
		glm_waitq_add_lifo(unit, cmd);
	} else {
		glm_waitq_add(unit, cmd);
	}

	/*
	 * If this target isn't active stick it on the hba's work queue
	 */
	if (unit->nt_state == NPT_STATE_DONE) {
		NDBG1(("glm_queue_pkt: done\n"));
		glm_queue_target(glm, unit);
	}
}

static int
glm_abort_ccb(struct scsi_address *ap, glm_t *glm, glm_unit_t *unit)
{
	struct scsi_pkt	*pktp;
	ncmd_t		*cmdp;
	int		rc;

	NDBG30(("glm_abort_ccb: state=%d\n", unit->nt_state));

	/* allocate a ccb */
	if ((pktp = scsi_hba_pkt_alloc(glm->g_dip, ap,
			0, 0, 0, 0, NULL, NULL)) == NULL) {
		NDBG30(("glm_abort_ccb: alloc\n"));
		return (FALSE);
	}

	cmdp = PKT2CMD(pktp);
	cmdp->cmd_pkt = (struct scsi_pkt *)pktp;
	pktp->pkt_scbp = (opaque_t)&cmdp->cmd_scb;
	cmdp->cmd_type = NRQ_ABORT;

	glm_queue_pkt(glm, unit, cmdp);
	glm_pollret(glm, cmdp);
	rc = (pktp->pkt_reason == CMD_CMPLT);
	scsi_hba_pkt_free(ap, pktp);

	NDBG30(("glm_abort_ccb: done\n"));
	return (rc);
}

static int
glm_abort_cmd(struct glm *glm, struct glm_unit *unit, struct glm_scsi_cmd *cmd)
{
	struct scsi_address ap;
	ap.a_hba_tran = glm->g_tran;
	ap.a_target = unit->nt_target;
	ap.a_lun = unit->nt_lun;

	/*
	 * If the current target is not the target passed in, and the
	 * target that timed out is currently disconnected with a
	 * disconnected cmd on it, try to reset that target.
	 *
	 * To get the Bus Device Reset msg out on the bus, we need
	 * to change the disk state back to DONE.  Otherwise the pkt
	 * will be queued, but never run.  The packet that timed out
	 * has already been mark as such.
	 */
	if ((glm->g_current && (glm->g_current != unit)) &&
	    (cmd->cmd_flags & CFLAG_CMDDISC) &&
	    (unit->nt_state == NPT_STATE_DISCONNECTED)) {
		unit->nt_state = NPT_STATE_DONE;
		glm->g_disc_num--;
		if (glm_do_scsi_reset(&ap, RESET_TARGET)) {
			return (TRUE);
		}
	}

	/*
	 * if the target won't listen, then a retry is useless
	 * there is also the possibility that the cmd still completed while
	 * we were trying to reset and the target driver may have done a
	 * device reset which has blown away this cmd.
	 * well, we've tried, now pull the chain
	 */
	return (glm_do_scsi_reset(&ap, RESET_ALL));
}

static int
glm_send_dev_reset(struct scsi_address *ap, glm_t *glm)
{
	glm_unit_t *unit;
	struct scsi_pkt *pktp;
	ncmd_t *cmdp;
	int rc;

	unit = ADDR2GLMUNITP(ap);

	/* allocate a pkt */
	if ((pktp = scsi_hba_pkt_alloc(glm->g_dip, ap,
			0, 0, 0, sizeof (ncmd_t), NULL, NULL)) == NULL) {
		NDBG30(("glm_send_dev_reset: alloc\n"));
		return (FALSE);
	}

	cmdp = PKT2CMD(pktp);
	cmdp->cmd_pkt = (struct scsi_pkt *)pktp;
	pktp->pkt_scbp	= (opaque_t)&cmdp->cmd_scb;
	cmdp->cmd_type = NRQ_DEV_RESET;

	/*
	 * Add it to the target's and hba's work queues
	 * maybe it should be added to the front of the target's
	 * queue and the target should be placed first on the
	 * hba's queue ???
	 */
	glm_queue_pkt(glm, unit, cmdp);

	glm_pollret(glm, cmdp);
	rc = (pktp->pkt_reason == CMD_CMPLT);
	scsi_hba_pkt_free(ap, pktp);
	return (rc);
}

/*
 * Called from glm_pollret when an interrupt is pending on the
 * HBA, or from the interrupt service routine glm_intr.
 * Read status back from your HBA, determining why the interrupt
 * happened.  If it's because of a completed command, update the
 * command packet that relates (you'll have some HBA-specific
 * information about tying the interrupt to the command, but it
 * will lead you back to the scsi_pkt that caused the command
 * processing and then the interrupt).
 * If the command has completed normally,
 *  1. set the SCSI status byte into *pktp->pkt_scbp
 *  2. set pktp->pkt_reason to an appropriate CMD_ value
 *  3. set pktp->pkt_resid to the amount of data not transferred
 *  4. set pktp->pkt_state's bits appropriately, according to the
 *	information you now have; things like bus arbitration,
 *	selection, command sent, data transfer, status back, ARQ
 *	done
 */
static void
glm_chkstatus(glm_t *glm, glm_unit_t *unit, struct glm_scsi_cmd *cmd)
{
	struct scsi_pkt *pkt = CMD2PKT(cmd);
	struct scsi_status *status;

	NDBG17(("glm_chkstatus: devaddr=0x%x\n", glm->g_devaddr));

	/*
	 * Get status from target.
	 */
	*pkt->pkt_scbp = unit->nt_dsap->nt_statbuf[0];
	status = (struct  scsi_status *)pkt->pkt_scbp;
	if (status->sts_chk) {
		glm_force_renegotiation(glm, unit->nt_target);
	}

	/*
	 * The active logical unit just completed an operation,
	 * pass the status back to the requestor.
	 */
	if (unit->nt_goterror) {
		/* interpret the error status */
		NDBG17(("glm_chkstatus: got error\n"));
		GLM_CHECK_ERROR(glm, unit, pkt);
	} else {
		NDBG17(("glm_chkstatus: okay\n"));

		/*
		 * Get residual byte count from the S/G DMA list.
		 * sync data for consistent memory xfers
		 */
		if (cmd->cmd_flags & CFLAG_DMAVALID) {
			if (cmd->cmd_flags & CFLAG_CMDIOPB &&
			    cmd->cmd_dmahandle != NULL) {
				(void) ddi_dma_sync(cmd->cmd_dmahandle, 0,
				    (uint_t)0, DDI_DMA_SYNC_FORCPU);
			}
			if (unit->nt_dsap->nt_curdp.nd_left != 0) {
				pkt->pkt_resid = glm_sg_residual(unit);
			} else {
				pkt->pkt_resid = 0;
			}
			pkt->pkt_state |= STATE_XFERRED_DATA;

			/*
			 * no data xfer.
			 */
			if (pkt->pkt_resid == cmd->cmd_dmacount) {
				pkt->pkt_state &= ~STATE_XFERRED_DATA;
			}
		}

		cmd->cmd_flags |= CFLAG_COMPLETED;
		/*
		 * XXX- Is there a more accurate way?
		 */
		pkt->pkt_state |= (STATE_GOT_BUS | STATE_GOT_TARGET
				| STATE_SENT_CMD
				| STATE_GOT_STATUS);
	}
	NDBG17(("glm_chkstatus: pkt=0x%x\n", pkt));
}

/*
 * Utility routine.  Poll for status of a command sent to HBA
 * without interrupts (a FLAG_NOINTR command).
 */
static void
glm_pollret(glm_t *glm, ncmd_t *poll_cmdp)
{
	ncmd_t *cmdp;
	ncmd_t *cmd_headp = NULL;
	ncmd_t **cmd_tailpp = &cmd_headp;
	int got_it = FALSE;

	NDBG17(("glm_pollret: cmdp=0x%x\n", poll_cmdp));

	/*
	 * Wait, using drv_usecwait(), long enough for the command to
	 * reasonably return from the target if the target isn't
	 * "dead".  A polled command may well be sent from scsi_poll, and
	 * there are retries built in to scsi_poll if the transport
	 * accepted the packet (TRAN_ACCEPT).  scsi_poll waits 1 second
	 * and retries the transport up to scsi_poll_busycnt times
	 * (currently 60) if
	 * 1. pkt_reason is CMD_INCOMPLETE and pkt_state is 0, or
	 * 2. pkt_reason is CMD_CMPLT and *pkt_scbp has STATUS_BUSY
	 */
	while (!got_it) {
		if (glm_wait_intr(glm) == FALSE) {
			NDBG17(("glm_pollret: command incomplete\n"));
			break;
		}

		/*
		 * requeue all completed packets on my local list
		 * until my polled packet returns
		 */
		while ((cmdp = glm_doneq_rm(glm)) != NULL) {
			/* if it's my packet, re-queue the rest and return */
			if (poll_cmdp == cmdp) {
				NDBG17(("glm_pollret: okay\n"));
				got_it = TRUE;
				continue;
			}

			/*
			 * requeue the other packets on my local list
			 * keep list in fifo order
			 */
			*cmd_tailpp = cmdp;
			cmd_tailpp = &cmdp->cmd_linkp;
			NDBG17(("glm_pollret: loop\n"));
		}
	}

	NDBG17(("glm_pollret: break\n"));

	if (!got_it) {
		NDBG17(("glm_pollret: got interrupt\n"));
		/* this isn't supposed to happen, the hba must be wedged */
		NDBG17(("glm_pollret: command incomplete\n"));
		if (poll_cmdp->cmd_queued == FALSE) {

			NDBG17(("glm_pollret: not on waitq\n"));

			/* it must be the active request */
			/* reset the bus and flush all ccb's */
			GLM_BUS_RESET(glm);

			/* set all targets on this hba to renegotiate syncio */
			glm_syncio_reset(glm, NULL);

			glm->g_wide_enabled = glm->g_wide_known = 0;

			/* try brute force to un-wedge the hba */
			glm_flush_hba(glm, TRUE, CMD_RESET, STATE_GOT_BUS,
								STAT_BUS_RESET);
			glm->g_state = NSTATE_IDLE;
		} else {
			glm_unit_t *unit = PKT2GLMUNITP(CMD2PKT(poll_cmdp));

			/* find and remove it from the waitq */
			NDBG17(("glm_pollret: delete from waitq\n"));
			glm_waitq_delete(unit, poll_cmdp);
		}

		CMD2PKT(poll_cmdp)->pkt_reason = CMD_INCOMPLETE;
		CMD2PKT(poll_cmdp)->pkt_state  = 0;
	}

	/* check for other completed packets that have been queued */
	if (cmd_headp) {
		register struct scsi_pkt *pktp;
		while ((cmdp = cmd_headp) != NULL) {
			cmd_headp = cmdp->cmd_linkp;
			pktp = CMD2PKT(cmdp);
			mutex_exit(&glm->g_mutex);
			(*pktp->pkt_comp)(pktp);
			mutex_enter(&glm->g_mutex);
		}
	}
	NDBG17(("glm_pollret: done\n"));
}

static void
glm_flush_lun(glm_t *glm, glm_unit_t *unit, uchar_t flush_all,
    uchar_t pkt_reason, u_long pkt_state, u_long pkt_statistics)
{
	ncmd_t *ncmdp;

	NDBG17(("glm_flush_lun: %d\n", flush_all));

	/*
	 * post the completion status, and put the ccb on the
	 * doneq to schedule the call of the completion function
	 */
	if ((ncmdp = unit->nt_ncmdp) != NULL) {
		unit->nt_ncmdp = NULL;
		glm_set_done(glm, unit, ncmdp,
		    pkt_reason, pkt_state, pkt_statistics);
	}

	if (flush_all) {
		/* flush all the queued ccb's and then mark the target idle */
		NDBG17(("glm_flush_lun: loop "));
		while ((ncmdp = glm_waitq_rm(unit)) != NULL) {
			NDBG17(("#"));
			glm_set_done(glm, unit, ncmdp, pkt_reason,
				pkt_state, pkt_statistics);
		}
		unit->nt_state = NPT_STATE_DONE;
		NDBG17(("\nglm_flush_lun: flush all done\n"));
		return;
	}
	if (unit->nt_waitq != NULL) {
		/* requeue this target on the hba's work queue */
		NDBG17(("glm_flush_lun: waitq not null\n"));
		unit->nt_state = NPT_STATE_QUEUED;
		glm_addbq(glm, unit);
		return;
	}

	unit->nt_state = NPT_STATE_DONE;
	NDBG17(("\nglm_flush_lun: done\n"));
}


/*
 * Flush all the disconnected requests for all the LUNs on
 * the specified target device.
 */
static void
glm_flush_target(glm_t *glm, ushort target, uchar_t flush_all,
    uchar_t pkt_reason, u_long pkt_state, u_long pkt_statistics)
{
	glm_unit_t *unit;
	ushort	 lun;

	/* completed Bus Device Reset, clean up disconnected LUNs */
	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		unit = NTL2UNITP(glm, target, lun);
		if (unit == NULL) {
			NDBG17(("glm_flush_target: null\n"));
			continue;
		}
		if (unit->nt_state != NPT_STATE_DISCONNECTED) {
			NDBG17(("glm_flush_target: no disco\n"));
			continue;
		}
		glm_flush_lun(glm, unit, flush_all, pkt_reason, pkt_state,
			pkt_statistics);

		glm->g_disc_num--;
	}
	NDBG17(("glm_flush_target: done\n"));
}

/*
 * Called after a SCSI Bus Reset to find and flush all the outstanding
 * scsi requests for any devices which disconnected and haven't
 * yet reconnected. Also called to flush everything if we're resetting
 * the driver.
 */
static void
glm_flush_hba(glm_t *glm, uchar_t flush_all, uchar_t pkt_reason,
    u_long pkt_state, u_long pkt_statistics)
{
	glm_unit_t **unitp;
	glm_unit_t *unit;
	int cnt;

	NDBG17(("glm_flush_hba: 0x%x %d\n", glm, flush_all));

	if (flush_all) {
		NDBG17(("glm_flush_hba: all\n"));

		/* first flush the currently active request if any */
		if ((unit = glm->g_current) != NULL &&
		    unit->nt_state == NPT_STATE_ACTIVE) {
			NDBG17(("glm_flush_hba: current unit=0x%x\n", unit));
			glm->g_current = NULL;
			glm_flush_lun(glm, unit, flush_all, pkt_reason,
						pkt_state, pkt_statistics);
		}

		/* next, all the queued devices waiting for this hba */
		while ((unit = glm_rmq(glm)) != NULL) {
			NDBG17(("glm_flush_hba: queued unit=0x%x\n", unit));
			glm_flush_lun(glm, unit, flush_all, pkt_reason,
				pkt_state, pkt_statistics);
		}
	}

	/* finally (or just) flush all the disconnected target, luns */
	unitp = &glm->g_units[0];
	for (cnt = 0; cnt < N_GLM_UNITS; cnt++) {
		/* skip it if device is not in use */
		if ((unit = *unitp++) == NULL) {
			NDBG17(("glm_flush_hba: null\n"));
			continue;
		}
		/* skip it if it's the wrong state */
		if (unit->nt_state != NPT_STATE_DISCONNECTED) {
			NDBG17(("glm_flush_hba: no disco\n"));
			continue;
		}
		glm_flush_lun(glm, unit, flush_all, pkt_reason,
			pkt_state, pkt_statistics);
		glm->g_disc_num--;
		ASSERT(unit->nt_state == NPT_STATE_DONE);
	}

	ASSERT(glm->g_disc_num == 0);

	/*
	 * perform the reset notification callbacks that are registered.
	 */
	(void) scsi_hba_reset_notify_callback(&glm->g_mutex,
		&glm->g_reset_notify_listf);

	NDBG17(("glm_flush_hba: done\n"));
}

/*ARGSUSED*/
static void
glm_set_done(glm_t *glm, glm_unit_t *unit, ncmd_t *cmdp, uchar_t pkt_reason,
    u_long pkt_state, u_long pkt_statistics)
{
	struct scsi_pkt	*pktp = CMD2PKT(cmdp);

	pktp->pkt_reason = pkt_reason;
	pktp->pkt_state |= pkt_state;
	pktp->pkt_statistics |= pkt_statistics;
	glm_doneq_add(glm, cmdp);
}

static void
glm_process_intr(glm_t *glm, uchar_t istat)
{
	ulong_t		action = 0;

	NDBG16(("glm_process_intr: n_state=0x%x istat=0x%x\n",
	    glm->g_state, istat));

	/*
	 * Always clear sigp bit if it might be set
	 */
	if (glm->g_state == NSTATE_WAIT_RESEL)
		GLM_RESET_SIGP(glm);

	/*
	 * Analyze DMA interrupts
	 */
	if (istat & NB_ISTAT_DIP)
		action |= GLM_DMA_STATUS(glm);

	/*
	 * Analyze SCSI errors and check for phase mismatch
	 */
	if (istat & NB_ISTAT_SIP)
		action |= GLM_SCSI_STATUS(glm);

	/*
	 * If no errors, no action, just restart the HBA
	 */
	if (action != 0) {
		action = glm_decide(glm, action);
	}

	/*
	 * Restart the current, or start a new, queue item
	 */
	glm_restart_hba(glm, action);
}


/*
 * Some event or combination of events has occurred. Decide which
 * one takes precedence and do the appropiate HBA function and then
 * the appropiate end of request function.
 */
static ulong_t
glm_decide(glm_t *glm, ulong_t action)
{
	glm_unit_t	*unit;

	/*
	 * If multiple errors occurred do the action for
	 * the most severe error.
	 */

	unit = glm->g_current;

	if (action & NACTION_CHK_INTCODE) {
		action = glm_check_intcode(glm, unit, action);
	}

	/* if sync i/o negotiation in progress, determine new syncio state */
	if (glm->g_state == NSTATE_ACTIVE &&
	    (action & (NACTION_GOT_BUS_RESET | NACTION_DO_BUS_RESET)) == 0) {
		if (action & NACTION_SDTR) {
			action = glm_syncio_decide(glm, unit, action);
		}
	}

	if (action & NACTION_GOT_BUS_RESET) {
		/*
		 * clear all requests waiting for reconnection.
		 */
		glm_flush_hba(glm, TRUE, CMD_RESET,
		    STATE_GOT_BUS, STAT_BUS_RESET);

		glm->g_wide_known = glm->g_wide_enabled = 0;
		glm_syncio_reset(glm, NULL);

		/*
		 * Now mark the hba as idle.
		 */
		glm->g_state = NSTATE_IDLE;

		/*
		 * Bus settle time.
		 */
		drv_usecwait(250000);
	}

	if (action & NACTION_SIOP_REINIT) {
		GLM_RESET(glm);
		GLM_INIT(glm);
		GLM_ENABLE_INTR(glm);
		/* the reset clears the byte counters so can't do save */
		action &= ~NACTION_SAVE_BCNT;
		NDBG1(("glm: HBA reset: devaddr=0x%x\n", glm->g_devaddr));
	}

	if (action & NACTION_CLEAR_CHIP) {
		/* Reset scsi offset. */
		RESET_SCSI_OFFSET(glm);

		/* Clear the DMA FIFO pointers */
		CLEAR_DMA(glm);

		/* Clear the SCSI FIFO pointers */
		CLEAR_SCSI_FIFO(glm);
	}

	if (action & NACTION_SIOP_HALT) {
		GLM_HALT(glm);
		NDBG1(("glm: HBA halt: devaddr=0x%x\n", glm->g_devaddr));
	}

	if (action & NACTION_DO_BUS_RESET) {
		GLM_BUS_RESET(glm);
		(void) glm_wait_intr(glm);

		/* clear invalid actions, if any */
		action &= NACTION_DONE | NACTION_ERR | NACTION_DO_BUS_RESET |
		    NACTION_BUS_FREE;

		NDBG1(("glm: bus reset: devaddr=0x%x\n", glm->g_devaddr));
	}

	if (action & NACTION_SAVE_BCNT) {
		/*
		 * Save the state of the data transfer scatter/gather
		 * for possible later reselect/reconnect.
		 */
		if (!GLM_SAVE_BYTE_COUNT(glm, unit)) {
			/* if this isn't an interrupt during a S/G dma */
			/* then the target changed phase when it shouldn't */
			NDBG1(("glm_decide: phase mismatch: devaddr=0x%x\n",
			    glm->g_devaddr));
		}
	}

	/*
	 * Check to see if the current request has completed.
	 * If the HBA isn't active it can't be done, we're probably
	 * just waiting for reselection and now need to reconnect to
	 * a new target.
	 */
	if (glm->g_state == NSTATE_ACTIVE) {
		action = glm_ccb_decide(glm, unit, action);
	}
	return (action);
}

static ulong_t
glm_ccb_decide(glm_t *glm, glm_unit_t *unit, ulong_t action)
{
	ncmd_t *cmd;

	if (action & NACTION_ERR) {
		/* error completion, save all the errors seen for later */
		unit->nt_goterror = TRUE;
	} else if ((action & NACTION_DONE) == 0) {
		/* the target's state hasn't changed */
		return (action);
	}

	/* detach this target from the hba */
	glm->g_current = NULL;
	glm->g_state = NSTATE_IDLE;

	/* if this target has more requests then requeue it fifo order */
	if (unit->nt_waitq != NULL) {
		unit->nt_state = NPT_STATE_QUEUED;
		glm_addbq(glm, unit);
	} else {
		unit->nt_state = NPT_STATE_DONE;
	}

	/* if no active request then just return */
	if ((cmd = unit->nt_ncmdp) == NULL) {
		NDBG1(("glm_ccb_decide: no active pkt.\n"));
		return (action);
	}

	/* decouple the request from the target */
	unit->nt_ncmdp = NULL;

	/* post the completion status into the scsi packet */
	ASSERT(cmd != NULL && unit != NULL);
	glm_chkstatus(glm, unit, cmd);

	/* add the completed request to end of the done queue */
	glm_doneq_add(glm, cmd);

	NDBG1(("glm_ccb_decide: end.\n"));
	return (action);
}

/*ARGSUSED*/
static void
glm_watch(caddr_t arg)
{
	struct glm *glm;

	for (glm = glm_head; glm != (struct glm *)NULL; glm = glm->g_next) {
		mutex_enter(&glm->g_mutex);

		/*
		 * For now, always call glm_watchsubr.
		 */
		glm_watchsubr(glm);

		if (glm->g_props_update) {
			int i;
			for (i = 0; i < NTARGETS_WIDE; i++) {
				if (glm->g_props_update & (1<<i)) {
					glm_update_props(glm, i);
				}
			}
			glm->g_props_update = 0;
		}
		mutex_exit(&glm->g_mutex);
	}
	(void) timeout(glm_watch, (caddr_t)0, glm_tick);
}

static void
glm_watchsubr(register struct glm *glm)
{
	struct glm_unit **unitp, *unit;
	int cnt;

	unitp = &glm->g_units[0];
	for (cnt = 0; cnt < N_GLM_UNITS; cnt++) {
		if ((unit = *unitp++) == NULL) {
			continue;
		}
		if (unit->nt_ncmdp != NULL) {
			unit->nt_ncmdp->cmd_time -= glm_scsi_watchdog_tick;
			if (unit->nt_ncmdp->cmd_time <= 0) {
				glm_cmd_timeout(glm, unit);
				return;
			}
		}
	}
}

/*
 * timeout recovery
 */
static void
glm_cmd_timeout(struct glm *glm, struct glm_unit *unit)
{
	int i;
	struct glm_scsi_cmd *cmd;

	/*
	 * If the scripts processor is active and there is no interrupt
	 * pending for next second then the current cmd must be stuck;
	 * switch to current unit and cmd.
	 */
	if (glm->g_state == NSTATE_ACTIVE) {
		for (i = 0; (i < 10000) && (INTPENDING(glm) == 0); i++) {
			drv_usecwait(100);
		}
		if (INTPENDING(glm) == 0) {
			ASSERT(glm->g_current != NULL);
			unit = glm->g_current;
		}
	}

	ASSERT(unit->nt_ncmdp != NULL);
	cmd = unit->nt_ncmdp;

	/*
	 * Mark this cmd as a timeout.
	 */
	glm_set_done(glm, unit, cmd, CMD_TIMEOUT,
	    (STATE_GOT_BUS|STATE_GOT_TARGET|STATE_SENT_CMD),
		(STAT_TIMEOUT|STAT_ABORTED));
	unit->nt_ncmdp = NULL;

	/*
	 * dump all we know about this timeout
	 */
	if (cmd->cmd_flags & CFLAG_CMDDISC) {
		ASSERT(unit != glm->g_current);
		ASSERT(unit->nt_state == NPT_STATE_DISCONNECTED);
		glm_log(glm, CE_WARN,
		    "Disconnected command timeout for Target %d.%d",
			unit->nt_target, unit->nt_lun);
	} else {
		ASSERT(unit == glm->g_current);
		glm_log(glm, CE_WARN,
		    "Connected command timeout for Target %d.%d",
			unit->nt_target, unit->nt_lun);
		/*
		 * connected cmd timeout are usually due to noisy buses.
		 */
		glm_sync_wide_backoff(glm, unit);
	}
	glm_abort_cmd(glm, unit, cmd);
}

static void
glm_sync_wide_backoff(struct glm *glm, struct glm_unit *unit)
{
	uchar_t target = unit->nt_target;
	ushort_t tshift = (1<<target);

	/*
	 * if this not the first time then disable wide.
	 */
	if (glm->g_backoff & tshift) {
		if ((glm->g_nowide & tshift) == 0) {
			glm_log(glm, CE_WARN,
			    "Target %d disabled wide SCSI mode",
				target);
		}
		/*
		 * do not reset the bit in g_nowide because that
		 * would not force a renegotiation of wide
		 * and do not change any register value yet because
		 * we may have reconnects before the renegotiations
		 */
		glm->g_target_scsi_options[target] &= ~SCSI_OPTIONS_WIDE;
	}

	if ((unit->nt_dsap->nt_selectparm.nt_sxfer & 0x1f) != 0) {
		if (glm->g_backoff & tshift &&
		    (unit->nt_dsap->nt_selectparm.nt_sxfer & 0x1f)) {
			glm_log(glm, CE_WARN,
			    "Target %d reverting to async. mode", target);
			glm->g_target_scsi_options[target] &=
				~(SCSI_OPTIONS_SYNC | SCSI_OPTIONS_FAST);
		} else {
			int period = glm->g_minperiod[target];

			/*
			 * backoff sync 100%.
			 */
			period = (period * 2);

			/*
			 * Backing off sync on slow devices when the 875
			 * is using FAST-20 timings can generate sync
			 * periods that are greater than our max sync.
			 * Adjust up to our max sync.
			 */
			if (period > MAX_SYNC_PERIOD(glm)) {
				period = MAX_SYNC_PERIOD(glm);
			}

			period = glm_period_round(glm, period);

			glm->g_backoff |= tshift;

			glm->g_target_scsi_options[target] &=
				~SCSI_OPTIONS_FAST20;

			glm->g_minperiod[target] = period;

			glm_log(glm, CE_WARN,
			    "Target %d reducing sync. transfer rate", target);
		}
	}
	glm->g_props_update |= (1<<target);
	glm_force_renegotiation(glm, target);
}

static void
glm_force_renegotiation(struct glm *glm, int target)
{
	register ushort_t tshift = (1<<target);

	if (glm->g_syncstate[target] != NSYNC_SDTR_REJECT) {
		glm->g_syncstate[target] = NSYNC_SDTR_NOTDONE;
	}
	glm->g_wide_known &= ~tshift;
	glm->g_wide_enabled &= ~tshift;
}

static int
glm_wait_intr(glm_t *glm)
{
	int cnt;
	uchar_t	istat;

	istat = GLM_GET_ISTAT(glm);

	/* keep trying for at least 60 seconds */
	for (cnt = 0; cnt < 600000; cnt += 1) {
		/* loop 600,000 times but wait at least 100 microseconds */
		/* each time around the loop */
		if (istat & (NB_ISTAT_DIP | NB_ISTAT_SIP)) {
			glm->g_polled_intr = 1;
			NDBG17(("glm_wait_intr: istat=0x%x\n", istat));
			/* process this interrupt */
			glm_process_intr(glm, istat);
			return (TRUE);
		}
		drv_usecwait(100);
		istat = GLM_GET_ISTAT(glm);
	}
	NDBG17(("glm_wait_intr: FAILED with istat=0x%x\n", istat));
	return (FALSE);
}

/*
 * glm interrupt handler
 *
 * Read the istat register first.  Check to see if a scsi interrupt
 * or dma interrupt is pending.  If that is true, handle those conditions
 * else, return DDI_INTR_UNCLAIMED.
 */
static uint_t
glm_intr(caddr_t arg)
{
	glm_t *glm = (glm_t *)arg;
	ncmd_t *cmd;
	uchar_t istat;
	register struct scsi_pkt *pkt;

	mutex_enter(&glm->g_mutex);

	/*
	 * Read the istat register.
	 */
	if ((istat = INTPENDING(glm)) == 0) {
		if (glm->g_polled_intr) {
			glm->g_polled_intr = 0;
			mutex_exit(&glm->g_mutex);
			return (DDI_INTR_CLAIMED);
		}
		mutex_exit(&glm->g_mutex);
		return (DDI_INTR_UNCLAIMED);
	}

	do {
		/*
		 * clear the next interrupt status from the hardware
		 */
		glm_process_intr(glm, istat);

		/*
		 * run the completion routines of all the completed commands
		 */
		while ((cmd = glm_doneq_rm(glm)) != NULL) {
			/* run this command's completion routine */
			pkt = CMD2PKT(cmd);
			mutex_exit(&glm->g_mutex);
			(*pkt->pkt_comp)(pkt);
			mutex_enter(&glm->g_mutex);
		}
	} while ((istat = INTPENDING(glm)) != 0);

	NDBG17(("glm_intr complete\n"));
	mutex_exit(&glm->g_mutex);
	return (DDI_INTR_CLAIMED);
}

static int
glm_setup_npt(glm_t *glm, glm_unit_t *unit, ncmd_t *cmd)
{
	struct glm_dsa	*dsap;
	struct scsi_pkt *pktp = CMD2PKT(cmd);
	ushort_t tshift = (1<<unit->nt_target);

	NDBG17(("glm_setup_npt: unit=0x%x\n", unit));

	dsap = unit->nt_dsap;

	unit->nt_type = cmd->cmd_type;
	unit->nt_goterror = FALSE;
	unit->nt_dma_status = 0;
	unit->nt_status0 = 0;
	unit->nt_status1 = 0;
	glm->g_wdtr_sent = 0;

	dsap->nt_statbuf[0] = 0;
	dsap->nt_errmsgbuf[0] = (uchar_t)MSG_NOP;

	switch (unit->nt_type) {
	case NRQ_NORMAL_CMD:

		NDBG17(("glm_setup_npt: normal\n"));

		/*
		 * check to see if target is allowed to disconnect
		 */
		if (pktp->pkt_flags & FLAG_NODISCON) {
			dsap->nt_msgoutbuf[0] = (MSG_IDENTIFY | unit->nt_lun);
		} else {
			dsap->nt_msgoutbuf[0] =
			    (MSG_DR_IDENTIFY | unit->nt_lun);
		}

		/*
		 * Single identify msg.
		 */
		dsap->nt_sendmsg.count = 1;

		/*
		 * save the cdb length.
		 */
		dsap->nt_cmd.count = cmd->cmd_cdblen;

		/*
		 * Copy the CDB to our DSA structure for table
		 * indirect scripts access.
		 */
		(void) ddi_rep_put8(unit->nt_accessp, (uint8_t *)pktp->pkt_cdbp,
		    dsap->nt_cdb, cmd->cmd_cdblen, DDI_DEV_AUTOINCR);

		/*
		 * setup the Scatter/Gather DMA list for this request
		 */
		if (cmd->cmd_cookiec > 0) {
			ASSERT(cmd->cmd_flags & CFLAG_DMAVALID);
			glm_sg_setup(unit, cmd);
		}

		if (((glm->g_wide_known | glm->g_nowide) & tshift) == 0) {
			glm_make_wdtr(glm, unit, GLM_XFER_WIDTH);
		} else if (NSYNCSTATE(glm, unit) == NSYNC_SDTR_NOTDONE) {
			NDBG31(("glm_setup_npt: try syncio\n"));
			/* haven't yet tried syncio on this target */
			glm_syncio_msg_init(glm, unit);
			glm->g_syncstate[unit->nt_target] = NSYNC_SDTR_SENT;
		}

		/*
		 * Start timeout.
		 */
		cmd->cmd_time = pktp->pkt_time;

		return (NSS_STARTUP);

	case NRQ_DEV_RESET:
		NDBG31(("glm_setup_npt: bus device reset\n"));
		/* reset the msg out length for single message byte */
		dsap->nt_msgoutbuf[0] = MSG_DEVICE_RESET;
		dsap->nt_sendmsg.count = 1;

		/* no command buffer */
		dsap->nt_cmd.count = 0;

		return (NSS_BUS_DEV_RESET);

	case NRQ_ABORT:
		NDBG31(("glm_setup_npt: abort\n"));
		/* reset the msg out length for two single */
		/* byte messages */
		dsap->nt_msgoutbuf[0] = MSG_IDENTIFY | unit->nt_lun;
		dsap->nt_msgoutbuf[1] = MSG_ABORT;
		dsap->nt_sendmsg.count = 2;

		/* no command buffer */
		dsap->nt_cmd.count = 0;
		return (NSS_BUS_DEV_RESET);

	default:
		cmn_err(CE_PANIC,
		    "glm: invalid queue entry cmd=0x%x", (int)cmd);
		/* NOTREACHED */
	}
}

/*
 * start a fresh request from the top of the device queue
 */
static void
glm_start_next(struct glm *glm)
{
	glm_unit_t *unit;
	struct glm_scsi_cmd *cmd;
	int script_type;

	NDBG31(("glm_start_next: glm=0x%x\n", glm));

	if ((unit = glm_rmq(glm)) == NULL) {
		/* no devs waiting for the hba, wait for disco-ed dev */
		glm_wait_for_reselect(glm, 0);
		return;
	}

	if ((cmd = glm_waitq_rm(unit)) == NULL) {
		/* the request queue is empty, wait for disconnected devs */
		glm_wait_for_reselect(glm, 0);
		return;
	}

	/* attach this target to the hba and make it active */
	glm->g_current = unit;

	unit->nt_ncmdp = cmd;
	glm->g_state = NSTATE_ACTIVE;
	unit->nt_state = NPT_STATE_ACTIVE;

	script_type = glm_setup_npt(glm, unit, cmd);

	GLM_SETUP_SCRIPT(glm, unit, GLM_SELECTION);
	GLM_START_SCRIPT(glm, script_type);
}

static void
glm_wait_for_reselect(glm_t *glm, ulong_t action)
{
	glm_unit_t	*unit = glm->g_hbap;
	struct glm_dsa	*dsap;

	dsap = unit->nt_dsap;

	glm->g_current = unit;
	glm->g_state = NSTATE_WAIT_RESEL;
	dsap->nt_errmsgbuf[0] = (uchar_t)MSG_NOP;

	action &= NACTION_ABORT | NACTION_MSG_REJECT | NACTION_MSG_PARITY |
			NACTION_INITIATOR_ERROR;

	if (action == 0 && glm->g_disc_num != 0) {
		/* wait for any disconnected targets */
		GLM_SETUP_SCRIPT(glm, unit, GLM_SELECTION);
		GLM_START_SCRIPT(glm, NSS_WAIT4RESELECT);
		NDBG19(("glm_wait_for_reselect: WAIT\n"));
		return;
	}

	if (action & NACTION_ABORT) {
		/* abort an invalid reconnect */
		dsap->nt_errmsgbuf[0] = (uchar_t)MSG_ABORT;
		GLM_START_SCRIPT(glm, NSS_ABORT);
		return;
	}

	if (action & NACTION_MSG_REJECT) {
		/* target sent me bad msg, send msg reject */
		dsap->nt_errmsgbuf[0] = (uchar_t)MSG_REJECT;
		GLM_START_SCRIPT(glm, NSS_ERR_MSG);
		NDBG19(("glm_wait_for_reselect: Message Reject\n"));
		return;
	}

	if (action & NACTION_MSG_PARITY) {
		/* got a parity error during message in phase */
		dsap->nt_errmsgbuf[0] = (uchar_t)MSG_MSG_PARITY;
		GLM_START_SCRIPT(glm, NSS_ERR_MSG);
		NDBG19(("glm_wait_for_reselect: Message Parity Error\n"));
		return;
	}

	if (action & NACTION_INITIATOR_ERROR) {
		/* catchall for other errors */
		dsap->nt_errmsgbuf[0] = (uchar_t)MSG_INITIATOR_ERROR;
		GLM_START_SCRIPT(glm, NSS_ERR_MSG);
		NDBG19(("glm_wait_for_reselect: Initiator Detected Error\n"));
		return;
	}

	/* no disconnected targets go idle */
	glm->g_current = NULL;
	glm->g_state = NSTATE_IDLE;
	NDBG19(("glm_wait_for_reselect: IDLE\n"));
}

/*
 * How the hba continues depends on whether sync i/o
 * negotiation was in progress and if so how far along.
 * Or there might be an error message to be sent out.
 */
static void
glm_restart_current(glm_t *glm, ulong_t action)
{
	glm_unit_t	*unit = glm->g_current;
	struct glm_dsa	*dsap;

	if (unit == NULL) {
		/* the current request just finished, do the next one */
		glm_start_next(glm);
		return;
	}

	dsap = unit->nt_dsap;

	/* Determine how to get the device at the top of the queue restarted */
	dsap->nt_errmsgbuf[0] = (uchar_t)MSG_NOP;

	switch (unit->nt_state) {
	case NPT_STATE_ACTIVE:
		NDBG19(("glm_restart_current: active\n"));

		action &= NACTION_ACK | NACTION_EXT_MSG_OUT |
			    NACTION_MSG_REJECT | NACTION_MSG_PARITY |
			    NACTION_INITIATOR_ERROR;

		if (action == 0) {
			/* continue the script on the currently active target */
			GLM_START_SCRIPT(glm, NSS_CONTINUE);
			break;
		}

		if (action & NACTION_ACK) {
			/* just ack the last byte and continue */
			GLM_START_SCRIPT(glm, NSS_CLEAR_ACK);
			break;
		}

		if (action & NACTION_EXT_MSG_OUT) {
			/* send my SDTR message */
			GLM_START_SCRIPT(glm, NSS_EXT_MSG_OUT);
			break;
		}

		if (action & NACTION_MSG_REJECT) {
			/* target sent me bad msg, send msg reject */
			dsap->nt_errmsgbuf[0] = (uchar_t)MSG_REJECT;
			GLM_START_SCRIPT(glm, NSS_ERR_MSG);
			break;
		}

		if (action & NACTION_MSG_PARITY) {
			/* got a parity error during message in phase */
			dsap->nt_errmsgbuf[0] = (uchar_t)MSG_MSG_PARITY;
			GLM_START_SCRIPT(glm, NSS_ERR_MSG);
			break;
		}

		if (action & NACTION_INITIATOR_ERROR) {
			/* catchall for other errors */
			dsap->nt_errmsgbuf[0] = (uchar_t)
				MSG_INITIATOR_ERROR;
			GLM_START_SCRIPT(glm, NSS_ERR_MSG);
		}
		break;

	case NPT_STATE_DISCONNECTED:
		NDBG19(("glm_restart_current: disconnected\n"));
		/*
		 * a target wants to reconnect so make
		 * it the currently active target
		 */
		GLM_SETUP_SCRIPT(glm, unit, GLM_RESELECTION);
		GLM_START_SCRIPT(glm, NSS_CLEAR_ACK);
		break;

	default:
		glm_log(glm, CE_WARN, "glm_restart_current: invalid state %d",
		    unit->nt_state);
		return;
	}
	unit->nt_state = NPT_STATE_ACTIVE;
	NDBG19(("glm_restart_current: okay\n"));
}

static void
glm_restart_hba(glm_t *glm, ulong_t action)
{
	NDBG19(("glm_restart_hba\n"));

	/*
	 * run the target at the front of the queue unless we're
	 * just waiting for a reconnect. In which case just use
	 * the first target's data structure since it's handy.
	 */
	switch (glm->g_state) {
	case NSTATE_ACTIVE:
		NDBG19(("glm_restart_hba: ACTIVE\n"));
		glm_restart_current(glm, action);
		break;

	case NSTATE_WAIT_RESEL:
		NDBG19(("glm_restart_hba: WAIT\n"));
		glm_wait_for_reselect(glm, action);
		break;

	case NSTATE_IDLE:
		NDBG19(("glm_restart_hba: IDLE\n"));
		/* start whatever's on the top of the queue */
		glm_start_next(glm);
		break;
	}
}

static void
glm_queue_target(glm_t *glm, glm_unit_t *unit)
{
	NDBG1(("glm_queue_target\n"));

	unit->nt_state = NPT_STATE_QUEUED;
	glm_addbq(glm, unit);

	switch (glm->g_state) {
	case NSTATE_IDLE:
		/* the device is idle, start first queue entry now */
		glm_restart_hba(glm, 0);
		break;
	case NSTATE_ACTIVE:
		/* queue the target and return without doing anything */
		break;
	case NSTATE_WAIT_RESEL:
		/*
		 * If we're waiting for reselection of a disconnected target
		 * then set the Signal Process bit in the ISTAT register and
		 * return. The interrupt routine restarts the queue.
		 */
		GLM_SET_SIGP(glm);
		break;
	}
}

static glm_unit_t *
glm_get_target(glm_t *glm)
{
	uchar_t target, lun;

	/*
	 * Get the LUN from the IDENTIFY message byte
	 */
	lun = glm->g_hbap->nt_dsap->nt_msginbuf[0];

	if (IS_IDENTIFY_MSG(lun) == FALSE)
		return (NULL);

	lun = lun & MSG_LUNRTN;

	/*
	 * Get the target from the HBA's id register
	 */
	if (GLM_GET_TARGET(glm, &target))
		return (NTL2UNITP(glm, target, lun));

	return (NULL);
}

static ulong_t
glm_check_intcode(glm_t *glm, glm_unit_t *unit, ulong_t action)
{
	struct glm_scsi_cmd *cmd;
	struct scsi_pkt *pkt;
	glm_unit_t *re_unit;
	ulong_t intcode;
	char *errmsg;
	uchar_t width;

	if (action & (NACTION_GOT_BUS_RESET | NACTION_GOT_BUS_RESET
			| NACTION_SIOP_HALT
			| NACTION_SIOP_REINIT | NACTION_BUS_FREE
			| NACTION_DONE | NACTION_ERR)) {
		return (action);
	}

	/* SCRIPT interrupt instruction */
	/* Get the interrupt vector number */
	intcode = GLM_GET_INTCODE(glm);

	NDBG1(("glm_check_intcode: start.  intcode0x%x\n", intcode));

	switch (intcode) {
	default:
		break;

	case NINT_OK:
		return (NACTION_DONE | action);

	case NINT_SDP_MSG:
		/* Save Data Pointers msg */
		NDBG1(("\n\nintcode SDP\n\n"));
		unit->nt_savedp = unit->nt_dsap->nt_curdp;
		unit->nt_savedp.nd_left = unit->nt_dsap->nt_curdp.nd_left;
		return (NACTION_ACK | action);

	case NINT_DISC:
		/* remove this target from the top of queue */
		NDBG1(("\n\nintcode DISC\n\n"));
		glm->g_disc_num++;
		unit->nt_state = NPT_STATE_DISCONNECTED;
		glm->g_state = NSTATE_IDLE;
		cmd = glm->g_current->nt_ncmdp;
		pkt = CMD2PKT(cmd);
		cmd->cmd_flags |= CFLAG_CMDDISC;
		pkt->pkt_statistics |= STAT_DISCON;
		glm->g_current = NULL;
		return (action);

	case NINT_RP_MSG:
		/* Restore Data Pointers */
		NDBG1(("\n\nintcode RP\n\n"));
		unit->nt_dsap->nt_curdp = unit->nt_savedp;
		unit->nt_dsap->nt_curdp.nd_left = unit->nt_savedp.nd_left;
		return (NACTION_ACK | action);

	case NINT_RESEL:
		/* reselected by a disconnected target */
		NDBG1(("\n\nintcode RESEL\n\n"));
		/*
		 * One of two situations:
		 */
		switch (glm->g_state) {
		case NSTATE_ACTIVE:
			/*
			 * Reselection during select. Leave the target glm
			 * was trying to activate on the top of the queue.
			 * If glm was trying to initiated a sdtr and got
			 * preempted, be sure to set state back to NOTDONE.
			 */
			if (glm->g_syncstate[unit->nt_target] ==
							NSYNC_SDTR_SENT) {
				glm->g_syncstate[unit->nt_target] =
					NSYNC_SDTR_NOTDONE;
			}
			unit->nt_state = NPT_STATE_QUEUED;
			glm_waitq_add_lifo(unit, unit->nt_ncmdp);
			unit->nt_ncmdp = NULL;
			glm_addfq(glm, unit);

			/* pretend we were waiting for reselection */
			glm->g_state = NSTATE_WAIT_RESEL;
			break;

		case NSTATE_WAIT_RESEL:
			/* Target reselected while hba was waiting. */
			/* unit points to the HBA which is never really */
			/* queued. */
			break;

		default:
			/* should never happen */
			NDBG1(("\n\nintcode RESEL botched\n\n"));
			return (NACTION_DO_BUS_RESET | action);
		}

		/* Get target structure of device that wants to reconnect */
		if ((re_unit = glm_get_target(glm)) == NULL) {
			/* invalid reselection */
			return (NACTION_DO_BUS_RESET | action);
		}

		if (re_unit->nt_state != NPT_STATE_DISCONNECTED) {
			/* should send out ABORT message here ??? */
			return (NACTION_ABORT | action);
		}

		unit = re_unit;
		ASSERT(unit->nt_ncmdp != NULL);

		/* one less outstanding disconnected target */
		glm->g_disc_num--;

		unit->nt_ncmdp->cmd_flags &= ~CFLAG_CMDDISC;

		/* implicit restore data pointers */

		unit->nt_dsap->nt_curdp = unit->nt_savedp;
		unit->nt_dsap->nt_curdp.nd_left = unit->nt_savedp.nd_left;

		/* Put it on the front of the queue */
		glm->g_current = unit;
		glm->g_state = NSTATE_ACTIVE;
		glm->g_wdtr_sent = 0;
		return (NACTION_ACK | action);

	case NINT_SIGPROC:
		/* Give up waiting, start up another target */
		if (glm->g_state != NSTATE_WAIT_RESEL) {
			/* big trouble, bus reset time ? */
			return (NACTION_DO_BUS_RESET | NACTION_ERR | action);
		}
		NDBG31(("%s", (GLM_GET_ISTAT(glm) & NB_ISTAT_CON)
				? "glm: connected after sigproc\n"
				: ""));
		glm->g_state = NSTATE_IDLE;
		return (action);

	case NINT_SDTR:
		if (glm->g_state != NSTATE_ACTIVE) {
			/* reset the bus */
			return (NACTION_DO_BUS_RESET);

		}
		switch (NSYNCSTATE(glm, unit)) {
		default:
			/* reset the bus */
			NDBG31(("\n\nintcode SDTR state botch\n\n"));
			return (NACTION_DO_BUS_RESET);

		case NSYNC_SDTR_REJECT:
			/*
			 * glm is not doing sdtr, however, the disk initiated
			 * a sdtr message, respond with async (per the scsi
			 * spec).
			 */
			NDBG31(("\n\nintcode SDTR reject\n\n"));
			break;

		case NSYNC_SDTR_DONE:
			/* target wants to renegotiate */
			NDBG31(("\n\nintcode SDTR done, renegotiating\n\n"));
			glm_syncio_reset(glm, unit);
			NSYNCSTATE(glm, unit) = NSYNC_SDTR_RCVD;
			break;

		case NSYNC_SDTR_NOTDONE:
			/* target initiated negotiation */
			NDBG31(("\n\nintcode SDTR notdone\n\n"));
			glm_syncio_reset(glm, unit);
			NSYNCSTATE(glm, unit) = NSYNC_SDTR_RCVD;
			break;

		case NSYNC_SDTR_SENT:
			/* target responded to my negotiation */
			NDBG31(("\n\nintcode SDTR sent\n\n"));
			break;
		}
		return (NACTION_SDTR | action);

	case NINT_NEG_REJECT:
		NDBG31(("\n\nintcode NEG_REJECT \n\n"));

		/*
		 * A sdtr or wdtr responce was rejected.  We need to
		 * figure out what the driver was negotiating and
		 * either disable wide or disable sync.
		 */

		/*
		 * If target rejected WDTR, revert to narrow.
		 */
		if (unit->nt_dsap->nt_sendmsg.count > 1 &&
		    unit->nt_dsap->nt_msgoutbuf[2] == MSG_WIDE_DATA_XFER) {
			glm_set_wide_scntl3(glm, unit, 0);
			glm->g_wdtr_sent = 0;
		}

		/*
		 * If target rejected SDTR:
		 * Set all LUNs on this target to async i/o
		 */
		if (unit->nt_dsap->nt_sendmsg.count > 1 &&
		    unit->nt_dsap->nt_msgoutbuf[2] == MSG_SYNCHRONOUS) {
			glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
		}
		return (NACTION_ACK | action);

	case NINT_WDTR:
		/*
		 * Get the byte sent back by the target.
		 */
		width = (unit->nt_dsap->nt_wideibuf[1] & 0xff);
		ASSERT(width <= 1);

		glm->g_wide_known |= (1<<unit->nt_target);

		/*
		 * If we negotiated wide (or narrow), sync negotiation
		 * was lost.  Re-negotiate sync after wdtr.
		 */
		if ((++(glm->g_wdtr_sent)) & 1) {
			/*
			 * send back a wdtr responce.
			 */
			unit->nt_dsap->nt_sendmsg.count = 0;
			glm_make_wdtr(glm, unit, width);
			action |= NACTION_EXT_MSG_OUT;
		} else {
			glm_set_wide_scntl3(glm, unit, width);
			glm->g_wdtr_sent = 0;
			if (NSYNCSTATE(glm, unit) != NSYNC_SDTR_REJECT) {
				action |= NACTION_SDTR;
			} else {
				action |= NACTION_ACK;
			}
		}
		glm->g_props_update |= (1<<unit->nt_target);
		glm_syncio_reset(glm, unit);

		return (action);

	case NINT_MSGREJ:
		if (glm->g_state != NSTATE_ACTIVE) {
			/* reset the bus */
			return (NACTION_DO_BUS_RESET);
		}

		if (NSYNCSTATE(glm, unit) == NSYNC_SDTR_SENT) {
			/* the target can't handle sync i/o */
			glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
			return (NACTION_ACK | action);
		}

		if (unit->nt_dsap->nt_sendmsg.count > 1 &&
		    unit->nt_dsap->nt_msgoutbuf[3] == MSG_WIDE_DATA_XFER) {
			glm_set_wide_scntl3(glm, unit, 0);
			glm->g_wdtr_sent = 0;
			return (NACTION_ACK | action);
		}

		return (NACTION_ERR);

	case NINT_DEV_RESET:
		if (unit->nt_type != NRQ_DEV_RESET) {
			NDBG30(("glm_check_intcode: abort completed\n"));
			return (NACTION_DONE | action);
		}

		/* clear disco-ed requests for all the LUNs on this device */
		glm_flush_target(glm, unit->nt_target, FALSE, CMD_RESET,
			STATE_GOT_BUS, STAT_DEV_RESET);

		/* clear sdtr/wdtr I/O state on this target */
		glm_force_renegotiation(glm, unit->nt_target);

		NDBG30(("glm_check_intcode: bus device reset completed\n"));

		return (NACTION_DONE | NACTION_CLEAR_CHIP | action);
	}

	/*
	 * All of the interrupt codes handled above are the of
	 * the "expected" non-error type. The following interrupt
	 * codes are for unusual errors detected by the SCRIPT.
	 * For now treat all of them the same, mark the request
	 * as failed due to an error and then reset the SCSI bus.
	 */
handle_error:

	/*
	 * Some of these errors should be getting BUS DEVICE RESET
	 * rather than bus_reset.
	 */
	switch (intcode) {
	case NINT_SELECTED:
		errmsg = "got selected\n";
		break;
	case NINT_UNS_MSG:
		errmsg = "got an unsupported message";
		break;
	case NINT_ILI_PHASE:
		errmsg = "got incorrect phase";
		break;
	case NINT_UNS_EXTMSG:
		errmsg = "got unsupported extended message";
		break;
	case NINT_MSGIN:
		errmsg = "Message-In was expected";
		break;
	case NINT_MSGREJ:
		errmsg = "got unexpected Message Reject";
		break;
	case NINT_REJECT:
		errmsg = "unable to send Message Reject";
		break;
	case NINT_TOOMUCHDATA:
		errmsg = "got too much data to/from target";
		break;

	default:
		glm_log(glm, CE_WARN, "glm: invalid intcode=%lu", intcode);
		errmsg = "default";
		break;
	}

bus_reset:
	glm_log(glm, CE_WARN, "Resetting scsi bus, %s from (%d,%d)",
		errmsg, unit->nt_target, unit->nt_lun);

	glm_sync_wide_backoff(glm, unit);
	return (NACTION_DO_BUS_RESET | NACTION_ERR);
}

/*
 * figure out the recovery for a parity interrupt or a SGE interrupt.
 */
static ulong_t
glm_parity_check(uchar_t phase)
{
	switch (phase) {
	case NSOP_MSGIN:
		return (NACTION_MSG_PARITY);
	case NSOP_MSGOUT:
		return (NACTION_CLEAR_CHIP |
		    NACTION_DO_BUS_RESET | NACTION_ERR);
	case NSOP_COMMAND:
	case NSOP_STATUS:
		return (NACTION_INITIATOR_ERROR);
	case NSOP_DATAIN:
	case NSOP_DATAOUT:
		return (NACTION_SAVE_BCNT | NACTION_INITIATOR_ERROR);
	default:
		return (NACTION_ERR);
	}
}

/* add unit to the front of queue */
static void
glm_addfq(glm_t	*glm, glm_unit_t *unit)
{
	/* See if it's already in the queue */
	if (unit->nt_linkp != NULL || unit == glm->g_backp ||
	    unit == glm->g_forwp)
		cmn_err(CE_PANIC, "glm: glm_addfq: queue botch");

	if ((unit->nt_linkp = glm->g_forwp) == NULL)
		glm->g_backp = unit;
	glm->g_forwp = unit;
}

/* add unit to the back of queue */
static void
glm_addbq(glm_t	*glm, glm_unit_t *unit)
{
	unit->nt_linkp = NULL;

	if (glm->g_forwp == NULL)
		glm->g_forwp = unit;
	else
		glm->g_backp->nt_linkp = unit;

	glm->g_backp = unit;
}

/*
 * remove current target from front of hba's queue
 */
static glm_unit_t *
glm_rmq(glm_t *glm)
{
	glm_unit_t	*unit = glm->g_forwp;

	if (unit != NULL) {
		if ((glm->g_forwp = unit->nt_linkp) == NULL)
			glm->g_backp = NULL;
		unit->nt_linkp = NULL;
	}
	return (unit);
}

/*
 * These routines manipulate the queue of commands that
 * are waiting for their completion routines to be called.
 * The queue is usually in FIFO order but on an MP system
 * it's possible for the completion routines to get out
 * of order. If that's a problem you need to add a global
 * mutex around the code that calls the completion routine
 * in the interrupt handler.
 */
static void
glm_doneq_add(glm_t *glm, ncmd_t *cmdp)
{
	cmdp->cmd_linkp = NULL;
	*glm->g_donetail = cmdp;
	glm->g_donetail = &cmdp->cmd_linkp;
}

static ncmd_t *
glm_doneq_rm(glm_t *glm)
{
	ncmd_t	*cmdp;

	/* pop one off the done queue */
	if ((cmdp = glm->g_doneq) != NULL) {
		/* if the queue is now empty fix the tail pointer */
		if ((glm->g_doneq = cmdp->cmd_linkp) == NULL)
			glm->g_donetail = &glm->g_doneq;
		cmdp->cmd_linkp = NULL;
	}
	return (cmdp);
}

/*
 * These routines manipulate the target's queue of pending requests
 */
static void
glm_waitq_add(glm_unit_t *unit, ncmd_t *cmdp)
{
	cmdp->cmd_queued = TRUE;
	cmdp->cmd_linkp = NULL;
	*(unit->nt_waitqtail) = cmdp;
	unit->nt_waitqtail = &cmdp->cmd_linkp;
}

static void
glm_waitq_add_lifo(glm_unit_t *unit, ncmd_t *cmdp)
{
	cmdp->cmd_queued = TRUE;
	if ((cmdp->cmd_linkp = unit->nt_waitq) == NULL) {
		unit->nt_waitqtail = &cmdp->cmd_linkp;
	}
	unit->nt_waitq = cmdp;
}

static ncmd_t *
glm_waitq_rm(glm_unit_t *unit)
{
	ncmd_t *cmdp;

	/* pop one off the wait queue */
	if ((cmdp = unit->nt_waitq) != NULL) {
		/* if the queue is now empty fix the tail pointer */
		if ((unit->nt_waitq = cmdp->cmd_linkp) == NULL)
			unit->nt_waitqtail = &unit->nt_waitq;
		cmdp->cmd_linkp = NULL;
		cmdp->cmd_queued = FALSE;
	}

	NDBG19(("glm_waitq_rm: unit=0x%x cmdp=0x%x\n", unit, cmdp));

	return (cmdp);
}                

/*
 * remove specified target from the middle of the hba's queue
 */
static void
glm_waitq_delete(glm_unit_t *unit, ncmd_t *cmdp)
{
	ncmd_t  *prevp = unit->nt_waitq;

	if (prevp == cmdp) {
		if ((unit->nt_waitq = cmdp->cmd_linkp) == NULL)
			unit->nt_waitqtail = &unit->nt_waitq;

		cmdp->cmd_linkp = NULL;
		cmdp->cmd_queued = FALSE;
		NDBG19(("glm_waitq_delete: unit=0x%x cmdp=0x%x\n",
		unit, cmdp));
		return;
	}

	while (prevp != NULL) {
		if (prevp->cmd_linkp == cmdp) {
			if ((prevp->cmd_linkp = cmdp->cmd_linkp) == NULL)
				unit->nt_waitqtail = &unit->nt_waitq;

			cmdp->cmd_linkp = NULL;
			cmdp->cmd_queued = FALSE;
			NDBG19(("glm_waitq_delete: unit=0x%x cmdp=0x%x\n",
				unit, cmdp));
			return;
		}
		prevp = prevp->cmd_linkp;
	}
	cmn_err(CE_PANIC, "glm: glm_waitq_delete: queue botch");
}

/*
 * establish a new sync i/o state for all the luns on a target
 */
static void
glm_syncio_state(glm_t *glm, glm_unit_t *unit, uchar_t state, uchar_t sxfer,
	uchar_t sscfX10)
{
	ushort target;
	ushort lun;

	/*
	 * Change state of all LUNs on this target.  We may be responding
	 * to a target initiated sdtr and we may not be using syncio.
	 * We don't want to change state of the target, but we do need
	 * to respond to the target's requestion for sdtr per the scsi spec.
	 */
	if (NSYNCSTATE(glm, unit) != NSYNC_SDTR_REJECT) {
		NSYNCSTATE(glm, unit) = state;
	}

	target = unit->nt_target;
	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		/* store new sync i/o parms in each per-target-struct */
		if ((unit = NTL2UNITP(glm, target, lun)) != NULL) {
			unit->nt_dsap->nt_selectparm.nt_sxfer = sxfer;
			unit->nt_sscfX10 = sscfX10;
		}
	}
}

static void
glm_syncio_disable(glm_t *glm)
{
	ushort	 target;

	NDBG31(("glm_syncio_disable: devaddr=0x%x\n", glm->g_devaddr));

	for (target = 0; target < NTARGETS_WIDE; target++)
		glm->g_syncstate[target] = NSYNC_SDTR_REJECT;
}

static void
glm_syncio_reset_target(glm_t *glm, int target)
{
	glm_unit_t *unit;
	ushort_t lun;

	/* check if sync i/o negotiation disabled on this target */
	if (target == glm->g_glmid)
		glm->g_syncstate[target] = NSYNC_SDTR_REJECT;
	else if (glm->g_syncstate[target] != NSYNC_SDTR_REJECT)
		glm->g_syncstate[target] = NSYNC_SDTR_NOTDONE;

	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		if ((unit = NTL2UNITP(glm, target, lun)) != NULL) {
			/* byte assignment */
			unit->nt_dsap->nt_selectparm.nt_sxfer = 0;
		}
	}
}

static void
glm_syncio_reset(glm_t *glm, glm_unit_t *unit)
{
	ushort	target;

	NDBG31(("glm_syncio_reset: devaddr=0x%x\n", glm->g_devaddr));

	if (unit != NULL) {
		/* only reset the luns on this target */
		glm_syncio_reset_target(glm, unit->nt_target);
		return;
	}

	/* set the max offset to zero to disable sync i/o */
	for (target = 0; target < NTARGETS_WIDE; target++) {
		glm_syncio_reset_target(glm, target);
	}
}

static void
glm_syncio_msg_init(glm_t *glm, glm_unit_t *unit)
{
	struct glm_dsa *dsap;
	uchar_t msgcount;
	uchar_t target = unit->nt_target;
	uchar_t period = (glm->g_hba_period/4);
	uchar_t offset = glm->g_sync_offset;

	dsap = unit->nt_dsap;
	msgcount = dsap->nt_sendmsg.count;

	/*
	 * Use target's period not the period of the hba if
	 * this target experienced a sync backoff.
	 */
	if (glm->g_backoff & (1<<unit->nt_target)) {
		period = (glm->g_minperiod[unit->nt_target]/4);
	}

	/*
	 * sanity check of period and offset
	 */
	if (glm->g_target_scsi_options[target] & SCSI_OPTIONS_FAST20) {
		if (period < (uchar_t)(DEFAULT_FAST20SYNC_PERIOD/4)) {
			period = (uchar_t)(DEFAULT_FAST20SYNC_PERIOD/4);
		}
	} else if (glm->g_target_scsi_options[target] & SCSI_OPTIONS_FAST) {
		if (period < (uchar_t)(DEFAULT_FASTSYNC_PERIOD/4)) {
			period = (uchar_t)(DEFAULT_FASTSYNC_PERIOD/4);
		}
	} else if (glm->g_target_scsi_options[target] & SCSI_OPTIONS_SYNC) {
		if (period < (uchar_t)(DEFAULT_SYNC_PERIOD/4)) {
			period = (uchar_t)(DEFAULT_SYNC_PERIOD/4);
		}
	} else {
		offset = 0;
	}

	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_EXTENDED;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)3;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_SYNCHRONOUS;
	dsap->nt_msgoutbuf[msgcount++] = period;
	dsap->nt_msgoutbuf[msgcount++] = offset;

	dsap->nt_sendmsg.count = msgcount;
}

/*
 * glm sent a sdtr to a target and the target responded.  Find out
 * the offset and sync period and enable sync scsi if needed.
 *
 * called from: glm_syncio_decide.
 */
static int
glm_syncio_enable(glm_t *glm, glm_unit_t *unit)
{
	uchar_t sxfer;
	uchar_t sscfX10;
	int time_ns;
	uchar_t offset;
	struct glm_dsa *dsap;

	dsap = unit->nt_dsap;

	/*
	 * units for transfer period factor are 4 nsec.
	 *
	 * These two values are the sync period and offset
	 * the target responded with.
	 */
	time_ns = (dsap->nt_syncibuf[1] * 4);
	offset = dsap->nt_syncibuf[2];

	/*
	 * IF this target is FAST-20 capable use the round
	 * value of 50 ns.
	 */
	if (dsap->nt_syncibuf[1] == (DEFAULT_FAST20SYNC_PERIOD/4)) {
		time_ns = DEFAULT_FAST20SYNC_PERIOD;
	}

	/*
	 * If the target returns a zero offset, go async.
	 */
	if (offset == 0) {
		glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
		return (TRUE);
	}

	/*
	 * Check the period returned by the target.  Target shouldn't
	 * try to decrease my period
	 */
	if ((time_ns < CONVERT_PERIOD(glm->g_speriod)) ||
	    !glm_max_sync_divisor(glm, time_ns, &sxfer, &sscfX10)) {
		NDBG31(("glm_syncio_enable: invalid period: %d\n", time_ns));
		return (FALSE);
	}

	/*
	 * check the offset returned by the target.
	 */
	if (offset > glm->g_sync_offset) {
		NDBG31(("glm_syncio_enable: invalid offset=%d\n", offset));
		return (FALSE);
	}

	/* encode the divisor and offset values */
	sxfer = (((sxfer - 4) << 5) + offset);

	unit->nt_fastscsi = (time_ns < 200) ? TRUE : FALSE;

	/*
	 * Enable FAST-20 timing in the chip for this target.
	 */
	if (time_ns == DEFAULT_FAST20SYNC_PERIOD) {
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= NB_SCNTL3_FAST20;
	}

	/* set the max offset and clock divisor for all LUNs on this target */
	NDBG31(("glm_syncio_enable: target=%d sxfer=%x, sscfX10=%d",
		unit->nt_target, sxfer, sscfX10));

	glm->g_minperiod[unit->nt_target] = time_ns;

	glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, sxfer, sscfX10);
	glm->g_props_update |= (1<<unit->nt_target);
	return (TRUE);
}


/*
 * The target started the synchronous i/o negotiation sequence by
 * sending me a SDTR message. Look at the target's parms and the
 * HBA's defaults and decide on the appropriate comprise. Send the
 * larger of the two transfer periods and the smaller of the two offsets.
 */
static int
glm_syncio_respond(glm_t *glm, glm_unit_t *unit)
{
	uchar_t	sxfer;
	uchar_t	sscfX10;
	int	time_ns;
	uchar_t offset;
	struct glm_dsa *dsap;

	dsap = unit->nt_dsap;

	/*
	 * Use the smallest offset
	 */
	offset = dsap->nt_syncibuf[2];

	if (glm->g_syncstate[unit->nt_target] == NSYNC_SDTR_REJECT) {
		offset = 0;
	}

	if (offset > glm->g_sync_offset)
		offset = glm->g_sync_offset;

	/*
	 * units for transfer period factor are 4 nsec.
	 */
	time_ns = (dsap->nt_syncibuf[1] * 4);

	/*
	 * If target is FAST-20 capable use the round value
	 * of 50 instead of 48.
	 */
	if (dsap->nt_syncibuf[1] == (DEFAULT_FAST20SYNC_PERIOD/4)) {
		time_ns = DEFAULT_FAST20SYNC_PERIOD;
	}

	if (glm->g_backoff & (1<<unit->nt_target)) {
		time_ns = glm->g_minperiod[unit->nt_target];
	}

	/*
	 * Use largest time period.
	 */
	if (time_ns < glm->g_hba_period) {
		time_ns = glm->g_hba_period;
	}

	/*
	 * Target has requested a sync period slower than
	 * our max.  respond with our max sync rate.
	 */
	if (time_ns > MAX_SYNC_PERIOD(glm)) {
		time_ns = MAX_SYNC_PERIOD(glm);
	}

	if (!glm_max_sync_divisor(glm, time_ns, &sxfer, &sscfX10)) {
		NDBG31(("glm_syncio_respond: invalid period: %d,%d\n",
		    time_ns, offset));
		return (FALSE);
	}

	sxfer = (((sxfer - 4) << 5) + offset);

	/* set the max offset and clock divisor for all LUNs on this target */
	glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, sxfer, sscfX10);

	/* report to target the adjusted period */
	if ((time_ns = glm_period_round(glm, time_ns)) == -1) {
		NDBG31(("glm_syncio_respond: round failed time=%d\n",
		    time_ns));
		return (FALSE);
	}

	glm->g_minperiod[unit->nt_target] = time_ns;

	dsap->nt_msgoutbuf[0] = 0x01;
	dsap->nt_msgoutbuf[1] = 0x03;
	dsap->nt_msgoutbuf[2] = 0x01;
	dsap->nt_msgoutbuf[3] = (time_ns / 4);
	dsap->nt_msgoutbuf[4] = (uchar_t)offset;
	dsap->nt_sendmsg.count = 5;

	unit->nt_fastscsi = (time_ns < 200) ? TRUE : FALSE;

	/*
	 * Enable FAST-20 bit.
	 */
	if (time_ns == DEFAULT_FAST20SYNC_PERIOD) {
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= NB_SCNTL3_FAST20;
	}

	glm->g_props_update |= (1<<unit->nt_target);

	return (TRUE);
}

static ulong_t
glm_syncio_decide(glm_t *glm, glm_unit_t *unit, ulong_t action)
{
	if (action & (NACTION_SIOP_HALT | NACTION_SIOP_REINIT
			| NACTION_BUS_FREE)) {
		/* set all LUNs on this target to renegotiate syncio */
		glm_syncio_reset(glm, unit);
		return (action);
	}

	if (action & (NACTION_DONE | NACTION_ERR)) {
		/* the target finished without responding to SDTR */
		/* set all LUN's on this target to async mode */
		glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
		return (action);
	}

	if (action & (NACTION_MSG_PARITY | NACTION_INITIATOR_ERROR)) {
		/* allow target to try to do error recovery */
		return (action);
	}

	if ((action & NACTION_SDTR) == 0) {
		return (action);
	}

	/* if got good SDTR response, enable sync i/o */
	switch (NSYNCSTATE(glm, unit)) {
	case NSYNC_SDTR_SENT:
		if (glm_syncio_enable(glm, unit)) {
			/* reprogram the sxfer register */
			GLM_SET_SYNCIO(glm, unit);
			action |= NACTION_ACK;
		} else {
			/*
			 * The target sent us bogus sync msg.
			 * reject this msg. Disallow sync.
			 */
			NSYNCSTATE(glm, unit) = NSYNC_SDTR_DONE;
			action |= NACTION_MSG_REJECT;
		}
		return (action);

	case NSYNC_SDTR_RCVD:
	case NSYNC_SDTR_REJECT:
		/*
		 * if target initiated SDTR handshake, send sdtr.
		 */
		if (glm_syncio_respond(glm, unit)) {
			/* reprogram the sxfer register */
			GLM_SET_SYNCIO(glm, unit);
			return (NACTION_EXT_MSG_OUT | action);
		}
		break;

	case NSYNC_SDTR_NOTDONE:
		unit->nt_dsap->nt_sendmsg.count = 0;
		glm_syncio_msg_init(glm, unit);
		glm_syncio_state(glm, unit, NSYNC_SDTR_SENT, 0, 0);
		return (NACTION_EXT_MSG_OUT | action);
	}

	/* target and hba counldn't agree on sync i/o parms, so */
	/* set all LUN's on this target to async mode until */
	/* next bus reset */
	glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
	return (NACTION_MSG_REJECT | action);
}

/*
 * The chip uses a two stage divisor chain. The first stage can
 * divide by 1, 1.5, 2, 3, or 4.  The second stage can divide by values
 * from 4 to 11 (inclusive). If the board is using a 40MHz clock this
 * allows sync i/o rates  from 10 MB/sec to 1.212 MB/sec. The following
 * table factors a desired overall divisor into the appropriate values
 * for each of the two stages.  The first and third columns of this
 * table are scaled by a factor of 10 to handle the 1.5 case without
 * using floating point numbers.
 */
typedef	struct glm_divisor_table {
	int	divisorX10;	/* divisor times 10 */
	uchar_t	sxfer;		/* sxfer period divisor */
	uchar_t	sscfX10;	/* synchronous scsi clock divisor times 10 */
} ndt_t;

static	ndt_t	DivisorTable[] = {
	{ 40,	4,	10 },
	{ 50,	5,	10 },
	{ 60,	4,	15 },
	{ 70,	7,	10 },
	{ 75,	5,	15 },
	{ 80,	4,	20 },
	{ 90,	6,	15 },
	{ 100,	5,	20 },
	{ 105,	7,	15 },
	{ 110,	11,	10 },
	{ 120,	8,	15 },
	{ 135,	9,	15 },
	{ 140,	7,	20 },
	{ 150,	10,	15 },
	{ 160,	8,	20 },
	{ 165,	11,	15 },
	{ 180,	9,	20 },
	{ 200,	10,	20 },
	{ 210,	7,	30 },
	{ 220,	11,	20 },
	{ 240,	8,	30 },
	{ 270,	9,	30 },
	{ 300,	10,	30 },
	{ 330,	11,	30 },
	0
};

/*
 * Find the clock divisor which gives a period that is at least
 * as long as syncioperiod. If an divisor can't be found that
 * gives the exactly desired syncioperiod then the divisor which
 * results in the next longer valid period will be returned.
 *
 * In the above divisor lookup table the periods and divisors
 * are scaled by a factor of ten to handle the .5 fractional values.
 * I could have just scaled everything by a factor of two but I
 * think x10 is easier to understand and easier to setup.
 */
static int
glm_max_sync_divisor(glm_t *glm, int syncioperiod, uchar_t *sxferp,
    uchar_t *sscfX10p)
{
	ndt_t *dtp;
	int divX100;

	divX100 = (syncioperiod * 100);
	divX100 /= glm->g_speriod;

	for (dtp = DivisorTable; dtp->divisorX10 != 0; dtp++) {
		if (dtp->divisorX10 >= divX100) {
			goto got_it;
		}
	}
	return (FALSE);

got_it:
	*sxferp = dtp->sxfer;
	*sscfX10p = dtp->sscfX10;
	return (TRUE);
}

static int
glm_period_round(glm_t *glm, int syncioperiod)
{
	int	clkperiod;
	uchar_t	sxfer;
	uchar_t	sscfX10;
	int	tmp;

	if (glm_max_sync_divisor(glm, syncioperiod, &sxfer, &sscfX10)) {
		clkperiod = glm->g_speriod;

		switch (sscfX10) {
		case 10:
			/* times 1 */
			tmp = (clkperiod * sxfer);
			return (tmp/10);
		case 15:
			/* times 1.5 */
			tmp = (15 * clkperiod * sxfer);
			return ((tmp + 5) / 100);
		case 20:
			/* times 2 */
			tmp = (2 * clkperiod * sxfer);
			return (tmp/10);
		case 30:
			/* times 3 */
			tmp = (3 * clkperiod * sxfer);
			return (tmp/10);
		}
	}
	return (-1);
}

/*
 * Determine frequency of the HBA's clock chip and determine what
 * rate to use for synchronous i/o on each target. Because of the
 * way the chip's divisor chain works it's only possible to achieve
 * timings that are integral multiples of the clocks fundamental
 * period.
 */
static void
glm_max_sync_rate_init(glm_t *glm)
{
	int i;
	static char *prop_cfreq = "clock-frequency";
	int period;

	/*
	 * Determine clock frequency of attached Symbios chip.
	 */
	i = ddi_prop_get_int(DDI_DEV_T_ANY, glm->g_dip, 0, prop_cfreq, -1);
	glm->g_sclock = (i/(MEG));

	/*
	 * If we have a Rev 2 (or greater) 875, double the clock
	 * and enable FAST-20.
	 */
	if (glm->g_devid == GLM_53c875 && glm->g_revid > REV1 &&
	    ((glm->g_scsi_options & SCSI_OPTIONS_FAST20) != 0)) {
		ClrSetBits(glm->g_devaddr + NREG_STEST1, 0, NB_STEST1_DBLEN);
		drv_usecwait(20);
		ClrSetBits(glm->g_devaddr + NREG_STEST3, 0, NB_STEST3_HSC);
		ClrSetBits(glm->g_devaddr + NREG_STEST1, 0, NB_STEST1_DBLSEL);
		ClrSetBits(glm->g_devaddr + NREG_STEST3, NB_STEST3_HSC, 0);
		glm->g_sclock *= 2;
	}

	/*
	 * calculate the fundamental period in nanoseconds.
	 *
	 * FAST SCSI = 250. (25ns * 10)
	 * FAST-20 = 125. (12.5ns * 10)
	 *
	 * This is needed so that for FAST-20 timings, we don't
	 * lose the .5.
	 */
	glm->g_speriod = (10000 / glm->g_sclock);

	/*
	 * Round max sync rate to the closest value the hba's
	 * divisor chain can produce.
	 *
	 * equation for CONVERT_PERIOD:
	 *
	 * For FAST SCSI: ((250 << 2) / 10) = 100ns.
	 * For FAST-20  : ((125 << 2) / 10) =  50ns.
	 */
	if ((glm->g_hba_period = period =
	    glm_period_round(glm, CONVERT_PERIOD(glm->g_speriod))) <= 0) {
		glm_syncio_disable(glm);
		return;
	}

	/*
	 * Set each target to the correct period.
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		if (glm->g_target_scsi_options[i] & SCSI_OPTIONS_FAST20 &&
		    period == DEFAULT_FAST20SYNC_PERIOD) {
			glm->g_minperiod[i] = DEFAULT_FAST20SYNC_PERIOD;
		} else if (glm->g_target_scsi_options[i] & SCSI_OPTIONS_FAST) {
			glm->g_minperiod[i] = DEFAULT_FASTSYNC_PERIOD;
		} else {
			glm->g_minperiod[i] = DEFAULT_SYNC_PERIOD;
		}
	}
}

static void
glm_make_wdtr(struct glm *glm, struct glm_unit *unit, uchar_t width)
{
	struct glm_dsa *dsap;
	uchar_t msgcount;

	dsap = unit->nt_dsap;
	msgcount = dsap->nt_sendmsg.count;

	if (((glm->g_target_scsi_options[unit->nt_target] &
	    SCSI_OPTIONS_WIDE) == 0) ||
		(glm->g_nowide & (1<<unit->nt_target))) {
			width = 0;
	}

	width = min(GLM_XFER_WIDTH, width);

	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_EXTENDED;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)2;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_WIDE_DATA_XFER;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)width;

	dsap->nt_sendmsg.count = msgcount;

	/*
	 * increment wdtr flag, odd value indicates that we initiated
	 * the negotiation.
	 */
	glm->g_wdtr_sent++;

	glm_set_wide_scntl3(glm, unit, width);
}

static void
glm_set_wide_scntl3(struct glm *glm, struct glm_unit *unit, uchar_t width)
{
	ASSERT(width <= 1);
	switch (width) {
	case 0:
		unit->nt_dsap->nt_selectparm.nt_scntl3 &= ~NB_SCNTL3_EWS;
		break;
	case 1:
		/*
		 * The scntl3:NB_SCNTL3_EWS bit controls wide.
		 */
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= NB_SCNTL3_EWS;
		glm->g_wide_enabled |= (1<<unit->nt_target);
		break;
	}
	ddi_put8(glm->g_datap, (uint8_t *)(glm->g_devaddr + NREG_SCNTL3),
		unit->nt_dsap->nt_selectparm.nt_scntl3);
}

static void
glm53c810_reset(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;
	ddi_acc_handle_t datap = glm->g_datap;

	NDBG10(("glm53c810_reset: devaddr=0x%x\n", devaddr));

	/* Reset the 53c810 chip */
	(void) ddi_put8(datap, (uint8_t *)(devaddr + NREG_ISTAT),
		NB_ISTAT_SRST);

	/* wait a tick and then turn the reset bit off */
	drv_usecwait(100);
	(void) ddi_put8(datap, (uint8_t *)(devaddr + NREG_ISTAT), 0);

	/* clear any pending SCSI interrupts */
	(void) ddi_get8(datap, (uint8_t *)(devaddr + NREG_SIST0));

	/* need short delay before reading SIST1 */
	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));

	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));

	(void) ddi_get8(datap, (uint8_t *)(devaddr + NREG_SIST1));

	/* need short delay before reading DSTAT */
	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));
	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));

	/* clear any pending DMA interrupts */
	(void) ddi_get8(datap, (uint8_t *)(devaddr + NREG_DSTAT));

	NDBG1(("NCR53c810: Software reset completed\n"));
}

static void
glm53c810_init(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;
	ddi_acc_handle_t datap = glm->g_datap;
	uchar_t dmode = 0;

	NDBG1(("glm53c810_init: devaddr=0x%x\n", devaddr));

	/* Enable Parity checking and generation */
	ClrSetBits(devaddr + NREG_SCNTL0, 0, NB_SCNTL0_EPC);

	/* disable extra clock cycle of data setup so that */
	/* the hba can do 10MB/sec fast scsi */
	ClrSetBits(devaddr + NREG_SCNTL1, NB_SCNTL1_EXC, 0);

	/* Set the HBA's SCSI id, and enable reselects */
	ClrSetBits(devaddr + NREG_SCID, NB_SCID_ENC,
	    ((glm->g_glmid & NB_SCID_ENC) | NB_SCID_RRE));

	/* Disable auto switching */
	ClrSetBits(devaddr + NREG_DCNTL, 0, NB_DCNTL_COM);

#if defined(__ppc)
	/* Enable totem pole interrupt mode */
	/* PowerPC reference platform explicitly requires this init. */
	ClrSetBits(devaddr + NREG_DCNTL, 0, NB_DCNTL_IRQM);
#endif /* defined(__ppc) */

	/* set the selection time-out value. */
	ClrSetBits(devaddr + NREG_STIME0, NB_STIME0_SEL,
				(uchar_t)glm_selection_timeout);

	/* Set the scsi id bit to match the HBA's idmask */
	ddi_put16(datap, (uint16_t *)(devaddr + NREG_RESPID),
		(1 << glm->g_glmid));

	/* disable SCSI-1 single initiator option */
	/* enable TolerANT (active negation) */
	ClrSetBits(devaddr + NREG_STEST3, 0, (NB_STEST3_TE | NB_STEST3_DSI));

	/* setup the minimum transfer period (i.e. max transfer rate) */
	/* for synchronous i/o for each of the targets */
	glm_max_sync_rate_init(glm);

	/* set the scsi core divisor */
	if (glm->g_sclock <= 25) {
		glm->g_scntl3 = NB_SCNTL3_CCF1;
	} else if (glm->g_sclock < 38) {
		glm->g_scntl3 = NB_SCNTL3_CCF15;
	} else if (glm->g_sclock <= 50) {
		glm->g_scntl3 = NB_SCNTL3_CCF2;
	} else if (glm->g_sclock <= 75) {
		glm->g_scntl3 = NB_SCNTL3_CCF3;
	} else if (glm->g_sclock <= 80) {
		glm->g_scntl3 = NB_SCNTL3_CCF4;
	}
	ddi_put8(datap, (uint8_t *)(devaddr + NREG_SCNTL3), glm->g_scntl3);

	/*
	 * This bit along with bits 7&8 of the dmode register are
	 * are used to determine burst size.
	 *
	 * Also enable use of larger dma fifo in the 875.
	 */
	if (glm->g_devid == GLM_53c875) {
		ClrSetBits(devaddr + NREG_CTEST5, 0,
		    (NB_CTEST5_DFS | NB_CTEST5_BL2));
	}

	/*
	 * Set the dmode register.
	 *
	 * Default to 825, enable 875 if necessary.
	 */
	dmode = (NB_825_DMODE_BL | NB_DMODE_ERL);
	if (glm->g_devid == GLM_53c875) {
		dmode = (NB_DMODE_BL | NB_DMODE_ERL);
		ClrSetBits(devaddr + NREG_DMODE, 0, dmode);
	} else {
		ClrSetBits(devaddr + NREG_DMODE, 0, dmode);
	}

	/*
	 * Set the dcntl register.
	 *
	 * If this device is the 875, enable both the prefetch for
	 * scripts and Cache Line Size Enable.
	 */
	if (glm->g_devid == GLM_53c875) {
		uchar_t	dcntl;
		dcntl = ddi_get8(datap, (uchar_t *)(devaddr + NREG_DCNTL));
		dcntl |= NB_DCNTL_CLSE;
		/*
		 * Rev. 1 of the 875 has a bug with prefetch and interrupts.
		 * If a scsi interrupt is detected while prefetch is in
		 * progress, the 875 internal bus may hang.
		 */
		if (glm_use_scripts_prefetch) {
			dcntl |= NB_DCNTL_PFEN;
		}
		ddi_put8(datap, (uint8_t *)(devaddr + NREG_DCNTL), dcntl);
	}

	NDBG1(("glm53c810_init: devaddr=0x%x completed\n", devaddr));
}

static void
glm53c810_enable(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;

	/* enable all fatal interrupts, disable all non-fatal interrupts */
	ClrSetBits(devaddr + NREG_SIEN0, (NB_SIEN0_CMP | NB_SIEN0_SEL |
	    NB_SIEN0_RSL), (NB_SIEN0_MA | NB_SIEN0_SGE | NB_SIEN0_UDC |
	    NB_SIEN0_RST | NB_SIEN0_PAR));

	/* enable all fatal interrupts, disable all non-fatal interrupts */
	ClrSetBits(devaddr + NREG_SIEN1, (NB_SIEN1_GEN | NB_SIEN1_HTH),
	    NB_SIEN1_STO);

	/* enable all valid except SCRIPT Step Interrupt */
	ClrSetBits(devaddr + NREG_DIEN, NB_DIEN_SSI, (NB_DIEN_MDPE |
	    NB_DIEN_BF | NB_DIEN_ABRT | NB_DIEN_SIR | NB_DIEN_IID));

	/* enable master parity error detection logic */
	ClrSetBits(devaddr + NREG_CTEST4, 0, NB_CTEST4_MPEE);
}

static void
glm53c810_disable(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;

	/* disable all SCSI interrrupts */
	ClrSetBits(devaddr + NREG_SIEN0, (NB_SIEN0_MA | NB_SIEN0_CMP |
	    NB_SIEN0_SEL | NB_SIEN0_RSL | NB_SIEN0_SGE | NB_SIEN0_UDC |
	    NB_SIEN0_RST | NB_SIEN0_PAR), 0);

	ClrSetBits(devaddr + NREG_SIEN1, (NB_SIEN1_GEN | NB_SIEN1_HTH |
	    NB_SIEN1_STO), 0);

	/* disable all DMA interrupts */
	ClrSetBits(devaddr + NREG_DIEN, (NB_DIEN_MDPE | NB_DIEN_BF |
	    NB_DIEN_ABRT | NB_DIEN_SSI | NB_DIEN_SIR | NB_DIEN_IID), 0);

	/* disable master parity error detection */
	ClrSetBits(devaddr + NREG_CTEST4, NB_CTEST4_MPEE, 0);
}

static uchar_t
glm53c810_get_istat(glm_t *glm)
{
	return (ddi_get8(glm->g_datap,
			(uchar_t *)(glm->g_devaddr + NREG_ISTAT)));
}

static void
glm53c810_halt(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;
	uchar_t	first_time = TRUE;
	int	loopcnt;
	uchar_t	istat;
	uchar_t	dstat;

	/* turn on the abort bit */
	istat = NB_ISTAT_ABRT;
	ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_ISTAT), istat);

	/* wait for and clear all pending interrupts */
	for (;;) {

		/* wait up to 1 sec. for a DMA or SCSI interrupt */
		for (loopcnt = 0; loopcnt < 1000; loopcnt++) {
			istat = glm53c810_get_istat(glm);
			if (istat & (NB_ISTAT_SIP | NB_ISTAT_DIP))
				goto got_it;

			/* wait 1 millisecond */
			drv_usecwait(1000);
		}
		NDBG10(("glm53c810_halt: 0x%x: can't halt\n", devaddr));
		return;

	got_it:
		/* if there's a SCSI interrupt pending clear it and loop */
		if (istat & NB_ISTAT_SIP) {
			/* reset the sip latch registers */
			(void) ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SIST0));

			/* need short delay before reading SIST1 */
			(void) ddi_get32(glm->g_datap,
				(uint32_t *)(devaddr + NREG_SCRATCHA));
			(void) ddi_get32(glm->g_datap,
				(uint32_t *)(devaddr + NREG_SCRATCHA));

			(void) ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SIST1));
			continue;
		}

		if (first_time) {
			/* reset the abort bit before reading DSTAT */
			ddi_put8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_ISTAT), 0);
			first_time = FALSE;
		}
		/* read the DMA status register */
		dstat = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_DSTAT));
		if (dstat & NB_DSTAT_ABRT) {
			/* got the abort interrupt */
			NDBG10(("glm53c810_halt: devaddr=0x%x: okay\n",
			    devaddr));
			return;
		}
		/* must have been some other pending interrupt */
		drv_usecwait(1000);
	}
}

static void
glm53c810_set_sigp(glm_t *glm)
{
	ClrSetBits(glm->g_devaddr + NREG_ISTAT, 0, NB_ISTAT_SIGP);
}

static void
glm53c810_reset_sigp(glm_t *glm)
{
	(void) ddi_get8(glm->g_datap,
			(uint8_t *)(glm->g_devaddr + NREG_CTEST2));
}

static ulong_t
glm53c810_get_intcode(glm_t *glm)
{
	return (ddi_get32(glm->g_datap,
			(uint32_t *)(glm->g_devaddr + NREG_DSPS)));
}

/*
 * Utility routine; check for error in execution of command in ccb,
 * handle it.
 */
static void
glm53c810_check_error(glm_unit_t *unit, struct scsi_pkt *pktp)
{
	/* store the default error results in packet */
	pktp->pkt_state |= STATE_GOT_BUS;

	if (unit->nt_status0 == 0 && unit->nt_status1 == 0 &&
	    unit->nt_dma_status == 0) {
		NDBG17(("glm53c810_check_error: A\n"));
		pktp->pkt_statistics |= STAT_BUS_RESET;
		pktp->pkt_reason = CMD_RESET;
		return;
	}

	if (unit->nt_status1 & NB_SIST1_STO) {
		NDBG17(("glm53c810_check_error: B\n"));
		pktp->pkt_state  |= STATE_GOT_BUS;
	}
	if (unit->nt_status0 & NB_SIST0_UDC) {
		NDBG17(("glm53c810_check_error: C\n"));
		pktp->pkt_state  |= (STATE_GOT_BUS | STATE_GOT_TARGET);
		pktp->pkt_statistics = 0;
	}
	if (unit->nt_status0 & NB_SIST0_RST) {
		NDBG17(("glm53c810_check_error: D\n"));
		pktp->pkt_state  |= STATE_GOT_BUS;
		pktp->pkt_statistics |= STAT_BUS_RESET;
	}
	if (unit->nt_status0 & NB_SIST0_PAR) {
		NDBG17(("glm53c810_check_error: E\n"));
		pktp->pkt_statistics |= STAT_PERR;
	}
	if (unit->nt_dma_status & NB_DSTAT_ABRT) {
		NDBG17(("glm53c810_check_error: F\n"));
		pktp->pkt_statistics |= STAT_ABORTED;
	}


	/* Determine the appropriate error reason */

	/* watch out, on the 8xx chip the STO bit was moved */
	if (unit->nt_status1 & NB_SIST1_STO) {
		NDBG17(("glm53c810_check_error: G\n"));
		pktp->pkt_reason = CMD_INCOMPLETE;

	} else if (unit->nt_status0 & NB_SIST0_UDC) {
		NDBG17(("glm53c810_check_error: H\n"));
		pktp->pkt_reason = CMD_UNX_BUS_FREE;
	} else if (unit->nt_status0 & NB_SIST0_RST) {
		NDBG17(("glm53c810_check_error: I\n"));
		pktp->pkt_reason = CMD_RESET;

	} else {
		NDBG17(("glm53c810_check_error: J\n"));
		pktp->pkt_reason = CMD_INCOMPLETE;
	}
}

/*
 * for SCSI or DMA errors I need to figure out reasonable error
 * recoveries for all combinations of (hba state, scsi bus state,
 * error type). The possible error recovery actions are (in the
 * order of least to most drastic):
 *
 * 	1. send message parity error to target
 *	2. send abort
 *	3. send abort tag
 *	4. send initiator detected error
 *	5. send bus device reset
 *	6. bus reset
 */

static ulong_t
glm53c810_dma_status(glm_t *glm)
{
	glm_unit_t	*unit;
	caddr_t devaddr = glm->g_devaddr;
	ulong_t	 action = 0;
	uchar_t	 dstat;

	/* read DMA interrupt status register, and clear the register */
	dstat = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_DSTAT));

	NDBG21(("glm53c810_dma_status: devaddr=0x%x dstat=0x%x\n",
	    devaddr, dstat));

	/*
	 * DMA errors leave the HBA connected to the SCSI bus.
	 * Need to clear the bus and reset the chip.
	 */
	switch (glm->g_state) {
	case NSTATE_IDLE:
		/* this shouldn't happen */
		glm_log(glm, CE_WARN, "Unexpected DMA state: IDLE. dstat=%b",
		    dstat, dstatbits);
		action = NACTION_SIOP_REINIT;
		break;

	case NSTATE_ACTIVE:
		unit = glm->g_current;
		unit->nt_dma_status |= dstat;
		if (dstat & NB_DSTAT_ERRORS) {
			glm_log(glm, CE_WARN,
			    "Unexpected DMA state: ACTIVE. dstat=%b",
				dstat, dstatbits);
			action = (NACTION_SIOP_REINIT | NACTION_DO_BUS_RESET |
			    NACTION_ERR);

		} else if (dstat & NB_DSTAT_SIR) {
			/* SCRIPT software interrupt */
			if (ddi_get8(glm->g_datap, (uint8_t *)(devaddr +
			    NREG_SCRATCHA0)) == 0) {
				/* the dma list has completed */
				unit->nt_dsap->nt_curdp.nd_left = 0;
				unit->nt_savedp.nd_left = 0;
			}
			action |= NACTION_CHK_INTCODE;
		}
		break;

	case NSTATE_WAIT_RESEL:
		if (dstat & NB_DSTAT_ERRORS) {
			glm_log(glm, CE_WARN,
			    "Unexpected DMA state: IDLE. dstat=%b",
				dstat, dstatbits);
			action = NACTION_SIOP_REINIT;

		} else if (dstat & NB_DSTAT_SIR) {
			/* SCRIPT software interrupt */
			action |= NACTION_CHK_INTCODE;
		}
		break;
	}
	return (action);
}

static ulong_t
glm53c810_scsi_status(glm_t *glm)
{
	glm_unit_t	*unit;
	caddr_t	 devaddr = glm->g_devaddr;
	ulong_t	 action = 0;
	uchar_t	 sist0;
	uchar_t	 sist1;
	uchar_t	 scntl1;

	/* read SCSI interrupt status register, and clear the register */
	sist0 = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_SIST0));

	(void) ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_SCRATCHA));

	(void) ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_SCRATCHA));

	sist1 = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_SIST1));

	NDBG21(("glm53c810_scsi_status: devaddr=0x%x sist0=0x%x sist1=0x%x\n",
	    devaddr, sist0, sist1));

	/*
	 * the scsi timeout, unexpected disconnect, and bus reset
	 * interrupts leave the bus in the bus free state ???
	 *
	 * the scsi gross error and parity error interrupts leave
	 * the bus still connected ???
	 */
	switch (glm->g_state) {
	case NSTATE_IDLE:
		if ((sist0 & (NB_SIST0_SGE | NB_SIST0_PAR | NB_SIST0_UDC)) ||
		    (sist1 & NB_SIST1_STO)) {
			/* shouldn't happen, clear chip */
			action = (NACTION_CLEAR_CHIP | NACTION_DO_BUS_RESET);
		}

		if (sist0 & NB_SIST0_RST) {
			action = (NACTION_CLEAR_CHIP | NACTION_GOT_BUS_RESET);
		}
		break;

	case NSTATE_ACTIVE:
		unit = glm->g_current;
		unit->nt_status0 |= sist0;
		unit->nt_status1 |= sist1;

		/*
		 * If phase mismatch, then figure out the residual for
		 * the current dma scatter/gather segment
		 */
		if (sist0 & NB_SIST0_MA) {
			action = NACTION_SAVE_BCNT;
		}

		if (sist0 & (NB_SIST0_PAR | NB_SIST0_SGE)) {
			/* attempt recovery if selection done and connected */
			if (ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SCNTL1))
						& NB_SCNTL1_CON) {
				uchar_t phase;
				phase = ddi_get8(glm->g_datap,
				    (uint8_t *)(devaddr + NREG_SSTAT1)) &
				    NB_SSTAT1_PHASE;
				action = glm_parity_check(phase);
			} else {
				action = NACTION_ERR;
			}
		}

		/*
		 * The target dropped the bus.
		 */
		if (sist0 & NB_SIST0_UDC) {
			action = (NACTION_SAVE_BCNT | NACTION_ERR |
			    NACTION_CLEAR_CHIP);
		}

		/*
		 * selection timeout.
		 * make sure we negotiate when this target comes
		 * on line later on
		 */
		if (sist1 & NB_SIST1_STO) {
			/* bus is now idle */
			action = (NACTION_SAVE_BCNT | NACTION_ERR |
			    NACTION_CLEAR_CHIP);
			glm_force_renegotiation(glm, unit->nt_target);
			glm->g_wdtr_sent = 0;
		}

		if (sist0 & NB_SIST0_RST) {
			/* bus is now idle */
			action = (NACTION_CLEAR_CHIP | NACTION_GOT_BUS_RESET |
				NACTION_ERR);
		}
		break;

	case NSTATE_WAIT_RESEL:
		if (sist0 & NB_SIST0_PAR) {
			/* attempt recovery if reconnected */
			scntl1 = ddi_get8(glm->g_datap,
				(uint8_t *)(glm->g_devaddr + NREG_SCNTL1));

			if (scntl1 & NB_SCNTL1_CON) {
				action = NACTION_MSG_PARITY;
			} else {
				/* don't respond */
				action = NACTION_BUS_FREE;
			}
		}

		if (sist0 & NB_SIST0_UDC) {
			/* target reselected then disconnected, ignore it */
			action = (NACTION_BUS_FREE | NACTION_CLEAR_CHIP);
		}

		if ((sist0 & NB_SIST0_SGE) || (sist1 & NB_SIST1_STO)) {
			/* shouldn't happen, clear chip and bus reset */
			action = (NACTION_CLEAR_CHIP | NACTION_DO_BUS_RESET);
		}

		if (sist0 & NB_SIST0_RST) {
			/* got bus reset, restart the wait for reselect */
			action = (NACTION_CLEAR_CHIP | NACTION_GOT_BUS_RESET |
				NACTION_BUS_FREE);
		}
		break;
	}
	return (action);
}

/*
 * If the phase-mismatch which preceeds the Save Data Pointers
 * occurs within in a Scatter/Gather segment there's a residual
 * byte count that needs to be computed and remembered. It's
 * possible for a Disconnect message to occur without a preceeding
 * Save Data Pointers message, so at this point we only compute
 * the residual without fixing up the S/G pointers.
 */
static int
glm53c810_save_byte_count(glm_t *glm, glm_unit_t *unit)
{
	caddr_t	devaddr = glm->g_devaddr;
	uint_t	dsp;
	int	index;
	ulong_t	remain;
	uchar_t	opcode;
	ushort_t tmp;
	uchar_t	sstat0;
	uchar_t	sstat2;
	int	rc;
	ushort_t dfifo;
	ushort_t ctest5;
	ulong_t	dbc;

	NDBG17(("glm53c810_save_byte_count devaddr=0x%x\n", devaddr));

	/*
	 * Only need to do this for phase mismatch interrupts
	 * during actual data in or data out move instructions.
	 */
	if ((unit->nt_ncmdp->cmd_flags & CFLAG_DMAVALID) == 0) {
		/* since no data requested must not be S/G dma */
		rc = FALSE;
		goto out;
	}

	/*
	 * fetch the instruction pointer and back it up
	 * to the actual interrupted instruction.
	 */
	dsp = ddi_get32(glm->g_datap, (uint32_t *)(devaddr + NREG_DSP));
	dsp -= 8;

	/* check for MOVE DATA_OUT or MOVE DATA_IN instruction */
	opcode = ddi_get8(glm->g_datap, (uchar_t *)(devaddr + NREG_DCMD));

	if (opcode == (NSOP_MOVE | NSOP_DATAOUT)) {
		/* move data out */
		index = glm->g_do_list_end - dsp;

	} else if (opcode == (NSOP_MOVE | NSOP_DATAIN)) {
		/* move data in */
		index = glm->g_di_list_end - dsp;

	} else {
		/* not doing data dma so nothing to update */
		NDBG17(("glm53c810_save_byte_count: devaddr=%#x %#x not move\n",
		    devaddr, opcode));
		rc = FALSE;
		goto out;
	}

	/*
	 * convert byte index into S/G DMA list index
	 */
	index = (index/8);

	if (index < 0 || index > GLM_MAX_DMA_SEGS) {
		/* it's out of dma list range, must have been some other move */
		NDBG17((
		    "glm53c810_save_byte_count: devaddr=0x%x 0x%x not dma\n",
		    devaddr, index));
		rc = FALSE;
		goto out;
	}

	/*
	 * 875 has a larger fifo, so the math is a little different.
	 */
	if (glm->g_devid == GLM_53c875) {
		/* read the dbc register. */
		dbc = (ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_DBC)) & 0xffffff);

		ctest5 = ((ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_CTEST5)) & 0x3) << 8);

		dfifo = (ctest5 |
		    ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_DFIFO)));

		/* actual number left untransferred. */
		remain = dbc + ((dfifo - (dbc & 0x3ff)) & 0x3ff);
	} else {

		/* get the residual from the byte count register */
		dbc =  ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_DBC)) & 0xffffff;

		/* number of bytes stuck in the DMA FIFO */
		dfifo = (ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_DFIFO)) & 0x7f);

		/* actual number left untransferred. */
		remain = dbc + ((dfifo - (dbc & 0x7f)) & 0x7f);
	}

	/*
	 * Add one if there's a byte stuck in the SCSI fifo
	 */
	tmp = ddi_get8(glm->g_datap, (uchar_t *)(devaddr + NREG_CTEST2));

	if (tmp & NB_CTEST2_DDIR) {
		/* transfer was incoming (SCSI -> host bus) */
		sstat0 = ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_SSTAT0));

		sstat2 = ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_SSTAT2));

		if (sstat0 & NB_SSTAT0_ILF)
			remain++;	/* Add 1 if SIDL reg is full */

		/* Wide byte left async. */
		if (sstat2 & NB_SSTAT2_ILF1)
			remain++;

		/* check for synchronous i/o */
		if (unit->nt_dsap->nt_selectparm.nt_sxfer != 0) {
			tmp = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SSTAT1));
			remain += (tmp >> 4) & 0xf;
		}
	} else {
		/* transfer was outgoing (host -> SCSI bus) */
		sstat0 = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SSTAT0));

		sstat2 = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SSTAT2));

		if (sstat0 & NB_SSTAT0_OLF)
			remain++;	/* Add 1 if data is in SODL reg. */

		/* Check data for wide byte left. */
		if (sstat2 & NB_SSTAT2_OLF1)
			remain++;

		/* check for synchronous i/o */
		if ((unit->nt_dsap->nt_selectparm.nt_sxfer != 0) &&
		    (sstat0 & NB_SSTAT0_ORF)) {
			remain++;	/* Add 1 if data is in SODR */
			/* Check for Wide byte left. */
			if (sstat2 & NB_SSTAT2_ORF1)
				remain++;
		}
	}

	/* update the S/G pointers and indexes */
	glm_sg_update(unit, index, remain);
	rc = TRUE;

out:
	/* Clear the DMA FIFO pointers */
	CLEAR_DMA(glm);

	/* Clear the SCSI FIFO pointers */
	CLEAR_SCSI_FIFO(glm);

	NDBG17(("glm53c810_save_byte_count: devaddr=0x%x index=%d remain=%d\n",
	    devaddr, index, remain));
	return (rc);
}

static int
glm53c810_get_target(struct glm *glm, uchar_t *tp)
{
	caddr_t devaddr = glm->g_devaddr;
	uchar_t id;

	/*
	 * get the id byte received from the reselecting target
	 */
	id = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_SSID));

	NDBG20(("glm53c810_get_target: devaddr=0x%x lcrc=0x%x\n", devaddr, id));

	/* is it valid? */
	if (id & NB_SSID_VAL) {
		/* mask off extraneous bits */
		id &= NB_SSID_ENCID;
		NDBG20(("glm53c810_get_target: ID %d reselected\n", id));
		*tp = id;
		return (TRUE);
	}
	glm_log(glm, CE_WARN,
	    "glm53c810_get_target: invalid reselect id %d", id);
	return (FALSE);
}

static void
glm53c810_set_syncio(glm_t *glm, glm_unit_t *unit)
{
	caddr_t	devaddr = glm->g_devaddr;
	uchar_t	sxfer = unit->nt_dsap->nt_selectparm.nt_sxfer;
	uchar_t	scntl3 = 0;

	/* Set SXFER register */
	ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_SXFER), sxfer);

	/* Set sync i/o clock divisor in SCNTL3 registers */
	if (sxfer != 0) {
		switch (unit->nt_sscfX10) {
		case 10:
			scntl3 = NB_SCNTL3_SCF1 | glm->g_scntl3;
			break;

		case 15:
			scntl3 = NB_SCNTL3_SCF15 | glm->g_scntl3;
			break;

		case 20:
			scntl3 = NB_SCNTL3_SCF2 | glm->g_scntl3;
			break;

		case 30:
			scntl3 = NB_SCNTL3_SCF3 | glm->g_scntl3;
			break;
		}
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= scntl3;
	}

	ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_SCNTL3),
	    unit->nt_dsap->nt_selectparm.nt_scntl3);

	/* set extended filtering if not Fast-SCSI (i.e., < 5MB/sec) */
	if (sxfer == 0 || unit->nt_fastscsi == FALSE) {
		ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_STEST2),
		    NB_STEST2_EXT);
	} else {
		ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_STEST2), 0);
	}
}

static void
glm53c810_setup_script(glm_t *glm, glm_unit_t *unit, int resel)
{
	caddr_t	devaddr = glm->g_devaddr;

	uchar_t nleft = unit->nt_dsap->nt_curdp.nd_left;

	NDBG18(("glm53c810_setup_script: devaddr=0x%x\n", devaddr));

	/* Set the Data Structure address register to */
	/* the physical address of the active table */
	ddi_put32(glm->g_datap,
	    (uint32_t *)(devaddr + NREG_DSA), unit->nt_dsa_addr);

	/*
	 * Set syncio clock divisors and offset registers in case
	 * reconnecting after target reselected.
	 *
	 * If we are starting a selection, notify the scripts via
	 * the scratcha1 register.  This register will be cleared
	 * if the selection suceeds.
	 *
	 * Also, if we are selecting we do not need to set sync/wide
	 * registers via pio's, scripts will do that for us.
	 */
	if (resel) {
		glm53c810_set_syncio(glm, unit);
	}

	/*
	 * Setup scratcha0 as the number of segments left to do
	 */
	ddi_put8(glm->g_datap,
	    (uint8_t *)(glm->g_devaddr + NREG_SCRATCHA0), nleft);

	NDBG18(("glm53c810_setup_script: devaddr=0x%x okay\n", devaddr));
}

static void
glm53c810_start_script(glm_t *glm, int script)
{
	/* Send the SCRIPT entry point address to the glm chip */
	ddi_put32(glm->g_datap, (uint32_t *)(glm->g_devaddr + NREG_DSP),
		glm->g_glm_scripts[script]);

	NDBG18(("glm53c810_start_script: devaddr=0x%x script=%d\n",
	    glm->g_devaddr, script));
}

static void
glm53c810_bus_reset(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;

	/* Reset the scsi bus */
	ClrSetBits(devaddr + NREG_SCNTL1, 0, NB_SCNTL1_RST);

	/* Wait at least 25 microsecond */
	drv_usecwait((clock_t)25);

	/* Turn off the bit to complete the reset */
	ClrSetBits(devaddr + NREG_SCNTL1, NB_SCNTL1_RST, 0);
}

/*
 * Error logging, printing, and debug print routines.
 */
static char *glm_label = "glm";

static void
glm_log(struct glm *glm, int level, char *fmt, ...)
{
	char buf[256];
	dev_info_t *dev;
	va_list ap;

	if (glm) {
		dev = glm->g_dip;
	} else {
		dev = 0;
	}

	va_start(ap, fmt);
	(void) vsprintf(buf, fmt, ap);
	va_end(ap);

	scsi_log(dev, glm_label, level, "%s", buf);
}

#if defined(GLM_DEBUG)
/*VARARGS1*/
static void
glm_printf(char *fmt, ...)
{
	char buf[256];
	dev_info_t *dev;
	va_list	ap;

	dev = 0;

	va_start(ap, fmt);
	(void) vsprintf(buf, fmt, ap);
	va_end(ap);

	scsi_log(dev, "glm", CE_CONT, "%s", buf);
}
#endif	/* defined(GLM_DEBUG) */
