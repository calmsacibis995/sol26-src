/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_ECPPVAR_H
#define	_SYS_ECPPVAR_H

#pragma ident	"@(#)ecppvar.h	2.13	96/09/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	LOOP_TIMER	500000
#define	ATTACH_DEBUG	1

struct ecppunit {
	int instance;
	dev_info_t *dip;	/* device information */
	struct config_reg *c_reg; /* configuration register */
	struct info_reg *i_reg; /* info registers */
	struct fifo_reg *f_reg; /* fifo register */
	struct cheerio_dma_reg *dmac; /* ebus dmac registers */
	ddi_acc_handle_t c_handle;
	ddi_acc_handle_t i_handle;
	ddi_acc_handle_t f_handle;
	ddi_acc_handle_t d_handle;
	/* for DMA devices */
	ddi_dma_attr_t attr;		/* DMA attribute characterics */
	ddi_dma_handle_t dma_handle;	/* active DMA mapping for this unit */
	ddi_dma_cookie_t dma_cookie;
	uint_t dma_cookie_count;
	int timeout;			/* timeout id from DMA transfer */
	/* For Streams */
	boolean_t oflag;		/* ecpp is already open */
	boolean_t service;		/* ecpp there is work to do */
	boolean_t e_busy;		/* ecpp busy flag */
	queue_t		*readq;		/* pointer to readq */
	queue_t		*writeq;	/* pointer to writeq */
	mblk_t	*msg;			/* current message block */
	int timeout_id;			/* for timer */
	u_char about_to_untimeout;	/* timeout lock */
	/*  CPR support */
	boolean_t suspended;		/* TRUE if driver suspended */
	/* 1284 */
	int    current_mode;		/* 1284 mode */
	u_char current_phase;		/* 1284 ECP phase */
	u_char backchannel;		/* backchannel mode supported */
	u_char error_status;		/* port error status */
	struct ecpp_transfer_parms xfer_parms;
	struct ecpp_regs regs;		/* control/status registers */
	u_char saved_dsr;		/* store the dsr returned from TESTIO */
	boolean_t timeout_error;	/* store the timeout for GETERR */
	u_char port;			/* xfer type: dma/pio/tfifo */
	ddi_iblock_cookie_t ecpp_trap_cookie; /* interrupt cookie */
	kmutex_t umutex;	/* lock for this structure */
	kcondvar_t pport_cv;		/* cv for changing port type */
	kcondvar_t transfer_cv;		/* cv for close/flush	*/
	u_char terminate;		/* flag to indicate closing */
};

/* current_phase values */

#define	ECPP_PHASE_INIT		0x00	/* initialization */
#define	ECPP_PHASE_NEGO		0x01	/* negotiation */
#define	ECPP_PHASE_TERM		0x02	/* termination */
#define	ECPP_PHASE_PO		0x03	/* power-on */

#define	ECPP_PHASE_CMPT_FWD	0x10	/* compatibility mode forward */
#define	ECPP_PHASE_CMPT_IDLE	0x11	/* compatibility mode idle */

#define	ECPP_PHASE_NIBT_REVDATA	0x20	/* nibble/byte reverse data */
#define	ECPP_PHASE_NIBT_AVAIL	0x21	/* nibble/byte reverse data available */
#define	ECPP_PHASE_NIBT_NAVAIL	0x22	/* nibble/byte reverse data not avail */
#define	ECPP_PHASE_NIBT_REVIDLE	0x22	/* nibble/byte reverse idle */
#define	ECPP_PHASE_NIBT_REVINTR	0x23	/* nibble/byte reverse interrupt */

#define	ECPP_PHASE_ECP_SETUP	0x30	/* ecp setup */
#define	ECPP_PHASE_ECP_FWD_XFER	0x31	/* ecp forward transfer */
#define	ECPP_PHASE_ECP_FWD_IDLE	0x32	/* ecp forward idle */
#define	ECPP_PHASE_ECP_FWD_REV	0x33	/* ecp forward to reverse */
#define	ECPP_PHASE_ECP_REV_XFER	0x34	/* ecp reverse transfer */
#define	ECPP_PHASE_ECP_REV_IDLE	0x35	/* ecp reverse idle */
#define	ECPP_PHASE_ECP_REV_FWD	0x36	/* ecp reverse to forward */

#define	FAILURE_PHASE		0x80
#define	UNDEFINED_PHASE		0x81

/* ecpp return values */
#define	SUCCESS		1
#define	FAILURE		2

/* ecpp states */
#define	ECPP_IDLE	1
#define	ECPP_BUSY	2
#define	ECPP_DATA	3

#define	TRUE		1
#define	FALSE		0

#define	ECPP_BACKCHANNEL	0x45
#define	LINE_SIZE		80

/* port error_status values */
#define	ECPP_NO_1284_ERR	0x0
#define	ECPP_1284_ERR		0x1

#define	PP_PUTB(x, y, z)  	ddi_putb(x, y, z)
#define	PP_GETB(x, y)		ddi_getb(x, y)

