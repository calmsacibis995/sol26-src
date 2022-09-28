/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SCSI_ADAPTERS_ISPVAR_H
#define	_SYS_SCSI_ADAPTERS_ISPVAR_H

#pragma ident	"@(#)ispvar.h	1.67	96/10/18 SMI"
#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Convenient short hand defines
 */
#define	TRUE			 1
#define	FALSE			 0
#define	UNDEFINED		-1

#define	CNUM(isp)		(ddi_get_instance(isp->isp_tran.tran_dev))

#define	ISP_RETRY_DELAY		5
#define	ISP_RETRIES		0	/* retry of selections */
#define	ISP_INITIAL_SOFT_SPACE	5	/* Used for the softstate_init func */

#define	MSW(x)			(short)(((long)x >> 16) & 0xFFFF)
#define	LSW(x)			(short)((long)x & 0xFFFF)

#define	MSB(x)			(char)(((short)x >> 8) & 0xFF)
#define	LSB(x)			(char)((short)x & 0xFF)

#define	TGT(sp)			(CMD2PKT(sp)->pkt_address.a_target)
#define	LUN(sp)			(CMD2PKT(sp)->pkt_address.a_lun)

#define	ISP_SBUS		0
#define	ISP_PCI			1

/*
 *  Use for Qfull Capability
 */
#define	ISP_GET_QFULL_CAP	1
#define	ISP_SET_QFULL_CAP	0

/*
 *	Tag reject
 */
#define	TAG_REJECT	28

/*
 * Interrupt actions returned by isp_i_flag_event()
 */
#define	ACTION_CONTINUE		0	/* Continue */
#define	ACTION_RETURN		1	/* Exit */
#define	ACTION_IGNORE		2	/* Ignore */

/*
 * Reset actions for isp_i_reset_interface()
 */
#define	ISP_RESET_BUS_IF_BUSY	0x01	/* reset scsi bus if it is busy */
#define	ISP_FORCE_RESET_BUS	0x02	/* reset scsi bus on error reco */
#define	ISP_DOWNLOAD_FW_ON_ERR	0x04	/* Download the firmware after reset */
#define	ISP_FIRMWARE_ERROR	0x08    /* Sys_Err Async Event happened */


/*
 * firmware download options for isp_i_download_fw()
 */
#define	ISP_DOWNLOAD_FW_OFF		0
#define	ISP_DOWNLOAD_FW_IF_NEWER	1
#define	ISP_DOWNLOAD_FW_ALWAYS		2

/*
 * extracting period and offset from isp_synch
 */
#define	PERIOD_MASK(val)	((val) & 0xff)
#define	OFFSET_MASK(val)	(((val) >>8) & 0xff)

/*
 * timeout values
 */
#define	ISP_GRACE		10	/* Timeout margin (sec.) */
#define	ISP_TIMEOUT_DELAY(secs, delay)	(secs * (1000000 / delay))

/*
 * delay time for polling loops
 */
#define	ISP_NOINTR_POLL_DELAY_TIME		1000	/* usecs */

/*
 * busy wait delay time after chip reset
 */
#define	ISP_CHIP_RESET_BUSY_WAIT_TIME		100	/* usecs */

/*
 * timeout for ISP coming out of reset
 */
#define	ISP_RESET_WAIT				1000	/* ms */
#define	ISP_SOFT_RESET_TIME			1	/* second */


/*
 * Debugging macros
 */
#ifdef ISPDEBUG
#define	ISP_DEBUG	if (ispdebug) isp_i_log
#define	ISP_DEBUG2	if (ispdebug > 1) isp_i_log
#else	/* ISPDEBUG */
#define	ispdebug	(0)
#define	INFORMATIVE	(0)
#define	DEBUGGING	(0)
#define	DEBUGGING_ALL	(0)

#define	ISP_DEBUG	if (0) isp_i_log
#define	ISP_DEBUG2	if (0) isp_i_log
#endif /* ISPDEBUG */

/*
 * Size definitions for request and response queues.
 */
