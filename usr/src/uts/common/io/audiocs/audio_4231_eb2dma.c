/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_4231_eb2dma.c	1.17	96/10/15 SMI"

/*
 * Platform-specific code for the EB2 DMA controller used with the
 * SPARC CS4231 audio board (in PCI systems).
 */

#include <sys/types.h>
#include <sys/stream.h>		/* mblk_t */
#include <sys/kmem.h>		/* kmem_free() */
#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/sunddi.h>		/* dditypes.h */
#include <sys/audio_4231.h>
#include <sys/audio_4231_dma.h>
#include <sys/audiodebug.h>		/* ATRACE */
#include <sys/ddi.h>

extern uint_t audio_4231_playintr(cs_unit_t *);
extern void audio_4231_recintr(cs_unit_t *);
extern void audio_4231_clear(aud_dma_list_t *, cs_unit_t *);
extern uint_t audio_4231_sampleconv(cs_stream_t *, uint_t);
extern void audio_4231_dma_errprt(int);

extern cs_unit_t *cs_units;		/* device controller array */
extern int audio_4231_bsize;
extern caddr_t play_addr;

static int audio_4231_cap_burstsize = EB2_SIXTY4;
static int audio_4231_play_burstsize = EB2_SIXTY4;


/*
 * Local routines
 */
#ifdef MULTI_DEBUG
extern uint_t audio_4231_eb2cintr();
#endif
extern uint_t audio_4231_eb2recintr();
extern uint_t audio_4231_eb2playintr();
void audio_4231_eb2cycpend(ddi_acc_handle_t, uint32_t *);

extern ddi_iblock_cookie_t audio_4231_trap_cookie;

/*
 * attribute structure for the Ebus2 DMAC
 */
ddi_dma_attr_t  aud_eb2dma_attr = {
	DMA_ATTR_V0,			/* version */
	0x00000000,			/* dlim_addr_lo */
	(unsigned long long)0xfffffffe,	/* dlim_addr_hi */
	0xffffff,			/* 24 bit counter */
	0x01,				/* alignment */
	0x74,				/* 4 and 16 byte transfers */
	0x01,				/* min xfer size */
	0xFFFF,				/* maxxfersz 64K */
	0xFFFF,				/* segment size */
	0x01,				/* no scatter gather */
	0x01,				/* XXX granularity ?? */
};


extern aud_dma_list_t dma_played_list[DMA_LIST_SIZE];
extern aud_dma_list_t dma_recorded_list[DMA_LIST_SIZE];


/*
 * audio_4231_dma_ops routines for the Ebus2 DMA engine
 */
/*ARGSUSED*/
void
audio_4231_eb2_reset(cs_unit_t *unitp, ddi_acc_handle_t handle)
{
	ddi_put32(unitp->cnf_handle_eb2play,
		(uint32_t *)EB2_PLAY_CSR, EB2_RESET);
	audio_4231_eb2cycpend(unitp->cnf_handle_eb2play,
	    (uint32_t *)EB2_PLAY_CSR);
	AND_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR, ~EB2_RESET);

	ddi_put32(unitp->cnf_handle_eb2record,
		(uint32_t *)EB2_REC_CSR, EB2_RESET);
	audio_4231_eb2cycpend(unitp->cnf_handle_eb2record,
	    (uint32_t *)EB2_REC_CSR);
	AND_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR, ~EB2_RESET);
}

/*
 * audio_4231_eb2_mapregs():
 *	Map in the CS4231 and Ebus DMA registers
 *
 * RETURNS:
 *	DDI_SUCCESS/DDI_FAILURE
 */
