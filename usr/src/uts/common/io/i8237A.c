/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)i8237A.c	1.17	96/05/27 SMI"

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1988, 1989 Intel Corp.			*/
/*	All Rights Reserved	*/
/*
 *	INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *	This software is supplied under the terms of a license
 *	agreement or nondisclosure agreement with Intel Corpo-
 *	ration and may not be copied or disclosed except in
 *	accordance with the terms of that agreement.
*/

/*
 * Set features for each architecture.  List of features:
 *	ADDR_32:	Address is 32 bits
 *	COUNT_24:	Count is 24 bits
 *	DMA_4CSCD:	DMA channel 4 is used for cascade of channels 0-3)
 *	DMA_INTR:	DMA interrupt is available (always with DMA_BUF_CHAIN)
 *	DMA_BUF_CHAIN:	DMA buffer chaining is available (always with DMA_INTR)
 *	MEM_TO_MEM:	Memory to memory transfers available
 *	NO_PROG_WIDTH:	Channel data width is NOT programmable
 *	SCATER_GATHER	Scatter-gather DMA is available (code not implemented)
 *	ISA_MODE	Standard ISA modes available
 *	MCA_EXT_MODE:	MCA extension modes available
 *	EISA_EXT_MODE:	EISA extension modes available
 */
#ifdef prep
#define	ADDR_32
/* Count is 16 bits (default) */
#define	DMA_4CSCD
#define	SCATER_GATHER
#define	ISA_MODE
#define	EISA_EXT_MODE

#elif EISA_BUS
#define	ADDR_32
#define	COUNT_24
#define	DMA_4CSCD
#define	DMA_INTR
#define	DMA_BUF_CHAIN
#define	MEM_TO_MEM
#define	ISA_MODE
#define	EISA_EXT_MODE

#elif MCA_BUS
/*
 * Address is 24 bits (default)
 * Count is 16 bits (default)
 */
#define	MCA_EXT_MODE

#else	/* ISA_BUS - default */
/*
 * Address is 24 bits (default) with no carry between lo word and hi byte
 * Count is 16 bits (default)
 */
#define	DMA_4CSCD
#define	NO_PROG_WIDTH
#define	ISA_MODE

#endif

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/dma_engine.h>
#include <sys/dma_i8237A.h>
#ifdef MCA_BUS
#include <sys/mcdma.h>
#endif	/* MCA_BUS */

#if defined(DEBUG)
#include <sys/promif.h>
static int i8237debug = 0;
#define	dprintf(x)	if (i8237debug) (void)prom_printf x
#else
#define	dprintf(x)
#endif	/* defined(DEBUG) */


extern int EISA_chaining = -1;

/*
 * data structures for maintaining the DMACs
 */
static kmutex_t dma_engine_lock;
static struct d37A_chan_reg_addr chan_addr[] = { D37A_BASE_REGS_VALUES };
static u_short d37A_chnl_path[] = {
	DMAE_PATH_8,	/* first 4 DMA channels default to 8-bit xfers */
	DMAE_PATH_8,
	DMAE_PATH_8,
	DMAE_PATH_8,
	0,
	DMAE_PATH_16,	/* last 3 DMA channels default to 16-bit xfers */
	DMAE_PATH_16,
	DMAE_PATH_16};
static u_short d37A_chnl_mode[] = {
	DMAE_TRANS_SNGL, DMAE_TRANS_SNGL, DMAE_TRANS_SNGL, DMAE_TRANS_SNGL,
#ifdef DMA_4CSCD
	DMAE_TRANS_CSCD,
#else 	/* !DMA_4CSCD */
	DMAE_TRANS_SNGL,
#endif	/* !DMA_4CSCD */
	DMAE_TRANS_SNGL, DMAE_TRANS_SNGL, DMAE_TRANS_SNGL};
#ifdef DMA_BUF_CHAIN
static ddi_dma_cookie_t *d37A_next_cookie[] =
	{0, 0, 0, 0, 0, 0, 0, 0};
