/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_4231_8237dma.c	1.12	96/09/24 SMI"

/*
 * Platform-specific code for the 8237 (system) DMA controller used with the
 * x86/PowerPC CS4231 audio device.
 */

#include <sys/errno.h>		/* ENXIO */
#include <sys/types.h>
#include <sys/systm.h>		/* XXXPPC - temp hack for spl() */
#include <sys/archsystm.h>	/* va_to_pa() */
#include <sys/stream.h>		/* mblk_t */
#include <sys/kmem.h>		/* kmem_free() */
#include <sys/debug.h>		/* ASSERT() */
#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/sunddi.h>		/* dditypes.h */
#include <sys/dma_engine.h>	/* ddi_dmae_*() */
#include <sys/audio_4231.h>
#include <sys/audio_4231_dma.h>
#include <sys/audio_4231_debug.h>
#include <sys/audiodebug.h>	/* ATRACE */
#include <sys/ddi.h>

extern uint_t audio_4231_playintr(cs_unit_t *);
extern void audio_4231_recintr(cs_unit_t *);
extern void audio_4231_pollready(void);
extern uint_t audio_4231_sampleconv(cs_stream_t *, uint_t length);
extern void audio_4231_samplecalc(cs_unit_t *, uint_t, uint_t);
extern void audio_4231_clear(aud_dma_list_t *, cs_unit_t *);
extern void audio_4231_dma_errprt(int);
extern int splx(int);
extern int spl6(void);
extern u_int va_to_pa(u_int);
extern void prom_printf(char *fmt, ...);

extern cs_unit_t *cs_units;		/* device controller array */

/*
 * Local DMA engine definitions
 */
#define	PB_CHNL		(unitp->physdma.pb_chan)
#define	RC_CHNL		(unitp->physdma.rc_chan)
#define	PB_BUFADDR	(unitp->physdma.PBbuffer[0])
#define	RC_BUFADDR	(unitp->physdma.RCbuffer[0])
#define	PB_BUFLEN	(unitp->physdma.PBbuflen)
#define	RC_BUFLEN	(unitp->physdma.RCbuflen)
#define	BUFLEN 32768	/* XXXPPC static I/O buffer size for now */
#define	DMA_FIFO_PAD	8

extern int aud_errmask;
extern int aud_errlevel;

#if defined(DEBUG)
extern void audio_4231_dumpregs(void);
extern void audio_4231_int_status(void);
extern int audiocs_debug;
extern int audiocs_pio;
#endif	/* defined(DEBUG) */


/*
 * Local routines
 */
static uint_t audio_4231_8237_cintr();
static int audio_4231_8237_setup_dma_buffers(cs_unit_t *);
static void audio_4231_8237_program_dma_engine(cs_unit_t *, uint_t);

extern ddi_iblock_cookie_t audio_4231_trap_cookie;

/*
 * attribute structure for the Intel 8237 DMAC
 */
ddi_dma_attr_t  aud_8237dma_attr = {
	DMA_ATTR_V0,			/* version number		*/
	(unsigned long long)0x00000000,	/* low address of range		*/
	(unsigned long long)0x00ffffff,	/* high address range		*/
	(unsigned long long)0x0000ffff,	/* counter register upper bound	*/
	(unsigned long long)0x00000001,	/* alignment requirements	*/
	1,				/* allowable burstsizes (flags)	*/
	DMA_UNIT_8,			/* min DMA access size (bytes)	*/
	(unsigned long long)0xffffffff,	/* max DMA access size (bytes)	*/
	(unsigned long long)0x0000ffff,	/* segment boundaries (if any)	*/
	1,				/* scatter/gather list length	*/
	1,				/* transfer size granularity	*/
	0x0				/* attribute flags - reserved	*/
};

ddi_device_acc_attr_t dma_acc_attr;	/* access attribs for I/O buffer */
ddi_device_acc_attr_t reg_acc_attr;	/* access attribs for dev regs	*/

extern aud_dma_list_t dma_played_list[DMA_LIST_SIZE];
extern aud_dma_list_t dma_recorded_list[DMA_LIST_SIZE];


/*
 * audio_4231_dma_ops routines for the 8237A DMA engine
 */
/*ARGSUSED*/
void
audio_4231_8237_reset(cs_unit_t *unitp, ddi_acc_handle_t handle)
{
	struct aud_4231_chip *chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_CINT, ("8237_reset: unitp 0x%x\n",
	    unitp));
	/*
	 * disable audio chip playback/record
	 */
	chip = unitp->chip;

	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(handle, CS4231_IDR, PEN_DISABLE & CEN_DISABLE);

	/*
	 * stop the DMA subsystem
	 */
	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_CINT, ("8237_reset: "
	    "stopping DMA subsystem.\n"));
	(void) ddi_dmae_stop(unitp->dip, PB_CHNL); /* always DDI_SUCCESS */
	(void) ddi_dmae_stop(unitp->dip, RC_CHNL);
}

