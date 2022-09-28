/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SCSI_ADAPTERS_GLMVAR_H
#define	_SYS_SCSI_ADAPTERS_GLMVAR_H

#pragma ident	"@(#)glmvar.h	1.40	96/10/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Compile options
 */
#ifdef DEBUG
#define	GLM_DEBUG		/* turn on debugging code */
#endif	/* DEBUG */

#define	N_GLM_UNITS		(NTARGETS_WIDE * NLUNS_PER_TARGET)
#define	ALL_TARGETS		0xffff

#define	GLM_INITIAL_SOFT_SPACE	4	/* Used	for the	softstate_init func */

/*
 * Wide support.
 */
#define	GLM_XFER_WIDTH	1

/*
 * If your HBA supports DMA or bus-mastering, you may have your own
 * scatter-gather list for physically non-contiguous memory in one
 * I/O operation; if so, there's probably a size for that list.
 * It must be placed in the ddi_dma_lim_t structure, so that the system
 * DMA-support routines can use it to break up the I/O request, so we
 * define it here.
 */
#if defined(sparc)
#define	GLM_MAX_DMA_SEGS	1
#else
#define	GLM_MAX_DMA_SEGS	17
#endif

/*
 * Scatter-gather list structure defined by HBA hardware
 */
typedef	struct NcrTableIndirect {	/* Table Indirect entries */
	ulong_t count;		/* 24 bit count */
	caddr_t address;	/* 32 bit physical address */
} glmti_t;

typedef	struct DataPointers {
	glmti_t nd_data[GLM_MAX_DMA_SEGS];	/* Pointers to data buffers */
	uchar_t nd_left;		/* number of entries left to process */
} ntd_t;

#define	PKT2CMD(pktp)	((struct glm_scsi_cmd *)((pktp)->pkt_ha_private))
#define	CMD2PKT(cmdp)	((struct scsi_pkt *)((cmdp)->cmd_pkt))

typedef struct 	glm_scsi_cmd {
	ulong_t			cmd_flags;	/* flags from scsi_init_pkt */
	ddi_dma_handle_t	cmd_dmahandle;	/* dma handle */
	ddi_dma_cookie_t	cmd_cookie;
	uint_t			cmd_cookiec;
	uint_t			cmd_winindex;
	uint_t			cmd_nwin;
	struct scsi_pkt		*cmd_pkt;
	uchar_t			cmd_cdblen;	/* length of cdb */
	struct scsi_arq_status	cmd_scb;
	uchar_t			cmd_rqslen;	/* len of requested rqsense */
	ulong_t			cmd_dmacount;
	int			cmd_time;
	uchar_t			cmd_queued;	/* true if queued */
	uchar_t			cmd_type;
	struct glm_scsi_cmd	*cmd_linkp;
	glmti_t			cmd_sg[GLM_MAX_DMA_SEGS]; /* S/G structure */
} ncmd_t;

/*
 * These are the defined cmd_flags for this structure.
 */
#define	CFLAG_CMDDISC		0x0001	/* cmd currently disconnected */
#define	CFLAG_WATCH		0x0002	/* watchdog time for this command */
#define	CFLAG_FINISHED		0x0004	/* command completed */
#define	CFLAG_CHKSEG		0x0008	/* check cmd_data within seg */
#define	CFLAG_COMPLETED		0x0010	/* completion routine called */
#define	CFLAG_PREPARED		0x0020	/* pkt has been init'ed */
#define	CFLAG_IN_TRANSPORT	0x0040	/* in use by host adapter driver */
#define	CFLAG_RESTORE_PTRS	0x0080	/* implicit restore ptr on reconnect */
#define	CFLAG_TRANFLAG		0x00ff	/* covers transport part of flags */
#define	CFLAG_CMDPROXY		0x000100 /* cmd is a 'proxy' command */
#define	CFLAG_CMDARQ		0x000200 /* cmd is a 'rqsense' command */
#define	CFLAG_DMAVALID		0x000400 /* dma mapping valid */
#define	CFLAG_DMASEND		0x000800 /* data	is going 'out' */
#define	CFLAG_CMDIOPB		0x001000 /* this	is an 'iopb' packet */
#define	CFLAG_CDBEXTERN		0x002000 /* cdb kmem_alloc'd */
#define	CFLAG_SCBEXTERN		0x004000 /* scb kmem_alloc'd */
#define	CFLAG_FREE		0x008000 /* packet is on	free list */
#define	CFLAG_PRIVEXTERN	0x020000 /* target private kmem_alloc'd */
#define	CFLAG_DMA_PARTIAL	0x040000 /* partial xfer OK */