#endif	/* DMA_BUF_CHAIN */


u_int d37A_intr(caddr_t);
int d37A_set_mode(struct ddi_dmae_req *, int);
int d37A_write_addr(u_long, int);
u_long d37A_read_addr(int);
int d37A_write_count(long, int);
long d37A_read_count(int);


/*
 *  Routine: d37A_init()
 *  purpose: initializes the 8237A.
 *  caller:  dma_init()
 *  calls:   d37A macros, d37A_init()
 */

/*ARGSUSED*/
int
d37A_init(dev_info_t *dip)
{
#ifdef DMA_INTR
	ddi_iblock_cookie_t iblk_cookie = 0;
	int	error;

	if ((error = ddi_add_intr(dip, (u_int)0, &iblk_cookie,
	    (ddi_idevice_cookie_t *)0, d37A_intr, (caddr_t)NULL)) !=
	    DDI_SUCCESS) {
		if (error != DDI_INTR_NOTFOUND)
			cmn_err(CE_WARN, "!d37A_init: cannot add dma intr\n");
		EISA_chaining = 0;
	}
	mutex_init(&dma_engine_lock, "DMA engine lock",
	    MUTEX_DRIVER, (void *)iblk_cookie);
#else	/* !DMA_INTR */
	mutex_init(&dma_engine_lock, "DMA engine lock",
	    MUTEX_DRIVER, (void *)NULL);
#endif	/* !DMA_INTR */

#if	defined(prep)
	/*
	 * Enable cascade channel so channels 0-3 can be enabled dynamically
	 */
	d37A_dma_enable(DMAE_CH4);

	/*
	 * Ensure that both DMA controllers are enabled.
	 */
	outb(DMAC1_CMD, DMA_CTLR_INIT);
	outb(DMAC2_CMD, DMA_CTLR_INIT);

#endif	/* defined(prep) */
	return (DDI_SUCCESS);
}

/*
 *  Routine: d37A_valid()
 *  purpose: validates the channel to be acquired.
 *  caller:  i_dmae_acquire()
 *  calls:
 */

int
d37A_dma_valid(int chnl)
{
#ifdef DMA_4CSCD
	if (chnl == 4)
		return (0);
#endif	/* DMA_4CSCD */
	return (1);
}

/*
 *  Routine: d37A_release()
 *  purpose: resets the 8237A mode.
 *  caller:  i_dmae_free()
 *  calls:
 */

void
d37A_dma_release(int chnl)
{
#ifdef DMA_4CSCD
	if (chnl == 4)
		return;
#endif	/* DMA_4CSCD */
	d37A_chnl_mode[chnl] = DMAE_TRANS_SNGL;
}

/*
 *  routine: d37A_dma_disable()
 *  purpose: Prevent the DMAC from responding to external hardware
 *		requests for DMA service on the given channel
 *  caller:  dma_disable()
 *  calls:   d37A macros
 */
void
d37A_dma_disable(int chnl)
{
	dprintf(("d37A_dma_disable: chnl=%d mask_reg=0x%x\n",
	    chnl, chan_addr[chnl].mask_reg));

	outb(chan_addr[chnl].mask_reg, (chnl & 3) | DMA_SETMSK);
}


/*
 *  routine: d37A_dma_enable()
 *  purpose: Enable to DMAC to respond to hardware requests for DMA
 *		service on the specified channel.
 *  caller:  dma_enable()
 *  calls:   d37A macros
 */

void
d37A_dma_enable(int chnl)
{
	dprintf(("d37A_dma_enable: chnl=%d mask_reg=0x%x val=0x%x\n",
	    chnl, chan_addr[chnl].mask_reg, chnl & 3));

/*	mutex_enter(&dma_engine_lock);	*/
	outb(chan_addr[chnl].mask_reg, chnl & 3);
/*	mutex_exit(&dma_engine_lock);	*/
}


/*
 *  routine: d37A_get_best_mode()
 *  purpose: stub routine - determine optimum transfer method
 *  caller:  dma_get_best_mode().
 *  calls:
 */