/*
 * audio_4231_8237_mapregs():
 *	Map in the CS4231 device registers, acquire the DMA channels
 *	from the system DMA engine, and allocate I/O buffers.
 *
 * RETURNS:
 *	DDI_SUCCESS/DDI_FAILURE
 */
int
audio_4231_8237_mapregs(dev_info_t *dip, cs_unit_t *unitp)
{
	uint_t	dmachan[2];
	int len;
	offset_t sz;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_ATTA, ("8237_mapregs: dip 0x%x\n", dip));

	/*
	 * the 8237 system DMA engine requires physical addresses be
	 * provided in the DMA cookie - this is a key difference from
	 * any of the SPARC controllers, which can use virtual addresses
	 * for DMA transfers.
	 */
	reg_acc_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	reg_acc_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	reg_acc_attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;

	len = sizeof (dmachan);
	if (GET_INT_PROP(dip, "dma-channels", dmachan, &len) !=
	    DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: bad dma-channels property\n");
		return (DDI_FAILURE);
	}
	PB_CHNL = dmachan[PLAYBACK];
	RC_CHNL = dmachan[RECORD];

	/*
	 * map in CS4231 device register set
	 */
#if defined(AUDIOCS_DEBUG)
	DPRINTF(("r3: 0x%x	&unitp->dip\n", dip));
	DPRINTF(("r5: 0x%x	&unitp->chip\n", &unitp->chip));
	DPRINTF(("r6: 0x%x	sizeof (struct pioregs)\n", sz));
	DPRINTF(("r7: 0x%x	&reg_acc_attr\n", &reg_acc_attr));
	DPRINTF(("r9: 0x%x	&CS4231_HANDLE:\n", &CS4231_HANDLE));
#endif	/* defined(AUDIOCS_DEBUG) */

	sz = (offset_t)sizeof (struct aud_4231_pioregs);
	if (ddi_regs_map_setup(dip, (uint_t)0, (caddr_t *)&unitp->chip,
	    (offset_t)0, sz, &reg_acc_attr, &CS4231_HANDLE) != DDI_SUCCESS) {
		/* Deallocate structures allocated above */
		kmem_free(unitp->allocated_memory, unitp->allocated_size);
		return (DDI_FAILURE);
	}
	handle = CS4231_HANDLE;
	ASSERT(handle != NULL);
	chip = unitp->chip;
	ASSERT(chip != NULL);

	/*
	 * Initialize with playback/record modes disabled.
	 */
	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(handle, CS4231_IDR, PEN_DISABLE & CEN_DISABLE);

	/*
	 * XCTL0/XCTL1 are general purpose external control lines whose
	 * usage is hardware implementation specific.  Therefore, the exact
	 * functionality of these lines must be verified for each platform.
	 *
	 * Sandalfoot-specific initialization
	 * (Sandalfoot Technical Specification, vsn 5.0, section 9.8,
	 *  Audio System Setup)
	 *
	 * For Sandalfoot, XCTL0 must be set to 1 to enable the internal
	 * speaker, and the mono microphone input must be muted before
	 * enabling the speaker.
	 */
	REG_SELECT(MONO_IOCR);	/* mute internal speaker */
	OR_SET_BYTE_R(handle, CS4231_IDR, MONO_INPUT_MUTE);
	REG_SELECT(PIN_CR);	/* enable internal speaker */

	switch (unitp->intl_spkr_mute) { /* how do we mute the speaker? */

	case XCTL0_OFF:	/* XCTL0 == 0 mutes internal speaker */
		OR_SET_BYTE_R(handle, CS4231_IDR, XCTL0);
		break;

	case REG_83E:
		AND_SET_BYTE_R(handle, CS4231_IDR, ~REG_83E_MASK);
		break;

	case XCTL0_ON:	/* XCTL0 == 1 mutes internal speaker */
	default:
		AND_SET_BYTE_R(handle, CS4231_IDR, ~XCTL0);
		break;
	}	/* switch */

#if defined(AUDIOCS_DEBUG)
	DPRINTF(("audiocs: playback DMA channel: 0x%x\n", PB_CHNL));
	DPRINTF(("audiocs: record DMA channel: 0x%x\n", RC_CHNL));
	DPRINTF(("sizeof i8237_dma: %d\n", sizeof (struct i8237_dma)));
	DPRINTF(("sizeof cs_stream: %d\n", sizeof (struct cs_stream)));
	DPRINTF(("sizeof cs_unit_t: %d\n", sizeof (cs_unit_t)));
	DPRINTF(("sizeof eb2_dmar: %d\n", sizeof (struct eb2_dmar)));
	DPRINTF(("sizeof aud_cmd_t: %d\n", sizeof (aud_cmd_t)));
	DPRINTF(("sizeof aud_cmdlist: %d\n", sizeof (aud_cmdlist_t)));
	DPRINTF(("sizeof audio_prinfo: %d\n", sizeof (audio_prinfo_t)));
	DPRINTF(("sizeof audio_info: %d\n", sizeof (audio_info_t)));
	DPRINTF(("sizeof aud_state_t: %d\n", sizeof (aud_state_t)));
	DPRINTF(("sizeof aud_stream_t: %d\n", sizeof (aud_stream_t)));
	DPRINTF(("sizeof aud_ops_t: %d\n", sizeof (aud_ops_t)));
	DPRINTF(("sizeof aud_4231_dma_ops: %d\n", sizeof (ops_t)));
	DPRINTF(("&unitp->physdma: 0x%x\n", &unitp->physdma));
	DPRINTF(("&CS4231_HANDLE: 0x%x\n", &CS4231_HANDLE));