#define	ISP_MAX_REQUESTS	256
#define	ISP_MAX_RESPONSES	256
#define	ISP_QUEUE_SIZE		\
	(ISP_MAX_REQUESTS  * sizeof (struct isp_request) + \
	    ISP_MAX_RESPONSES * sizeof (struct isp_response))

/*
 * ISP request packet as defined by the Firmware Interface
 */
struct isp_dataseg {
	long	d_base;
	long	d_count;
};


struct cq_header {
#if defined(_BIG_ENDIAN)
	u_char	cq_entry_count;
	u_char	cq_entry_type;
	u_char	cq_flags;
	u_char	cq_seqno;
#else
	u_char	cq_entry_type;
	u_char	cq_entry_count;
	u_char	cq_seqno;
	u_char	cq_flags;
#endif
};

struct isp_request {
	struct cq_header	req_header;
	opaque_t		req_token;

#if defined(_BIG_ENDIAN)
	u_char			req_target;
	u_char			req_lun_trn;
#else
	u_char			req_lun_trn;
	u_char			req_target;
#endif

	u_short			req_cdblen;
#define	req_modifier		req_cdblen	/* marker packet */
	u_short			req_flags;
	u_short			req_reserved;
	u_short			req_time;
	u_short			req_seg_count;

	u_long			req_cdb[3];
	struct isp_dataseg	req_dataseg[4];
};

#define	ISP_REQ_TOKEN_OFF	sizeof (struct cq_header)
#define	ISP_REQ_LUN_OFF		ISP_REQ_TOKEN_OFF + sizeof (opaque_t)
#define	ISP_REQ_CDB_OFF		ISP_REQ_LUN_OFF + 6 * sizeof (u_short)
#define	ISP_REQ_DATA_OFF	ISP_REQ_CDB_OFF + 3 * sizeof (u_long)

#define	ISP_UPDATE_QUEUE_SPACE(isp) \
	isp->isp_request_out = ISP_GET_REQUEST_OUT(isp); \
	if (isp->isp_request_in == isp->isp_request_out) { \
		isp->isp_queue_space = ISP_MAX_REQUESTS - 1; \
	} else if (isp->isp_request_in > isp->isp_request_out) { \
		isp->isp_queue_space = ((ISP_MAX_REQUESTS - 1) - \
		    (isp->isp_request_in - isp->isp_request_out)); \
	} else { \
		isp->isp_queue_space = isp->isp_request_out - \
		    isp->isp_request_in - 1; \
	}

/*
 * Header flags definitions
 */
#define	CQ_FLAG_CONTINUATION	0x01
#define	CQ_FLAG_FULL		0x02
#define	CQ_FLAG_BADHEADER	0x04
#define	CQ_FLAG_BADPACKET	0x08
#define	CQ_FLAG_ERR_MASK	\
	(CQ_FLAG_FULL | CQ_FLAG_BADHEADER | CQ_FLAG_BADPACKET)

/*
 * Header entry_type definitions
 */
#define	CQ_TYPE_REQUEST		1
#define	CQ_TYPE_DATASEG		2
#define	CQ_TYPE_RESPONSE	3
#define	CQ_TYPE_MARKER		4
#define	CQ_TYPE_CMDONLY		5

/*
 * Copy cdb into request using long word transfers to save time.
 */
#define	ISP_CDBMAX	12
#define	ISP_LOAD_REQUEST_CDB(req, sp, cdbsize) { \
	register long *cdbp, *sp_cdbp; \
	(req)->req_cdblen = (short)(cdbsize); \
	cdbp = (long *)(req)->req_cdb; \
	sp_cdbp = (long *)CMD2PKT(sp)->pkt_cdbp; \
	*cdbp = *sp_cdbp, *(cdbp+1) = *(sp_cdbp+1), \
	*(cdbp+2) = *(sp_cdbp+2); \
}

/*
 * marker packet (req_modifier) values
 */
#define	SYNCHRONIZE_NEXUS	0
#define	SYNCHRONIZE_TARGET	1
#define	SYNCHRONIZE_ALL		2

/*
 * request flag values
 */