/* ARGSUSED */
u_char
d37A_get_best_mode(struct ddi_dmae_req *dmaereqp)
{
	return (DMAE_CYCLES_2);
}

#ifdef DMA_INTR
/*
 *  routine: d37A_intr()
 *  purpose: stub routine
 *  caller:
 *  calls:  dma_intr().
 */
/* ARGSUSED */
u_int
d37A_intr(caddr_t arg)
{
	void dEISA_setchain();
	register int chnl, istate, nstate;
	uint	mask;

	if ((istate = (inb(EISA_DMAIS) & 0xef)) != 0) {
		/* channel 4 can't interrupt */
		chnl = 0;
		nstate = istate;
		mutex_enter(&dma_engine_lock);
		do {
			if (istate & 1) {
				dEISA_setchain(d37A_next_cookie[chnl], chnl);
#ifdef DEBUG
				if (chnl < 4)
					mask = inb(DMAC1_ALLMASK) >> (chnl);
				else
					mask = inb(DMAC2_ALLMASK) >> (chnl - 4);
				if (mask & 1)
prom_printf("eisa: dma buffer chaining failure chnl %d!\n", chnl);

#endif	/* DEBUG */
			}
			chnl++;
			istate >>= 1;
		} while (istate);
		chnl = 0;
		do {
			if ((nstate & 1) && d37A_next_cookie[chnl])
				d37A_next_cookie[chnl] = _dmae_nxcookie(chnl);
			chnl++;
			nstate >>= 1;
		} while (nstate);
		mutex_exit(&dma_engine_lock);
		return (DDI_INTR_CLAIMED);
	}
	return (DDI_INTR_UNCLAIMED);
}
#endif	/* DMA_INTR */


#ifdef DMA_BUF_CHAIN
/*
 *  routine: dEISA_setchain()
 *  purpose: Set next buffer address/count from chain
 *  caller:  d37A_intr()
 *  calls:   d37A macros
 */
void
dEISA_setchain(ddi_dma_cookie_t *cp, int chnl)
{
	if (cp) {
		dprintf(("dEISA_setchain: chnl=%d next_addr=%x count=%x\n",
		    chnl, cp->dmac_address, cp->dmac_size));
		d37A_write_addr(cp->dmac_address, chnl);
		d37A_write_count(cp->dmac_size, chnl);
		outb(chan_addr[chnl].scm_reg, chnl | EISA_ENCM | EISA_CMOK);
	} else {
		/*
		 *  clear chain enable bit
		 */
		outb(chan_addr[chnl].scm_reg, chnl);
		dprintf(("dEISA_setchain: chnl=%d end\n", chnl));
	}
}
#endif	/* DMA_BUF_CHAIN */


/*
 *  routine: d37A_prog_chan()
 *  purpose: program the Mode registers and the Base registers of a
 *		DMA channel for a subsequent hardware-initiated transfer.
 *  caller:  dma_prog_chan()
 *  calls:   d37A_write_addr(), d37A_write_count(), d37A macros.
 */