/*
 * Information maintained about each (target,lun) the HBA serves.
 *
 * DSA reg points here when this target is active
 * Table Indirect pointers for GLM SCRIPTS
 */
struct glm_dsa {
	struct	{
		uchar_t	nt_pad0;	/* currently unused */
		uchar_t	nt_sxfer;	/* SCSI transfer/period parms */
		uchar_t	nt_sdid;	/* SCSI destination ID for SELECT */
		uchar_t	nt_scntl3;	/* FAST-20 and Wide bits. */
	} nt_selectparm;
	glmti_t	nt_sendmsg;		/* pointer to sendmsg buffer */
	glmti_t	nt_rcvmsg;		/* pointer to msginbuf */
	glmti_t	nt_cmd;			/* pointer to cdb buffer */
	glmti_t	nt_status;		/* pointer to status buffer */
	glmti_t	nt_extmsg;		/* pointer to extended message buffer */
	glmti_t	nt_syncin;		/* pointer to sync in buffer */
	glmti_t	nt_syncout;		/* pointer to sync out buffer */
	glmti_t	nt_errmsg;		/* pointer to message reject buffer */
	glmti_t nt_widein;		/* pointer to wide in buffer */
	glmti_t nt_wideout;		/* pointer to wide out buffer */
	ntd_t nt_curdp;			/* current S/G data pointers */

	/* these are the buffers the HBA actually does i/o to/from */
	uchar_t nt_cdb[12];		/* scsi command description block */

	/* keep these two together so HBA can transmit in single move */
	uchar_t	nt_msgoutbuf[12];
	uchar_t	nt_msginbuf[1];		/* first byte of message in */
	uchar_t	nt_extmsgbuf[1];	/* length of extended message */
	uchar_t	nt_syncibuf[3];		/* room for sdtr inbound message */
	uchar_t	nt_statbuf[1];		/* status byte */
	uchar_t	nt_errmsgbuf[1];	/* error message for target */
	uchar_t	nt_wideibuf[2];		/* room for wdtr inbound msg. */
};

typedef struct glm_unit {
	int		nt_refcnt;	/* reference count */
	struct glm_dsa	*nt_dsap;
	ntd_t		nt_savedp;	/* saved S/G data pointers */
	ncmd_t		*nt_ncmdp;	/* cmd for active request */
	ncmd_t		*nt_waitq;	/* wait queue link pointer */
	ncmd_t		**nt_waitqtail;	/* wait queue tail ptr */
	struct glm_unit *nt_linkp;	/* wait queue link pointer */
	ulong_t		nt_dsa_addr;	/* addr of table indirects */

	ddi_dma_attr_t	nt_dma_attr;	/* per-target */
	ddi_dma_handle_t nt_dma_p;	/* per-target DMA handle */
	ddi_acc_handle_t nt_accessp;	/* handle for common access fns. */

	uchar_t		nt_state;	/* current state of this target */
	uchar_t		nt_type;	/* type of request */

	uchar_t		nt_goterror;	/* true if error occurred */
	uchar_t		nt_dma_status;	/* copy of DMA error bits */
	uchar_t		nt_status0;	/* copy of SCSI bus error bits */
	uchar_t		nt_status1;	/* copy of SCSI bus error bits */

	ushort		nt_target;	/* target number */
	ushort		nt_lun;		/* logical unit number */
	uchar_t		nt_fastscsi;	/* true if > 5MB/sec, tp < 200ns */
	uchar_t		nt_sscfX10;	/* sync i/o clock divisor */

	unsigned 	nt_arq : 1;	/* auto-request sense enable */
	unsigned	nt_tagque : 1;	/* tagged queueing enable */
	unsigned	nt_resv : 6;
} glm_unit_t;