#define	ISP_REQ_FLAG_NODISCON		0x0001
#define	ISP_REQ_FLAG_HEAD_TAG		0x0002
#define	ISP_REQ_FLAG_ORDERED_TAG	0x0004
#define	ISP_REQ_FLAG_SIMPLE_TAG		0x0008
#define	ISP_REQ_FLAG_USE_TRN		0x0010
#define	ISP_REQ_FLAG_DATA_READ		0x0020
#define	ISP_REQ_FLAG_DATA_WRITE		0x0040
#define	ISP_REQ_FLAG_DATA_WRITE		0x0040
#define	ISP_REQ_FLAG_DISARQ		0x0100

/*
 * translate scsi_pkt flags into ISP request packet flags
 * It would be illegal if two flags are set; the driver does not
 * check for this. Setting NODISCON and a tag flag is harmless.
 */
#define	ISP_SET_PKT_FLAGS(scsa_flags, isp_flags) {		\
	isp_flags = (scsa_flags >> 11) & 0xe; /* tags */ \
	isp_flags |= (scsa_flags >> 1) & 0x1; /* no disconnect */  \
}

/*
 * isp_request size minus header.
 */
#define	ISP_PAYLOAD		\
	(sizeof (struct isp_request) - sizeof (struct cq_header))

/*
 * throttle values for ISP request queue
 */
#define	SHUTDOWN_THROTTLE	-1	/* do not submit any requests */
#define	CLEAR_THROTTLE		(ISP_MAX_REQUESTS -1)

/*
 * XXX: Note, this request queue macro *ASSUMES* that queue full cannot
 *	occur.
 */
#define	ISP_GET_NEXT_REQUEST_IN(isp, ptr) { \
	(ptr) = (isp)->isp_request_ptr; \
	if ((isp)->isp_request_in == (ISP_MAX_REQUESTS - 1)) {	 \
		(isp)->isp_request_in = 0; \
		(isp)->isp_request_ptr = (isp)->isp_request_base; \
	} else { \
		(isp)->isp_request_in++; \
		(isp)->isp_request_ptr++; \
	} \
}

/*
 * slots queue for isp timeout handling
 * Must be a multiple of 8
 */
#define	ISP_DISK_QUEUE_DEPTH	100
#define	ISP_MAX_SLOTS		((NTARGETS_WIDE * ISP_DISK_QUEUE_DEPTH) + \
				ISP_MAX_REQUESTS)

/*
 * ISP response packet as defined by the Firmware Interface
 */
struct isp_response {
	struct cq_header	resp_header;
	opaque_t		resp_token;

	u_short			resp_scb;
	u_short			resp_reason;
	u_short			resp_state;
	u_short			resp_status_flags;
	u_short			resp_time;
	u_short			resp_rqs_count;

	u_long			resp_resid;
	u_long			resp_reserved[2];
	u_long			resp_request_sense[8];
};

#define	ISP_RESP_TOKEN_OFF	sizeof (struct cq_header)
#define	ISP_RESP_STATUS_OFF	ISP_RESP_TOKEN_OFF + sizeof (opaque_t)
#define	ISP_RESP_RESID_OFF	ISP_RESP_STATUS_OFF + 6 * sizeof (u_short)
#define	ISP_RESP_RQS_OFF	ISP_RESP_RESID_OFF + 3 * sizeof (u_long)

#define	ISP_GET_NEXT_RESPONSE_OUT(isp, ptr) { \
	(ptr) = (isp)->isp_response_ptr; \
	if ((isp)->isp_response_out == (ISP_MAX_RESPONSES - 1)) {  \
		(isp)->isp_response_out = 0; \
		(isp)->isp_response_ptr = (isp)->isp_response_base; \
	} else { \
		(isp)->isp_response_out++; \
		(isp)->isp_response_ptr++; \
	} \
}

#define	ISP_IS_RESPONSE_INVALID(resp) \
	((resp)->resp_header.cq_entry_type != CQ_TYPE_RESPONSE)


#define	ISP_GET_PKT_STATE(state)	((u_long) (state >> 8))
#define	ISP_GET_PKT_STATS(stats)	((u_long) (stats))

#define	ISP_STAT_NEGOTIATE	0x0080