#endif	/* defined(AUDIOCS_DEBUG) */

	if (ddi_dmae_alloc(dip, PB_CHNL, DDI_DMA_SLEEP, (caddr_t)0) !=
	    DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: cannot acquire PB DMA channel %d\n",
		    (int)PB_CHNL);
		kmem_free(unitp->allocated_memory, unitp->allocated_size);
		ddi_regs_map_free(&CS4231_HANDLE);
		return (DDI_FAILURE);
	}
	if (ddi_dmae_alloc(dip, RC_CHNL, DDI_DMA_SLEEP, (caddr_t)0) !=
	    DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: cannot acquire RC DMA channel %d\n",
		    (int)RC_CHNL);
		(void) ddi_dmae_release(dip, dmachan[PLAYBACK]);
		kmem_free(unitp->allocated_memory, unitp->allocated_size);
		ddi_regs_map_free(&CS4231_HANDLE);
		return (DDI_FAILURE);
	}

	/*
	 * allocate DMA I/O buffers - one each for playback, record
	 */
	if (audio_4231_8237_setup_dma_buffers(unitp) != DDI_SUCCESS) {
		/* always DDI_SUCCESS */
		(void) ddi_dmae_release(dip, dmachan[PLAYBACK]);
		(void) ddi_dmae_release(dip, dmachan[RECORD]);
		kmem_free(unitp->allocated_memory, unitp->allocated_size);
		ddi_regs_map_free(&CS4231_HANDLE);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

void
audio_4231_8237_unmapregs(cs_unit_t *unitp)
{
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_DETA, ("8237_unmapregs: unitp 0x%x\n",
	    unitp));

	/* free the DMA I/O buffers */
	ddi_dma_mem_free(&ACC_PB_HANDLE);
	ddi_dma_mem_free(&ACC_RC_HANDLE);
	ddi_dma_free_handle(&DMA_PB_HANDLE);
	ddi_dma_free_handle(&DMA_RC_HANDLE);

	(void) ddi_dmae_release(unitp->dip, RC_CHNL);	/* always DDI_SUCCESS */
	(void) ddi_dmae_release(unitp->dip, PB_CHNL);
	ddi_regs_map_free(&CS4231_HANDLE);
}

/*
 * audio_4231_8237_addintr():
 *
 * We only expect one hard interrupt address at level 5.
 * for the 8237 dma interface.
 *
 * RETURNS:
 *	DDI_SUCCESS/DDI_FAILURE
 */
/*ARGSUSED*/
int
audio_4231_8237_addintr(dev_info_t *dip, cs_unit_t *unitp)
{
	int rc;
	int len;
	uint_t intr_val[2];

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_ATTA, ("8237_addintr: dip 0x%x\n", dip));
	len = sizeof (intr_val);
	if (GET_INT_PROP(dip, "interrupts", intr_val, &len) !=
	    DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: missing interrupts property\n");
		return (DDI_PROBE_FAILURE);
	}

	rc = (ddi_add_intr(dip, 0, &audio_4231_trap_cookie,
	    (ddi_idevice_cookie_t *)0, audio_4231_8237_cintr, (caddr_t)0));

	return (rc);
}

/*ARGSUSED*/
void
audio_4231_8237_version(cs_unit_t *unitp, caddr_t verp)
{
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_IOCT, ("8237_version: unitp 0x%x\n",
	    unitp));

#if defined(__ppc)
	strcpy(verp, CS_DEV_VERSION_D);
#elif defined(i386)
	strcpy(verp, CS_DEV_VERSION_E);
#endif	/* defined(__ppc) */
}

void
audio_4231_8237_start_input(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	int bufsz;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_RCRD,
	    ("8237_start_input: unitp 0x%x\n", unitp));

	handle = CS4231_HANDLE;
	chip = unitp->chip;
	bufsz = unitp->input.as.info.buffer_size;
	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD, ("8237_start_input: RCbuflen "
	    "0x%x\tunitp->info->input_buffer_size 0x%x\n", RC_BUFLEN, bufsz));

	REG_SELECT(INTERFACE_CR);	/* disable chip record */
	AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, CEN_DISABLE);

	unitp->input.active = B_TRUE;
	DMA_RC_COOKIE.dmac_size = bufsz;
	unitp->physdma.samplecount[RECORD] =
	    audio_4231_sampleconv(&unitp->input, bufsz);
	AUD_DMA_REC_NEXT(unitp);
}