int
d37A_prog_chan(struct ddi_dmae_req *dmaereqp, ddi_dma_cookie_t *cp, int chnl)
{
	if (d37A_chnl_mode[chnl] == DMAE_TRANS_CSCD) {
		dprintf(("d37A_prog_chan err: chnl=%d in cascade mode\n",
		    chnl));
		return (DDI_FAILURE);
	}
#ifndef MEM_TO_MEM
	if (dmaereqp && dmaereqp->der_dest == DMAE_DEST_MEM) {
dprintf(("d37A_prog_chan err: memory to memory mode not supported.\n"));
		return (DDI_FAILURE);
	}
#endif	/* !MEM_TO_MEM */

	dprintf(("d37A_prog_chan: chnl=%d dmaereq=%x\n",
	    chnl, (u_long) dmaereqp));

	if (dmaereqp) {
		switch (chnl) {
		case DMAE_CH0:
		case DMAE_CH1:
		case DMAE_CH2:
		case DMAE_CH3:
#ifdef NO_PROG_WIDTH
			if (dmaereqp->der_path &&
			    dmaereqp->der_path != DMAE_PATH_8) {
dprintf(("d37A_prog_chan err: chnl %d not programmed.\n", chnl));
				return (DDI_FAILURE);
			}
#endif	/* NO_PROG_WIDTH */
			break;

#ifndef DMA_4CSCD
		case DMAE_CH4:
#endif	/* !DMA_4CSCD */
		case DMAE_CH5:
		case DMAE_CH6:
		case DMAE_CH7:
#ifdef NO_PROG_WIDTH
			if (dmaereqp->der_path &&
			    dmaereqp->der_path != DMAE_PATH_16) {
dprintf(("d37A_prog_chan err: chnl %d not programmed.\n", chnl));
				return (DDI_FAILURE);
			}
#endif	/* NO_PROG_WIDTH */
			break;

		default:
dprintf(("d37A_prog_chan err: chnl %d not programmed.\n", chnl));
			return (DDI_FAILURE);
		}
	} else
		chnl &= 3;
	mutex_enter(&dma_engine_lock);

	d37A_dma_disable(chnl);
	if (dmaereqp)
		d37A_set_mode(dmaereqp, chnl);

	if (cp) {
		d37A_write_addr(cp->dmac_address, chnl);
		d37A_write_count(cp->dmac_size, chnl);

#ifdef DMA_BUF_CHAIN
		if (dmaereqp && dmaereqp->der_bufprocess == DMAE_BUF_CHAIN &&
		    (d37A_next_cookie[chnl] = _dmae_nxcookie(chnl))) {
			/*
			 * i/o operation has more than 1 cookie
			 * so enable dma buffer chaining
			 */
			drv_usecwait(10);
			outb(chan_addr[chnl].scm_reg, chnl | EISA_ENCM);
			drv_usecwait(15);
			dEISA_setchain(d37A_next_cookie[chnl], chnl);
			d37A_next_cookie[chnl] = _dmae_nxcookie(chnl);
		}
#endif	/* DMA_BUF_CHAIN */
	}
	mutex_exit(&dma_engine_lock);
	return (DDI_SUCCESS);
}


/*
 *  routine: d37A_dma_swsetup()
 *  purpose: program the Mode registers and the Base register for the
 *		specified channel.
 *  caller:  dma_swsetup()
 *  calls:   d37A_write_addr(), d37A_write_count(), d37A macros.
 */

int
d37A_dma_swsetup(struct ddi_dmae_req *dmaereqp, ddi_dma_cookie_t *cp, int chnl)
{
	if (d37A_chnl_mode[chnl] == DMAE_TRANS_CSCD) {
		dprintf(("d37A_dma_swsetup err: chnl %d not programmed\n",
		    chnl));
		return (DDI_FAILURE);
	}

	dprintf(("d37A_dma_swsetup: chnl=%d dmaereq=%x.\n",
	    chnl, (u_long) dmaereqp));

	/* MUST BE IN BLOCK MODE FOR SOFTWARE INITIATED REQUESTS */
	if (dmaereqp->der_trans != DMAE_TRANS_BLCK)
		dmaereqp->der_trans = DMAE_TRANS_BLCK;

	switch (chnl) {
	case DMAE_CH0:
	case DMAE_CH1:
	case DMAE_CH2:
	case DMAE_CH3:
#ifdef NO_PROG_WIDTH
		if (dmaereqp->der_path && dmaereqp->der_path != DMAE_PATH_8) {
dprintf(("d37A_dma_swsetup err: chnl %d not programmed.\n", chnl));
			return (DDI_FAILURE);
		}
#endif	/* NO_PROG_WIDTH */
		break;

#ifndef DMA_4CSCD
	case DMAE_CH4:
#endif	/* !DMA_4CSCD */
	case DMAE_CH5:
	case DMAE_CH6:
	case DMAE_CH7:
#ifdef NO_PROG_WIDTH
		if (dmaereqp->der_path && dmaereqp->der_path != DMAE_PATH_16) {
dprintf(("d37A_dma_swsetup err: chnl %d not programmed.\n", chnl));
			return (DDI_FAILURE);
		}
#endif	/* NO_PROG_WIDTH */
		break;

	default:
		dprintf(("d37A_dma_swsetup err: chnl %d not set up.\n", chnl));
		return (DDI_FAILURE);
	};

	mutex_enter(&dma_engine_lock);

	d37A_dma_disable(chnl);
	d37A_set_mode(dmaereqp, chnl);

	d37A_write_addr(cp->dmac_address, chnl);
	d37A_write_count(cp->dmac_size, chnl);

#ifdef DMA_BUF_CHAIN
	if (dmaereqp->der_bufprocess == DMAE_BUF_CHAIN &&
	    (d37A_next_cookie[chnl] = _dmae_nxcookie(chnl))) {
		/*
		 * i/o operation has more than 1 cookie
		 * so enable dma buffer chaining
		 */
		outb(chan_addr[chnl].scm_reg, chnl | EISA_ENCM);
		dEISA_setchain(d37A_next_cookie[chnl], chnl);
		d37A_next_cookie[chnl] = _dmae_nxcookie(chnl);
	}
#endif	/* DMA_BUF_CHAIN */
	mutex_exit(&dma_engine_lock);
	return (DDI_SUCCESS);
}