#define	ISP_SET_REASON(sp, reason) { \
	if ((sp) && CMD2PKT(sp)->pkt_reason == CMD_CMPLT) \
		CMD2PKT(sp)->pkt_reason = (reason); \
}

/*
 * mutex and semaphore short hands
 */
#define	ISP_MBOX_SEMA(isp)	(&isp->isp_mbox.mbox_sema)

#define	ISP_REQ_MUTEX(isp)	(&isp->isp_request_mutex)
#define	ISP_RESP_MUTEX(isp)	(&isp->isp_response_mutex)
#define	ISP_WAITQ_MUTEX(isp)	(&isp->isp_waitq_mutex)


#define	ISP_MUTEX_ENTER(isp)	mutex_enter(ISP_RESP_MUTEX(isp)),	\
				mutex_enter(ISP_REQ_MUTEX(isp))
#define	ISP_MUTEX_EXIT(isp)	mutex_exit(ISP_RESP_MUTEX(isp)),	\
				mutex_exit(ISP_REQ_MUTEX(isp))


/*
 * HBA interface macros
 */
#define	SDEV2TRAN(sd)		((sd)->sd_address.a_hba_tran)
#define	SDEV2ADDR(sd)		(&((sd)->sd_address))
#define	PKT2TRAN(pkt)		((pkt)->pkt_address.a_hba_tran)
#define	ADDR2TRAN(ap)		((ap)->a_hba_tran)

#define	TRAN2ISP(tran)		((struct isp *)(tran)->tran_hba_private)
#define	SDEV2ISP(sd)		(TRAN2ISP(SDEV2TRAN(sd)))
#define	PKT2ISP(pkt)		(TRAN2ISP(PKT2TRAN(pkt)))
#define	ADDR2ISP(ap)		(TRAN2ISP(ADDR2TRAN(ap)))

#define	CMD2ADDR(cmd)		(&CMD2PKT(cmd)->pkt_address)
#define	CMD2TRAN(cmd)		(CMD2PKT(cmd)->pkt_address.a_hba_tran)
#define	CMD2ISP(cmd)		(TRAN2ISP(CMD2TRAN(cmd)))


/*
 * isp softstate structure
 */

/*
 * deadline slot structure for timeout handling
 */
struct isp_slot {
	struct isp_cmd *slot_cmd;
#ifdef OLDTIMEOUT
	clock_t		slot_deadline;
#endif
};


struct isp {

	/*
	 * Transport structure for this instance of the hba
	 */
	scsi_hba_tran_t		*isp_tran;

	/*
	 * dev_info_t reference can be found in the transport structure
	 */
	dev_info_t		*isp_dip;

	/*
	 * Bus the card is connected to. 0 - Sbus, 1 - PCI
	 */
	u_char			isp_bus;

	/*
	 * Clock frequency of the chip
	 */
	int			isp_clock_frequency;

	/*
	 * Interrupt block cookie
	 */
	ddi_iblock_cookie_t	isp_iblock;

	/*
	 * linked list of all isp's for isp_intr_loop() and debugging
	 */
	struct isp		*isp_next;

	/*
	 * Firmware revision number
	 */
	u_short			isp_major_rev;
	u_short			isp_minor_rev;
	u_short			isp_cust_prod;

	/*
	 * scsi options, scsi_tag_age_limit  per isp
	 */
	u_short			isp_target_scsi_options_defined;
	int			isp_scsi_options;
	int			isp_target_scsi_options[NTARGETS_WIDE];
	int			isp_scsi_tag_age_limit;

	/*
	 * scsi_reset_delay per isp
	 */
	u_int			isp_scsi_reset_delay;

	/*
	 * current host ID
	 */
	u_char			isp_initiator_id;

	/*
	 * suspended flag for power management
	 */
	u_char			isp_suspended;

	/*
	 * Host adapter capabilities and offset/period values per target
	 */
	u_short			isp_cap[NTARGETS_WIDE];
	u_short			isp_synch[NTARGETS_WIDE];