void
audio_4231_8237_start_output(cs_unit_t *unitp)
{
	int oldint;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;


	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY,
	    ("8237_start_output: unitp 0x%x\n", unitp));

	/*
	 * we start out with a freshly initialized aud_dma_list
	 */
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	REG_SELECT(INTERFACE_CR);	/* disable chip playback */
	AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, PEN_DISABLE);

	audio_4231_clear((aud_dma_list_t *)&dma_played_list, unitp);
	/*
	 * leave the parameters in the I/O buffer's DMA cookie unchanged.
	 * We control the amount of data transferred via the chip's
	 * count registers.
	 *
	 * loading the playback count registers with the number of samples,
	 * then enabling playback will initiate the DMA transfer.
	 */

	oldint = spl6();	/* disable interrupts */
	(void) audio_4231_playintr(unitp);
	splx(oldint);		/* re-enable interrupts */
}

void
audio_4231_8237_stop_input(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_RCRD, ("8237_stop_input: unitp 0x%x\n",
	    unitp));

	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, CEN_DISABLE);

	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_RCRD, ("audio_4231_8237_stop_input: "
	    "stopping DMA RC channel.\n"));
	(void) ddi_dmae_stop(unitp->dip, RC_CHNL);
}

void
audio_4231_8237_stop_output(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY, ("8237_stop_output: unitp 0x%x\n",
	    unitp));

	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, PEN_DISABLE);
	audio_4231_clear((aud_dma_list_t *)&dma_played_list, unitp);
	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_PLAY, ("audio_4231_8237_stop_output: "
	    "stopping DMA PB channel.\n"));
	(void) ddi_dmae_stop(unitp->dip, PB_CHNL);
}

void
audio_4231_8237_get_count(cs_unit_t *unitp, uint32_t *pcount, uint32_t *ccount)
{

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_GCNT, ("8237_get_count: unitp 0x%x\n",
	    unitp));
	if (pcount != NULL)
		(void) ddi_dmae_getcnt(unitp->dip, PB_CHNL, (int *)pcount);
	if (ccount != NULL)
		(void) ddi_dmae_getcnt(unitp->dip, RC_CHNL, (int *)ccount);
}

void
audio_4231_8237_get_ncount(cs_unit_t *unitp, int direction, uint32_t *ncount)
{
	switch (direction) {
	case PLAYBACK:
	default:
		*ncount = 0;
		break;

	case RECORD:
		*ncount = unitp->input.as.info.buffer_size;
		break;

	}	/* direction */
}

/*ARGSUSED*/
ddi_acc_handle_t
audio_4231_8237_get_acchandle(cs_unit_t *unitp, uint32_t *eb2intr, int direction)
{
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_GCNT, ("8237_get_count: unitp 0x%x\n",
	    unitp));
	return (CS4231_HANDLE);
}

uint_t
audio_4231_8237_dma_setup(cs_unit_t *unitp, uint_t direction, aud_cmd_t *cmdp,
    size_t length)
{
	uint_t ccount;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_INTR, ("8237_dma_setup: datap 0x%x "
	    "vaddr 0x%x length 0x%x\n", (u_int)cmdp->data, PB_BUFADDR, length));
	ATRACE(audio_4231_8237_dma_setup, 'DMAs', direction);

	switch (direction) {
	case PLAYBACK:
	default:
		DMA_PB_COOKIE.dmac_size = length;
		ATRACE(audio_4231_8237_dma_setup, 'PBln', length);
		bcopy((caddr_t)cmdp->data, (caddr_t)PB_BUFADDR, (size_t)length);
		/*CONSTANTCONDITION*/
		if (DMA_FIFO_PAD > 0) {	/* copy extra x chars to buffer, */
			caddr_t fp;	/* in case FIFO asks for more data */
			uchar_t padchar;
			int x;

			fp = (caddr_t)(PB_BUFADDR + length);
			x = DMA_FIFO_PAD;
			padchar = *(fp - 1);
			while (x != 0) {
				*fp++ = padchar;
				x--;
			}
		}

		if (ddi_dma_sync(DMA_PB_HANDLE, 0, 0,
		    DDI_DMA_SYNC_FORDEV) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "unable to flush cache for PB.\n");
		}

		unitp->typ_playlength = length;
		ccount = audio_4231_sampleconv(&unitp->output, length);
		unitp->physdma.samplecount[PLAYBACK] = ccount;
		ATRACE(audio_4231_8237_dma_setup, 'PBSn', ccount);
		break;

	case RECORD:
		DMA_RC_COOKIE.dmac_size = length;
		ATRACE(audio_4231_8237_dma_setup, 'RCln', length);
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_INTR, ("8237_dma_setup: "
		    "record length (bcopy'd bytes) 0x%x\n", length));
		bcopy((caddr_t)RC_BUFADDR, (caddr_t)cmdp->data, (size_t)length);
		if (ddi_dma_sync(DMA_RC_HANDLE, 0, 0,
		    DDI_DMA_SYNC_FORCPU) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "unable to flush cache for RC.\n");
		}

		unitp->typ_reclength = length;
		ccount = audio_4231_sampleconv(&unitp->input, length);
		unitp->physdma.samplecount[RECORD] = ccount;
		ATRACE(audio_4231_8237_dma_setup, 'RCSn', ccount);
		break;

	}
	return (B_TRUE);
}