int
audio_4231_eb2_mapregs(dev_info_t *dip, cs_unit_t *unitp)
{
	ddi_device_acc_attr_t attr;

/*
 * Map in the DMA registers for the EB2 device
 */
	attr.devacc_attr_version =  DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_BE_ACC;

	if (ddi_regs_map_setup(dip, 0,
	    (caddr_t *)&unitp->chip, 0,
	    sizeof (struct aud_4231_pioregs), &attr,
		    &unitp->cnf_handle) != DDI_SUCCESS) {
		/* Deallocate structures allocated above */
		kmem_free(unitp->allocated_memory,
		    unitp->allocated_size);
		return (DDI_FAILURE);
	}

	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	if (ddi_regs_map_setup(dip, 1,
	    (caddr_t *)&unitp->eb2_play_dmar, 0,
	    sizeof (struct eb2_dmar), &attr,
		&unitp->cnf_handle_eb2play) != DDI_SUCCESS) {
		/* Deallocate structures allocated above */
		kmem_free(unitp->allocated_memory,
		    unitp->allocated_size);
		ddi_regs_map_free(&unitp->cnf_handle);
		return (DDI_FAILURE);
	}

	/* Map in the ebus record CSR etc. */
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	if (ddi_regs_map_setup(dip, 2,
	    (caddr_t *)&unitp->eb2_record_dmar, 0,
	    sizeof (struct eb2_dmar), &attr,
		&unitp->cnf_handle_eb2record) != DDI_SUCCESS) {
		/* Deallocate structures allocated above */
		kmem_free(unitp->allocated_memory,
		    unitp->allocated_size);
		ddi_regs_map_free(&unitp->cnf_handle_eb2play);
		ddi_regs_map_free(&unitp->cnf_handle);
		return (DDI_FAILURE);
	}
	ATRACE(audio_4231_eb2_mapregs, 'RECS',
	    ddi_get32(unitp->cnf_handle_eb2record,
		(uint32_t *)EB2_REC_CSR));

	/* Map in the codec_auxio . */
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	if (ddi_regs_map_setup(dip, 3,
	    (caddr_t *)&unitp->audio_auxio, 0,
	    sizeof (uint32_t), &attr,
		&unitp->cnf_handle_auxio) != DDI_SUCCESS) {
		/* Deallocate structures allocated above */
		kmem_free(unitp->allocated_memory,
		    unitp->allocated_size);
		ddi_regs_map_free(&unitp->cnf_handle_eb2record);
		ddi_regs_map_free(&unitp->cnf_handle_eb2play);
		ddi_regs_map_free(&unitp->cnf_handle);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

void
audio_4231_eb2_unmapregs(cs_unit_t *unitp)
{
	ddi_regs_map_free(&unitp->cnf_handle);
	ddi_regs_map_free(&unitp->cnf_handle_eb2play);
	ddi_regs_map_free(&unitp->cnf_handle_eb2record);
	ddi_remove_intr(unitp->dip, (uint_t)1, audio_4231_trap_cookie);
}

/*
 * audio_4231_eb2_addintr():
 *
 * Map in the eb2 playback interrupts  Record first? XXX
 * We don't need to go to the cintr routine for Eb2, we can just
 * map in the real intr routines.
 *
 * For the ebus 2 (cheerio) we are going to have 2 levels of interrupt;
 * one for record and one for playback. They are at the same level for now.
 * HW intr 8.
 *
 * RETURNS:
 *	DDI_SUCCESS/DDI_FAILURE
 */
int
audio_4231_eb2_addintr(dev_info_t *dip, cs_unit_t *unitp)
{

#ifdef MULTI_DEBUG
	if (ddi_add_intr(dip, 0, &audio_4231_trap_cookie,
	    (ddi_idevice_cookie_t *)0, audio_4231_eb2cintr,
	    (caddr_t)0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: bad 0 interrupt specification");
		ddi_remove_intr(dip, (uint_t)0, audio_4231_trap_cookie);
		ddi_regs_map_free(&unitp->cnf_handle_eb2record);
		ddi_regs_map_free(&unitp->cnf_handle_eb2play);
		ddi_regs_map_free(&unitp->cnf_handle);
		return (DDI_FAILURE);

	}
#else

	if (ddi_add_intr(dip, 0, &audio_4231_trap_cookie,
	    (ddi_idevice_cookie_t *)0, audio_4231_eb2recintr,
	    (caddr_t)0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: bad 0 interrupt specification");
		ddi_regs_map_free(&unitp->cnf_handle_eb2record);
		ddi_regs_map_free(&unitp->cnf_handle_eb2play);
		ddi_regs_map_free(&unitp->cnf_handle);
		return (DDI_FAILURE);
	}

	if (ddi_add_intr(dip, 1, &audio_4231_trap_cookie,
	    (ddi_idevice_cookie_t *)0, audio_4231_eb2playintr,
	    (caddr_t)0) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: bad 1 interrupt specification");
		ddi_remove_intr(dip, (uint_t)0, audio_4231_trap_cookie);
		ddi_regs_map_free(&unitp->cnf_handle_eb2record);
		ddi_regs_map_free(&unitp->cnf_handle_eb2play);
		ddi_regs_map_free(&unitp->cnf_handle);
		return (DDI_FAILURE);
	}
#endif
	return (DDI_SUCCESS);
}

/*
 * audio_4231_eb2_version();
 *
 * returns the version number of the DMA engine used by this driver.
 */
void
audio_4231_eb2_version(cs_unit_t *unitp, caddr_t verp)
{
	if (unitp->module_type == B_TRUE) {
		if (unitp->cd_input_line != NO_INTERNAL_CD) {
			strcpy(verp, CS_DEV_VERSION_F);
		} else {
			strcpy(verp, CS_DEV_VERSION_G);
		}
	} else {
		if (unitp->cd_input_line != NO_INTERNAL_CD) {
			strcpy(verp, CS_DEV_VERSION);
		} else {
			strcpy(verp, CS_DEV_VERSION_C);
		}
	}
}

void
audio_4231_eb2_start_input(cs_unit_t *unitp)
{

	ATRACE(audio_4231_eb2_start_input, 'RCSR',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_CSR));
	OR_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR,
	    EB2_RESET);
	ATRACE(audio_4231_eb2_start_input, 'RCS1',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_CSR));

	audio_4231_eb2cycpend(unitp->cnf_handle_eb2record,
	    (uint32_t *)EB2_REC_CSR);
	ATRACE(audio_4231_eb2_start_input, 'CYCL',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_CSR));
	AND_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR,
	    ~EB2_RESET);
	ATRACE(audio_4231_eb2_start_input, 'RSET',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_CSR));
	audio_4231_recintr(unitp);

	ATRACE(audio_4231_eb2_start_input, 'RST1',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_CSR));
	AND_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR,
	    ~EB2_DISAB_CSR_DRN);
	OR_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR,
	    EB2_CAP_SETUP);

	ATRACE(audio_4231_eb2_start_input, 'RRSR',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_CSR));
	ATRACE(audio_4231_eb2_start_input, 'RACR',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_ACR));
	ATRACE(audio_4231_eb2_start_input, 'RBCR',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_BCR));
}