/*
 *  routine: d37A_dma_swstart()
 *  purpose: SW start transfer setup on the indicated channel.
 *  caller:  dma_swstart()
 *  calls:   d37A_dma_enable(), d37A macros
 */

void
d37A_dma_swstart(int chnl)
{
	dprintf(("d37A_dma_swstart: chnl=%d\n", chnl));

	mutex_enter(&dma_engine_lock);
	d37A_dma_enable(chnl);
	outb(chan_addr[chnl].reqt_reg, DMA_SETMSK | chnl); /* set request bit */
	mutex_exit(&dma_engine_lock);
}


/*
 *  routine: d37A_dma_stop()
 *  purpose: Stop any activity on the indicated channel.
 *  caller:  dma_stop()
 *  calls:   d37A macros
 */

void
d37A_dma_stop(int chnl)
{
	dprintf(("d37A_dma_stop: chnl=%d\n", chnl));

	mutex_enter(&dma_engine_lock);
	d37A_dma_disable(chnl);
	outb(chan_addr[chnl].reqt_reg, chnl & 3);    /* reset request bit */
	mutex_exit(&dma_engine_lock);
}


/*
 *  routine: d37A_get_chan_stat()
 *  purpose: retrieve the Current Address and Count registers for the
 *		specified channel.
 *  caller:  dma_get_chan_stat()
 *  calls:   d37A_read_addr(), d37A_read_count().
 */
void
d37A_get_chan_stat(int chnl, u_long *addressp, int *countp)
{
	u_long taddr;
	int tcount;

	mutex_enter(&dma_engine_lock);
	taddr = d37A_read_addr(chnl);
	tcount = d37A_read_count(chnl);
	mutex_exit(&dma_engine_lock);
	if (addressp)
		*addressp = taddr;
	if (countp)
		*countp = tcount;
	dprintf(("d37A_get_chan_stat: chnl=%d address=%x count=%x \n",
	    chnl, taddr, tcount));
}


/*
 *  routine: d37A_set_mode()
 *  purpose: program the Mode registers of the
 *		DMAC for a subsequent hardware-initiated transfer.
 *  caller:  d37A_prog_chan(), d37A_dma_swsetup
 *  calls:
 */