/*ARGSUSED*/
uint32_t
audio_4231_8237_play_last(cs_unit_t *unitp, ddi_acc_handle_t handle,
    uint32_t eb2intr)
{
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY, ("8237_play_last: unitp 0x%x\n",
	    unitp));

	audio_4231_8237_stop_output(unitp);
	return ((uint32_t)0);
}

/*ARGSUSED*/
uint32_t
audio_4231_8237_play_cleanup(cs_unit_t *unitp, ddi_acc_handle_t handle,
    uint32_t eb2intr)
{
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY,
	    ("8237_play_cleanup: unitp 0x%x\n", unitp));

	return ((uint32_t)0);
}

/*ARGSUSED*/
void
audio_4231_8237_play_next(cs_unit_t *unitp, ddi_acc_handle_t handle)
{
	ddi_dma_cookie_t *cookiep = &unitp->play_dma_cookie;
	int nsamples;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	ATRACE(audio_4231_8237_play_next, 'Pcnt', cookiep->dmac_size);
	unitp->playlastaddr = cookiep->dmac_address;
	ATRACE(audio_4231_8237_play_next, 'PPA ', cookiep->dmac_address);

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY,
	    ("8237_play_next: cookiep 0x%x\n", cookiep));

	nsamples = unitp->physdma.samplecount[PLAYBACK] - 1;
	REG_SELECT(PLAYB_LBR);
	ddi_putb(CS4231_HANDLE, CS4231_IDR, (uchar_t)(nsamples));
	ATRACE(audio_4231_8237_play_next, 'Plbr', nsamples);

	nsamples >>= 8;
	REG_SELECT(PLAYB_UBR);
	ddi_putb(CS4231_HANDLE, CS4231_IDR, (uchar_t)nsamples);
	ATRACE(audio_4231_8237_play_next, 'Pubr', nsamples);

	audio_4231_8237_program_dma_engine(unitp, PLAYBACK);

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_PLAY, ("4231_8237_play_next: turn"
	    "it loose!\nistatus: 0x%x\n", ddi_getb(handle, CS4231_STATUS)));
	AUD_ERRDUMPREGS(AUD_EP_L1, AUD_EM_PLAY);

	ATRACE(audio_4231_8237_play_next, 'Pidr', tmpval);
	REG_SELECT(INTERFACE_CR);
	OR_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, PEN_ENABLE);
}

/*ARGSUSED*/
void
audio_4231_8237_rec_next(cs_unit_t *unitp)
{
	int nsamples;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	ddi_dma_cookie_t *cookiep;

	handle = CS4231_HANDLE;
	chip = unitp->chip;
	cookiep = &DMA_RC_COOKIE;

	ATRACE(audio_4231_8237_rec_next, 'Rcnt', cookiep->dmac_size);
	ATRACE(audio_4231_8237_rec_next, 'RPA ', cookiep->dmac_address);

	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("8237_rec_next: cookiep 0x%x "
	    "paddr 0x%x size 0x%x\n", cookiep, cookiep->dmac_address,
	    cookiep->dmac_size));

	nsamples = unitp->physdma.samplecount[RECORD];
	REG_SELECT(CAPTURE_LBR);
	ddi_putb(CS4231_HANDLE, CS4231_IDR, (uchar_t)(nsamples - 1));
	ATRACE(audio_4231_8237_rec_next, 'Rlbr', (uchar_t)nsamples);
	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("8237_rec_next: #samples 0x%x\n",
	    nsamples));

	nsamples >>= 8;
	REG_SELECT(CAPTURE_UBR);
	ddi_putb(CS4231_HANDLE, CS4231_IDR, (uchar_t)nsamples);
	ATRACE(audio_4231_8237_rec_next, 'Rubr', nsamples);
	unitp->typ_reclength = cookiep->dmac_size;

	audio_4231_8237_program_dma_engine(unitp, RECORD);

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD, ("4231_8237_rec_next: ready "
	    "to record - about to enable CS4231:CEN\n"));
	AUD_ERRDUMPREGS(AUD_EP_L1, AUD_EM_RCRD);

	REG_SELECT(INTERFACE_CR);
	OR_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, CEN_ENABLE);
}