void
audio_4231_eb2_start_output(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;
	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(handle, CS4231_IDR, PEN_DISABLE);

	OR_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR, EB2_RESET);
	audio_4231_eb2cycpend(unitp->cnf_handle_eb2play,
	    (uint32_t *)EB2_PLAY_CSR);
	AND_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR, ~EB2_RESET);
	audio_4231_clear((aud_dma_list_t *)&dma_played_list, unitp);
	ATRACE(audio_4231_eb2_start_output, 'EB2S',
	    ddi_get32(unitp->cnf_handle_eb2play, (uint32_t *)EB2_PLAY_CSR));

	if (audio_4231_playintr(unitp)) {
		OR_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR,
		    EB2_PLAY_SETUP);
		ATRACE(audio_4231_eb2_start_output, 'CSR0',
		    ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_CSR));

		REG_SELECT(INTERFACE_CR);
		OR_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, PEN_ENABLE);
		ATRACE(audio_4231_eb2_start_output, 'ECSR',
		    ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_CSR));
		ATRACE(audio_4231_eb2_start_output, 'EACR',
		    ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_ACR));
		ATRACE(audio_4231_eb2_start_output, 'EBCR',
		    ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_BCR));
	}
}

void
audio_4231_eb2_stop_input(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	uint32_t eb2csr;

	handle = unitp->cnf_handle;
	chip = unitp->chip;

	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, CEN_DISABLE);

	/*
	 * Set flag to notify eb2intr of pending interrupt on stop
	 */
	eb2csr = ddi_get32(unitp->cnf_handle_eb2record,
			(uint32_t *)EB2_REC_CSR);
	ATRACE(audio_4231_eb2_stop_input, 'RCSR', eb2csr);
	if (eb2csr & EB2_TC)
		unitp->rec_intr_flag = 1;
	AND_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR, ~EB2_INT_EN);

	/*
	 * wait for fifo in cheerio to drain
	 */
	drv_usecwait(1000);
	AND_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR, ~EB2_EN_DMA);
}