int
d37A_set_mode(struct ddi_dmae_req *dmaereqp, int chnl)
{
	u_char mode = 0, emode = 0;

#ifdef ISA_MODE
#if defined(lint)
	emode = emode;
#endif
	mode = chnl & 3;

	switch (dmaereqp->der_command) {
	case DMAE_CMD_READ:
		mode |= DMAMODE_READ;
		break;
	case DMAE_CMD_WRITE:
		mode |= DMAMODE_WRITE;
		break;
	case DMAE_CMD_VRFY:
		mode |= DMAMODE_VERF;
		break;
	case DMAE_CMD_TRAN:
		mode |= 0x0C;	/* for Adaptec 1st party DMA on chnl 0 */
		break;
	default:
		return (DDI_FAILURE);
	}

	if (dmaereqp->der_bufprocess == DMAE_BUF_AUTO)
		mode |= DMAMODE_AUTO;

	if (dmaereqp->der_step == DMAE_STEP_DEC)
		mode |= DMAMODE_DECR;

	switch (dmaereqp->der_trans) {
	case DMAE_TRANS_SNGL:
		mode |= DMAMODE_SINGLE;
		break;
	case DMAE_TRANS_BLCK:
		mode |= DMAMODE_BLOCK;
		break;
	case DMAE_TRANS_DMND:
		break;
	case DMAE_TRANS_CSCD:
		mode |= DMAMODE_CASC;
		break;
	default:
		return (DDI_FAILURE);
	}
	d37A_chnl_mode[chnl] = dmaereqp->der_trans;

	dprintf(("d37A_set_mode: chnl=%d mode_reg=0x%x mode=0x%x\n",
	    chnl, chan_addr[chnl].mode_reg, mode));
	outb(chan_addr[chnl].mode_reg, mode);
#endif	/* ISA_MODE */

#ifdef EISA_EXT_MODE
	emode = chnl & 3;
	d37A_chnl_path[chnl] = dmaereqp->der_path;

	switch (dmaereqp->der_path) {
	case DMAE_PATH_8:
		/* emode |= EISA_DMA_8; */
		break;
	case DMAE_PATH_16:
		emode |= EISA_DMA_16;
		break;
	case DMAE_PATH_32:
		emode |= EISA_DMA_32;
		break;
	case DMAE_PATH_16B:
		emode |= EISA_DMA_16B;
		break;
	default:
		switch (chnl) {
		case DMAE_CH0:
		case DMAE_CH1:
		case DMAE_CH2:
		case DMAE_CH3:
			d37A_chnl_path[chnl] = DMAE_PATH_8;
			/* emode |= EISA_DMA_8; */
			break;
		case DMAE_CH5:
		case DMAE_CH6:
		case DMAE_CH7:
			d37A_chnl_path[chnl] = DMAE_PATH_16;
			emode |= EISA_DMA_16;
			break;
		}
	}
	emode |= (dmaereqp->der_cycles & 3) << 4;
	outb(chan_addr[chnl].emode_reg, emode);

	dprintf(("d37A_set_mode: chnl=%d em_reg=0x%x emode=0x%x\n",
	    chnl, chan_addr[chnl].emode_reg, emode));
#endif	/* EISA_EXT_MODE */

#ifdef MCA_EXT_MODE
#if defined(lint)
	mode = mode;
#endif
	chnl &= 3;
	d37A_chnl_path[chnl] = dmaereqp->der_path;

	switch (dmaereqp->der_command) {
	case DMAE_CMD_READ:
		emode = PS2DMA_MRD;
		break;
	case DMAE_CMD_WRITE:
		emode = PS2DMA_MWT;
		break;
	case DMAE_CMD_VRFY:
		emode = PS2DMA_MVF;
		break;
	default:
		return (DDI_FAILURE);
	}

	switch (dmaereqp->der_path) {
	case DMAE_PATH_8:
		emode |= PS2DMA_M8;
		break;
	case DMAE_PATH_16:
		emode |= PS2DMA_M16;
		break;
	default:
		switch (chnl) {
		case DMAE_CH0:
		case DMAE_CH1:
		case DMAE_CH2:
		case DMAE_CH3:
			d37A_chnl_path[chnl] = DMAE_PATH_8;
			emode |= PS2DMA_M8;
			break;
		case DMAE_CH4:
		case DMAE_CH5:
		case DMAE_CH6:
		case DMAE_CH7:
			d37A_chnl_path[chnl] = DMAE_PATH_16;
			emode |= PS2DMA_M16;
			break;
		default:
			return (DDI_FAILURE);
		}
	}
	if (dmaereqp->der_ioadr) {
		outb(PS2DMA_CTL, PS2DMA_IOR + chnl);
		outb(PS2DMA_DAT, dmaereqp->der_ioadr & 0xff);
		outb(PS2DMA_DAT, (dmaereqp->der_ioadr >> 8) & 0xff);
		emode |= PS2DMA_MIO;
	}
	if (dmaereqp->der_arbus && (chnl == 0 || chnl == 4)) {
		outb(PS2DMA_CTL, PS2DMA_ARB + chnl);
		outb(PS2DMA_DAT, dmaereqp->der_arbus & 0x0f);
	}
	outb(PS2DMA_CTL, PS2DMA_WMR + chnl);
	outb(PS2DMA_DAT, emode);

	dprintf(("d37A_set_mode: chnl=%d emode=0x%x\n",
	    chnl, emode));
#endif	/* MCA_EXT_MODE */
	return (DDI_SUCCESS);
}