/*
 * The states a HBA to (target, lun) nexus can have are:
 */
#define	NPT_STATE_DONE		((uchar_t)0) /* processing complete */
#define	NPT_STATE_IDLE		((uchar_t)1) /* HBA is waiting for work */
#define	NPT_STATE_WAIT		((uchar_t)2) /* HBA is waiting for reselect */
#define	NPT_STATE_QUEUED	((uchar_t)3) /* target request is queued */
#define	NPT_STATE_DISCONNECTED	((uchar_t)4) /* disconncted, wait reselect. */
#define	NPT_STATE_ACTIVE	((uchar_t)5) /* this target is the active one */

/*
 * types of requests passed to glm_send_cmd(), stored in nt_type
 */
#define	NRQ_NORMAL_CMD		((uchar_t)0)	/* normal command */
#define	NRQ_ABORT		((uchar_t)1)	/* Abort message */
#define	NRQ_ABORT_TAG		((uchar_t)2)	/* Abort Tag message */
#define	NRQ_DEV_RESET		((uchar_t)3)	/* Bus Device Reset message */

/*
 * These flags are passed with GLM_SETUP_SRIPT to tell
 * the driver to either restore the sync parameters or not.
 */
#define	GLM_SELECTION		0
#define	GLM_RESELECTION		1

/*
 * macro to get offset of glm_unit_t members for compiling into the SCRIPT
 */
#define	NTOFFSET(label) ((long)&(((struct glm_dsa *)0)->label))

typedef struct glm {
	int		g_instance;

	struct glm *g_next;

	scsi_hba_tran_t		*g_tran;
	kmutex_t		g_mutex;
	ddi_iblock_cookie_t	g_iblock;
	dev_info_t 		*g_dip;

	struct glmops	*g_ops;		/* ptr to GLM SIOP ops table */
	glm_unit_t	*g_units[N_GLM_UNITS];
					/* ptrs to per-target data */

	glm_unit_t	*g_current;	/* ptr to active target's DSA */
	glm_unit_t	*g_forwp;	/* ptr to front of the wait queue */
	glm_unit_t	*g_backp;	/* ptr to back of the wait queue */
	glm_unit_t	*g_hbap;	/* the HBA's target struct */
	ncmd_t		*g_doneq;	/* queue of completed commands */
	ncmd_t		**g_donetail;	/* queue tail ptr */

	ddi_acc_handle_t g_datap;	/* operating regs data access handle */
	caddr_t		g_devaddr;	/* ptr to io/mem mapped-in regs */

	ushort_t	g_devid;	/* device id of chip. */
	uchar_t		g_revid;	/* revision of chip. */

	uchar_t		g_sync_offset;	/* default offset for this chip. */

	/*
	 * Used for memory or onboard scripts.
	 */
	ddi_acc_handle_t g_ram_handle;
	caddr_t		g_scripts_ram;
	ulong_t		g_ram_base_addr;
#define	NSS_FUNCS	8	/* number of defined SCRIPT functions */
	uint_t		g_glm_scripts[NSS_FUNCS];
	uint_t		g_do_list_end;
	uint_t		g_di_list_end;

	/*
	 * Used for initializing configuration space.
	 */
	ddi_acc_handle_t g_conf_handle;
	caddr_t		g_conf_addr;

	/*
	 * scsi_options for bus and per target
	 */
	int		g_target_scsi_options_defined;
	int		g_scsi_options;
	int		g_target_scsi_options[NTARGETS_WIDE];

	/*
	 * These u_shorts are bit maps for targets
	 */
	u_short		g_wide_known;	/* wide negotiate on next cmd */
	u_short		g_nowide;	/* no wide for this target */
	u_short		g_wide_enabled;	/* wide enabled for this target */
	u_char		g_wdtr_sent;

	/*
	 * sync/wide backoff bit mask
	 */
	u_short		g_backoff;

	/*
	 * This u_short is a bit map for targets who need to have
	 * their properties update deferred.
	 */
	u_short		g_props_update;

	/*
	 * This	byte is	a bit map for targets who don't	appear
	 * to be able to support tagged	commands.
	 */
	u_short		g_notag;

	/*
	 * tag age limit per bus
	 */
	int		g_scsi_tag_age_limit;

	/*
	 * list of reset notification requests
	 */
	struct scsi_reset_notify_entry	*g_reset_notify_listf;

	/*
	 * scsi	reset delay per	bus
	 */
	u_int		g_scsi_reset_delay;

	int		g_sclock;	/* hba's SCLK freq. in MHz */
	int		g_speriod;	/* hba's SCLK period, in nsec. */

	/*
	 * hba's sync period.
	 */
	uchar_t		g_hba_period;

	/* sync i/o state flags */
	uchar_t		g_syncstate[NTARGETS_WIDE];
	/* min sync i/o period per tgt */
	int		g_minperiod[NTARGETS_WIDE];
	uchar_t		g_scntl3;	/* 53c8xx hba's core clock divisor */

	uchar_t		g_glmid;	/* this hba's target number and ... */
	uchar_t		g_disc_num;	/* number of disconnected devs */
	uchar_t		g_state;	/* the HBA's current state */
	uchar_t		g_polled_intr;	/* intr was polled. */
	uchar_t		g_suspended;	/* true	if driver is suspended */
} glm_t;
_NOTE(MUTEX_PROTECTS_DATA(glm::g_mutex, glm))
_NOTE(SCHEME_PROTECTS_DATA("save sharing", glm::g_next))
_NOTE(SCHEME_PROTECTS_DATA("stable data", glm::g_target_scsi_options))
_NOTE(SCHEME_PROTECTS_DATA("stable data", glm::g_dip glm::g_tran))