void
audio_4231_eb2_stop_output(cs_unit_t *unitp)
{
	AND_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR,
	    (~EB2_EN_DMA));
	AND_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR,
	    (~EB2_INT_EN));
	/*
	 * wait for fifo in cheerio to drain
	 */
	drv_usecwait(1000);
	OR_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR, EB2_RESET);
	audio_4231_eb2cycpend(unitp->cnf_handle_eb2play,
	    (uint32_t *)EB2_PLAY_CSR);
	AND_SET_LONG_R(unitp->cnf_handle_eb2play, EB2_PLAY_CSR, ~EB2_RESET);
	audio_4231_clear((aud_dma_list_t *)&dma_played_list, unitp);
	ATRACE(audio_4231_eb2_stop_output, 'PCSR',
	    ddi_get32(unitp->cnf_handle_eb2play, (uint32_t *)EB2_PLAY_CSR));
}

void
audio_4231_eb2_get_count(cs_unit_t *unitp, uint32_t *pcount, uint32_t *ccount)
{
	if (pcount != NULL)
		*pcount = ddi_get32(unitp->cnf_handle_eb2play,
				(uint32_t *)EB2_PLAY_BCR);
	if (ccount != NULL)
		*ccount = ddi_get32(unitp->cnf_handle_eb2record,
				(uint32_t *)EB2_REC_BCR);
}

void
audio_4231_eb2_get_ncount(cs_unit_t *unitp, int direction, uint32_t *ncount)
{
	uint32_t eb2csr;

	/*
	 * The next counter on Eb2 is not readable if DMA_CHAINING is on,
	 * therefore we must subtract the buffer size if record and
	 * AUD_4231_BSIZE if play?
	 */

	switch (direction) {
	case PLAYBACK:
	default:
			eb2csr = ddi_get32(unitp->cnf_handle_eb2play,
					(uint32_t *)EB2_PLAY_BCR);
			if (eb2csr & EB2_NADDR_LOADED) {
				*ncount = audio_4231_bsize;
			} else {
				*ncount = 0;
			}
			break;

	case RECORD:
			*ncount = unitp->input.as.info.buffer_size;
			break;
	}
}

/* XXXMERGE check invocations of this routine - do we still need the */
/* &eb2intr parameter? */
ddi_acc_handle_t
audio_4231_eb2_get_acchandle(cs_unit_t *unitp, uint32_t *eb2intr, int direction)
{
	if (direction == PLAYBACK) {
		*eb2intr = ddi_get32(unitp->cnf_handle_eb2play,
				(uint32_t *)EB2_PLAY_CSR);
		return (unitp->cnf_handle_eb2play);
	} else {
		*eb2intr = ddi_get32(unitp->cnf_handle_eb2record,
				(uint32_t *)EB2_REC_CSR);
		return (unitp->cnf_handle_eb2record);
	}
}

uint_t
audio_4231_eb2_dma_setup(cs_unit_t *unitp, uint_t direction,
    aud_cmd_t *cmdp, size_t length)
{
	int e;
	ddi_dma_handle_t *dma_handle;
	ddi_dma_cookie_t *dma_cookie;
	uint_t *dma_cookie_count;
	char *dirp[] = {
		"playback",
		"record"
	};

	if (direction == PLAYBACK) {
		dma_handle = &unitp->play_dma_handle;
		dma_cookie = &unitp->play_dma_cookie;
		dma_cookie_count = &unitp->play_dma_cookie_count;
	} else {
		dma_handle = &unitp->cap_dma_handle;
		dma_cookie = &unitp->cap_dma_cookie;
		dma_cookie_count = &unitp->cap_dma_cookie_count;
	}
	e = ddi_dma_alloc_handle(unitp->dip, unitp->dma_attrp,
	    DDI_DMA_DONTWAIT, NULL, dma_handle);

	if (e == DDI_DMA_BADATTR) {
		cmn_err(CE_WARN, "BAD_ATTR val 0x%X in %s!", e,
		    dirp[direction]);
		return (B_FALSE);
	} else if (e != DDI_SUCCESS) {
		cmn_err(CE_WARN, "DMA_ALLOC_HANDLE failed in %s!",
		    dirp[direction]);
		return (B_FALSE);
	}

	e  = ddi_dma_addr_bind_handle(*dma_handle, (struct as *)0,
		(caddr_t)cmdp->data, length,
		(direction == PLAYBACK) ? DDI_DMA_WRITE : DDI_DMA_READ,
		DDI_DMA_DONTWAIT, NULL, dma_cookie, dma_cookie_count);

	if (e != DDI_SUCCESS) {
		audio_4231_dma_errprt(e);
		return (B_FALSE);
	}
	return (B_TRUE);
}