/*ARGSUSED*/
void
audio_4231_8237_rec_cleanup(cs_unit_t *unitp)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD,
	    ("8237_rec_cleanup: disabling chip record.\n"));
	REG_SELECT(INTERFACE_CR);
	AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, CEN_DISABLE);

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD,
	    ("8237_rec_cleanup: stopping DMA engine.\n"));
	(void) ddi_dmae_stop(unitp->dip, RC_CHNL);

	/*
	 * wait for fifo in cheerio to drain
	 */
	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD, ("8237_rec_cleanup: input.error "
	    "SET\n"));
	unitp->input.error = B_TRUE;
	unitp->input.active = B_FALSE;
	unitp->input.cmdptr = NULL;
}

/*
 * common interrupt routine, vectors to playintr/recintr.
 */
static uint_t
audio_4231_8237_cintr()
{
	cs_unit_t *unitp;
	int count;
	int rc = DDI_INTR_UNCLAIMED;
	/*LINTED*/
	uchar_t status;
	uchar_t estatus;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY | AUD_EM_RCRD | AUD_EM_INTR,
	    ("8237_cintr: dropping into cintr\n"));
	AUD_ERRDUMPREGS(AUD_EP_L2, AUD_EM_PLAY | AUD_EM_RCRD | AUD_EM_INTR);

	/* acquire spin lock */
	/* XXX hack - current driver only supports one device */
	unitp = &cs_units[0];
	LOCK_UNITP(unitp);
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	/*
	 * Save the current state of the playback/record interrupt bits.
	 * These bits are volatile; i.e., they are reset either by
	 * overwriting a particular bit in this register, or by *any*
	 * write to the status register (r2).
	 */
	while ((status = ddi_getb(handle, CS4231_STATUS)) & INT_ACTIVE) {
#if defined(DEBUG)
		if (aud_errmask & (AUD_EM_PLAY | AUD_EM_RCRD | AUD_EM_INTR) &&
		    aud_errlevel <= AUD_EP_L2) {
			audio_4231_int_status();
		}
#endif	/* defined(DEBUG) */
		ATRACE(audio_4231_8237_cintr, 'Stat', status);
#if 0					/* vla fornow..... */
		/*
		 * This register also contains status, but the information in
		 * this particular register is less detailed than that which
		 * can be read from the Alternate Feature Status register.
		 * It is only important for supporting devices under Mode 1.
		 */
		REG_SELECT(TEST_IR);
		status = ddi_getb(handle, CS4231_IDR);
#endif					/* vla fornow..... */

		REG_SELECT(ALT_FEAT_STATR);
		estatus = ddi_getb(handle, CS4231_IDR);

		ddi_putb(handle, CS4231_STATUS, 0);	/* clear INT status */

		rc = DDI_INTR_CLAIMED;

		if (estatus & CS_CI) {	/* record interrupt pending */
			REG_SELECT(INTERFACE_CR);
			AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, CEN_DISABLE);

			/* capture underrun */
			if ((estatus & CS_CU) && (unitp->input.active !=
			    B_TRUE)) {
				AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD,
				    ("8237_cintr: underrun or not active\n"));
				ATRACE(audio_4231_8237_cintr, 'NoRc', status);
				unitp->input.active = B_FALSE;
			} else {
				ATRACE(audio_4231_8237_cintr, 'Recd', status);
				unitp->input.samples +=
				    audio_4231_sampleconv(&unitp->input,
				    unitp->typ_reclength);
				AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD,
				    ("8237_cintr: input.samples 0x%x\n",
				    unitp->input.samples));
				audio_4231_recintr(unitp);
			}
		}

		if (estatus & CS_PI) {	/* playback interrupt pending */
			REG_SELECT(INTERFACE_CR);
			AND_SET_BYTE_R(CS4231_HANDLE, CS4231_IDR, PEN_DISABLE);

			if (unitp->output.active == B_TRUE) {
				ATRACE(audio_4231_8237_cintr, 'Play', status);
				count = audio_4231_sampleconv(&unitp->output,
				    unitp->typ_playlength);
				unitp->output.samples += count;
				ATRACE(audio_4231_8237_cintr, 'PBsn', count);
				AUD_ERRPRINT(AUD_EP_L1, AUD_EM_INTR,
				    ("4231_samplecalc: PB samples: 0x%x\n",
				    count));
				(void) audio_4231_playintr(unitp);
				audio_process_output(&unitp->output.as);

			} else if (estatus & CS_PU) { /* playback underrun */
				ATRACE(audio_4231_8237_cintr, 'NoPl', estatus);
				if (unitp->output.as.openflag) {
					audio_4231_samplecalc(unitp,
					    unitp->typ_playlength, PLAYBACK);
				}
				audio_4231_clear((aud_dma_list_t *)
				    &dma_played_list, unitp);
				unitp->output.active = B_FALSE;
				unitp->output.cmdptr = NULL;
				audio_process_output(&unitp->output.as);

			} else {	/* unexpected playback interrupt */
				ATRACE(audio_4231_8237_cintr, 'unex', estatus);
				AUD_ERRPRINT(AUD_EP_L3,
				    AUD_EM_INTR | AUD_EM_PLAY,
				    ("unexpected PB intr\n", estatus));
				cmn_err(CE_WARN,
				    "unexpected audio PB interrupt.\n");
				break;
			}
		}

		if (estatus & CS_TI) {	/* timer interrupt pending */
			cmn_err(CE_WARN, "audio_4231_8237_cintr: timer "
			    "interrupts not supported!\n");
		}
	}

	AUD_ERRPRINT(AUD_EP_L2, (AUD_EM_PLAY | AUD_EM_RCRD | AUD_EM_INTR),
	    ("8237_cintr: ISR routine completed.\n"));

	ATRACE(audio_4231_8237_cintr, ' RET', unitp);
	UNLOCK_UNITP(unitp);
	return (rc);
}