/*
 * HBA state.
 */
#define	NSTATE_IDLE		((uchar_t)0) /* HBA is idle */
#define	NSTATE_ACTIVE		((uchar_t)1) /* HBA is processing a target */
#define	NSTATE_WAIT_RESEL	((uchar_t)2) /* HBA is waiting for reselect */

/*
 * states of the hba while negotiating synchronous i/o with a target
 */
#define	NSYNCSTATE(glmp, nptp)	(glmp)->g_syncstate[(nptp)->nt_target]
#define	NSYNC_SDTR_NOTDONE	((uchar_t)0) /* SDTR negotiation needed */
#define	NSYNC_SDTR_SENT		((uchar_t)1) /* waiting for target SDTR msg */
#define	NSYNC_SDTR_RCVD		((uchar_t)2) /* target waiting for SDTR msg */
#define	NSYNC_SDTR_ACK		((uchar_t)3) /* ack target's SDTR message */
#define	NSYNC_SDTR_REJECT	((uchar_t)4) /* send Message Reject to target */
#define	NSYNC_SDTR_DONE		((uchar_t)5) /* final state */

/*
 * action codes for interrupt decode routines in interrupt.c
 */
#define	NACTION_DONE		0x01	/* target request is done */
#define	NACTION_ERR		0x02	/* target's request got error */
#define	NACTION_GOT_BUS_RESET	0x04	/* reset the SCSI bus */
#define	NACTION_SAVE_BCNT	0x08	/* save scatter/gather byte ptr */
#define	NACTION_SIOP_HALT	0x10	/* halt the current HBA program */
#define	NACTION_SIOP_REINIT	0x20	/* totally reinitialize the HBA */
#define	NACTION_SDTR		0x40	/* got SDTR interrupt */
#define	NACTION_EXT_MSG_OUT	0x80	/* send Extended message */
#define	NACTION_ACK		0x100	/* ack the last byte and continue */
#define	NACTION_CHK_INTCODE	0x200	/* SCRIPTS software interrupt */
#define	NACTION_INITIATOR_ERROR	0x400	/* send IDE message and then continue */
#define	NACTION_MSG_PARITY	0x800	/* send MPE error and then continue */
#define	NACTION_MSG_REJECT	0x1000	/* send MR and then continue */
#define	NACTION_BUS_FREE	0x2000	/* reselect error caused disconnect */
#define	NACTION_ABORT		0x4000	/* abort */
#define	NACTION_DO_BUS_RESET	0x8000	/* reset the SCSI bus */
#define	NACTION_CLEAR_CHIP	0x10000	/* clear chip's fifo's */

/*
 * regspec defines.
 */