#define	OR_SET_BYTE_R(handle, addr, val) \
{		\
	register uint8_t tmpval;			\
	tmpval = ddi_get8(handle, (uint8_t *)addr);	\
	tmpval |= val;					\
	ddi_put8(handle, (uint8_t *)addr, tmpval);	\
}

#define	OR_SET_LONG_R(handle, addr, val) \
{		\
	register uint32_t tmpval;			\
	tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	tmpval |= val;					\
	ddi_put32(handle, (uint32_t *)addr, tmpval);	\
}

#define	AND_SET_BYTE_R(handle, addr, val) \
{		\
	register uint8_t tmpval;			\
	tmpval = ddi_get8(handle, (uint8_t *)addr);	\
	tmpval &= val; 					\
	ddi_put8(handle, (uint8_t *)addr, tmpval);	\
}

#define	AND_SET_LONG_R(handle, addr, val) \
{		\
	register uint32_t tmpval;			\
	tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	tmpval &= val; 					\
	ddi_put32(handle, (uint32_t *)addr, tmpval);	\
}

#define	NOR_SET_LONG_R(handle, addr, val, mask) \
{		\
	register uint32_t tmpval;			\
	tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	tmpval &= ~(mask);				\
	tmpval |= val;					\
	ddi_put32(handle, (uint32_t *)addr, tmpval);	\
}

/* debugging flags */
#define	DEBUG_CODE		1	/* code to be removed by beta */

#define	DEBUG_ATTACH	0	/* attach/dettach print switch */
#define	DEBUG_OPEN	0	/* open/close print switch */
#define	DEBUG_FWD	0	/* forward transfer print switch */
#define	DEBUG_IOC	0	/* ioctl print switch */
#define	DEBUG_REV	0	/* reverse transfer print switch */
#define	DEBUG_MODE	0	/* 1284 mode/phase print switch */

int debug_attach = DEBUG_ATTACH;
int debug_open = DEBUG_OPEN;
int debug_fwd = DEBUG_FWD;
int debug_ioc = DEBUG_IOC;
int debug_rev = DEBUG_REV;
int debug_mode = DEBUG_MODE;

#define	PRNattach0(a)		if (debug_attach) printf(a)
#define	PRNattach1(a, x)		if (debug_attach) printf(a, x)
#define	PRNattach2(a, x, y)	if (debug_attach) printf(a, x, y)

#define	PRNopen0(a)		if (debug_open) printf(a)
#define	PRNopen1(a, x)		if (debug_open) printf(a, x)
#define	PRNopen2(a, x, y)		if (debug_open) printf(a, x, y)

#define	PRNfwdx0(a)		if (debug_fwd) printf(a)
#define	PRNfwdx1(a, x)		if (debug_fwd) printf(a, x)
#define	PRNfwdx2(a, x, y)		if (debug_fwd) printf(a, x, y)

#define	PRNioc0(a)		if (debug_ioc) printf(a)
#define	PRNioc1(a, x)		if (debug_ioc) printf(a, x)
#define	PRNioc2(a, x, y)		if (debug_ioc) printf(a, x, y)

#define	PRNrevx0(a)		if (debug_rev) printf(a)
#define	PRNrevx1(a, x)		if (debug_rev) printf(a, x)
#define	PRNrevx2(a, x, y)		if (debug_rev) printf(a, x, y)

#define	PRNmode0(a)		if (debug_mode) printf(a)
#define	PRNmode1(a, x)		if (debug_mode) printf(a, x)
#define	PRNmode2(a, x, y)		if (debug_mode) printf(a, x, y)

#if defined(POSTRACE)

#ifndef NPOSTRACE
#define	NPOSTRACE 1024
#endif

struct postrace {
	int count;
	int function;		/* address of function */
	int trace_action;	/* descriptive 4 characters */
	int object;		/* object operated on */
};

/*
 * For debugging, allocate space for the trace buffer
 */

extern struct postrace postrace_buffer[];
extern struct postrace *postrace_ptr;
extern int postrace_count;

#define	PTRACEINIT() {				\
	if (postrace_ptr == NULL)		\
		postrace_ptr = postrace_buffer; \
	}

#define	LOCK_TRACE()	(uint_t) ddi_enter_critical()
#define	UNLOCK_TRACE(x)	ddi_exit_critical((uint_t) x)

#define	PTRACE(func, act, obj) {		\
	int __s = LOCK_TRACE();			\
	int *_p = &postrace_ptr->count;	\
	*_p++ = ++postrace_count;		\
	*_p++ = (int)(func);			\
	*_p++ = (int)(act);			\
	*_p++ = (int)(obj);			\
	if ((struct postrace *)(void *)_p >= &postrace_buffer[NPOSTRACE])\
		postrace_ptr = postrace_buffer; \
	else					\
		postrace_ptr = (struct postrace *)(void *)_p; \
	UNLOCK_TRACE(__s);			\
	}

#else	/* !POSTRACE */

/* If no tracing, define no-ops */
#define	PTRACEINIT()
#define	PTRACE(a, b, c)

#endif	/* !POSTRACE */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ECPPVAR_H */