/*
 * Other routines specific to this DMA engine
 */

/*
 * Allocate DMA-able buffers for playback and record I/O
 * RETURNS:	DDI_SUCCESS/DDI_FAILURE
 */
static int
audio_4231_8237_setup_dma_buffers(cs_unit_t *unitp)
{
	int rc, rc2;
	uint io_buflen;
	uint_t ccount, ccount2;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_ATTA,
	    ("8237_setup_dma_buffers: unitp 0x%x\n", unitp));
	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_ATTA, ("r4: 0x%x	unitp->dma_attrp\n",
	    unitp->dma_attrp));
	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_ATTA, ("r7: 0x%x	&DMA_PB_HANDLE:\n",
	    &DMA_PB_HANDLE));
	rc = ddi_dma_alloc_handle(unitp->dip, unitp->dma_attrp, DDI_DMA_SLEEP,
	    (caddr_t)0, &DMA_PB_HANDLE);

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_ATTA, ("r7: 0x%x	&DMA_RC_HANDLE:\n",
	    &DMA_RC_HANDLE));
	rc2 = ddi_dma_alloc_handle(unitp->dip, unitp->dma_attrp, DDI_DMA_SLEEP,
	    (caddr_t)0, &DMA_RC_HANDLE);

	switch (rc | rc2) {
	case DDI_SUCCESS:
		break;
	case DDI_DMA_BADATTR:
		cmn_err(CE_WARN, "audiocs: Bad attr structure!");
		ddi_dma_free_handle(&DMA_PB_HANDLE);
		return (DDI_FAILURE);
	case DDI_DMA_NORESOURCES:
		cmn_err(CE_WARN, "audiocs: No resources available.");
		ddi_dma_free_handle(&DMA_PB_HANDLE);
		return (DDI_FAILURE);
	default:
		cmn_err(CE_WARN, "audiocs: Unknown alloc error 0x%x.",
		    rc | rc2);
		ddi_dma_free_handle(&DMA_PB_HANDLE);
		return (DDI_FAILURE);
	}

	/*
	 * allocate two 32K I/O buffers - one each for playback and record.
	 */

	if (ddi_dma_mem_alloc(DMA_PB_HANDLE, BUFLEN, &dma_acc_attr,
	    DDI_DMA_STREAMING | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, (caddr_t)0,
	    (caddr_t *)&PB_BUFADDR, &io_buflen, &ACC_PB_HANDLE) !=
	    DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: Unable to alloc PB I/O buffer.");
		ddi_dma_free_handle(&DMA_PB_HANDLE);
		ddi_dma_free_handle(&DMA_RC_HANDLE);
		return (DDI_FAILURE);
	}
	PB_BUFLEN = io_buflen / 2;
	unitp->physdma.PBbuffer[1] = (uchar_t *)(PB_BUFADDR + PB_BUFLEN);

	if (ddi_dma_mem_alloc(DMA_RC_HANDLE, BUFLEN, &dma_acc_attr,
	    DDI_DMA_STREAMING | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, (caddr_t)0,
	    (caddr_t *)&RC_BUFADDR, &io_buflen, &ACC_RC_HANDLE) !=
	    DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: Unable to alloc REC I/O buffer.");
		ddi_dma_mem_free(&ACC_PB_HANDLE);
		ddi_dma_free_handle(&DMA_PB_HANDLE);
		ddi_dma_free_handle(&DMA_RC_HANDLE);
		return (DDI_FAILURE);
	}
	RC_BUFLEN = io_buflen / 2;
	unitp->physdma.RCbuffer[1] = (uchar_t *)(RC_BUFADDR + RC_BUFLEN);

	rc = ddi_dma_addr_bind_handle(DMA_PB_HANDLE, (struct as *)0,
	    (caddr_t)PB_BUFADDR, PB_BUFLEN,
	    DDI_DMA_WRITE | DDI_DMA_STREAMING | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, (caddr_t)0, &DMA_PB_COOKIE, &ccount);

	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_PLAY, ("virtual addr of DMA_PB_COOKIE: "
	    "0x%x\n", &DMA_PB_COOKIE));
	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_PLAY, ("cookie contents: 0x%x/0x%x\n",
	    DMA_PB_COOKIE.dmac_address, DMA_PB_COOKIE.dmac_size));

	rc2 = ddi_dma_addr_bind_handle(DMA_RC_HANDLE, (struct as *)0,
	    (caddr_t)RC_BUFADDR, RC_BUFLEN,
	    DDI_DMA_READ | DDI_DMA_STREAMING | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, (caddr_t)0, &DMA_RC_COOKIE, &ccount2);

	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_RCRD, ("virtual addr of DMA_RC_COOKIE: "
	    "0x%x\n", &DMA_RC_COOKIE));
	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_RCRD, ("cookie contents: 0x%x/0x%x\n",
	    DMA_RC_COOKIE.dmac_address, DMA_RC_COOKIE.dmac_size));
	DMA_RC_COOKIE.dmac_size = 0;

	/*
	 * Initialize the DMA engine to reasonable values, because the
	 * get/setinfo ioctl may check the transfer count before any transfer
	 * has been set up.
	 */
	audio_4231_8237_program_dma_engine(unitp, PLAYBACK);
	audio_4231_8237_program_dma_engine(unitp, RECORD);

	if (ccount != 1 || ccount2 != 1)
		cmn_err(CE_WARN, "audiocs: too many DMA cookies! (%d,%d)",
		    ccount, ccount2);

	rc |= rc2;
	audio_4231_dma_errprt(rc);
	if (rc == DDI_DMA_MAPPED || rc == DDI_DMA_PARTIAL_MAP)
		return (DDI_SUCCESS);

	ddi_dma_mem_free(&ACC_PB_HANDLE);
	ddi_dma_mem_free(&ACC_RC_HANDLE);
	ddi_dma_free_handle(&DMA_PB_HANDLE);
	ddi_dma_free_handle(&DMA_RC_HANDLE);
	return (DDI_FAILURE);
}