#define	CONFIG_SPACE	0	/* regset[0] - configuration space */
#define	IO_SPACE	1	/* regset[1] - used for i/o mapped device */
#define	MEM_SPACE	2	/* regset[2] - used for memory mapped device */
#define	BASE_REG2	3	/* regset[3] - used for 875 scripts ram */

/*
 * Handy constants
 */
#define	FALSE		0
#define	TRUE		1
#define	UNDEFINED	-1
#define	MEG		(1000 * 1000)

/*
 * Handy macros
 */
#define	Tgt(pkt)	((pkt)->pkt_address.a_target)
#define	Lun(pkt)	((pkt)->pkt_address.a_lun)

/*
 * macro to return the effective address of a given per-target field
 */
#define	EFF_ADDR(start, offset)		((start) + (offset))

#define	SDEV2ADDR(devp)		(&((devp)->sd_address))
#define	SDEV2TRAN(devp)		((devp)->sd_address.a_hba_tran)
#define	PKT2TRAN(pktp)		((pktp)->pkt_address.a_hba_tran)
#define	ADDR2TRAN(ap)		((ap)->a_hba_tran)

#define	TRAN2GLM(hba)		((glm_t *)(hba)->tran_hba_private)
#define	SDEV2GLM(sd)		(TRAN2GLM(SDEV2TRAN(sd)))
#define	PKT2GLM(pktp)		(TRAN2GLM(PKT2TRAN(pktp)))
#define	PKT2GLMUNITP(pktp)	NTL2UNITP(PKT2GLM(pktp), \
					(pktp)->pkt_address.a_target, \
					(pktp)->pkt_address.a_lun)
#define	ADDR2GLM(ap)		(TRAN2GLM(ADDR2TRAN(ap)))
#define	ADDR2GLMUNITP(ap)	NTL2UNITP(ADDR2GLM(ap), \
					(ap)->a_target, (ap)->a_lun)

#define	NTL2UNITP(glm_blkp, targ, lun)	\
				((glm_blkp)->g_units[TL2INDEX(targ, lun)])

#define	POLL_TIMEOUT		(2 * SCSI_POLL_TIMEOUT * 1000000)
#define	SHORT_POLL_TIMEOUT	(1000000)	/* in usec, about 1 secs */

/*
 * Map (target, lun) to g_units array index
 */
#define	TL2INDEX(target, lun)	((target) * NLUNS_PER_TARGET + (lun))

/*
 * Op vectors
 */
typedef	struct glmops {
	char	*glm_chip;
	void	(*glm_reset)(glm_t *glmp);
	void	(*glm_init)(glm_t *glmp);
	void	(*glm_enable)(glm_t *glmp);
	void	(*glm_disable)(glm_t *glmp);
	uchar_t	(*glm_get_istat)(glm_t *glmp);
	void	(*glm_halt)(glm_t *glmp);
	void	(*glm_set_sigp)(glm_t *glmp);
	void	(*glm_reset_sigp)(glm_t *glmp);
	ulong	(*glm_get_intcode)(glm_t *glmp);
	void	(*glm_check_error)(glm_unit_t *nptp, struct scsi_pkt *pktp);
	ulong	(*glm_dma_status)(glm_t *glmp);
	ulong	(*glm_scsi_status)(glm_t *glmp);
	int	(*glm_save_byte_count)(glm_t *glmp, glm_unit_t *nptp);
	int	(*glm_get_target)(glm_t *glmp, uchar_t *tp);
	void	(*glm_setup_script)(glm_t *glmp, glm_unit_t *nptp, int resel);
	void	(*glm_start_script)(glm_t *glmp, int script);
	void	(*glm_set_syncio)(glm_t *glmp, glm_unit_t *nptp);
	void	(*glm_bus_reset)(glm_t *glmp);
} nops_t;