	/*
	 * ISP Hardware register pointers.
	 */
	struct isp_biu_regs	*isp_biu_reg;
	struct isp_mbox_regs	*isp_mbox_reg;
	struct isp_sxp_regs	*isp_sxp_reg;
	struct isp_risc_regs	*isp_risc_reg;

	uint_t			isp_reg_number;

	/*
	 * mbox values are stored here before and after the mbox cmd
	 * (protected by semaphore inside isp_mbox)
	 */
	struct isp_mbox		isp_mbox;

	/*
	 * shutdown flag if things get really confused
	 */
	u_char			isp_shutdown;

	/*
	 * flag for updating properties in isp_i_watch()
	 * to avoid updating in interrupt context
	 */
	u_short			isp_prop_update;

	/*
	 * request and response queue dvma space
	 */
	caddr_t			isp_cmdarea;
	ddi_dma_cookie_t	isp_dmacookie;
	ddi_dma_handle_t	isp_dmahandle;
	ddi_acc_handle_t	isp_dma_acc_handle;
	u_long			isp_request_dvma,
				isp_response_dvma;
	/*
	 * data access handles
	 */
	ddi_acc_handle_t	isp_pci_config_acc_handle;
	ddi_acc_handle_t	isp_biu_acc_handle;
	ddi_acc_handle_t	isp_mbox_acc_handle;
	ddi_acc_handle_t	isp_sxp_acc_handle;
	ddi_acc_handle_t	isp_risc_acc_handle;

	/*
	 * ISP input request and output response queue pointers
	 * and mutexes protecting request and response queue
	 */
	u_int			isp_queue_space;
	kmutex_t		isp_request_mutex;
	kmutex_t		isp_response_mutex;
	u_short			isp_request_in,
				isp_request_out;
	u_short			isp_response_in,
				isp_response_out;

	struct isp_request	*isp_request_ptr,
				*isp_request_base;
	struct isp_response	*isp_response_ptr,
				*isp_response_base;
	/*
	 * waitQ (used for storing cmds in case request mutex is held)
	 */
	kmutex_t		isp_waitq_mutex;
	struct	isp_cmd		*isp_waitf;
	struct	isp_cmd		*isp_waitb;
	int			isp_waitq_timeout;

	int			isp_burst_size;
	u_short			isp_conf1_fifo;


#ifdef ISP_PERF
	/*
	 * performance counters
	 */
	u_int			isp_request_count,
				isp_mail_requests;
	u_int			isp_intr_count,
				isp_perf_ticks;
	u_int			isp_rpio_count,
				isp_wpio_count;
#endif

	/*
	 * These are for handling cmd. timeouts.
	 *
	 * Because the ISP request queue is a round-robin, entries
	 * in progress can be overwritten. In order to provide crash
	 * recovery, we have to keep a list of requests in progress
	 * here.
	 */
	u_short			isp_free_slot;
#ifdef OLDTIMEOUT
	u_short			isp_last_slot_watched;
#else
	u_short			isp_alive;
#endif

	/*
	 * list of reset notification requests
	 */
	struct scsi_reset_notify_entry	*isp_reset_notify_listf;
	struct kmem_cache		*isp_kmem_cache;

	struct	isp_slot	isp_slots[ISP_MAX_SLOTS];
};


_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_tran))
_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_major_rev))
_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_minor_rev))
_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_initiator_id))
_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_cmdarea isp::isp_dmahandle))
_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_dmacookie))

_NOTE(SCHEME_PROTECTS_DATA("Failure Mode", isp::isp_shutdown))
_NOTE(SCHEME_PROTECTS_DATA("Semaphore", isp::isp_mbox))
_NOTE(SCHEME_PROTECTS_DATA("save sharing", isp::isp_alive))
_NOTE(SCHEME_PROTECTS_DATA("save sharing", isp::isp_prop_update))
_NOTE(SCHEME_PROTECTS_DATA("save sharing", isp::isp_target_scsi_options))

#ifdef OLDTIMEOUT
_NOTE(SCHEME_PROTECTS_DATA("Watch Thread", isp::isp_last_slot_watched))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_request_mutex, isp::isp_free_slot))
#endif