/*
 *  routine: d37A_write_addr()
 *  purpose: write the 24- or 32-bit physical address into the Base Address
 *		Register for the indicated channel.
 *  caller:  d37A_prog_chan(), d37A_dma_swsetup().
 *  calls:   d37A macros
 */

int
d37A_write_addr(u_long paddress, int chnl)
{
	u_char *adr_byte;

	dprintf(("d37A_write_addr: chnl=%d address=%x\n", chnl, paddress));

	switch (d37A_chnl_path[chnl]) {
	case DMAE_PATH_8:
	case DMAE_PATH_16B:
	case DMAE_PATH_32:
		/*
		 * program DMA controller with byte address
		 */
		break;

	case DMAE_PATH_16:
		/*
		 * convert byte address to shifted word address
		 */
		paddress = (paddress & ~0x1ffff) | ((paddress & 0x1ffff) >> 1);
		break;

	default:
		return (DDI_FAILURE);
	}
	kpreempt_disable();	/* don't preempt thread while using flip-flop */
	outb(chan_addr[chnl].ff_reg, 0);	/* set flipflop */

	adr_byte = (u_char *)&paddress;
	outb(chan_addr[chnl].addr_reg, adr_byte[0]);
	outb(chan_addr[chnl].addr_reg, adr_byte[1]);
	outb(chan_addr[chnl].page_reg, adr_byte[2]);
#ifdef ADDR_32
	outb(chan_addr[chnl].hpage_reg, adr_byte[3]);
#endif	/* ADDR_32 */

	kpreempt_enable();
	return (DDI_SUCCESS);
}


/*
 *  routine: d37A_read_addr()
 *  purpose: read the 24- or 32-bit physical address from the Current Address
 *		Register for the indicated channel.
 *  caller:  d37A_get_chan_stat().
 *  calls:   d37A macros
 */

u_long
d37A_read_addr(int chnl)
{
	u_long paddress = 0;
	u_char *adr_byte;

	kpreempt_disable();	/* don't preempt thread while using flip-flop */
	adr_byte = (u_char *)&paddress;
#ifdef MCA_BUS
	outb(PS2DMA_CTL, PS2DMA_MAR + chnl);	/* set memory address reg */
	adr_byte[0] = inb(PS2DMA_DAT);
	adr_byte[1] = inb(PS2DMA_DAT);
	adr_byte[2] = inb(PS2DMA_DAT);
#else	/* !MCA_BUS */
	outb(chan_addr[chnl].ff_reg, 0);	/* set flipflop */

	adr_byte[0] = inb(chan_addr[chnl].addr_reg);
	adr_byte[1] = inb(chan_addr[chnl].addr_reg);
	adr_byte[2] = inb(chan_addr[chnl].page_reg);
#ifdef ADDR_32
	adr_byte[3] = inb(chan_addr[chnl].hpage_reg);
#endif	/* ADDR_32 */
#endif	/* !MCA_BUS */

	kpreempt_enable();

	switch (d37A_chnl_path[chnl]) {
	case DMAE_PATH_8:
	case DMAE_PATH_16B:
	case DMAE_PATH_32:
		/*
		 * return with byte address
		 */
		break;

	case DMAE_PATH_16:
		/*
		 * convert shifted word address to byte address
		 */
		paddress = (paddress & ~0x1ffff) | ((paddress & 0x0ffff) << 1);
		break;

	default:
		return ((u_long)DDI_FAILURE);
	}

	dprintf(("d37A_read_addr: chnl=%d address=%x.\n",
	    chnl, (u_long) paddress));
	return (paddress);
}