#define	GLM_RESET(P)			((P)->g_ops->glm_reset)(P)
#define	GLM_INIT(P)			((P)->g_ops->glm_init)(P)
#define	GLM_ENABLE_INTR(P)		((P)->g_ops->glm_enable)(P)
#define	GLM_DISABLE_INTR(P)		((P)->g_ops->glm_disable)(P)
#define	GLM_GET_ISTAT(P)		((P)->g_ops->glm_get_istat)(P)
#define	GLM_HALT(P)			((P)->g_ops->glm_halt)(P)
#define	GLM_SET_SIGP(P)			((P)->g_ops->glm_set_sigp)(P)
#define	GLM_RESET_SIGP(P)		((P)->g_ops->glm_reset_sigp)(P)
#define	GLM_GET_INTCODE(P)		((P)->g_ops->glm_get_intcode)(P)
#define	GLM_CHECK_ERROR(P, nptp, pktp)	((P)->g_ops->glm_check_error)(nptp, \
						pktp)
#define	GLM_DMA_STATUS(P)		((P)->g_ops->glm_dma_status)(P)
#define	GLM_SCSI_STATUS(P)		((P)->g_ops->glm_scsi_status)(P)
#define	GLM_SAVE_BYTE_COUNT(P, nptp)	((P)->g_ops->glm_save_byte_count)(P, \
						nptp)
#define	GLM_GET_TARGET(P, tp)		((P)->g_ops->glm_get_target)(P, tp)
#define	GLM_SETUP_SCRIPT(P, nptp, resel) \
	((P)->g_ops->glm_setup_script)(P, nptp, resel)
#define	GLM_START_SCRIPT(P, script)	((P)->g_ops->glm_start_script)(P, \
						script)
#define	GLM_SET_SYNCIO(P, nptp)		((P)->g_ops->glm_set_syncio)(P, nptp)
#define	GLM_BUS_RESET(P)		((P)->g_ops->glm_bus_reset)(P)

#define	INTPENDING(glm) \
	(GLM_GET_ISTAT(glm) & (NB_ISTAT_DIP | NB_ISTAT_SIP))

/*
 * All models of the NCR are assumed to have consistent definitions
 * of the following bits in the ISTAT register. The ISTAT register
 * can be at different offsets but these bits must be the same.
 * If this isn't true then we'll have to add functions to the
 * glmops table to access these bits similar to how the glm_get_intcode()
 * function is defined.
 */
#define	NB_ISTAT_CON		0x08	/* connected */
#define	NB_ISTAT_SIP		0x02	/* scsi interrupt pending */
#define	NB_ISTAT_DIP		0x01	/* dma interrupt pending */

/*
 * Function Codes for the SCRIPTS entry points
 */
#define	NSS_STARTUP		0	/* select target and start request */
#define	NSS_CONTINUE		1	/* continue after phase mismatch */
#define	NSS_WAIT4RESELECT	2	/* wait for reselect */
#define	NSS_CLEAR_ACK		3	/* continue after both SDTR msgs okay */
#define	NSS_EXT_MSG_OUT		4	/* send out extended msg to target */
#define	NSS_ERR_MSG		5	/* send Message Reject message */
#define	NSS_BUS_DEV_RESET	6	/* do Bus Device Reset */
#define	NSS_ABORT		7	/* abort commands */

/*
 * SCRIPTS command opcodes for Block Move instructions
 */
#define	NSOP_MOVE_MASK		0xF8	/* just the opcode bits */
#define	NSOP_MOVE		0x18	/* MOVE FROM ... */
#define	NSOP_CHMOV		0x08	/* CHMOV FROM ... */

#define	NSOP_PHASE		0x0f	/* the expected phase bits */
#define	NSOP_DATAOUT		0x00	/* data out */
#define	NSOP_DATAIN		0x01	/* data in */
#define	NSOP_COMMAND		0x02	/* command */
#define	NSOP_STATUS		0x03	/* status */
#define	NSOP_MSGOUT		0x06	/* message out */
#define	NSOP_MSGIN		0x07	/* message out */

/*
 * Interrupt vectors numbers that script may generate
 */
