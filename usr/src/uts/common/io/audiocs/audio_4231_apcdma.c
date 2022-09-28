/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_4231_apcdma.c	1.14	96/09/24 SMI"

/*
 * Platform-specific code for the APC DMA controller used with the
 * SPARC CS4231 audio board (in Sbus systems).
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

extern void audio_4231_start(aud_stream_t *);
extern uint_t audio_4231_playintr(cs_unit_t *);
extern void audio_4231_recintr(cs_unit_t *);
extern void audio_4231_pollready();
extern void audio_4231_clear(aud_dma_list_t *, cs_unit_t *);
extern void audio_4231_samplecalc(cs_unit_t *, uint_t, uint_t);
extern uint_t audio_4231_sampleconv(cs_stream_t *, uint_t);
extern void audio_4231_dma_errprt(int);

extern cs_unit_t *cs_units;		/* device controller array */

/*
 * Local DMA register definitions
 */
#define	APC_DMACSR	&unitp->chip->dmaregs.dmacsr

/*
 * Local routines
 */
static void audio_4231_pollpipe(cs_unit_t *);
static void audio_4231_pollppipe(cs_unit_t *);
static uint_t audio_4231_apc_cintr();

extern ddi_iblock_cookie_t audio_4231_trap_cookie;

/*
 * attribute structure for the APC DMAC
 * The APC chip can support full 32-bit DMA addresses
 */