/*ARGSUSED*/
uint32_t
audio_4231_eb2_play_last(cs_unit_t *unitp, ddi_acc_handle_t handle,
    uint32_t eb2intr)
{
	return (ddi_get32(unitp->cnf_handle_eb2play,
		(uint32_t *)EB2_PLAY_BCR));
}

/*ARGSUSED*/
uint32_t
audio_4231_eb2_play_cleanup(cs_unit_t *unitp, ddi_acc_handle_t handle,
    uint32_t eb2intr)
{
	if (!(eb2intr & EB2_NADDR_LOADED) && (eb2intr & EB2_EN_NEXT)) {
		return (ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_BCR));
	}
	return (0);
}

/*ARGSUSED*/
void
audio_4231_eb2_play_next(cs_unit_t *unitp, ddi_acc_handle_t handle)
{
	ddi_dma_cookie_t *cookiep = &unitp->play_dma_cookie;

	ATRACE(audio_4231_eb2_play_next, 'EB2C', cookiep->dmac_size);
	ddi_put32(unitp->cnf_handle_eb2play,
		(uint32_t *)EB2_PLAY_BCR, cookiep->dmac_size);
	unitp->typ_playlength = cookiep->dmac_size;
	unitp->output.samples +=
	    audio_4231_sampleconv(&unitp->output, unitp->typ_playlength);
	unitp->playlastaddr = cookiep->dmac_address;
	ATRACE(audio_4231_eb2_play_next, 'EB2A', cookiep->dmac_address);
	ddi_put32(unitp->cnf_handle_eb2play, (uint32_t *)EB2_PLAY_ACR,
	    cookiep->dmac_address);

	ATRACE(audio_4231_eb2_play_next, 'CHPC',
	    ddi_get32(unitp->cnf_handle_eb2play, (uint32_t *)EB2_PLAY_BCR));
	ATRACE(audio_4231_eb2_play_next, 'CHPA',
	    ddi_get32(unitp->cnf_handle_eb2play, (uint32_t *)EB2_PLAY_ACR));
	ATRACE(audio_4231_eb2_play_next, 'ECSR',
	    ddi_get32(unitp->cnf_handle_eb2play, (uint32_t *)EB2_PLAY_CSR));
}

void
audio_4231_eb2_rec_next(cs_unit_t *unitp)
{
	ddi_dma_cookie_t *cookiep = &unitp->cap_dma_cookie;

	ddi_put32(unitp->cnf_handle_eb2record,
		(uint32_t *)EB2_REC_BCR, cookiep->dmac_size);
	unitp->typ_reclength = cookiep->dmac_size;

	ATRACE(audio_4231_recintr, 'CHPC',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_BCR));
	ATRACE(audio_4231_recintr, 'CHPA',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_ACR));
	ATRACE(audio_4231_recintr, 'ECSR',
	    ddi_get32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_CSR));
	/*
	 * This wait is necessary to avoid a target abort in the
	 * PCI bus. The dma engine seems to have a problem with
	 * back to back writes of addresses and counts
	 */
	drv_usecwait(30);
	ddi_put32(unitp->cnf_handle_eb2record, (uint32_t *)EB2_REC_ACR,
	    cookiep->dmac_address);
}

/* XXXMERGE need handle and friends here */
void
audio_4231_eb2_rec_cleanup(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = unitp->cnf_handle;
	chip = unitp->chip;

	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(handle, CS4231_IDR, CEN_DISABLE);
	AND_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR,
	    ~(EB2_INT_EN));

	/*
	 * wait for fifo in cheerio to drain
	 */
	drv_usecwait(1000);
	AND_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR, ~(EB2_EN_DMA));
	unitp->input.error = B_TRUE;
	unitp->input.active = B_FALSE;
	unitp->input.cmdptr = NULL;
}