/*
 *  routine: d37A_write_count()
 *  purpose: write the 16- or 24-bit count into the Base Count Register for
 *		the indicated channel.
 *  caller:  d37A_prog_chan(), d37A_dma_swsetup()
 *  calls:   d37A macros
 */

int
d37A_write_count(long count, int chnl)
{
	u_char *count_byte;

	dprintf(("d37A_write_count: chnl=%d count=0x%x\n", chnl, count));

	switch (d37A_chnl_path[chnl]) {
	case DMAE_PATH_16:
		/*
		 * Convert byte count to word count
		 */
		count >>= 1;
		/* FALLTHROUGH */
	case DMAE_PATH_8:
	case DMAE_PATH_16B:
	case DMAE_PATH_32:
		--count;
		break;

	default:
		return (DDI_FAILURE);
	}

	kpreempt_disable();	/* don't preempt thread while using flip-flop */
	outb(chan_addr[chnl].ff_reg, 0);	/* set flipflop */

	count_byte = (u_char *)&count;
	outb(chan_addr[chnl].cnt_reg, count_byte[0]);
	outb(chan_addr[chnl].cnt_reg, count_byte[1]);
#ifdef COUNT_24
	outb(chan_addr[chnl].hcnt_reg, count_byte[2]);
#endif	/* COUNT_24 */

	kpreempt_enable();
	return (DDI_SUCCESS);
}


/*
 *  routine: d37A_read_count()
 *  purpose: read the 16- or 24-bit count from the Current Count Register for
 *		the indicated channel
 *  caller:  d37A_get_chan_stat()
 *  calls:   d37A macros
 */

long
d37A_read_count(int chnl)
{
	long count = 0;
	u_char *count_byte;

	kpreempt_disable();	/* don't preempt thread while using flip-flop */
	count_byte = (u_char *)&count;
#ifdef MCA_BUS
	outb(PS2DMA_CTL, PS2DMA_TCR + chnl);	/* set transfer count reg */
	count_byte[0] = inb(PS2DMA_DAT);
	count_byte[1] = inb(PS2DMA_DAT);
#else	/* !MCA_BUS */
	outb(chan_addr[chnl].ff_reg, 0);	/* set flipflop */

	count_byte[0] = inb(chan_addr[chnl].cnt_reg);
	count_byte[1] = inb(chan_addr[chnl].cnt_reg);
#ifdef COUNT_24
	count_byte[2] = inb(chan_addr[chnl].hcnt_reg);
#endif	/* COUNT_24 */
#endif	/* !MCA_BUS */

#ifdef COUNT_24
	if ((u_long)count == 0xffffff)
#else	/* !COUNT_24 */
	if ((u_long)count == 0xffff)
#endif	/* !COUNT_24 */
		count = -1;

	kpreempt_enable();

	switch (d37A_chnl_path[chnl]) {
	case DMAE_PATH_8:
	case DMAE_PATH_16B:
	case DMAE_PATH_32:
		++count;
		break;

	case DMAE_PATH_16:
		/*
		 * Convert incremented word count to byte count
		 */
		count = (++count) << 1;
		break;
	}
	dprintf(("d37A_read_count: chnl=%d count=0x%x\n", chnl, count));
	return (count);
}