_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_request_base
				isp::isp_response_base))
_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_request_dvma
				isp::isp_response_dvma))
_NOTE(DATA_READABLE_WITHOUT_LOCK(isp::isp_request_dvma
				isp::isp_response_dvma))

_NOTE(SCHEME_PROTECTS_DATA("HW Registers", isp::isp_biu_reg))
_NOTE(SCHEME_PROTECTS_DATA("HW Registers", isp::isp_mbox_reg))
_NOTE(SCHEME_PROTECTS_DATA("HW Registers", isp::isp_sxp_reg))
_NOTE(SCHEME_PROTECTS_DATA("HW Registers", isp::isp_risc_reg))

_NOTE(MUTEX_PROTECTS_DATA(isp::isp_request_mutex,
	isp::isp_cap isp::isp_synch))
_NOTE(SCHEME_PROTECTS_DATA("ISR only accesses after fully prepared",
	isp::isp_slots))

_NOTE(MUTEX_PROTECTS_DATA(isp::isp_waitq_mutex,
	isp::isp_waitf isp::isp_waitb))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_request_mutex, isp::isp_queue_space))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_request_mutex, isp::isp_request_in))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_request_mutex, isp::isp_request_out))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_request_mutex, isp::isp_request_ptr))

_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp::isp_response_in))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp::isp_response_out))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp::isp_response_ptr))

/*
 * Convenient short-hand macros for reading/writing ISP registers
 */
#define	ISP_WRITE_BIU_REG(isp, regp, value)				\
	ddi_putw((isp)->isp_biu_acc_handle, (regp), (value))

#define	ISP_READ_BIU_REG(isp, regp)					\
	ddi_getw((isp)->isp_biu_acc_handle, (regp))

#define	ISP_WRITE_MBOX_REG(isp, regp, value)				\
	ddi_putw((isp)->isp_mbox_acc_handle, (regp), (value))

#define	ISP_READ_MBOX_REG(isp, regp)					\
	ddi_getw((isp)->isp_mbox_acc_handle, (regp))

#define	ISP_WRITE_SXP_REG(isp, regp, value)				\
	ddi_putw((isp)->isp_sxp_acc_handle, (regp), (value))

#define	ISP_READ_SXP_REG(isp, regp)					\
	ddi_getw((isp)->isp_sxp_acc_handle, (regp))

#define	ISP_WRITE_RISC_REG(isp, regp, value)				\
	ddi_putw((isp)->isp_risc_acc_handle, (regp), (value))

#define	ISP_READ_RISC_REG(isp, regp)					\
	ddi_getw((isp)->isp_risc_acc_handle, (regp))

/*
 * Convenient short-hand macros for setting/clearing register bits
 */
#define	ISP_SET_BIU_REG_BITS(isp, regp, value)				\
	ISP_WRITE_BIU_REG((isp), (regp),				\
		(ISP_READ_BIU_REG((isp), (regp)) | (value)))

#define	ISP_CLR_BIU_REG_BITS(isp, regp, value)				\
	ISP_WRITE_BIU_REG((isp), (regp),				\
		(ISP_READ_BIU_REG((isp), (regp)) & ~(value)))

/*
 * Hardware  access definitions for ISP chip
 *
 */
#ifdef ISP_PERF
#define	ISP_WRITE_RISC_HCCR(isp, value)					\
	ISP_WRITE_RISC_REG((isp), &(isp)->isp_risc_reg->isp_hccr,	\
		(value));						\
	(isp)->isp_wpio_count++

#define	ISP_READ_RISC_HCCR(isp)						\
	((isp)->isp_rpio_count++,					\
	ISP_READ_RISC_REG((isp), &(isp)->isp_risc_reg->isp_hccr))

#define	ISP_REG_GET_RISC_INT(isp)					\
	((isp)->isp_rpio_count++,					\
	(ISP_READ_BIU_REG((isp), &(isp)->isp_biu_reg->isp_bus_isr) &	\
		ISP_BUS_ISR_RISC_INT))