/*
 * Other routines specific to this DMA engine
 */
uint_t
audio_4231_eb2recintr()
{
	cs_unit_t *unitp;
	uint32_t eb2intr;


	/*
	 * Figure out which chip interrupted.
	 * Since we only have one chip, we punt and assume device zero.
	 */
	unitp = &cs_units[0];
	/* Acquire spin lock */

#ifndef MULTI_DEBUG
	LOCK_UNITP(unitp);
#endif

	/* clear off the intr first */
	eb2intr = ddi_get32(unitp->cnf_handle_eb2record,
			(uint32_t *)EB2_REC_CSR);
	OR_SET_LONG_R(unitp->cnf_handle_eb2record, EB2_REC_CSR,
	    EB2_TC);
	/*
	 * Check to see if it is a Err, if not check to see if
	 * it isn't a TC intr (which we are expecting), if not
	 * a TC intr it is a dev intr. Not expected for now.
	 */
	if (eb2intr & EB2_ERR_PEND) {
		cmn_err(CE_WARN,
		    "EB2 Record Error CSR is 0x%X, ACR = 0x%X, BCR = 0x%X",
		    eb2intr,
		    ddi_get32(unitp->cnf_handle_eb2record,
			(uint32_t *)EB2_PLAY_ACR),
		    ddi_get32(unitp->cnf_handle_eb2record,
			(uint32_t *)EB2_PLAY_BCR));
		OR_SET_LONG_R(unitp->cnf_handle_eb2record,
		    EB2_REC_CSR, EB2_RESET);
		audio_4231_eb2cycpend(unitp->cnf_handle_eb2record,
		    (uint32_t *)EB2_REC_CSR);
		AND_SET_LONG_R(unitp->cnf_handle_eb2record,
		    EB2_REC_CSR,
		    ~EB2_RESET);
		ATRACE(audio_4231_eb2recintr, 'ERR ',
		    ddi_get32(unitp->cnf_handle_eb2record,
			(uint32_t *)EB2_REC_CSR));
		unitp->rec_intr_flag = 0;
#ifndef MULTI_DEBUG
		UNLOCK_UNITP(unitp);
#endif
		return (DDI_INTR_CLAIMED);
	} else if (!(eb2intr & EB2_TC)) {
		int	rc = DDI_INTR_UNCLAIMED;

		/*
		 * if EB2_TC interrupt was pending during stop, then claim it
		 */
		if (unitp->rec_intr_flag) {
			unitp->rec_intr_flag = 0;
			rc = DDI_INTR_CLAIMED;
		}
#ifndef MULTI_DEBUG
		UNLOCK_UNITP(unitp);
#endif
		return (rc);
	}
	unitp->input.samples +=
	    audio_4231_sampleconv(&unitp->input,
	    unitp->typ_reclength);
	unitp->rec_intr_flag = 0;
	audio_4231_recintr(unitp);
#ifndef MULTI_DEBUG
	UNLOCK_UNITP(unitp);
#endif
	return (DDI_INTR_CLAIMED);
}

uint_t
audio_4231_eb2playintr()
{
	cs_unit_t *unitp;
	uint32_t eb2intr;
	uint32_t byte_count;


	/*
	 * Figure out which chip interrupted.
	 * Since we only have one chip, we punt and assume device zero.
	 */
	unitp = &cs_units[0];
	/* Acquire spin lock */

#ifndef MULTI_DEBUG
	LOCK_UNITP(unitp);
#endif

	/* check to see if this is an eb2 intr */
	eb2intr = ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_CSR);

	/* clear the intr */
	OR_SET_LONG_R(unitp->cnf_handle_eb2play,
	    EB2_PLAY_CSR, EB2_TC);
	/*
	 * Check to see if it is a Err, if not check to see if
	 * it isn't a TC intr (which we are expecting), if not
	 * a TC intr it is a dev intr. Not expected for now.
	 */
	if (eb2intr & EB2_ERR_PEND) {
		byte_count = ddi_get32(unitp->cnf_handle_eb2play,
				(uint32_t *)EB2_PLAY_BCR);
		cmn_err(CE_WARN,
		    "EB2 PLay Error CSR is 0x%X, ACR = 0x%X, BCR = 0x%X",
		    eb2intr,
		    ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_ACR),
		    byte_count);

		OR_SET_LONG_R(unitp->cnf_handle_eb2play,
		    EB2_PLAY_CSR,
		    EB2_RESET);
		audio_4231_eb2cycpend(unitp->cnf_handle_eb2play,
		    (uint32_t *)EB2_PLAY_CSR);
		AND_SET_LONG_R(unitp->cnf_handle_eb2play,
		    EB2_PLAY_CSR,
		    ~EB2_RESET);
		audio_4231_clear((aud_dma_list_t *)&dma_played_list,
				    unitp);
		audio_process_output(&unitp->output.as);