ddi_dma_attr_t  aud_apcdma_attr = {
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
 * audio_4231_dma_ops routines for the APC DMA engine
 */
void
audio_4231_apc_reset(cs_unit_t *unitp, ddi_acc_handle_t handle)
{
	/*
	 * The APC has a bug where the reset is not done
	 * until you do the next pio to the APC. This
	 * next write to the CSR causes the posted reset to
	 * happen.
	 */
	ddi_put32(handle, (uint32_t *)APC_DMACSR, APC_RESET);
	ddi_put32(handle, (uint32_t *)APC_DMACSR, 0x00);

	OR_SET_LONG_R(handle, APC_DMACSR, APC_CODEC_PDN);
	drv_usecwait(20);
	AND_SET_LONG_R(handle, APC_DMACSR, ~APC_CODEC_PDN);
}

/*
 * audio_4231_apc_mapregs():
 *	Map in the CS4231 and APC DMA registers
 *
 * RETURNS:
 *	DDI_SUCCESS/DDI_FAILURE
 */
int
audio_4231_apc_mapregs(dev_info_t *dip, cs_unit_t *unitp)
{
	ddi_device_acc_attr_t acc_attr;

	acc_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	acc_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	acc_attr.devacc_attr_endian_flags = DDI_STRUCTURE_BE_ACC;

	if (ddi_regs_map_setup(dip, 0, (caddr_t *)&unitp->chip, 0,
	    sizeof (struct aud_4231_chip), &acc_attr,
	    &CS4231_HANDLE) != DDI_SUCCESS) {
		/* Deallocate structures allocated above */
		kmem_free(unitp->allocated_memory, unitp->allocated_size);
		ddi_regs_map_free(&CS4231_HANDLE);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

void
audio_4231_apc_unmapregs(cs_unit_t *unitp)
{
	ddi_regs_map_free(&CS4231_HANDLE);
}

/*
 * audio_4231_apc_addintr():
 *
 * We only expect one hard interrupt address at level 5.
 * for the apc dma interface.
 *
 * RETURNS:
 *	DDI_SUCCESS/DDI_FAILURE
 */
int
audio_4231_apc_addintr(dev_info_t *dip, cs_unit_t *unitp)
{
/* XXXMERGE need to add unitp arg to all addintr calls */
	if (ddi_add_intr(dip, 0, &audio_4231_trap_cookie,
	    (ddi_idevice_cookie_t *)0, audio_4231_apc_cintr,
	    (caddr_t)0) != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "audiocs: bad 0 interrupt specification");
		ddi_regs_map_free(&CS4231_HANDLE);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * audio_4231_apc_version();
 *
 * returns the version number of the DMA engine used by this driver.
 */
void
audio_4231_apc_version(cs_unit_t *unitp, caddr_t verp)
{
	if (unitp->cd_input_line != NO_INTERNAL_CD) {
		strcpy(verp, CS_DEV_VERSION);
	} else {
		strcpy(verp, CS_DEV_VERSION_B);
	}
}

void
audio_4231_apc_start_input(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;


	handle = unitp->cnf_handle;
	chip = unitp->chip;

	AND_SET_LONG_R(CS4231_HANDLE, APC_DMACSR, CAP_UNPAUSE);
	audio_4231_recintr(unitp);
	NOR_SET_LONG_R(CS4231_HANDLE, APC_DMACSR, CAP_SETUP, APC_INTR_MASK);
	/*
	 * This was moved into here from the *generic*
	 * because there is a timing problem on the SPARC
	 * platforms when doing simultaneous play and
	 * record. The record dma never starts because the
	 * CODEC seems to stall the dma engine or miss the
	 * dma_req.
	 */
	audio_4231_pollready();
	REG_SELECT(INTERFACE_CR);
	OR_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, CEN_ENABLE);
}

void
audio_4231_apc_start_output(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AND_SET_LONG_R(CS4231_HANDLE, APC_DMACSR, PLAY_UNPAUSE);
	ATRACE(audio_4231_apc_start_output, 'rscp', chip->dmaregs.dmacsr);

	if (audio_4231_playintr(unitp)) {
		NOR_SET_LONG_R(CS4231_HANDLE, APC_DMACSR, PLAY_SETUP,
		    APC_INTR_MASK);
		ATRACE(audio_4231_apc_start_output, 'RSCP',
		    chip->dmaregs.dmacsr);

		/*
		 * generic code
		 */
		REG_SELECT(INTERFACE_CR);
		OR_SET_BYTE_R(handle, CS4231_IDR, PEN_ENABLE);
		ATRACE(audio_4231_apc_start_output, 'rdiP', chip->pioregs.idr);
	}
}

void
audio_4231_apc_stop_input(cs_unit_t *unitp)
{
	NOR_SET_LONG_R(CS4231_HANDLE, APC_DMACSR, APC_CPAUSE, APC_INTR_MASK);
	audio_4231_pollpipe(unitp);
}

void
audio_4231_apc_stop_output(cs_unit_t *unitp)
{
	NOR_SET_LONG_R(CS4231_HANDLE, APC_DMACSR, APC_PPAUSE, APC_INTR_MASK);
	audio_4231_pollppipe(unitp);
	audio_4231_clear((aud_dma_list_t *)&dma_played_list, unitp);
}

void
audio_4231_apc_get_count(cs_unit_t *unitp, uint32_t *pcount, uint32_t *ccount)
{
	struct aud_4231_chip *chip;

	chip = unitp->chip;
	if (pcount != NULL)
		*pcount = ddi_get32(CS4231_HANDLE,
				(uint32_t *)&chip->dmaregs.dmapc);
	if (ccount != NULL)
		*ccount = ddi_get32(CS4231_HANDLE,
				(uint32_t *)&chip->dmaregs.dmacc);
}

void
audio_4231_apc_get_ncount(cs_unit_t *unitp, int direction, uint32_t *ncount)
{

	switch (direction) {
	case PLAYBACK:
	default:
			*ncount = ddi_get32(CS4231_HANDLE,
			    (uint32_t *)&unitp->chip->dmaregs.dmapnc);
			break;
	case RECORD:
			*ncount = ddi_get32(CS4231_HANDLE,
			    (uint32_t *)&unitp->chip->dmaregs.dmacnc);
			break;
	}
}

/*ARGSUSED*/
ddi_acc_handle_t
audio_4231_apc_get_acchandle(cs_unit_t *unitp, uint32_t *eb2intr, int direction)
{
	return (CS4231_HANDLE);
}

uint_t
audio_4231_apc_dma_setup(cs_unit_t *unitp, uint_t direction,
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

/* XXXMERGE - don't forget to update the 8237 physdma structure; */
/* remove the redundant cookies, etc. */
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
	    DDI_DMA_DONTWAIT, 0, dma_cookie, dma_cookie_count);

/* XXXMERGE make DMA errprt fn generic */
	if (e != DDI_SUCCESS) {
		audio_4231_dma_errprt(e);
		return (B_FALSE);
	}
	return (B_TRUE);
}

/*ARGSUSED*/
uint32_t
audio_4231_apc_play_last(cs_unit_t *unitp, ddi_acc_handle_t handle,
    uint32_t eb2intr)
{
	return (ddi_get32(CS4231_HANDLE,
		(uint32_t *)&unitp->chip->dmaregs.dmapc));
}

/*ARGSUSED*/
uint32_t
audio_4231_apc_play_cleanup(cs_unit_t *unitp, ddi_acc_handle_t handle,
    uint32_t eb2intr)
{
	return (ddi_get32(handle, (uint32_t *)&unitp->chip->dmaregs.dmapc));
}

void
audio_4231_apc_play_next(cs_unit_t *unitp, ddi_acc_handle_t handle)
{
	ddi_dma_cookie_t *cookiep = &unitp->play_dma_cookie;

	ddi_put32(handle, (uint32_t *)&unitp->chip->dmaregs.dmapnva,
		cookiep->dmac_address);
	ddi_put32(handle, (uint32_t *)&unitp->chip->dmaregs.dmapnc,
		cookiep->dmac_size);
	ATRACE(audio_4231_apc_play_next, ' AVP', unitp->chip->dmaregs.dmapva);
	ATRACE(audio_4231_apc_play_next, 'TNCP', unitp->chip->dmaregs.dmapc);
	ATRACE(audio_4231_apc_play_next, 'AVNP', unitp->chip->dmaregs.dmapnva);
	ATRACE(audio_4231_apc_play_next, ' CNP', unitp->chip->dmaregs.dmapnc);
	ATRACE(audio_4231_apc_play_next, 'RSCP', unitp->chip->dmaregs.dmacsr);

	unitp->typ_playlength = cookiep->dmac_size;
}

void
audio_4231_apc_rec_next(cs_unit_t *unitp)
{
	ddi_dma_cookie_t *cookiep = &unitp->cap_dma_cookie;

	ddi_put32(CS4231_HANDLE, (uint32_t *)&unitp->chip->dmaregs.dmacnva,
	    cookiep->dmac_address);
	ddi_put32(CS4231_HANDLE, (uint32_t *)&unitp->chip->dmaregs.dmacnc,
	    cookiep->dmac_size);

	unitp->typ_reclength = cookiep->dmac_size;
}

void
audio_4231_apc_rec_cleanup(cs_unit_t *unitp)
{
	NOR_SET_LONG_R(CS4231_HANDLE, APC_DMACSR, APC_CPAUSE, APC_INTR_MASK);
	audio_4231_pollpipe(unitp);
}

/*
 * Other routines specific to this DMA engine
 */

/*
 * Common interrupt routine. vectors to play or record.
 */
uint_t
audio_4231_apc_cintr()
{
	cs_unit_t *unitp;
	struct aud_4231_chip *chip;
	ddi_acc_handle_t handle;
	long dmacsr, rc;

	/* Acquire spin lock */

	/*
	 * Figure out which chip interrupted.
	 * Since we only have one chip, we punt and assume device zero.
	 */
	unitp = &cs_units[0];
	LOCK_UNITP(unitp);
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	rc = DDI_INTR_UNCLAIMED;

	/* read and store the APC csr */
	dmacsr = ddi_get32(handle, (uint32_t *)&chip->dmaregs.dmacsr);

	/* clear all possible ints */
	ddi_put32(handle, (uint32_t *)&chip->dmaregs.dmacsr, dmacsr);

	ATRACE(audio_4231_apc_cintr, 'RSCD', dmacsr);

	/*
	 * We want to update the record samples and play samples
	 * when we take an interrupt only. This is because of the
	 * prime condition that we do on a start. We end up
	 * getting dmasize ahead on the sample count because of the
	 * dual dma registers in the APC chip.
	 */

	if (dmacsr & APC_CI) {	/* capture interrupt */
		if (dmacsr & APC_CD) {	/* capture NVA dirty */
			unitp->input.samples +=
				audio_4231_sampleconv(&unitp->input,
				    unitp->typ_reclength);
			audio_4231_recintr(unitp);
		}

		rc = DDI_INTR_CLAIMED;
	}

	/* capture pipe empty interrupt */
	if ((dmacsr & APC_CMI) && (unitp->input.active != B_TRUE)) {
		ATRACE(audio_4231_apc_cintr, 'PCON', dmacsr);

		unitp->input.active = B_FALSE;
		rc = DDI_INTR_CLAIMED;
	}

	/* playback pipe empty interrupt */
	if ((dmacsr & APC_PMI) && (unitp->output.active != B_TRUE)) {
		ATRACE(audio_4231_apc_cintr, 'LPON', dmacsr);

		if (unitp->output.as.openflag) {
			audio_4231_samplecalc(unitp, unitp->typ_playlength,
				    PLAYBACK);
		}
		audio_4231_clear((aud_dma_list_t *)&dma_played_list,
				    unitp);
		unitp->output.active = B_FALSE;
		unitp->output.cmdptr = NULL;
		audio_process_output(&unitp->output.as);
		rc = DDI_INTR_CLAIMED;
	}
	if (dmacsr & APC_PI) {	/* playback interrupt */
		if (dmacsr & APC_PD) {	/* playback NVA dirty */
			if (unitp->output.active &&
			    ddi_get32(handle,
				(uint32_t *)&chip->dmaregs.dmapc)) {
				unitp->output.samples +=
				    audio_4231_sampleconv(&unitp->output,
					unitp->typ_playlength);
			}

			(void) audio_4231_playintr(unitp);
			audio_process_output(&unitp->output.as);
		}

		rc = DDI_INTR_CLAIMED;

	}


	if (dmacsr & APC_EI) {	/* general interrupt */
		ATRACE(audio_4231_apc_cintr, '!RRE', dmacsr);

		rc = DDI_INTR_CLAIMED;
#ifdef AUDIOTRACE
		cmn_err(CE_WARN, "audio_4231_apc_cintr: BUS ERROR! dmacsr 0x%x",
				    dmacsr);
#endif
	}

	ATRACE(audio_4231_apc_cintr, 'TER ', dmacsr);
	UNLOCK_UNITP(unitp);
	return (rc);


}

void
audio_4231_pollppipe(cs_unit_t *unitp)
{

	ulong_t dmacsr;
	ddi_acc_handle_t handle;
	int x = 0;

	handle = CS4231_HANDLE;

	dmacsr = ddi_get32(handle, (uint32_t *)APC_DMACSR);
	while (!(dmacsr & APC_PM) && x <= CS_TIMEOUT) {
		dmacsr = ddi_get32(handle, (uint32_t *)APC_DMACSR);
		x++;
	}
}


void
audio_4231_pollpipe(cs_unit_t *unitp)
{

	ddi_acc_handle_t handle;
	ulong_t dmacsr;
	int x = 0;

	handle = CS4231_HANDLE;
	dmacsr = ddi_get32(handle, (uint32_t *)APC_DMACSR);

	while (!(dmacsr & APC_CM) && x <= CS_TIMEOUT) {
		dmacsr = ddi_get32(handle, (uint32_t *)APC_DMACSR);
		x++;
	}
}

ops_t audio_4231_apcdma_ops = {
	"APC DMA controller",
	audio_4231_apc_reset,
	audio_4231_apc_mapregs,
	audio_4231_apc_unmapregs,
	audio_4231_apc_addintr,
	audio_4231_apc_version,
	audio_4231_apc_start_input,
	audio_4231_apc_start_output,
	audio_4231_apc_stop_input,
	audio_4231_apc_stop_output,
	audio_4231_apc_get_count,
	audio_4231_apc_get_ncount,
	audio_4231_apc_get_acchandle,
	audio_4231_apc_dma_setup,
	audio_4231_apc_play_last,
	audio_4231_apc_play_cleanup,
	audio_4231_apc_play_next,
	audio_4231_apc_rec_next,
	audio_4231_apc_rec_cleanup
};