#define	NINT_OK		0xff00		/* device accepted the command */
#define	NINT_ILI_PHASE	0xff01		/* Illegal Phase */
#define	NINT_UNS_MSG	0xff02		/* Unsupported message */
#define	NINT_UNS_EXTMSG 0xff03		/* Unsupported extended message */
#define	NINT_MSGIN	0xff04		/* Message in expected */
#define	NINT_MSGREJ	0xff05		/* Message reject */
#define	NINT_RESEL	0xff06		/* C710 chip reselcted */
#define	NINT_SELECTED	0xff07		/* C710 chip selected */
#define	NINT_DISC	0xff09		/* Diconnect message received */
#define	NINT_RESEL_ERR	0xff0a		/* Reselect id error */
#define	NINT_SDP_MSG	0xff0b		/* Save Data Pointer message */
#define	NINT_RP_MSG	0xff0c		/* Restore Pointer message */
#define	NINT_SIGPROC	0xff0e		/* Signal Process */
#define	NINT_TOOMUCHDATA 0xff0f		/* Too much data to/from target */
#define	NINT_SDTR	0xff10		/* SDTR message received */
#define	NINT_NEG_REJECT 0xff11		/* invalid negotiation exchange */
#define	NINT_REJECT	0xff12		/* failed to send msg reject */
#define	NINT_DEV_RESET	0xff13		/* bus device reset completed */
#define	NINT_WDTR	0xff14		/* WDTR complete. */

/*
 * defaults for	the global properties
 */
#define	DEFAULT_SCSI_OPTIONS	SCSI_OPTIONS_DR
#define	DEFAULT_TAG_AGE_LIMIT	2
#define	DEFAULT_RESET_DELAY	3000
#define	DEFAULT_WD_TICK		10

/*
 * debugging.
 */
#if defined(GLM_DEBUG)

static void glm_printf(char *fmt, ...);

#define	GLM_DBGPR(m, args)	\
	if (glm_debug_flags & (m)) \
		glm_printf args
#else	/* ! defined(GLM_DEBUG) */
#define	GLM_DBGPR(m, args)
#endif	/* defined(GLM_DEBUG) */

#define	NDBG0(args)	GLM_DBGPR(0x01, args)
#define	NDBG1(args)	GLM_DBGPR(0x02, args)
#define	NDBG2(args)	GLM_DBGPR(0x04, args)
#define	NDBG3(args)	GLM_DBGPR(0x08, args)

#define	NDBG4(args)	GLM_DBGPR(0x10, args)
#define	NDBG5(args)	GLM_DBGPR(0x20, args)
#define	NDBG6(args)	GLM_DBGPR(0x40, args)
#define	NDBG7(args)	GLM_DBGPR(0x80, args)

#define	NDBG8(args)	GLM_DBGPR(0x0100, args)
#define	NDBG9(args)	GLM_DBGPR(0x0200, args)
#define	NDBG10(args)	GLM_DBGPR(0x0400, args)
#define	NDBG11(args)	GLM_DBGPR(0x0800, args)

#define	NDBG12(args)	GLM_DBGPR(0x1000, args)
#define	NDBG13(args)	GLM_DBGPR(0x2000, args)
#define	NDBG14(args)	GLM_DBGPR(0x4000, args)
#define	NDBG15(args)	GLM_DBGPR(0x8000, args)

#define	NDBG16(args)	GLM_DBGPR(0x010000, args)
#define	NDBG17(args)	GLM_DBGPR(0x020000, args)
#define	NDBG18(args)	GLM_DBGPR(0x040000, args)
#define	NDBG19(args)	GLM_DBGPR(0x080000, args)

#define	NDBG20(args)	GLM_DBGPR(0x100000, args)
#define	NDBG21(args)	GLM_DBGPR(0x200000, args)
#define	NDBG22(args)	GLM_DBGPR(0x400000, args)
#define	NDBG23(args)	GLM_DBGPR(0x800000, args)

#define	NDBG24(args)	GLM_DBGPR(0x1000000, args)
#define	NDBG25(args)	GLM_DBGPR(0x2000000, args)
#define	NDBG26(args)	GLM_DBGPR(0x4000000, args)
#define	NDBG27(args)	GLM_DBGPR(0x8000000, args)

#define	NDBG28(args)	GLM_DBGPR(0x10000000, args)
#define	NDBG29(args)	GLM_DBGPR(0x20000000, args)
#define	NDBG30(args)	GLM_DBGPR(0x40000000, args)
#define	NDBG31(args)	GLM_DBGPR(0x80000000, args)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_ADAPTERS_GLMVAR_H */