#define	ISP_CLEAR_SEMAPHORE_LOCK(isp)					\
	ISP_CLR_BIU_REG_BITS((isp), &(isp)->isp_biu_reg->isp_bus_sema,\
		ISP_BUS_SEMA_LOCK);					\
	(isp)->isp_wpio_count++, (isp)->isp_rpio_count++

#define	ISP_SET_REQUEST_IN(isp)						\
	ISP_WRITE_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox4,	\
		(isp)->isp_request_in);					\
	(isp)->isp_wpio_count++, (isp)->isp_request_count++

#define	ISP_SET_RESPONSE_OUT(isp)					\
	ISP_WRITE_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox5,	\
		isp->isp_response_out);					\
	(isp)->isp_wpio_count++

#define	ISP_GET_REQUEST_OUT(isp)					\
	((isp)->isp_rpio_count++,					\
	ISP_READ_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox4))

#define	ISP_GET_RESPONSE_IN(isp)					\
	((isp)->isp_rpio_count++,					\
	ISP_READ_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox5))

#define	ISP_INT_PENDING(isp)						\
	((isp)->isp_rpio_count++,					\
	(ISP_READ_BIU_REG((isp), &(isp)->isp_biu_reg->isp_bus_isr) &	\
		ISP_BUS_ISR_RISC_INT))

#define	ISP_CHECK_SEMAPHORE_LOCK(isp)					\
	((isp)->isp_rpio_count++,					\
	(ISP_READ_BIU_REG((isp), &(isp)->isp_biu_reg->isp_bus_sema) &	\
		ISP_BUS_SEMA_LOCK))

#else	/* ISP_PERF */
#define	ISP_WRITE_RISC_HCCR(isp, value)					\
	ISP_WRITE_RISC_REG((isp), &(isp)->isp_risc_reg->isp_hccr,	\
		(value));

#define	ISP_READ_RISC_HCCR(isp)						\
	ISP_READ_RISC_REG((isp), &(isp)->isp_risc_reg->isp_hccr)

#define	ISP_REG_GET_RISC_INT(isp)					\
	(ISP_READ_BIU_REG((isp), &(isp)->isp_biu_reg->isp_bus_isr) &	\
		ISP_BUS_ISR_RISC_INT)

#define	ISP_CLEAR_SEMAPHORE_LOCK(isp)					\
	ISP_CLR_BIU_REG_BITS((isp), &(isp)->isp_biu_reg->isp_bus_sema,	\
		ISP_BUS_SEMA_LOCK)

#define	ISP_SET_REQUEST_IN(isp)						\
	ISP_WRITE_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox4,	\
		(isp)->isp_request_in)

#define	ISP_SET_RESPONSE_OUT(isp)					\
	ISP_WRITE_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox5,	\
		isp->isp_response_out)

#define	ISP_GET_REQUEST_OUT(isp)					\
	ISP_READ_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox4)

#define	ISP_GET_RESPONSE_IN(isp)					\
	ISP_READ_MBOX_REG((isp), &(isp)->isp_mbox_reg->isp_mailbox5)

#define	ISP_INT_PENDING(isp)						\
	(ISP_READ_BIU_REG((isp), &(isp)->isp_biu_reg->isp_bus_isr) &	\
		ISP_BUS_ISR_RISC_INT)

#define	ISP_CHECK_SEMAPHORE_LOCK(isp)					\
	(ISP_READ_BIU_REG((isp), &(isp)->isp_biu_reg->isp_bus_sema) &	\
		ISP_BUS_SEMA_LOCK)

#endif /* ISP_PERF */

#define	ISP_REG_SET_HOST_INT(isp)					\
	ISP_WRITE_RISC_HCCR((isp), ISP_HCCR_CMD_SET_HOST_INT);		\

#define	ISP_REG_GET_HOST_INT(isp)					\
	(ISP_READ_RISC_HCCR((isp)) & ISP_HCCR_HOST_INT)

#define	ISP_CLEAR_RISC_INT(isp)						\
	ISP_WRITE_RISC_HCCR((isp), ISP_HCCR_CMD_CLEAR_RISC_INT);

/*
 * short hand-macros to copy entries in/out of queues (IOPB area).
 */