static void
audio_4231_8237_program_dma_engine(cs_unit_t *unitp, uint_t direction)
{
	int x;
	struct ddi_dmae_req	dmae_req;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_STRT,
	    ("8237_program_dma_engine: unitp 0x%x\n", unitp));
	bzero((caddr_t)&dmae_req, sizeof (dmae_req));
	dmae_req.der_command =
	    (direction == PLAYBACK) ? DMAE_CMD_WRITE : DMAE_CMD_READ;
	/*
	 * The SIO (82378ZB) chip contains the functionality of two 82C37
	 * DMA controllers...  with some extensions.
	 *
	 * These extra two parameters do not apply to systems running
	 * the older part.
	 */
	dmae_req.der_path = DMAE_PATH_8;  	/* force to 8-bit mode */
/*	dmae_req.der_cycles = DMAE_CYCLES_2;	/+ Type "A" timing */

/* XXXPPC test for now, in case FIFO asks for more data */
	DMA_PB_COOKIE.dmac_size += DMA_FIFO_PAD; /* if FIFO wants more data */
	DMA_RC_COOKIE.dmac_size += DMA_FIFO_PAD;
	ATRACE(audio_4231_8237_program_dma_engine, 'dmrq', &dmae_req);
	if (ddi_dmae_prog(unitp->dip, &dmae_req,
	    (direction == PLAYBACK) ? &DMA_PB_COOKIE : &DMA_RC_COOKIE,
	    (direction == PLAYBACK) ? PB_CHNL : RC_CHNL) != DDI_SUCCESS) {
		AUD_ERRPRINT(AUD_EP_L2, AUD_EM_STRT, ("DMA channel not "
		    "programmed!\n"));
	}
	(void) ddi_dmae_getcnt(unitp->dip,
	    (direction == PLAYBACK) ? PB_CHNL : RC_CHNL, &x);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_STRT,
	    ("DMA start count: 0x%x direction %d\n", (uint)x, direction));
}


ops_t audio_4231_8237dma_ops = {
	"Intel i8237A DMA controller",
	audio_4231_8237_reset,
	audio_4231_8237_mapregs,
	audio_4231_8237_unmapregs,
	audio_4231_8237_addintr,
	audio_4231_8237_version,
	audio_4231_8237_start_input,
	audio_4231_8237_start_output,
	audio_4231_8237_stop_input,
	audio_4231_8237_stop_output,
	audio_4231_8237_get_count,
	audio_4231_8237_get_ncount,
	audio_4231_8237_get_acchandle,
	audio_4231_8237_dma_setup,
	audio_4231_8237_play_last,
	audio_4231_8237_play_cleanup,
	audio_4231_8237_play_next,
	audio_4231_8237_rec_next,
	audio_4231_8237_rec_cleanup,
};