#ifndef MULTI_DEBUG
		UNLOCK_UNITP(unitp);
#endif
		return (DDI_INTR_CLAIMED);

	} else if (!(eb2intr & EB2_TC)) {
#ifndef MULTI_DEBUG
		UNLOCK_UNITP(unitp);
#endif
		return (DDI_INTR_UNCLAIMED);

	} else if (eb2intr & EB2_TC) {
		/* XXX may only need to be done if active */
		(void) audio_4231_playintr(unitp);
		audio_process_output(&unitp->output.as); /* XXX needed? */
	}

#ifndef MULTI_DEBUG
	UNLOCK_UNITP(unitp);
#endif
	return (DDI_INTR_CLAIMED);
}

#ifdef MULTI_DEBUG
uint_t
audio_4231_eb2cintr()
{

	cs_unit_t *unitp;
	uint_t rc;
	uint32_t reccsr, playcsr;
	u_char tmpval;

	unitp = &cs_units[0];
	LOCK_UNITP(unitp);
	rc = DDI_INTR_UNCLAIMED;

	playcsr = ddi_get32(unitp->cnf_handle_eb2play,
			(uint32_t *)EB2_PLAY_CSR);
	reccsr = ddi_get32(unitp->cnf_handle_eb2record,
			(uint32_t *)EB2_REC_CSR);

	ATRACE(audio_4231_eb2cintr, 'PLCR', playcsr);
	ATRACE(audio_4231_eb2cintr, 'RECR', reccsr);

	if (playcsr & EB2_INT_PEND) {
		ATRACE(audio_4231_eb2cintr, 'PLAY', EB2_PLAY_CSR);
		audio_4231_eb2playintr();
		rc = DDI_INTR_CLAIMED;
	}
	if (reccsr & EB2_INT_PEND) {
		ATRACE(audio_4231_eb2cintr, 'RECR', EB2_REC_CSR);
		audio_4231_eb2recintr();
		rc = DDI_INTR_CLAIMED;
	}
	UNLOCK_UNITP(unitp);
	return (rc);
}
#endif

void
audio_4231_eb2cycpend(ddi_acc_handle_t handle, uint32_t *addr)
{
	int x = 0;
	uint32_t eb2csr;

	eb2csr = ddi_get32(handle, addr);

	while ((eb2csr & EB2_CYC_PENDING) && (x <= CS_TIMEOUT)) {
		eb2csr = ddi_get32(handle, addr);
		x++;
	}
}

ops_t audio_4231_eb2dma_ops = {
	"Ebus2 DMA controller",
	audio_4231_eb2_reset,
	audio_4231_eb2_mapregs,
	audio_4231_eb2_unmapregs,
	audio_4231_eb2_addintr,
	audio_4231_eb2_version,
	audio_4231_eb2_start_input,
	audio_4231_eb2_start_output,
	audio_4231_eb2_stop_input,
	audio_4231_eb2_stop_output,
	audio_4231_eb2_get_count,
	audio_4231_eb2_get_ncount,
	audio_4231_eb2_get_acchandle,
	audio_4231_eb2_dma_setup,
	audio_4231_eb2_play_last,
	audio_4231_eb2_play_cleanup,
	audio_4231_eb2_play_next,
	audio_4231_eb2_rec_next,
	audio_4231_eb2_rec_cleanup
};