#define	ISP_COPY_OUT_DMA_BYTE(isp, source, dest, count)			\
	ddi_rep_put8((isp)->isp_dma_acc_handle, (uint8_t *)(source),	\
		(uint8_t *)(dest), (count), DDI_DEV_AUTOINCR)
#define	ISP_COPY_OUT_DMA_WORD(isp, source, dest, count)			\
	ddi_rep_put16((isp)->isp_dma_acc_handle, (uint16_t *)(source), 	\
		(uint16_t *)(dest), (count), DDI_DEV_AUTOINCR)
#define	ISP_COPY_OUT_DMA_LONG(isp, source, dest, count)			\
	ddi_rep_put32((isp)->isp_dma_acc_handle, (uint32_t *)(source),	\
		(uint32_t *)(dest), (count), DDI_DEV_AUTOINCR)

#define	ISP_COPY_IN_DMA_BYTE(isp, source, dest, count)			\
	ddi_rep_get8((isp)->isp_dma_acc_handle, (uint8_t *)(dest),	\
		(uint8_t *)(source), (count), DDI_DEV_AUTOINCR)
#define	ISP_COPY_IN_DMA_WORD(isp, source, dest, count)			\
	ddi_rep_get16((isp)->isp_dma_acc_handle, (uint16_t *)(dest),	\
		(uint16_t *)(source), (count), DDI_DEV_AUTOINCR)
#define	ISP_COPY_IN_DMA_LONG(isp, source, dest, count)			\
	ddi_rep_get32((isp)->isp_dma_acc_handle, (uint32_t *)(dest),	\
		(uint32_t *)(source), (count), DDI_DEV_AUTOINCR)

#define	ISP_COPY_OUT_REQ(isp, source, dest)				\
	{								\
		register u_char *s, *d;					\
		s = (u_char *)(source);					\
		d = (u_char *)(dest);					\
		ISP_COPY_OUT_DMA_WORD((isp), s, d, 2);			\
		ISP_COPY_OUT_DMA_LONG((isp), (s + ISP_REQ_TOKEN_OFF),	\
			(d + ISP_REQ_TOKEN_OFF), 1);			\
		ISP_COPY_OUT_DMA_WORD((isp), (s + ISP_REQ_LUN_OFF),	\
			(d + ISP_REQ_LUN_OFF), 6);			\
		ISP_COPY_OUT_DMA_BYTE((isp), (s + ISP_REQ_CDB_OFF),	\
			(d + ISP_REQ_CDB_OFF), 12);			\
		ISP_COPY_OUT_DMA_LONG((isp), (s + ISP_REQ_DATA_OFF),	\
			(d + ISP_REQ_DATA_OFF), 2);			\
	}

#define	ISP_COPY_IN_TOKEN(isp, source, dest)				\
	ISP_COPY_IN_DMA_LONG((isp), ((u_char *)(source) +		\
		ISP_RESP_TOKEN_OFF), (dest), 1);

#define	ISP_COPY_IN_RESP(isp, source, dest)				\
	{								\
		register u_char *s, *d;					\
		s = (u_char *)(source);					\
		d = (u_char *)(dest);					\
		ISP_COPY_IN_DMA_WORD((isp), s, d, 2);			\
		ISP_COPY_IN_DMA_LONG((isp), (s + ISP_RESP_TOKEN_OFF),	\
			(d + ISP_RESP_TOKEN_OFF), 1);			\
		ISP_COPY_IN_DMA_WORD((isp), (s + ISP_RESP_STATUS_OFF),	\
			(d + ISP_RESP_STATUS_OFF), 6);			\
		ISP_COPY_IN_DMA_LONG((isp), (s + ISP_RESP_RESID_OFF),	\
			(d + ISP_RESP_RESID_OFF), 1);			\
		if (((struct isp_response *)(dest))->resp_scb != 0) {	\
			ISP_COPY_IN_DMA_BYTE((isp),			\
				(s + ISP_RESP_RQS_OFF),			\
				(d + ISP_RESP_RQS_OFF), 32);		\
		}							\
	}

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_ADAPTERS_ISPVAR_H */
