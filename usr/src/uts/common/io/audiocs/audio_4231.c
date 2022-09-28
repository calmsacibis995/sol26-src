/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audio_4231.c	1.34	96/10/15 SMI"

/*
 * AUDIO Chip driver - for CS 4231
 *
 * The basic facts:
 * 	- The digital representation is 8-bit u-law by default.
 *	  The high order bit is a sign bit, the low order seven bits
 *	  encode amplitude, and the entire 8 bits are inverted.
 */

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/file.h>
#include <sys/debug.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/audioio.h>
#include <sys/audiovar.h>
#include <sys/audiodebug.h>
#include <sys/audio_4231.h>
#include <sys/audio_4231_debug.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/promif.h>

/*
 * Local routines
 */
static int audio_4231_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int audio_4231_attach(dev_info_t *, ddi_attach_cmd_t);
static int audio_4231_detach(dev_info_t *, ddi_detach_cmd_t);
static int audio_4231_identify(dev_info_t *);
static int audio_4231_probe(dev_info_t *);
static int audio_4231_open(queue_t *, dev_t *, int, int, cred_t *);
static void audio_4231_close(aud_stream_t *);
static aud_return_t audio_4231_ioctl(aud_stream_t *, queue_t *, mblk_t *);
static aud_return_t audio_4231_mproto(aud_stream_t *, mblk_t *);
static void audio_4231_start(aud_stream_t *);
static void audio_4231_stop(aud_stream_t *);
static uint_t audio_4231_setflag(aud_stream_t *, enum aud_opflag, uint_t);
static aud_return_t audio_4231_setinfo(aud_stream_t *, mblk_t *, int *);
static void audio_4231_queuecmd(aud_stream_t *, aud_cmd_t *);
static void audio_4231_flushcmd(aud_stream_t *);
static void audio_4231_chipinit(cs_unit_t *);
static uint_t audio_4231_outport(cs_unit_t *, uint_t);
static uint_t audio_4231_output_muted(cs_unit_t *, uint_t);
static uint_t audio_4231_inport(cs_unit_t  *, uint_t);
static uint_t audio_4231_play_gain(cs_unit_t *, uint_t, uchar_t);
uint_t audio_4231_record_gain(cs_unit_t *, uint_t, uchar_t);
static uint_t audio_4231_monitor_gain(cs_unit_t *, uint_t);
uint_t audio_4231_playintr(cs_unit_t *);
void audio_4231_recintr(cs_unit_t *);
void audio_4231_pollready();
extern uint_t audio_4231_cintr();

static void audio_4231_initlist(aud_dma_list_t *, cs_unit_t *);
static void audio_4231_insert(aud_cmd_t *, ddi_dma_handle_t, uint_t,
					aud_dma_list_t *, cs_unit_t *);
static void audio_4231_remove(uint_t, aud_dma_list_t *);
void audio_4231_clear(aud_dma_list_t *, cs_unit_t *);
void audio_4231_samplecalc(cs_unit_t *, uint_t, uint_t);
uint_t audio_4231_sampleconv(cs_stream_t *, uint_t);
static void audio_4231_recordend(cs_unit_t *, aud_dma_list_t *);
static void audio_4231_initcmdp(aud_cmd_t *, uint_t);
void audio_4231_workaround(cs_unit_t *);
void audio_4231_config_queue(aud_stream_t *);
static void audio_4231_timeout();
static void audio_4231_mute_channel(cs_unit_t *, int);
static void audio_4231_unmute_channel(cs_unit_t *, int);
static void audio_4231_change_sample_rate(cs_unit_t *, uchar_t);
void audio_4231_dma_errprt(int);
#if defined(DEBUG)
void audio_4231_dumpregs(void);
void audio_4231_int_status(void);
extern void call_debug();
int audiocs_debug;
int audiocs_pio;
#endif	/* defined(DEBUG) */

/*
 * Local declarations
 */
cs_unit_t *cs_units;		/* device controller array */
static size_t cs_units_size;	/* size of allocated devices array */
ddi_iblock_cookie_t audio_4231_trap_cookie;
static uint_t CS4231_reva;

static uint_t audio_4231_acal = 0;

/*
 * Count of devices attached
 */
static uint_t devcount = 0;
/*
 * counter to keep track of the number of dma's that we have done
 * we must always be 2 behind in freeing up so that we don't free up
 * dma bufs in progress typ_playlength is saved in order to exact the
 * number of samples that we have played.
 * The calculation is as follows:
 * output.samples = (output.samples - nextcount) +
 *				 (typ_playcount - current count);
 */


/*
 * This is the size of the STREAMS buffers we send up the read side
 */
int audio_4231_bsize = AUD_CS4231_BSIZE;
int audio_4231_play_bsize = AUD_CS4231_MAXPACKET;
int audio_4231_play_hiwater = 0;
int audio_4231_play_lowater = 0;
int audio_4231_cmdpool = AUD_CS4231_CMDPOOL;
int audio_4231_recbufs = AUD_CS4231_RECBUFS;
int audio_4231_no_cd = 0;

/*
 * debugging hints:
 *	set aud_errlevel to 1
 *	set aud_errmask (see definitions in audio_4231_debug.h)
 */
int aud_errmask = 0;
int aud_errlevel = 0;
int playd_addr = 0;
int rec_addr = 0;

/*
 * XXX - This driver only supports one CS 4231 device
 */
#define	MAXUNITS	(1)

#define	AUDIO_ENCODING_DVI	(104)	/* DVI ADPCM PCM XXXXX */

#define	NEEDS_HW_INIT	0x494e4954	/* Val spells "init" */


/*
 * Declare audio ops vector for CS4231 support routines
 */
static struct aud_ops audio_4231_ops = {
	audio_4231_close,
	audio_4231_ioctl,
	audio_4231_mproto,
	audio_4231_start,
	audio_4231_stop,
	audio_4231_setflag,
	audio_4231_setinfo,
	audio_4231_queuecmd,
	audio_4231_flushcmd
};


/*
 * Streams declarations
 */

static struct module_info audio_4231_modinfo = {
	AUD_CS4231_IDNUM,	/* module ID number */
	AUD_CS4231_NAME,	/* module name */
	AUD_CS4231_MINPACKET,	/* min packet size accepted */
	AUD_CS4231_MAXPACKET,	/* max packet size accepted */
	AUD_CS4231_HIWATER,	/* hi-water mark */
	AUD_CS4231_LOWATER,	/* lo-water mark */
};

/*
 * Queue information structure for read queue
 */
static struct qinit audio_4231_rinit = {
	audio_rput,		/* put procedure */
	audio_rsrv,		/* service procedure */
	audio_4231_open,	/* called on startup */
	audio_close,		/* called on finish */
	NULL,			/* for 3bnet only */
	&audio_4231_modinfo,	/* module information structure */
	NULL,			/* module statistics structure */
};

/*
 * Queue information structure for write queue
 */
static struct qinit audio_4231_winit = {
	audio_wput,		/* put procedure */
	audio_wsrv,		/* service procedure */
	NULL,			/* called on startup */
	NULL,			/* called on finish */
	NULL,			/* for 3bnet only */
	&audio_4231_modinfo,	/* module information structure */
	NULL,			/* module statistics structure */
};

static struct streamtab audio_4231_str_info = {
	&audio_4231_rinit,	/* qinit for read side */
	&audio_4231_winit,	/* qinit for write side */
	NULL,			/* mux qinit for read */
	NULL,			/* mux qinit for write */
				/* list of modules to be pushed */
};

static 	struct cb_ops cb_audiocs_prop_op = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
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
	&audio_4231_str_info,	/* cb_stream */
	(int)(D_NEW | D_MP)	/* cb_flag */
};

/*
 * Declare ops vectors for auto configuration.
 */
struct dev_ops audiocs_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	audio_4231_getinfo,	/* devo_getinfo */
	audio_4231_identify,	/* devo_identify */
	audio_4231_probe,	/* devo_probe */
	audio_4231_attach,	/* devo_attach */
	audio_4231_detach,	/* devo_detach */
	nodev,			/* devo_reset */
	&(cb_audiocs_prop_op),	/* devo_cb_ops */
	(struct bus_ops *)NULL,	/* devo_bus_ops */
#ifdef APC_POWER
	ddi_power		/* devo_power */
#endif
};

aud_dma_list_t dma_played_list[DMA_LIST_SIZE];
aud_dma_list_t dma_recorded_list[DMA_LIST_SIZE];

/*
 * This driver requires that the Device-Independent Audio routines
 * be loaded in.
 */
char _depends_on[] = "misc/diaudio";


extern struct mod_ops mod_driverops;

static struct modldrv audio_4231_modldrv = {
	&mod_driverops,		/* Type of module */
	"CS4231 audio driver",	/* Descriptive name */
	&audiocs_ops		/* Address of dev_ops */
};

static struct modlinkage audio_4231_modlinkage = {
	MODREV_1,
	(void *)&audio_4231_modldrv,
	NULL
};


int
_init()
{
	return (mod_install(&audio_4231_modlinkage));
}


int
_fini()
{
	return (mod_remove(&audio_4231_modlinkage));
}


int
_info(struct modinfo *modinfop)
{
#if defined(AUDIOCS_DEBUG)
	debug_enter("\n\n\nAUDIOCS INFO\n\n");
#endif
	return (mod_info(&audio_4231_modlinkage, modinfop));
}


/*
 * Return the opaque device info pointer for a particular unit
 */
/*ARGSUSED*/
static int
audio_4231_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result)
{
	dev_t dev;
	int error;

	dev = (dev_t)arg;
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_GETI,
	    ("4231_getinfo: cmd=%x  dev=%d\n", infocmd, dev));

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (cs_units[CS_UNIT(dev)].dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *)cs_units[CS_UNIT(dev)].dip;
			error = DDI_SUCCESS;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		/* Instance num is unit num (with minor number flags masked) */
		*result = (void *)CS_UNIT(dev);
		error = DDI_SUCCESS;
		break;

	default:
		error = DDI_FAILURE;
		break;
	}
	return (error);
}


/*
 * Called from autoconf.c to locate device handlers
 */
static int
audio_4231_identify(dev_info_t *dip)
{
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_IDEN,
	    ("4231_identify: dip 0x%x\n", dip));

	if (strcmp(ddi_get_name(dip), "SUNW,CS4231") == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}


/*
 * probe for presence of audio device.
 */
static int
audio_4231_probe(dev_info_t *dip)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	ddi_device_acc_attr_t reg_acc_attr; /* access attribs for dev regs */

	if (ddi_dev_is_sid(dip) == DDI_SUCCESS)
		return (DDI_PROBE_DONTCARE);

	/*
	 * initialize device register access atttribute structure
	 */
	reg_acc_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	reg_acc_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	reg_acc_attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;

	/*
	 * map in CS4231 device register set
	 */
	if (ddi_regs_map_setup(dip, (uint_t)0, (caddr_t *)&chip,
	    (offset_t)0, (offset_t)0, &reg_acc_attr, &handle) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	/*
	 * Check for codec signature.
	 */
	REG_SELECT(MISC_IR);
	if ((ddi_getb(handle, &chip->pioregs.idr) & CODEC_ID_MASK) ==
	    CODEC_SIGNATURE) {
		ddi_regs_map_free(&handle);
		return (DDI_SUCCESS);
	}
	ddi_regs_map_free(&handle);
	return (DDI_FAILURE);
}


/*
 * Attach to the device.
 */
static int
/*ARGSUSED*/
audio_4231_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	aud_stream_t *as, *output_as, *input_as;
#if defined(i386) || defined(__ppc)
	struct aud_4231_chip *chip;
	ddi_acc_handle_t handle;
	char *CD_line;
#endif			/* defined(i386) || defined(ppc) */
	cs_unit_t *unitp;
	struct aud_cmd *pool;
	uint_t instance;
	char name[16];		/* XXX - A Constant! */
	int i, rc, len;
	int debug[2];
	char *spkr_mute;
#if defined(sparc) || defined(__sparc)
	char *tmp;
	int proplen;
#endif			/* defined(sparc) */

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_ATTA, ("4231_attach: dip 0x%x\n", dip));

	/*
	 * XXX - The fixed maximum number of units is bogus.
	 */
	if (devcount > MAXUNITS) {
		cmn_err(CE_WARN, "audiocs: multiple audio devices?");
		return (DDI_FAILURE);
	}

	ATRACEINIT();

	/* Get this instance number (becomes the low order unitnum) */
	instance = ddi_get_instance(dip);
	ASSERT(instance <= CS_UNITMASK);

	/*
	 * Each unit has a 'aud_state_t' that contains generic audio
	 * device state information.  Also, each unit has a 'cs_unit_t'
	 * that contains device-specific data.
	 * Allocate storage for them here.
	 */
	if (cs_units == NULL) {
		cs_units_size = MAXUNITS * sizeof (cs_unit_t);
		cs_units = kmem_zalloc(cs_units_size, KM_NOSLEEP);
		if (cs_units == NULL)
			return (DDI_FAILURE);
	}

	unitp = &cs_units[devcount];

	len = sizeof (debug);
	if (GET_INT_PROP(dip, "debug", debug, &len) == DDI_PROP_SUCCESS) {
		aud_errlevel = debug[0];
		aud_errmask = debug[1];
	}

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		LOCK_UNITP(unitp);
		if (!unitp->suspended) {
			UNLOCK_UNITP(unitp);
			return (DDI_SUCCESS);
		}
		/*
		 * XXX CPR call audio_4231_setinfo here with
		 * saved state.
		 */
		audio_4231_chipinit(unitp);
		unitp->suspended = B_FALSE;
		UNLOCK_UNITP(unitp);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	switch (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "pio?", -1)) {
	case 1:
		cmn_err(CE_WARN,
		    "audiocs: DMA driver on the P1.X Platform");
		return (DDI_FAILURE);
	case 0:
	default:
		break;
	}

	rc = ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
	    "intl_spkr_mute", (caddr_t)&spkr_mute, &len);

	if (rc == DDI_PROP_SUCCESS && strncmp(spkr_mute, "XCTL0_OFF", len) == 0)
		unitp->intl_spkr_mute = XCTL0_OFF;
	else if (rc == DDI_PROP_SUCCESS &&
	    strncmp(spkr_mute, "REG_83E", len) == 0)
		unitp->intl_spkr_mute = REG_83E;
	else
		unitp->intl_spkr_mute = XCTL0_ON;

#if defined(i386) || defined(__ppc)
	/*
	 * ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	 *    "device_type", -1);
	 */
	unitp->dma_engine = i8237_DMA;
	unitp->opsp = &audio_4231_8237dma_ops;
	unitp->dma_attrp = &aud_8237dma_attr;

#elif defined(sparc) || defined(__sparc)
	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_ALLOC,
	    DDI_PROP_DONTPASS, "dma-model", (caddr_t)&tmp, &proplen) ==
	    DDI_PROP_SUCCESS) {
		if (strcmp(tmp, "eb2dma") == 0) {
			unitp->eb2dma = B_TRUE;
			unitp->dma_engine = EB2_DMA;
			unitp->opsp = &audio_4231_eb2dma_ops;
			unitp->dma_attrp = &aud_eb2dma_attr;
		} else {
			unitp->eb2dma = B_FALSE; /* APC DMA */
			unitp->dma_engine = APC_DMA;
			unitp->opsp = &audio_4231_apcdma_ops;
			unitp->dma_attrp = &aud_apcdma_attr;
		}
	} else {
		unitp->eb2dma = B_FALSE; /* APC DMA */
		unitp->dma_engine = APC_DMA;
		unitp->opsp = &audio_4231_apcdma_ops;
		unitp->dma_attrp = &aud_apcdma_attr;
	}

	if (ddi_prop_op(DDI_DEV_T_ANY, dip, PROP_LEN_AND_VAL_ALLOC,
	    DDI_PROP_DONTPASS, "audio-module", (caddr_t)&tmp, &proplen) ==
	    DDI_PROP_SUCCESS) {
		unitp->module_type = B_TRUE;
	} else {
		unitp->module_type = B_FALSE;
	}

#ifdef MULTI_DEBUG
	unitp->eb2dma = B_TRUE; /* XXX FIX ME dma2 for now */
#endif

#else
#error One of sparc, i386 or ppc must be defined.
#endif

#ifdef MULTI_DEBUG
	unitp->eb2dma = B_TRUE; /* XXX FIX ME dma2 for now */
#endif

	switch (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "internal-loopback", B_FALSE)) {
	case 1:
		unitp->cd_input_line = NO_INTERNAL_CD;
		break;
	case 0:
		unitp->cd_input_line = INTERNAL_CD_ON_AUX1;
		break;
	default:
		break;
	}

#if defined(sparc) || defined(__sparc)
#ifdef MULTI_DEBUG
	unitp->cd_input_line = NO_INTERNAL_CD;
#endif
	if (audio_4231_no_cd) {
		unitp->cd_input_line = INTERNAL_CD_ON_AUX1;
	}
#elif defined(i386) || defined(__ppc)

	rc = ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
	    "intl_CDROM", (caddr_t)&CD_line, &len);

	if (rc == DDI_PROP_SUCCESS && strncmp(CD_line, "AUX1", len) == 0)
		unitp->cd_input_line = INTERNAL_CD_ON_AUX1;
	else if (rc == DDI_PROP_SUCCESS &&
	    strncmp(CD_line, "AUX2", len) == 0)
		unitp->cd_input_line = INTERNAL_CD_ON_AUX2;
	else
		unitp->cd_input_line = NO_INTERNAL_CD;

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_ATTA, ("4231_attach: cd_input_line "
	    "%d\n", unitp->cd_input_line));
#else
#error One of sparc, i386 or ppc must be defined.
#endif
	/*
	 * Allocate command list buffers, initialized below
	 */
	unitp->allocated_size = audio_4231_cmdpool * sizeof (aud_cmd_t);
	unitp->allocated_memory = kmem_zalloc(unitp->allocated_size,
	    KM_NOSLEEP);
	if (unitp->allocated_memory == NULL)
		return (DDI_FAILURE);

	/*
	 * Map in the CS4231 (and DMA) registers
	 */
	unitp->dip = dip;
	unitp->distate.ddstate = (caddr_t)unitp;

	rc = AUD_DMA_MAP_REGS(dip, unitp);
	if (rc != DDI_SUCCESS)
		return (DDI_FAILURE);

#if defined(i386) || defined(__ppc)
	handle = CS4231_HANDLE;
	chip = unitp->chip;
	/*
	 * In both the PPC and x86 versions, interrupts are generated by the
	 * CODEC; on SPARC systems, the DMA controller issues interrupts.
	 */
	REG_SELECT(PIN_CR);
	AND_SET_BYTE_R(handle, CS4231_IDR, ~INTR_ON);
	ddi_putb(handle, CS4231_STATUS, RESET_STATUS);
#endif

	unitp->distate.monitor_gain = 0;
	unitp->distate.output_muted = B_FALSE;
	unitp->distate.ops = &audio_4231_ops;
	unitp->playcount = 0;
	unitp->recordcount = 0;
	unitp->typ_playlength = 0;
	unitp->typ_reclength = 0;
	unitp->recordlastent = 0;

	/*
	 * Set up pointers between audio streams
	 */
	unitp->control.as.control_as = &unitp->control.as;
	unitp->control.as.output_as = &unitp->output.as;
	unitp->control.as.input_as = &unitp->input.as;
	unitp->output.as.control_as = &unitp->control.as;
	unitp->output.as.output_as = &unitp->output.as;
	unitp->output.as.input_as = &unitp->input.as;
	unitp->input.as.control_as = &unitp->control.as;
	unitp->input.as.output_as = &unitp->output.as;
	unitp->input.as.input_as = &unitp->input.as;

	as = &unitp->control.as;
	output_as = as->output_as;
	input_as = as->input_as;

	ASSERT(as != NULL);
	ASSERT(output_as != NULL);
	ASSERT(input_as != NULL);

	/*
	 * Initialize the play stream
	 */
	output_as->distate = &unitp->distate;
	output_as->type = AUDTYPE_DATA;
	output_as->mode = AUDMODE_AUDIO;
	output_as->signals_okay = B_FALSE;
	output_as->info.gain = AUD_CS4231_DEFAULT_PLAYGAIN;
	output_as->info.sample_rate = AUD_CS4231_SAMPLERATE;
	output_as->info.channels = AUD_CS4231_CHANNELS;
	output_as->info.precision = AUD_CS4231_PRECISION;
	output_as->info.encoding = AUDIO_ENCODING_ULAW;
	output_as->info.minordev = CS_MINOR_RW;
	output_as->info.balance = AUDIO_MID_BALANCE;
	output_as->info.buffer_size = audio_4231_play_bsize;

	/*
	 * Set the default output port according to capabilities
	 */
	output_as->info.avail_ports =
	    AUDIO_SPEAKER | AUDIO_HEADPHONE | AUDIO_LINE_OUT;
	output_as->info.port = AUDIO_SPEAKER;
	output_as->traceq = NULL;
	output_as->maxfrag_size = AUD_CS4231_MAX_BSIZE;

	/*
	 * Initialize the record stream (by copying play stream
	 * and correcting some values)
	 */
	input_as->distate = &unitp->distate;
	input_as->type = AUDTYPE_DATA;
	input_as->mode = AUDMODE_AUDIO;
	input_as->signals_okay = B_FALSE;
	input_as->info = output_as->info;
	input_as->info.gain = AUD_CS4231_DEFAULT_RECGAIN;
	input_as->info.sample_rate = AUD_CS4231_SAMPLERATE;
	input_as->info.channels = AUD_CS4231_CHANNELS;
	input_as->info.precision = AUD_CS4231_PRECISION;
	input_as->info.encoding = AUDIO_ENCODING_ULAW;
	input_as->info.minordev = CS_MINOR_RO;
	if (unitp->cd_input_line != NO_INTERNAL_CD) {
		input_as->info.avail_ports =
		    AUDIO_MICROPHONE | AUDIO_LINE_IN | AUDIO_INTERNAL_CD_IN;
	} else {
		input_as->info.avail_ports = AUDIO_MICROPHONE |
		    AUDIO_LINE_IN;
	}
	input_as->info.port = AUDIO_MICROPHONE;
	input_as->info.buffer_size = audio_4231_bsize;
	input_as->traceq = NULL;
	input_as->maxfrag_size = AUD_CS4231_MAX_BSIZE;

	/*
	 * Control stream info
	 */
	as->distate = &unitp->distate;
	as->type = AUDTYPE_CONTROL;
	as->mode = AUDMODE_NONE;
	as->signals_okay = B_TRUE;
	as->info.minordev = CS_MINOR_CTL;
	as->traceq = NULL;

	/*
	 * Initialize virtual chained DMA command block free
	 * lists.  Reserve a couple of command blocks for record
	 * buffers.  Then allocate the rest for play buffers.
	 */
	pool = (aud_cmd_t *)unitp->allocated_memory;
	unitp->input.as.cmdlist.free = NULL;
	unitp->output.as.cmdlist.free = NULL;
	for (i = 0; i < audio_4231_cmdpool; i++) {
		struct aud_cmdlist *list;

		list = (i < audio_4231_recbufs) ?
		    &unitp->input.as.cmdlist :
		    &unitp->output.as.cmdlist;
		pool->next = list->free;
		list->free = pool++;
	}

	for (i = 0; i < DMA_LIST_SIZE; i++) {
		dma_recorded_list[i].cmdp = (aud_cmd_t *)NULL;
		dma_recorded_list[i].buf_dma_handle = NULL;
		dma_played_list[i].cmdp = (aud_cmd_t *)NULL;
		dma_played_list[i].buf_dma_handle = NULL;
	}

	/*
	 * add interrupt handler
	 */
	rc = AUD_DMA_ADD_INTR(dip, unitp);
	if (rc != DDI_SUCCESS) {
		cmn_err(CE_WARN, "audiocs: bad interrupt specification");
		return (DDI_FAILURE);
	}

	mutex_init(&unitp->lock, "audio_4231 intr level lock",
	    MUTEX_DRIVER, (void *)audio_4231_trap_cookie);

	output_as->lock = input_as->lock = as->lock = &unitp->lock;

	cv_init(&unitp->output.as.cv, "aud odrain cv", CV_DRIVER,
	    &audio_4231_trap_cookie);
	cv_init(&unitp->control.as.cv, "aud wopen cv", CV_DRIVER,
	    &audio_4231_trap_cookie);

	/*
	 * Initialize the audio chip
	 */
	LOCK_UNITP(unitp);
	audio_4231_chipinit(unitp);
	UNLOCK_UNITP(unitp);

	strcpy(name, "sound,audio");
	if (ddi_create_minor_node(dip, name, S_IFCHR, instance,
	    DDI_NT_AUDIO, 0) == DDI_FAILURE) {
		goto remmutex;
	}

	strcpy(name, "sound,audioctl");
	if (ddi_create_minor_node(dip, name, S_IFCHR,
	    instance | CS_MINOR_CTL, DDI_NT_AUDIO, 0) ==
	    DDI_FAILURE) {
		goto remminornode;
	}

	ddi_report_dev(dip);

	/* Increment the device count now that this one is enabled */
	devcount = 0x00; /* for CPR XXX fix me */

#if defined(i386) || defined(__ppc)

	REG_SELECT(PIN_CR);
	OR_SET_BYTE_R(handle, CS4231_IDR, INTR_ON);

#endif	/* defined(i386) || defined(__ppc) */

	/*
	 * Initialize power management bookkeeping;
	 * components are created idle
	 */
	if (pm_create_components(dip, 1) == DDI_SUCCESS) {
		pm_busy_component(dip, 0);
		pm_set_normal_power(dip, 0, 1);
	} else {
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);

	/*
	 * Error cleanup handling
	 */
remminornode:
	ddi_remove_minor_node(dip, NULL);

remmutex:
	mutex_destroy(&unitp->lock);
	cv_destroy(&unitp->control.as.cv);
	cv_destroy(&unitp->output.as.cv);

#if defined(sparc) || defined(__sparc)
remhardint1:
	if (unitp->eb2dma) {
		ddi_remove_intr(dip, (uint_t)1, audio_4231_trap_cookie);
	}
#endif	/* defined(sparc) */

remhardint:
#if defined(i386) || defined(__ppc)
	REG_SELECT(PIN_CR);
	AND_SET_BYTE_R(handle, CS4231_IDR, ~INTR_ON);

#endif	/* defined(i386) || defined(__ppc) */
	ddi_remove_intr(dip, (uint_t)0, audio_4231_trap_cookie);

freemem:
	/* Deallocate structures allocated above */
	kmem_free(unitp->allocated_memory, unitp->allocated_size);

unmapregs:
	AUD_DMA_UNMAP_REGS(unitp);

	return (DDI_FAILURE);
}


/*
 * Detach from the device.
 */
/*ARGSUSED*/
static int
audio_4231_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	cs_unit_t *unitp;
#if defined(i386) || defined(__ppc)
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

#endif			/* defined(i386) || defined(__ppc) */

	/* XXX - only handles a single detach at present */
	unitp = &cs_units[0];

#if defined(i386) || defined(__ppc)
	handle = CS4231_HANDLE;
	chip = unitp->chip;
#endif			/* defined(i386) || defined(__ppc) */

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_DETA, ("4231_detach: dip 0x%x\n", dip));

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		LOCK_UNITP(unitp);
		if (unitp->suspended) {
			UNLOCK_UNITP(unitp);
			return (DDI_SUCCESS);
		}
		unitp->suspended = B_TRUE;
		UNLOCK_UNITP(unitp);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	ddi_remove_minor_node(dip, NULL);

	mutex_destroy(&unitp->lock);

	cv_destroy(&unitp->control.as.cv);
	cv_destroy(&unitp->output.as.cv);

#if defined(i386) || defined(__ppc)
	REG_SELECT(PIN_CR);
	AND_SET_BYTE_R(handle, CS4231_IDR, ~INTR_ON);

#endif	/* defined(i386) || defined(__ppc) */

	ddi_remove_intr(dip, (uint_t)0, audio_4231_trap_cookie);
	AUD_DMA_UNMAP_REGS(unitp);
	kmem_free(unitp->allocated_memory, unitp->allocated_size);
	pm_destroy_components(dip);

	return (DDI_SUCCESS);
}

/*
 * Device open routine: set device structure ptr and call generic routine
 */
/*ARGSUSED*/
static int
audio_4231_open(queue_t *q, dev_t *dp, int oflag, int sflag, cred_t *credp)
{
	cs_unit_t *unitp;
	aud_stream_t *as = NULL;
	minor_t minornum;
	int status;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_OPEN, ("4231_open: queuep 0x%x\n", q));

	/*
	 * Check that device is legal:
	 * Base unit number must be valid.
	 * If not a clone open, must be the control device.
	 */
	if (CS_UNIT(*dp) > devcount) {
		return (ENODEV);
	}

	minornum = geteminor(*dp);

	/*
	 * Get address of generic audio status structure
	 */
	unitp = &cs_units[CS_UNIT(*dp)];
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	ASSERT(handle != NULL);
	ASSERT(chip != NULL);

	/*
	 * Get the correct audio stream
	 */
	if (minornum == unitp->output.as.info.minordev || minornum == 0)
		as = &unitp->output.as;
	else if (minornum == unitp->input.as.info.minordev)
		as = &unitp->input.as;
	else if (minornum == unitp->control.as.info.minordev)
		as = &unitp->control.as;

	if (as == NULL)
		return (ENODEV);

	LOCK_AS(as);
	ATRACE(audio_4231_open, 'OPEN', as);

	/*
	 * Pick up the control device.
	 * Init softstate if HW state hasn't been done yet.
	 */
	if (as == as->control_as) {
		as->type = AUDTYPE_CONTROL;
	} else {
		as = (oflag & FWRITE) ? as->output_as : as->input_as;
		as->type = AUDTYPE_DATA;
		if (!unitp->hw_output_inited ||
		    !unitp->hw_input_inited) {
			as->output_as->info.sample_rate = AUD_CS4231_SAMPLERATE;
			as->output_as->info.channels = AUD_CS4231_CHANNELS;
			as->output_as->info.precision = AUD_CS4231_PRECISION;
			as->output_as->info.encoding = AUDIO_ENCODING_ULAW;
			as->input_as->info.sample_rate = AUD_CS4231_SAMPLERATE;
			as->input_as->info.channels = AUD_CS4231_CHANNELS;
			as->input_as->info.precision = AUD_CS4231_PRECISION;
			as->input_as->info.encoding = AUDIO_ENCODING_ULAW;
		}
	}

	if (ISDATASTREAM(as) && ((oflag & (FREAD|FWRITE)) == FREAD))
		as = as->input_as;

	if (ISDATASTREAM(as)) {
		minornum = as->info.minordev | CS_CLONE_BIT;
		sflag = CLONEOPEN;
	} else {
		minornum = as->info.minordev;
	}

	status = audio_open(as, q, dp, oflag, sflag);
	if (status != 0) {
		ATRACE(audio_4231_open, 'LIAF', as);
		goto done;
	}
	ATRACE(audio_4231_open, 'HERE', as);
	/*
	 * Reset to 8bit u-law mono (default) on open.
	 * This is here for compatibility with the man page
	 * interface for open of /dev/audio as described in audio(7).
	 */
	if (as == as->output_as && as->input_as->readq == NULL) {
		ATRACE(audio_4231_open, 'OUTA', as);
		as->output_as->info.sample_rate = AUD_CS4231_SAMPLERATE;
		as->output_as->info.channels = AUD_CS4231_CHANNELS;
		as->output_as->info.precision = AUD_CS4231_PRECISION;
		as->output_as->info.encoding = AUDIO_ENCODING_ULAW;
		unitp->hw_output_inited = B_FALSE;
		unitp->hw_input_inited = B_FALSE;
	} else if (as == as->input_as && as->output_as->readq == NULL) {
		ATRACE(audio_4231_open, 'INNA', as);
		as->input_as->info.sample_rate = AUD_CS4231_SAMPLERATE;
		as->input_as->info.channels = AUD_CS4231_CHANNELS;
		as->input_as->info.precision = AUD_CS4231_PRECISION;
		as->input_as->info.encoding = AUDIO_ENCODING_ULAW;
		unitp->hw_output_inited = B_FALSE;
		unitp->hw_input_inited = B_FALSE;
	}

	if (ISDATASTREAM(as) && (oflag & FREAD)) {
		/*
		 * Set input bufsize now, in case the value was patched
		 */
		as->input_as->info.buffer_size = audio_4231_bsize;

		audio_process_input(as->input_as);
	}

	*dp = makedevice(getmajor(*dp), CS_UNIT(*dp) | minornum);

done:
	UNLOCK_AS(as);
	ATRACE(audio_4231_open, 'DONE', as);
	return (status);
}


/*
 * Device-specific close routine, called from generic module.
 * Must be called with UNITP lock held.
 */
static void
audio_4231_close(aud_stream_t *as)
{
	cs_unit_t *unitp;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	ASSERT_ASLOCKED(as);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_CLOS, ("4231_close: as 0x%x\n", as));

	unitp = UNITP(as);
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	ASSERT(handle != NULL);
	ASSERT(chip != NULL);

	/*
	 * Reset status bits.  The device will already have been stopped.
	 */
	ATRACE(audio_4231_close, 'DONE', as);
	if (as == as->output_as) {
		audio_4231_clear((aud_dma_list_t *)&dma_played_list, unitp);
		/*
		 * If the variable has been tuned in /etc/system
		 * then set it to the tuned param else set it
		 * to the default.
		 */
		if (audio_4231_play_bsize != AUD_CS4231_MAXPACKET) {
			as->output_as->info.buffer_size =
			    audio_4231_play_bsize;
		} else {
			as->output_as->info.buffer_size =
			    AUD_CS4231_MAXPACKET;
		}
		unitp->output.samples = (uint_t)0x00;
		unitp->output.error = B_FALSE;
	} else {
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_CLOS, ("4231_close: resetting "
		    "input.error, input.samples\n"));
		unitp->input.samples = (uint_t)0x00;
		unitp->input.error = B_FALSE;
	}

	if (as == as->control_as) {
		as->control_as->info.open = B_FALSE;
	}

	if (!as->output_as->info.open && !as->input_as->info.open &&
		    !as->control_as->info.open) {
		unitp->hw_output_inited = B_FALSE;
		unitp->hw_input_inited = B_FALSE;
	}

	/*
	 * If a user process mucked up the device, reset it when fully
	 * closed
	 */
	if (unitp->init_on_close && !as->output_as->info.open &&
	    !as->input_as->info.open) {
		audio_4231_chipinit(unitp);
		unitp->init_on_close = B_FALSE;
	}

	ATRACE(audio_4231_close, 'CLOS', as);

}


/*
 * Process ioctls not already handled by the generic audio handler.
 *
 * If AUDIO_CHIP is defined, we support ioctls that allow user processes
 * to muck about with the device registers.
 * Must be called with UNITP lock held.
 */
static aud_return_t
audio_4231_ioctl(aud_stream_t *as, queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp;
	aud_return_t change;
	caddr_t uaddr;
	audio_device_t	*devtp;

	ASSERT_ASLOCKED(as);
	change = AUDRETURN_NOCHANGE; /* detect device state change */

	iocp = (struct iocblk *)(void *)mp->b_rptr;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_IOCT, ("4231_ioctl: cmd 0x%x\n",
	    iocp->ioc_cmd));

	switch (iocp->ioc_cmd) {
	default:
	einval:
#if defined(_BIG_ENDIAN)
		ATRACE(audio_4231_ioctl, 'LVNI', as);
#else	/* defined(_LITTLE_ENDIAN) */
		ATRACE(audio_4231_ioctl, 'INVL', as);
#endif	/* defined(_BIG_ENDIAN) */

		/* NAK the request */
		audio_ack(q, mp, EINVAL);
		goto done;

	case AUDIO_GETDEV:	/* return device type */
#if defined(_BIG_ENDIAN)
		ATRACE(audio_4231_ioctl, 'VEDG', as);
#else	/* defined(_LITTLE_ENDIAN) */
		ATRACE(audio_4231_ioctl, 'GDEV', as);
#endif	/* defined(_BIG_ENDIAN) */

		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;
		freemsg(mp->b_cont);
		mp->b_cont = allocb(sizeof (audio_device_t), BPRI_MED);
		if (mp->b_cont == NULL) {
			audio_ack(q, mp, ENOSR);
			goto done;
		}

		devtp = (audio_device_t *)(void *)mp->b_cont->b_rptr;
		mp->b_cont->b_wptr += sizeof (audio_device_t);
		strcpy(devtp->name, CS_DEV_NAME);

/* XXXPPC - need a new heuristic for determining version.... */
		AUD_DMA_VERSION(UNITP(as), devtp->version);

		strcpy(devtp->config, CS_DEV_CONFIG_ONBRD1);

		audio_copyout(q, mp, uaddr, sizeof (audio_device_t));
		break;

	case AUDIO_DIAG_LOOPBACK: /* set clear loopback mode */
		ATRACE(audio_4231_ioctl, 'POOL', as);
		/* Acknowledge the request and we're done */
		audio_ack(q, mp, 0);
		change = AUDRETURN_CHANGE;
		break;
	}

done:
	return (change);
}


/*
 * audio_4231_mproto - handle synchronous M_PROTO messages
 *
 * This driver does not support any M_PROTO messages, but we must
 * free the message.
 */
/*ARGSUSED*/
static aud_return_t
audio_4231_mproto(aud_stream_t *as, mblk_t *mp)
{
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_MPRO, ("4231_mproto: mblkp\n", mp));

	freemsg(mp);
	return (AUDRETURN_NOCHANGE);
}


/*
 * The next routine is used to start reads or writes.
 * If there is a change of state, enable the chip.
 * If there was already i/o active in the desired direction,
 * or if i/o is paused, don't bother re-initializing the chip.
 * Must be called with UNITP lock held.
 */
static void
audio_4231_start(aud_stream_t *as)
{
	cs_stream_t *css;
	int pause;
	cs_unit_t *unitp;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	ASSERT_ASLOCKED(as);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_STRT, ("4231_start: as 0x%x\n", as));
	ATRACE(audio_4231_start, '  AS', as);
	unitp = UNITP(as);
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	if (as == as->output_as) {
		ATRACE(audio_4231_start, 'OUAS', as);
		css = &unitp->output;
	} else {
		ATRACE(audio_4231_start, 'INAS', as);
		css = &unitp->input;
	}

	pause = as->info.pause;

	/*
	 * If we are paused, this must mean that we were paused while
	 * playing or recording.  In this case we just want to resume
	 * from where we left off.  If we are starting or re-starting
	 * we just want to start playing as in the normal case.
	 */

	/* If already active, paused, or nothing queued to the device, done */
	if (css->active || (pause == B_TRUE) || (css->cmdptr == NULL)) {

		/*
		 * if not active, and not paused, and the cmdlist
		 * pointer is NULL, but there are buffers queued up
		 * for the play, then we may have been called as a
		 * result of the audio_resume, so set the cmdptr
		 * to the head of the list and start the play side.
		 */

		if (!css->active && (pause == B_FALSE) && as->cmdlist.head &&
		    as->cmdlist.head->next && (as == as->output_as)) {
			css->cmdptr = as->cmdlist.head;
		} else {
			ATRACE(audio_4231_start, 'RET ', css);
			return;
		}
	}

	css->active = B_TRUE;

	if (!unitp->hw_output_inited || !unitp->hw_input_inited) {

		audio_4231_mute_channel(unitp, L_OUTPUT_CR);
		audio_4231_mute_channel(unitp, R_OUTPUT_CR);

		REG_SELECT(IAR_MCE | PLAY_DATA_FR);
		ddi_putb(handle, CS4231_IDR, (uchar_t)DEFAULT_DATA_FMAT);

		REG_SELECT(IAR_MCE | CAPTURE_DFR);
		ddi_putb(handle, CS4231_IDR, (uchar_t)DEFAULT_DATA_FMAT);
		REG_SELECT(CAPTURE_DFR);	/* turn off MCE */
		audio_4231_pollready();

		audio_4231_unmute_channel(unitp, L_OUTPUT_CR);
		audio_4231_unmute_channel(unitp, R_OUTPUT_CR);

		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_PLAY | AUD_EM_RCRD,
		    ("4231_start: register state after data format set:\n"));
		AUD_ERRDUMPREGS(AUD_EP_L1, AUD_EM_PLAY | AUD_EM_RCRD);

		unitp->hw_output_inited = B_TRUE;
		unitp->hw_input_inited = B_TRUE;
	}

	if ((pause == B_FALSE) && (as == as->output_as)) {
		ATRACE(audio_4231_start, 'IDOU', as);
		audio_4231_initlist((aud_dma_list_t *)&dma_played_list, unitp);
		/*
		 * We must clear off the pause first. If you
		 * don't it won't continue from a abort.
		 */
		AUD_DMA_START_OUTPUT(unitp);

	} else if ((pause == B_FALSE) && (as == as->input_as)) {
		ATRACE(audio_4231_start, 'IDIN', as);
		audio_4231_initlist((aud_dma_list_t *)&dma_recorded_list,
		    unitp);

		AUD_DMA_START_INPUT(unitp);

		audio_4231_pollready();
		REG_SELECT(INTERFACE_CR);
		OR_SET_BYTE_R(handle, CS4231_IDR, CEN_ENABLE);
	}

}


/*
 * The next routine is used to stop reads or writes.
 * Must be called with UNITP lock held.
 */

static void
audio_4231_stop(aud_stream_t *as)
{
	cs_unit_t *unitp;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	ASSERT_ASLOCKED(as);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_STOP, ("4231_stop: as 0x%x\n", as));
	ATRACE(audio_4231_stop, '  AS', as);

	unitp = UNITP(as);
	handle = CS4231_HANDLE;
	chip = unitp->chip;
	/*
	 * For record.
	 * You HAVE to make sure that all of the DMA is freed up
	 * on the stop and DMA is first stopped in this routine.
	 * Otherwise the flow of code will progress in such a way
	 * that the dma memory will be freed, the device closed
	 * and an intr will come in and scribble on supposedly
	 * clean memory (verify_pattern in streams with 0xcafefeed)
	 * This causes a panic in allocb...on the subsequent alloc of
	 * this block of previously freed memory.
	 * The CPAUSE stops the dma and the recordend frees up the
	 * dma. We poll for the pipe empty bit rather than handling it
	 * in the intr routine because it breaks the code flow since stop and
	 * pause share the same functionality.
	 */

	if (as == as->output_as) {
		unitp->output.active = B_FALSE;
		AUD_DMA_STOP_OUTPUT(unitp);
	} else {
		unitp->input.active = B_FALSE;
		AUD_DMA_STOP_INPUT(unitp);

		REG_SELECT(INTERFACE_CR);
		AND_SET_BYTE_R(handle, CS4231_IDR, CEN_DISABLE);

		/*
		 * process the last (incomplete) buffer.
		 */
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD,
		    ("4231_stop: calling recordend from stop routine\n"));
		audio_4231_recordend(UNITP(as),
		    (aud_dma_list_t *)&dma_recorded_list);
		audio_4231_clear((aud_dma_list_t *)&dma_recorded_list, unitp);
	}
}


/*
 * Get or set a particular flag value.  Must be called with UNITP lock
 * held.
 */
static uint_t
audio_4231_setflag(aud_stream_t *as, enum aud_opflag op, uint_t val)
{
	cs_stream_t *css;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_SETF, ("4231_setflag: 0x%x\n", op));

	css = (as == as->output_as) ? &UNITP(as)->output : &UNITP(as)->input;

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_SETF | AUD_EM_SETI, ("4231_setflag: "
	    "error %d, active %d\n", css->error, css->active));

	switch (op) {
	case AUD_ERRORRESET:	/* read reset error flag atomically */
		val = css->error;
		css->error = B_FALSE;
		break;

	case AUD_ACTIVE:	/* GET only */
		val = css->active;
		break;
	}

	return (val);
}


/*
 * Get or set device-specific information in the audio state structure.
 * Must be called with UNITP lock held.
 */
static aud_return_t
audio_4231_setinfo(aud_stream_t *as, mblk_t *mp, int *error)
{
	cs_unit_t *unitp;
	struct aud_4231_chip *chip;
	audio_info_t *ip;
	uint_t sample_rate, channels, precision, encoding;
	uint_t o_sample_rate, o_channels, o_precision, o_encoding;
	uint_t gain;
	uint_t capcount, playcount;
	uchar_t balance;
	uchar_t	tmp_bits;
	ddi_acc_handle_t handle;
	uint_t  tmp_pcount, tmp_ccount;

	ASSERT_ASLOCKED(as);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_SETI, ("4231_setinfo: as 0x%x\n", as));

	unitp = UNITP(as);
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_SETI, ("4231_setinfo: entry point: "
	    "mp 0x%x\n", mp));

	tmp_pcount = tmp_ccount = capcount = playcount = 0;
	/*
	 * Set device-specific info into device-independent structure
	 */
	AUD_DMA_GET_COUNT(unitp, (uint32_t *)&tmp_pcount,
	    (uint32_t *)&tmp_ccount);
	AUD_ERRPRINT(AUD_EP_L1, (AUD_EM_RCRD | AUD_EM_PLAY), ("4231_setinfo: "
	    "residual DMA pcount: 0x%x\t ccount: 0x%x\n", tmp_pcount,
	    tmp_ccount));

	if (unitp->input.active) {
		capcount = audio_4231_sampleconv(&unitp->input, tmp_ccount);
	} else if (unitp->output.active) {
		playcount = audio_4231_sampleconv(&unitp->output, tmp_pcount);
	}
	AUD_ERRPRINT(AUD_EP_L1, (AUD_EM_SETI | AUD_EM_RCRD | AUD_EM_PLAY),
	    ("4231_setinfo: playcount: 0x%x\n", playcount));
	AUD_ERRPRINT(AUD_EP_L1, (AUD_EM_SETI | AUD_EM_RCRD | AUD_EM_PLAY),
	    ("4231_setinfo: reccount: 0x%x\n", capcount));

	if (playcount > unitp->output.samples) {
		if (unitp->output.samples > 0) {
			as->output_as->info.samples = unitp->output.samples;
		} else {
			as->output_as->info.samples = 0;
		}
	} else {
/* XXXMERGE there are a couple of different cases; the eb2 case makes */
/* a distinction for the initial calculation.  Truly generic? */
		if ((unitp->output.samples - playcount) <
				    as->output_as->info.samples) {
			as->output_as->info.samples = unitp->output.samples;
		} else {
			as->output_as->info.samples =
			    unitp->output.samples - playcount;
		}
	}
	AUD_ERRPRINT(AUD_EP_L1, (AUD_EM_SETI | AUD_EM_PLAY), ("4231_setinfo: "
	    "out_as #samples: 0x%x\n", as->output_as->info.samples));
	if (capcount > unitp->input.samples) {
		if (unitp->input.samples > 0) {
			as->input_as->info.samples = unitp->input.samples;
		} else {
			as->input_as->info.samples = 0;
		}
	} else {
		as->input_as->info.samples = unitp->input.samples - capcount;
	}
	AUD_ERRPRINT(AUD_EP_L1, (AUD_EM_SETI | AUD_EM_RCRD), ("4231_setinfo: "
	    "input.samples: 0x%x\tinput_as #samples: 0x%x\n",
	    unitp->input.samples, as->input_as->info.samples));

	as->output_as->info.active = unitp->output.active;
	as->input_as->info.active = unitp->input.active;

	/*
	 * If getinfo, 'mp' is NULL...we're done
	 */
	if (mp == NULL)
		return (AUDRETURN_NOCHANGE);

	ip = (audio_info_t *)(void *)mp->b_cont->b_rptr;

	/*
	 * If any new value matches the current value, there
	 * should be no need to set it again here.
	 * However, it's work to detect this so don't bother.
	 */
	if (Modify(ip->play.gain) || Modifyc(ip->play.balance)) {
		if (Modify(ip->play.gain))
			gain = ip->play.gain;
		else
			gain = as->output_as->info.gain;

		if (Modifyc(ip->play.balance))
			balance = ip->play.balance;
		else
			balance = as->output_as->info.balance;

		as->output_as->info.gain = audio_4231_play_gain(unitp,
		    gain, balance);
		as->output_as->info.balance = balance;
	}

	if (Modify(ip->record.gain) || Modifyc(ip->record.balance)) {
		AUD_ERRPRINT(AUD_EP_L1, (AUD_EM_SETI | AUD_EM_RCRD),
		    ("4231_setinfo: record gain (old %d) (new %d)\n",
		    as->input_as->info.gain, ip->record.gain));
		if (Modify(ip->record.gain))
			gain = ip->record.gain;
		else
			gain = as->input_as->info.gain;

		if (Modifyc(ip->record.balance))
			balance = ip->record.balance;
		else
			balance = as->input_as->info.balance;

		as->input_as->info.gain = audio_4231_record_gain(unitp,
		    gain, balance);
		as->input_as->info.balance = balance;
	}

	if (Modify(ip->record.buffer_size)) {
		if ((ip->record.buffer_size <= 0) ||
		    (ip->record.buffer_size > AUD_CS4231_MAX_BSIZE)) {
			*error = EINVAL;
		} else {
			as->input_as->info.buffer_size = ip->record.buffer_size;
		}
	}

	if (Modify(ip->play.buffer_size)) {
		if ((ip->play.buffer_size <= 0) ||
		    (ip->play.buffer_size > AUD_CS4231_MAX_BSIZE)) {
			*error = EINVAL;
		} else {
			if (as == as->output_as) {
				freezestr(as->output_as->writeq);
				strqset(as->output_as->writeq, QMAXPSZ, 0,
				    ip->play.buffer_size);
				unfreezestr(as->output_as->writeq);
				as->output_as->info.buffer_size =
				    ip->play.buffer_size;
			}
		}
	}

	if (Modify(ip->monitor_gain)) {
		as->distate->monitor_gain = audio_4231_monitor_gain(unitp,
		    ip->monitor_gain);
	}

	if (Modifyc(ip->output_muted)) {
		as->distate->output_muted = audio_4231_output_muted(unitp,
		    ip->output_muted);
	}


	if (Modify(ip->play.port)) {
		as->output_as->info.port = audio_4231_outport(unitp,
		    ip->play.port);
	}

	if (Modify(ip->record.port)) {
		as->input_as->info.port = audio_4231_inport(unitp,
					    ip->record.port);
	}

	/*
	 * Save the old settings on any error of the following 4
	 * reset all back to the old and exit.
	 * DBRI compatibility.
	 */

	o_sample_rate = as->info.sample_rate;
	o_encoding = as->info.encoding;
	o_precision = as->info.precision;
	o_channels = as->info.channels;

	/*
	 * Set the sample counters atomically, returning the old values.
	 */
	if (Modify(ip->play.samples) || Modify(ip->record.samples)) {
		if (as->output_as->info.open) {
			as->output_as->info.samples = unitp->output.samples;
			if (Modify(ip->play.samples))
				unitp->output.samples = ip->play.samples;
		}

		if (as->input_as->info.open) {
			as->input_as->info.samples = unitp->input.samples;
			if (Modify(ip->record.samples))
				unitp->input.samples = ip->record.samples;
		}
	}

	if (Modify(ip->play.sample_rate))
		sample_rate = ip->play.sample_rate;
	else if (Modify(ip->record.sample_rate))
		sample_rate = ip->record.sample_rate;
	else if ((unitp->hw_output_inited == B_FALSE) ||
		(unitp->hw_input_inited == B_FALSE))
		sample_rate = NEEDS_HW_INIT;
	else
		sample_rate = as->info.sample_rate;

	if (Modify(ip->play.channels))
		channels = ip->play.channels;
	else if (Modify(ip->record.channels))
		channels = ip->record.channels;
	else if ((unitp->hw_output_inited == B_FALSE) ||
		(unitp->hw_input_inited == B_FALSE))
		channels = NEEDS_HW_INIT;
	else
		channels = as->info.channels;


	if (Modify(ip->play.precision))
		precision = ip->play.precision;
	else if (Modify(ip->record.precision))
		precision = ip->record.precision;
	else if ((unitp->hw_output_inited == B_FALSE) ||
		(unitp->hw_input_inited == B_FALSE))
		precision = NEEDS_HW_INIT;
	else
		precision = as->info.precision;

	if (Modify(ip->play.encoding))
		encoding = ip->play.encoding;
	else if (Modify(ip->record.encoding))
		encoding = ip->record.encoding;
	else if ((unitp->hw_output_inited == B_FALSE) ||
		(unitp->hw_input_inited == B_FALSE))
		encoding = NEEDS_HW_INIT;
	else
		encoding = as->info.encoding;

	/*
	 * Control stream cannot modify the play format
	 */
	if ((as != as->output_as) && (as != as->input_as)) {
		if ((Modify(ip->play.sample_rate)) ||
		    (Modify(ip->play.channels)) ||
		    (Modify(ip->play.precision)) ||
		    (Modify(ip->play.encoding)) ||
		    (Modify(ip->record.sample_rate)) ||
		    (Modify(ip->record.channels)) ||
		    (Modify(ip->record.precision)) ||
		    (Modify(ip->record.encoding))) {
			*error = EINVAL;
			goto playdone;
		}

		/*
		 * If not trying to change the data format, we're done
		 */
		goto done;
	}

	/*
	 * If setting to the current format, do not do anything.  Otherwise
	 * check and see if this is a valid format.
	 */

	if ((sample_rate == as->info.sample_rate) &&
	    (channels == as->info.channels) &&
	    (precision == as->info.precision) &&
	    (encoding == as->info.encoding) &&
	    (unitp->hw_output_inited == B_TRUE ||
		    unitp->hw_input_inited == B_TRUE)) {
		goto done;
	}

	/*
	 * setup default values if none specified and the audio
	 * chip has not been initialized
	 */
	if (sample_rate == NEEDS_HW_INIT)
		sample_rate = AUD_CS4231_SAMPLERATE;

	/*
	 * If we try and change the data format for either the play or
	 * record side, we must check to see if the other data channel
	 * is open first. If so then return an error.
	 */
	if ((Modify(ip->play.sample_rate)) || (Modify(ip->play.channels)) ||
	    (Modify(ip->play.precision)) || (Modify(ip->play.encoding))) {
		/*
		 * If a process wants to modify the play or record format,
		 * another process can not have it open for recording.
		 */
		if (as->input_as->info.open &&
		    (as->input_as->readq != as->output_as->readq)) {
			*error = EBUSY;
			goto playdone;
		}
	}

	if ((Modify(ip->record.sample_rate)) || (Modify(ip->record.channels)) ||
	    (Modify(ip->record.precision)) || (Modify(ip->record.encoding))) {
		/*
		 * If a process wants to modify the play or record format,
		 * another process can not have it open for playing.
		 */
		if (as->output_as->info.open &&
		    (as->input_as->readq != as->output_as->readq)) {
			*error = EBUSY;
			goto playdone;
		}
	}

	/*
	 * If we get here we must want to change the data format
	 * Changing the data format is done for both the play and
	 * record side for now.
	 */

	switch (sample_rate) {
	case 8000:		/* ULAW and ALAW */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_8000);
		sample_rate = AUD_CS4231_SAMPR8000;
		break;
	case 9600:		/* SPEECHIO */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_9600);
		sample_rate = AUD_CS4231_SAMPR9600;
		break;
	case 11025:
		audio_4231_change_sample_rate(unitp, CS4231_DFR_11025);
		sample_rate = AUD_CS4231_SAMPR11025;
		break;
	case 16000:		/* G_722 */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_16000);
		sample_rate = AUD_CS4231_SAMPR16000;
		break;
	case 18900:		/* CDROM_XA_C */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_18900);
		sample_rate = AUD_CS4231_SAMPR18900;
		break;
	case 22050:
		audio_4231_change_sample_rate(unitp, CS4231_DFR_22050);
		sample_rate = AUD_CS4231_SAMPR22050;
		break;
	case 32000:		/* DAT_32 */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_32000);
		sample_rate = AUD_CS4231_SAMPR32000;
		break;
	case 37800:		/* CDROM_XA_AB */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_37800);
		sample_rate = AUD_CS4231_SAMPR37800;
		break;
	case 44100:		/* CD_DA */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_44100);
		sample_rate = AUD_CS4231_SAMPR44100;
		break;
	case 48000:		/* DAT_48 */
		audio_4231_change_sample_rate(unitp, CS4231_DFR_48000);
		sample_rate = AUD_CS4231_SAMPR48000;
		break;
	default:
		*error = EINVAL;
		break;
	} /* switch on sampling rate */

	drv_usecwait(100); /* chip bug workaround */
	audio_4231_pollready();
	drv_usecwait(1000);	/* chip bug */

	/*
	 * verify valid precision values
	 */
	if (precision != NEEDS_HW_INIT && precision != AUD_CS4231_PRECISION &&
	    precision != (2 * AUD_CS4231_PRECISION)) {
		*error = EINVAL;
		goto playdone;
	}
	/*
	 * verify valid channel values
	 */
	if (channels != NEEDS_HW_INIT && channels != AUD_CS4231_CHANNELS &&
	    channels != (2 * AUD_CS4231_CHANNELS)) {
		*error = EINVAL;
	}

	if ((encoding == NEEDS_HW_INIT) || (Modify(ip->play.encoding)) ||
	    (Modify(ip->record.encoding)) && *error != EINVAL) {

		if (encoding == NEEDS_HW_INIT)
			encoding = AUDIO_ENCODING_ULAW;
		if (channels == NEEDS_HW_INIT)
			channels = AUD_CS4231_CHANNELS;
		if (precision == NEEDS_HW_INIT)
			precision = AUD_CS4231_PRECISION;

		switch (encoding) {
		case AUDIO_ENCODING_ULAW:
			if (Modify(channels) && (channels != 1))
				*error = EINVAL;
			REG_SELECT(IAR_MCE | PLAY_DATA_FR);
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_ULAW);
			ddi_putb(handle, CS4231_IDR,
			    (uchar_t)(CS4231_MONO_ON(tmp_bits)));

			REG_SELECT(IAR_MCE | CAPTURE_DFR);
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_ULAW);
			ddi_putb(handle, CS4231_IDR,
			    (uchar_t)(CS4231_MONO_ON(tmp_bits)));
			channels = 0x01;
			encoding = AUDIO_ENCODING_ULAW;
			break;

		case AUDIO_ENCODING_ALAW:
			if (Modify(channels) && (channels != 1))
				*error = EINVAL;
			REG_SELECT(IAR_MCE | PLAY_DATA_FR);
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_ALAW);
			ddi_putb(handle, CS4231_IDR,
			    (uchar_t)(CS4231_MONO_ON(tmp_bits)));

			REG_SELECT(IAR_MCE | CAPTURE_DFR);
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_ALAW);
			ddi_putb(handle, CS4231_IDR,
			    (uchar_t)(CS4231_MONO_ON(tmp_bits)));
			channels = 0x01;
			encoding = AUDIO_ENCODING_ALAW;
			break;

		case AUDIO_ENCODING_LINEAR:
			if (Modify(channels) && (channels != 2) &&
			    (channels != 1))
				*error = EINVAL;

			REG_SELECT(IAR_MCE | PLAY_DATA_FR);
#if defined(_BIG_ENDIAN)
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_LINEARBE);
#else	/* defined(_LITTLE_ENDIAN) */
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_LINEARLE);
#endif	/* defined(_BIG_ENDIAN) */

			if (channels == 2) {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_STEREO_ON(tmp_bits)));
			} else {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_MONO_ON(tmp_bits)));
			}

			REG_SELECT(IAR_MCE | CAPTURE_DFR);
#if defined(_BIG_ENDIAN)
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_LINEARBE);
#else	/* defined(_LITTLE_ENDIAN) */
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_LINEARLE);
#endif	/* defined(_BIG_ENDIAN) */

			if (channels == 2) {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_STEREO_ON(tmp_bits)));
			} else {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_MONO_ON(tmp_bits)));
			}
			encoding = AUDIO_ENCODING_LINEAR;
			break;

		case AUDIO_ENCODING_DVI:
			/* XXXX REV 2.0 FUTURE SUPPORT HOOK */
			if (Modify(channels) && (channels != 2) &&
				    (channels != 1))
				*error = EINVAL;

			REG_SELECT(IAR_MCE | PLAY_DATA_FR);
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_ADPCM);

			if (channels == 2) {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_STEREO_ON(tmp_bits)));
			} else {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_MONO_ON(tmp_bits)));
			}

			REG_SELECT(IAR_MCE | CAPTURE_DFR);
			tmp_bits = CHANGE_ENCODING(ddi_getb(handle, CS4231_IDR),
			    CS4231_DFR_ADPCM);

			if (channels == 2) {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_STEREO_ON(tmp_bits)));
			} else {
				ddi_putb(handle, CS4231_IDR,
				    (uchar_t)(CS4231_MONO_ON(tmp_bits)));
			}

			encoding = AUDIO_ENCODING_DVI;
			break;
		default:
			*error = EINVAL;
		} /* switch on audio encoding */
	playdone:;

	}

	/*
	 * pollready will do the mode change disable. This
	 * is common to sample rate and encoding changes.
	 * The MCE bit was turned on in the sample rate switch
	 * or the encoding statement.
	 */

	drv_usecwait(100); /* chip bug workaround */
	audio_4231_pollready();
	drv_usecwait(1000);	/* chip bug */

	/*
	 * We don't want to init if it is the control device
	 */
	if (as == &unitp->input.as || as == &unitp->output.as) {
		unitp->hw_output_inited = B_TRUE;
		unitp->hw_input_inited = B_TRUE;
	}
done:
	/*
	 * Update the "real" info structure (and others) accordingly
	 */

	if (*error == EINVAL || *error == EBUSY) {
		sample_rate = o_sample_rate;
		encoding = o_encoding;
		precision = o_precision;
		channels = o_channels;
	}


	/*
	 * one last chance to make sure that we have something
	 * working. Set to defaults if one of the 4 horsemen
	 * is zero.
	 */

	if (sample_rate == 0 || encoding == 0 || precision == 0 ||
			    channels == 0) {
		sample_rate = AUD_CS4231_SAMPLERATE;
		channels = AUD_CS4231_CHANNELS;
		precision = AUD_CS4231_PRECISION;
		encoding = AUDIO_ENCODING_ULAW;
		unitp->hw_output_inited = B_FALSE;
		unitp->hw_input_inited = B_FALSE;
	}

	ip->play.sample_rate = ip->record.sample_rate = sample_rate;
	ip->play.channels = ip->record.channels = channels;
	ip->play.precision = ip->record.precision = precision;
	ip->play.encoding = ip->record.encoding = encoding;

	as->input_as->info.sample_rate = sample_rate;
	as->input_as->info.channels = channels;
	as->input_as->info.precision = precision;
	as->input_as->info.encoding = encoding;

	as->output_as->info.sample_rate = sample_rate;
	as->output_as->info.channels = channels;
	as->output_as->info.precision = precision;
	as->output_as->info.encoding = encoding;

	as->control_as->info.sample_rate = sample_rate;
	as->control_as->info.channels = channels;
	as->control_as->info.precision = precision;
	as->control_as->info.encoding = encoding;

	/*
	 * Init the hi and lowater if they have been set in the
	 * /etc/system file
	 */

#ifdef FUTURE_SUPPORT
	audio_4231_config_queue(as->output_as);
#endif

	ddi_putb(handle, &chip->pioregs.iar, (uchar_t)IAR_MCD);
	drv_usecwait(100);	/* chip bug workaround */
	audio_4231_pollready();
	drv_usecwait(1000);	/* chip bug */

	AUD_ERRPRINT(AUD_EP_L2, AUD_EM_SETI, ("4231_setinfo: exit(0x%x)\n",
	    AUDRETURN_CHANGE));

	return (AUDRETURN_CHANGE);
} /* end of setinfo */

static void
audio_4231_queuecmd(aud_stream_t *as, aud_cmd_t *cmdp)
{
	cs_stream_t *css;

	ASSERT_ASLOCKED(as);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_QCMD, ("4231_queuecmd: cmdp 0x%x\n",
	    cmdp));
	ATRACE(audio_4231_queuecmd, '  AS', as);
	ATRACE(audio_4231_queuecmd, ' CMD', cmdp);

	if (as == as->output_as)
		css = &UNITP(as)->output;
	else
		css = &UNITP(as)->input;

	/*
	 * This device doesn't do packets, so make each buffer its own
	 * packet.
	 */

	cmdp->lastfragment = cmdp;

	/*
	 * If the virtual controller command list is NULL, then the interrupt
	 * routine is probably disabled.  In the event that it is not,
	 * setting a new command list below is safe at low priority.
	 */
	if (css->cmdptr == NULL && !css->active) {
		ATRACE(audio_4231_queuecmd, 'NULL', as->cmdlist.head);
		css->cmdptr = as->cmdlist.head;
		if (!css->active) {
			audio_4231_start(as); /* go, if not paused */
		}
	}


}

/*
 * Flush the device's notion of queued commands.
 * Must be called with UNITP lock held.
 */
static void
audio_4231_flushcmd(aud_stream_t *as)
{
	cs_unit_t *unitp;

	ASSERT_ASLOCKED(as);
	ATRACE(audio_4231_flushcmd, 'SA  ', as);
	unitp = UNITP(as);

	if (as == as->output_as) {
		unitp->output.cmdptr = NULL;
		audio_4231_clear((aud_dma_list_t *)&dma_played_list, unitp);
	} else {
		unitp->input.cmdptr = NULL;
		audio_4231_clear((aud_dma_list_t *)&dma_recorded_list, unitp);
	}
}

/*
 * Initialize the audio chip to a known good state.
 * called with UNITP LOCKED
 */
static void
audio_4231_chipinit(cs_unit_t *unitp)
{
	struct aud_4231_chip *chip;
	ddi_acc_handle_t handle;
	uchar_t	tmpval;

	chip = unitp->chip;
	handle = CS4231_HANDLE;

	ASSERT(chip != NULL);
	ASSERT(handle != NULL);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_CINT, ("4231_chipinit: unitp 0x%x\n",
	    unitp));

	AUD_DMA_RESET(unitp, handle);

	OR_SET_BYTE_R(handle, &chip->pioregs.iar, (uchar_t)IAR_MCE);
	drv_usecwait(100); /* chip bug workaround */
	audio_4231_pollready();
	drv_usecwait(1000);	/* chip bug */

	/*
	 * activate registers 16 - 31
	 * initialize encoding to 8-bit u-law for both playback and record
	 */
	REG_SELECT(IAR_MCE | MISC_IR);
	ddi_putb(handle, CS4231_IDR, MISC_IR_MODE2);

/* XXXMERGE - can replace with codec_model? */
	/* check for older version of this chip - affects pollready() */
	REG_SELECT(IAR_MCE | VERSION_R);
	tmpval = ddi_getb(handle, CS4231_IDR);
	if (tmpval & CS4231A)
		CS4231_reva = B_TRUE;
	else
		CS4231_reva = B_FALSE;

	/* get rid of that annoying popping! */
	audio_4231_mute_channel(unitp, L_OUTPUT_CR);
	audio_4231_mute_channel(unitp, R_OUTPUT_CR);

	/* initialize aux channels to known attenuation values */
	REG_SELECT(L_AUX1_INPUT_CR);
	ddi_putb(handle, CS4231_IDR, AUX_INIT_VALUE);
	REG_SELECT(R_AUX1_INPUT_CR);
	ddi_putb(handle, CS4231_IDR, AUX_INIT_VALUE);
	REG_SELECT(L_AUX2_INPUT_CR);
	ddi_putb(handle, CS4231_IDR, AUX_INIT_VALUE);
	REG_SELECT(R_AUX2_INPUT_CR);
	ddi_putb(handle, CS4231_IDR, AUX_INIT_VALUE);

	REG_SELECT(IAR_MCE | PLAY_DATA_FR);
	ddi_putb(handle, CS4231_IDR, DEFAULT_DATA_FMAT);

	REG_SELECT(IAR_MCE | CAPTURE_DFR);
	ddi_putb(handle, CS4231_IDR, DEFAULT_DATA_FMAT);

	audio_4231_pollready();

	/* Turn on the Output Level Bit to be 2.8 Vpp */
	REG_SELECT(IAR_MCE | ALT_FEA_EN1R);
	ddi_putb(handle, CS4231_IDR, (uchar_t)(OLB_ENABLE | DACZ_ON));

	/* Turn on the hi pass filter */
	REG_SELECT(IAR_MCE | ALT_FEA_EN2R);
	if (CS4231_reva)
		ddi_putb(handle, CS4231_IDR, (HPF_ON | XTALE_ON));
	else
		ddi_putb(handle, CS4231_IDR, HPF_ON);

	/* Clear the playback and capture interrupt flags */
	REG_SELECT(IAR_MCE | ALT_FEAT_STATR);
	ddi_putb(handle, CS4231_IDR, RESET_STATUS);

	/* re-enable mono input (internal speaker) */
	audio_4231_unmute_channel(unitp, L_OUTPUT_CR);
	audio_4231_unmute_channel(unitp, R_OUTPUT_CR);

	/* Init the play and Record gain registers */
	unitp->output.as.info.gain = audio_4231_play_gain(unitp,
	    AUD_CS4231_DEFAULT_PLAYGAIN, AUDIO_MID_BALANCE);
	unitp->input.as.info.gain = audio_4231_record_gain(unitp,
	    AUD_CS4231_DEFAULT_RECGAIN, AUDIO_MID_BALANCE);
	unitp->input.as.info.port = audio_4231_inport(unitp, AUDIO_MICROPHONE);
	unitp->output.as.info.port = audio_4231_outport(unitp, AUDIO_SPEAKER);
	unitp->distate.monitor_gain = audio_4231_monitor_gain(unitp, LOOPB_OFF);

	ddi_putb(handle, &chip->pioregs.iar, IAR_MCD);
	audio_4231_pollready();

	/*
	 * leave the auto-calibrate enabled to prevent
	 * floating test bit 55 from causing errors
	 */
	if (!audio_4231_acal) {
		REG_SELECT(IAR_MCE | INTERFACE_CR);

#if defined(sparc) || defined(__sparc)
		AND_SET_BYTE_R(handle, CS4231_IDR, ACAL_DISABLE);
#elif defined(i386) || defined(__ppc)
		AND_SET_BYTE_R(handle, CS4231_IDR, NO_CALIBRATION);
#else
#error One of sparc, i386 or ppc must be defined.
#endif
		ddi_putb(handle, &chip->pioregs.iar, IAR_MCD);
		audio_4231_pollready();
	}

	unitp->distate.output_muted = audio_4231_output_muted(unitp, 0x0);
	unitp->hw_output_inited = B_TRUE;
	unitp->hw_input_inited = B_TRUE;
	/*
	 * Let the chip settle down before we continue. If we
	 * don't the dac's in the 4231 are left at a high DC
	 * offset. This causes a "pop" on the first record
	 */
	drv_usecwait(160000);
}

static uint_t
audio_4231_output_muted(cs_unit_t *unitp, uint_t mute_on)
{

	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	/*
	 * Just do the mute on Index 6 & 7 R&L output.
	 */
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_HWOP, ("4231_output_muted: mute %d\n",
	    mute_on));

	if (mute_on) {
		REG_SELECT(L_OUTPUT_CR);
		OR_SET_BYTE_R(handle, CS4231_IDR, (uchar_t)OUTCR_MUTE);

		REG_SELECT(R_OUTPUT_CR);
		OR_SET_BYTE_R(handle, CS4231_IDR, (uchar_t)OUTCR_MUTE);
		unitp->distate.output_muted = B_TRUE;
	} else {
		REG_SELECT(L_OUTPUT_CR);
		AND_SET_BYTE_R(handle, CS4231_IDR, (uchar_t)OUTCR_UNMUTE);

		REG_SELECT(R_OUTPUT_CR);
		AND_SET_BYTE_R(handle, CS4231_IDR, (uchar_t)OUTCR_UNMUTE);
		unitp->distate.output_muted = B_FALSE;
	}

	return (unitp->distate.output_muted);

}

static uint_t
audio_4231_inport(cs_unit_t *unitp, uint_t val)
{

	uint_t ret_val;
	ddi_acc_handle_t handle;
	uchar_t tmpval;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_HWOP, ("4231_inport: val %d\n", val));

	/*
	 * In the CS423x we cannot have line, MIC or CDROM_IN enabled at
	 * the same time.  The line, mic, and AUX1 inputs are routed
	 * into an input mixer, so they may be muted at the mixer stage.
	 * However, the AUX2 inputs bypass the input mixer, so they
	 * must be explicitly muted.
	 */
	ret_val = 0;
	audio_4231_mute_channel(unitp, L_AUX2_INPUT_CR);
	audio_4231_mute_channel(unitp, R_AUX2_INPUT_CR);

	if ((val & AUDIO_INTERNAL_CD_IN) &&
	    (unitp->cd_input_line != NO_INTERNAL_CD)) {
		switch (unitp->cd_input_line) {

		case INTERNAL_CD_ON_AUX1:
			REG_SELECT(L_INPUT_CR);
			tmpval = ddi_getb(handle, CS4231_IDR);
			ddi_putb(handle, CS4231_IDR, CDROM_ENABLE(tmpval));

			REG_SELECT(R_INPUT_CR);
			tmpval = ddi_getb(handle, CS4231_IDR);
			ddi_putb(handle, CS4231_IDR, CDROM_ENABLE(tmpval));
			break;

		case INTERNAL_CD_ON_AUX2:
			REG_SELECT(L_INPUT_CR);
			tmpval = ddi_getb(handle, CS4231_IDR);
			ddi_putb(handle, CS4231_IDR, OUTPUTLOOP_ENABLE(tmpval));

			REG_SELECT(R_INPUT_CR);
			tmpval = ddi_getb(handle, CS4231_IDR);
			ddi_putb(handle, CS4231_IDR, OUTPUTLOOP_ENABLE(tmpval));

			audio_4231_unmute_channel(unitp, L_AUX2_INPUT_CR);
			audio_4231_unmute_channel(unitp, R_AUX2_INPUT_CR);
			AUD_ERRPRINT(AUD_EP_L1, AUD_EM_HWOP, ("4231_inport: "
			    "AUX2 inputs enabled\n"));
			break;

		default:
			break;
		}	/* cd_input_line switch */

		ret_val = AUDIO_INTERNAL_CD_IN;
	}
	if (val & AUDIO_LINE_IN) {

		REG_SELECT(L_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, LINE_ENABLE(tmpval));

		REG_SELECT(R_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, LINE_ENABLE(tmpval));

		ret_val = AUDIO_LINE_IN;

	} else if (val & AUDIO_MICROPHONE) {

		REG_SELECT(L_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, MIC_ENABLE(tmpval));

		REG_SELECT(R_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, MIC_ENABLE(tmpval));

		ret_val = AUDIO_MICROPHONE;
	}
#if defined(AUX1_LOOPBACK_TEST)
	/*
	 * SPARC-specific value used for factory loopback testing.
	 */
	if ((val & AUX1_LOOPBACK_TEST) &&
	    (unitp->cd_input_line == NO_INTERNAL_CD)) {

		REG_SELECT(L_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, CDROM_ENABLE(tmpval));

		REG_SELECT(R_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, CDROM_ENABLE(tmpval));

		ret_val = AUX1_LOOPBACK_TEST;

	} else if (val & CODEC_ANALOG_LOOPBACK) {

		REG_SELECT(L_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, OUTPUTLOOP_ENABLE(tmpval));

		REG_SELECT(R_INPUT_CR);
		tmpval = ddi_getb(handle, CS4231_IDR);
		ddi_putb(handle, CS4231_IDR, OUTPUTLOOP_ENABLE(tmpval));

		ret_val = CODEC_ANALOG_LOOPBACK;
	}
#endif			/* defined(AUX1_LOOPBACK_TEST) */

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_HWOP, ("4231_inport: (%d)\n", ret_val));
	return (ret_val);
}
/*
 * Must be called with UNITP lock held.
 */
static uint_t
audio_4231_outport(cs_unit_t *unitp, uint_t val)
{
	uint_t ret_val;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_HWOP, ("4231_outport: val %d\n", val));

	/*
	 *  Disable everything then selectively enable it.
	 */

	ret_val = 0;
	REG_SELECT(MONO_IOCR);
	OR_SET_BYTE_R(handle, CS4231_IDR, (uchar_t)MONO_SPKR_MUTE);

	/*
	 * On PowerPC platforms seen so far, we cannot selectively disable
	 * just the headphone output; the stereo line outputs are routed
	 * to all output lines in parallel, and the other output options
	 * do not have individual mute controls.
	 */
#if defined(sparc) || defined(__sparc)
	REG_SELECT(PIN_CR);
	OR_SET_BYTE_R(handle, CS4231_IDR, (PINCR_LINE_MUTE | PINCR_HDPH_MUTE));

	if (val & AUDIO_SPEAKER) {
		REG_SELECT(MONO_IOCR);
		AND_SET_BYTE_R(handle, CS4231_IDR, ~MONO_SPKR_MUTE);
		ret_val |= AUDIO_SPEAKER;
	}
	if (val & AUDIO_HEADPHONE) {
		REG_SELECT(PIN_CR);
		AND_SET_BYTE_R(handle, CS4231_IDR, ~PINCR_HDPH_MUTE);
		ret_val |= AUDIO_HEADPHONE;
	}

	if (val & AUDIO_LINE_OUT) {
		REG_SELECT(PIN_CR);
		AND_SET_BYTE_R(handle, CS4231_IDR, ~PINCR_LINE_MUTE);
		ret_val |= AUDIO_LINE_OUT;
	}
#elif defined(__ppc)
	REG_SELECT(MONO_IOCR);
	OR_SET_BYTE_R(handle, CS4231_IDR, MONO_SPKR_MUTE);

	if (val & AUDIO_SPEAKER) {
		REG_SELECT(MONO_IOCR);
		AND_SET_BYTE_R(handle, CS4231_IDR, ~MONO_SPKR_MUTE);
		ret_val |= AUDIO_SPEAKER;
	}

	/*
	 * This is completely implementation-specific; on the Sandalfoot,
	 * there is no way to separately enable the headphones and line
	 * outputs, because they are internally connected.
	 */
	if (val & AUDIO_HEADPHONE || val & AUDIO_LINE_OUT) {
/*
 * XXXPPC - this'll probably have to be tested on each platform!
 */
		ret_val |= AUDIO_HEADPHONE | AUDIO_LINE_OUT;

	}

#elif defined(i386)
#else
#error One of sparc, i386 or ppc must be defined.
#endif

	return (ret_val);
}

static uint_t
audio_4231_monitor_gain(cs_unit_t *unitp, uint_t val)
{
	int aten;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_HWOP, ("4231_monitor_gain: val %d\n",
	    val));

	aten = AUD_CS4231_MON_MAX_ATEN -
	    (val * (AUD_CS4231_MON_MAX_ATEN + 1) / (AUDIO_MAX_GAIN + 1));

	/*
	 * Normal monitor registers are the index 13. Line monitor for
	 * now can be registers 18 and 19. Which are actually MIX to
	 * OUT directly We don't use these for now 8/3/93.
	 */

	REG_SELECT(LOOPB_CR);
	if (aten >= AUD_CS4231_MON_MAX_ATEN) {
		ddi_putb(handle, CS4231_IDR, LOOPB_OFF);
	} else {

		/*
		 * Loop Back enable
		 * is in bit 0, 1 is reserved, thus the shift 2.
		 * all other aten and gains are in the low order
		 * bits, this one has to be differnt and be in the
		 * high order bits sigh...
		 */
		ddi_putb(handle, CS4231_IDR, ((aten << 2) | LOOPB_ON));
	}

	/*
	 * We end up returning a value slightly different than the one
	 * passed in - *most* applications expect this.
	 */
	return ((val == AUDIO_MAX_GAIN) ? AUDIO_MAX_GAIN :
	    ((AUD_CS4231_MAX_DEV_ATEN - aten) * (AUDIO_MAX_GAIN + 1) /
	    (AUD_CS4231_MAX_DEV_ATEN + 1)));
}

/*
 * Convert play gain to chip values and load them.
 * Return the closest appropriate gain value.
 * Must be called with UNITP lock held.
 */
static uint_t
audio_4231_play_gain(cs_unit_t *unitp, uint_t val, uchar_t balance)
{

	uint_t tmp_val, r, l;
	uint_t la, ra;
	u_char old_gain;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_HWOP,
	    ("4231_play_gain: val %d balance %d\n", val, balance));

	r = l = val;
	if (balance < AUDIO_MID_BALANCE) {
		r = MAX(0, (int)(val -
		    ((AUDIO_MID_BALANCE - balance) << AUDIO_BALANCE_SHIFT)));
	} else if (balance > AUDIO_MID_BALANCE) {
		l = MAX(0, (int)(val -
		    ((balance - AUDIO_MID_BALANCE) << AUDIO_BALANCE_SHIFT)));
	}

	if (l == 0) {
		la = AUD_CS4231_MAX_DEV_ATEN;
	} else {
		la = AUD_CS4231_MAX_ATEN -
		    (l * (AUD_CS4231_MAX_ATEN + 1) / (AUDIO_MAX_GAIN + 1));
	}
	if (r == 0) {
		ra = AUD_CS4231_MAX_DEV_ATEN;
	} else {
		ra = AUD_CS4231_MAX_ATEN -
		    (r * (AUD_CS4231_MAX_ATEN + 1) / (AUDIO_MAX_GAIN + 1));
	}

	/* Load output gain registers */

	REG_SELECT(L_OUTPUT_CR);
	old_gain = ddi_getb(handle, CS4231_IDR);
	ddi_putb(handle, CS4231_IDR, GAIN_SET(old_gain, la));
	REG_SELECT(R_OUTPUT_CR);
	old_gain = ddi_getb(handle, CS4231_IDR);
	ddi_putb(handle, CS4231_IDR, GAIN_SET(old_gain, ra));

	if ((val == 0) || (val == AUDIO_MAX_GAIN)) {
		tmp_val = val;
	} else {
		if (l == val) {
			tmp_val = ((AUD_CS4231_MAX_ATEN - la) *
			    (AUDIO_MAX_GAIN + 1) / (AUD_CS4231_MAX_ATEN + 1));
		} else if (r == val) {
			tmp_val = ((AUD_CS4231_MAX_ATEN - ra) *
			    (AUDIO_MAX_GAIN + 1) / (AUD_CS4231_MAX_ATEN + 1));
		}
	}
	return (tmp_val);
}


/*
 * Convert record gain to chip values and load them.
 * Return the closest appropriate gain value.
 * Must be called with UNITP lock held.
 */
uint_t
audio_4231_record_gain(cs_unit_t *unitp, uint_t val, uchar_t balance)
{
	uint_t tmp_val, r, l;
	uint_t lg, rg;
	ddi_acc_handle_t handle;
	uchar_t tmpval;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_HWOP,
	    ("4231_record_gain: val %d balance %d\n", val, balance));

	r = l = val;
	tmp_val = 0;

	if (balance < AUDIO_MID_BALANCE) {
		r = MAX(0, (int)(val -
		    ((AUDIO_MID_BALANCE - balance) << AUDIO_BALANCE_SHIFT)));
	} else if (balance > AUDIO_MID_BALANCE) {
		l = MAX(0, (int)(val -
		    ((balance - AUDIO_MID_BALANCE) << AUDIO_BALANCE_SHIFT)));
	}
	lg = l * (AUD_CS4231_MAX_GAIN + 1) / (AUDIO_MAX_GAIN + 1);
	rg = r * (AUD_CS4231_MAX_GAIN + 1) / (AUDIO_MAX_GAIN + 1);

	/* Load input gain registers */
	REG_SELECT(L_INPUT_CR);
	tmpval = ddi_getb(handle, CS4231_IDR);
	ddi_putb(handle, CS4231_IDR, RECGAIN_SET(tmpval, lg));
	REG_SELECT(R_INPUT_CR);
	tmpval = ddi_getb(handle, CS4231_IDR);
	ddi_putb(handle, CS4231_IDR, RECGAIN_SET(tmpval, rg));

	/*
	 * We end up returning a value slightly different than the one
	 * passed in - *most* applications expect this.
	 */
	if (l == val) {
		if (l == 0) {
			tmp_val = 0;
		} else {
			tmp_val = ((lg + 1) * AUDIO_MAX_GAIN) /
			    (AUD_CS4231_MAX_GAIN + 1);
		}
	} else if (r == val) {
		if (r == 0) {
			tmp_val = 0;
		} else {

			tmp_val = ((rg + 1) * AUDIO_MAX_GAIN) /
			    (AUD_CS4231_MAX_GAIN + 1);
		}
	}

	return (tmp_val);


}


uint_t
audio_4231_playintr(cs_unit_t *unitp)
{
	aud_cmd_t *cmdp;
	u_int length;
	uint32_t ltmpval = 0;
	uint32_t eb2intr;
	uint_t retval = B_TRUE;
	int lastcount = 0;
	int need_processing = 0;
	ddi_acc_handle_t handle;
	int jumpflag = 0;


	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY, ("4231_playintr: unitp 0x%x\n",
	    unitp));

	lastcount = (unitp->playcount % DMA_LIST_SIZE);
	unitp->samecmdp = 0;
	if (lastcount > 0)
		lastcount--;
	else
		lastcount = DMA_LIST_SIZE - 1;
	ATRACE(audio_4231_playintr, 'tsal', lastcount);
	ATRACE(audio_4231_playintr, 'yalp', unitp->playcount);

	handle = AUD_DMA_GET_ACCHANDLE(unitp, &eb2intr, PLAYBACK);

DONEXT:
	cmdp = unitp->output.cmdptr;
	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY, ("4231_playintr: cmdp 0x%x\n",
	    cmdp));

	if (cmdp == NULL) {
		if (unitp->eb2dma) {
			/*
			 * we only want to call clear and unmap the
			 * dma if the BCR is == 0, this indicates
			 * that all of the dma has gone to the CODEC
			 * of we clear before the BCR == 0, then we
			 * get errors from the DMA engine.
			 */
			audio_4231_samplecalc(unitp,
				unitp->typ_playlength, PLAYBACK);
			if (ddi_get32(unitp->cnf_handle_eb2play,
					    (uint32_t *)EB2_PLAY_BCR) == 0) {
				audio_4231_clear((aud_dma_list_t *)
					&dma_played_list, unitp);
			}
			unitp->output.active = B_FALSE;
			unitp->output.cmdptr = NULL;
			audio_process_output(&unitp->output.as);
		}
		unitp->output.active = B_FALSE;
		ATRACE(audio_4231_playintr, 'lluN', cmdp);
		AUD_ERRPRINT(AUD_EP_L0, AUD_EM_PLAY, ("4231_playintr: Null\n"));
		goto done;
	}

	if (cmdp == dma_played_list[lastcount].cmdp) {
		unitp->samecmdp++;
		ATRACE(audio_4231_playintr, 'emas', cmdp);
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY, ("4231_playintr: same\n"));

		if (cmdp->next != NULL) {
			cmdp = cmdp->next;
			unitp->output.cmdptr = cmdp;
			unitp->output.active = B_TRUE;
			ATRACE(audio_4231_playintr, 'dmcn', cmdp);
			AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY, ("4231_playintr: "
			    "next cmdp 0x%x\n", cmdp));
		} else {
			/*
			 * if the fifos have drained and there are no
			 * cmd buffers left to process, then clean up
			 * dma resources
			 */
			ltmpval = AUD_DMA_PLAY_LAST(unitp, handle, eb2intr);

			if (!ltmpval) {
				ATRACE(audio_4231_playintr, 'dmcL', cmdp);
				AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY,
				    ("4231_playintr: Lcmdp: 0x%x\n", cmdp));
				audio_4231_samplecalc(unitp,
					unitp->typ_playlength, PLAYBACK);
				audio_4231_clear((aud_dma_list_t *)
					&dma_played_list, unitp);
				unitp->output.error = B_TRUE;
				unitp->output.active = B_FALSE;
				unitp->output.cmdptr = NULL;
				audio_process_output(&unitp->output.as);
			} else {
				/*
				 * if the fifo's are not empty, then wait for
				 * the next interrupt to clean up dma resources
				 */

				/*
				 * if the fifo's are not empty, then wait for
				 * the next interrupt to clean up dma resources
				 * If there is no interrupt pending in
				 * the csr then we must be in a
				 * *prime* condition for the dma engine.
				 * in theis case we don't want to mark
				 * the active flag as FALSE because we
				 * are technically active.
				 */

				if (!unitp->eb2dma) {
					if (ddi_get32(handle,
				(uint32_t *)&unitp->chip->dmaregs.dmacsr) &
						APC_PI) {
						unitp->output.active = B_FALSE;
						unitp->output.error = B_TRUE;
					} else {
						unitp->output.active = B_TRUE;
					}
				}
				ATRACE(audio_4231_playintr, 'dmcN', cmdp);
				AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY,
				    ("4231_playintr: fifo not empty...\n"));
			}
			AUD_ERRPRINT(AUD_EP_L1, AUD_EM_PLAY, ("4231_playintr: "
			    "output.error SET\n"));
			goto done;
		}
	}

	if (unitp->output.active) {
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_PLAY, ("4231_playintr: "
		    "output.error RESET\n"));
		unitp->output.error = B_FALSE;
		ATRACE(audio_4231_playintr, ' DMC', unitp->output.cmdptr);
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY,
		    ("4231_playintr: out active cmdp 0x%x\n", cmdp));

		/*
		 * Ignore null and non-data buffers
		 */
		while (cmdp != NULL && (cmdp->skip || cmdp->done)) {
			cmdp->done = B_TRUE;
			need_processing++;
			cmdp = cmdp->next;
			unitp->output.cmdptr = cmdp;
			ATRACE(audio_4231_playintr, 'DMCS',
				    unitp->output.cmdptr);
			AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY,
			    ("4231_playintr: skip,done cmdp 0x%x\n", cmdp));
		}

		/*
		 * if no cmds to process and the fifo's have
		 * drained and no more dma in progress, then
		 * clean up resources
		 */
		ltmpval = AUD_DMA_PLAY_CLEANUP(unitp, handle, eb2intr);

		if (!cmdp && !ltmpval && need_processing) {
			ATRACE(audio_4231_playintr, 'cmdL', cmdp);
			audio_4231_samplecalc(unitp,
				unitp->typ_playlength, PLAYBACK);
/* XXXMERGE */
			if (unitp->eb2dma &&
			    (ddi_get32(unitp->cnf_handle_eb2play,
					    (uint32_t *)EB2_PLAY_BCR)) == 0) {
				audio_4231_clear((aud_dma_list_t *)
					&dma_played_list, unitp);
			} else if (!unitp->eb2dma) {
				audio_4231_clear((aud_dma_list_t *)
					&dma_played_list, unitp);
			}
			AUD_ERRPRINT(AUD_EP_L1, AUD_EM_PLAY, ("4231_playintr: "
			    "cleanup - output.error SET\n"));
			unitp->output.error = B_TRUE;
			unitp->output.active = B_FALSE;
			unitp->output.cmdptr = NULL;
			goto done;
		}
		/*
		 * Check for flow error EOF??
		 */
		if (cmdp == NULL) {		/* Flow error condition */
			AUD_ERRPRINT(AUD_EP_L1, AUD_EM_PLAY, ("4231_playintr: "
			    "flow error, output.error SET\n"));
			unitp->output.error = B_TRUE;
			unitp->output.active = B_FALSE;
			retval = B_FALSE;
			ATRACE(audio_4231_playintr, 'LLUN', cmdp);
			goto done;
		}

		if (unitp->output.cmdptr->skip ||
			    unitp->output.cmdptr->done) {
			AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY, ("4231_playintr: "
			    "output skip,done cmdp 0x%x\n", cmdp));
			ATRACE(audio_4231_playintr, 'piks', cmdp);
			need_processing++;
			goto done;
		}
		/*
		 * Transfer play data
		 */
		/*
		 * Setup for DMA transfers to the device from the buffer
		 */

		length = cmdp->enddata - cmdp->data;
#ifdef MULTI_DEBUG
		if (cmdp->data == NULL || length == NULL) {
			cmdp->skip = B_TRUE;
			need_processing++;
			goto done;
		}
#else
		if (cmdp->data == NULL || length == NULL ||
			    length > AUD_CS4231_BSIZE) {
			cmdp->skip = B_TRUE;
			need_processing++;
			goto done;
		}
#endif

		if (AUD_DMA_SETUP(unitp, PLAYBACK, cmdp, length) == B_FALSE)
			return (B_FALSE);

		if (DMA_PB_HANDLE) {
EB2JUMP:
			AUD_DMA_PLAY_NEXT(unitp, handle);

			if (jumpflag) {
				ATRACE(audio_4231_playintr, 'JUMP',
				    unitp->output.cmdptr);
				return (retval);
			}

			audio_4231_insert(cmdp, DMA_PB_HANDLE,
			    unitp->playcount,
			    (aud_dma_list_t *)&dma_played_list, unitp);
			unitp->playcount++;
			need_processing++;	/* needed XXXX ? */
			retval = B_TRUE;

			/*
			 * For the ebus dma because of chaining and
			 * end of play we don't want to grab the
			 * next cmd if we were done at the beginning
			 * of the isr. We also use the samecmd flag
			 * indicating that we were done when we came in
			 * here. If we set the unitp pointer to the
			 * next cmd we end up programming the engine
			 * with a bogus dma address and get a PCI target
			 * abort and or a channel engine error.
			 * XXX this may also apply to ppc and APC dma.
			 * But those 2 routines don't cause a jump in
			 * this isr logic.
			 */
			if (cmdp->next != NULL && !unitp->eb2dma) {
				unitp->output.cmdptr = cmdp->next;
			}

			ATRACE(audio_4231_playintr, 'DMCN',
			    unitp->output.cmdptr);
			unitp->typ_playlength = length;

			/*
			 * If we have written a valid cmd buf and
			 * at the time of writing that cmd buf to the
			 * eb2 engine we had underflowed, (indicated
			 * by the EB2_DMA_ON == 0) we need to try and
			 * write the next address again to re-enable
			 * chaining in the engine. Else we will
			 * proceed to run in non-chained mode.
			 * XXX this may not be the optimal way to
			 * do this.
			 */
			if (unitp->eb2dma && unitp->samecmdp == 0) {
				eb2intr = ddi_get32(unitp->cnf_handle_eb2play,
						(uint32_t *)EB2_PLAY_CSR);
				if (!(eb2intr & EB2_NADDR_LOADED) &&
				    (eb2intr & EB2_EN_NEXT)) {
					ATRACE(audio_4231_playintr, 'DNXT',
						jumpflag);
					jumpflag = 0;
					audio_4231_remove(unitp->playcount,
					    (aud_dma_list_t *)&dma_played_list);
					goto DONEXT;
				}
			}
		} else {
			cmn_err(CE_WARN, "audiocs: NULL DMA handle!");
		}

	} /* END OF PLAY */

	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_PLAY, ("4231_playintr: exit cmdp 0x%x\n",
	    cmdp));
done:

	audio_4231_remove(unitp->playcount, (aud_dma_list_t *)&dma_played_list);
	if (need_processing) {
		audio_gc_output(&unitp->output.as);
	}
	return (retval);
}

void
audio_4231_recintr(cs_unit_t *unitp)
{
	aud_cmd_t *cmdp;
	cs_stream_t *ds;
	ddi_acc_handle_t handle;
#if defined(sparc) || defined(__sparc)
	ddi_acc_handle_t rechandle;
#endif					/* defined(sparc) */
	struct aud_4231_chip *chip;
	u_int length;
	uint32_t ltmpval = 0;
	int int_active = 0;
	int lastcount = 0;
	uint32_t eb2intr;
	int jumpflag = 0;

#define	Interrupt	1
#define	Active		2

#if defined(sparc) || defined(__sparc)
	rechandle = AUD_DMA_GET_ACCHANDLE(unitp, &eb2intr, RECORD);
#endif					/* defined(sparc) */

	ds = &unitp->input;		/* Get cs stream pointer */

	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: unitp 0x%x\n",
	    unitp));

	handle = CS4231_HANDLE;
	chip = unitp->chip;

DONEXT:
	ATRACE(audio_4231_recintr, 'LOCk', &unitp->input);

	/* General end of record condition */
	if (ds->active != B_TRUE) {
		ATRACE(audio_4231_recintr, 'RREA', &unitp->input);
		int_active |= Interrupt;
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
		    "int_active %d\n", int_active));
		goto done;
	}

	lastcount = (unitp->recordcount % DMA_LIST_SIZE);
	if (lastcount > 0)
		lastcount--;
	else
		lastcount = DMA_LIST_SIZE - 1;
	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
	    "recordcount %d   lastcount %d\n", unitp->recordcount, lastcount));
	ATRACE(audio_4231_recintr, 'tsal', lastcount);
	ATRACE(audio_4231_recintr, ' cer', unitp->recordcount);

	cmdp = unitp->input.cmdptr;
	if (cmdp == NULL) {			/* We're Done */
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD, ("4231_recintr: "
		    "EOR, input.error SET\n"));
		unitp->input.error = B_TRUE;
		unitp->input.active = B_FALSE;
		unitp->input.cmdptr = NULL;
		int_active |= Interrupt;
		ATRACE(audio_4231_recintr, 'LLUN', cmdp);
		goto done;
	}

	if (cmdp == dma_recorded_list[lastcount].cmdp) {
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
		    "same cmdp %x\n", cmdp));
		ATRACE(audio_4231_recintr, 'emas', cmdp);
		int_active |= Interrupt;
		if (cmdp->next != NULL) {
			cmdp = cmdp->next;
			unitp->input.cmdptr = cmdp;
			unitp->input.active = B_TRUE;
			ATRACE(audio_4231_recintr, 'dmcn', cmdp);
			AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
			    "next cmdp %x\n", cmdp));
		} else {
			/*
			 * if the fifos have drained and there are no
			 * cmd buffers left to process, then clean up
			 * dma resources and shut down the codec
			 */

#if defined(sparc) || defined(__sparc)
			if (!unitp->eb2dma) {
				if (!ddi_get32(rechandle,
				    (uint32_t *)&unitp->chip->dmaregs.dmacc)) {
					ATRACE(audio_4231_recintr, 'dmcL',
					    cmdp);
					unitp->input.error = B_TRUE;
					unitp->input.active = B_FALSE;
					unitp->input.cmdptr = NULL;
					goto done;
				} else {
					/*
					 * if the fifo's are not empty, then
					 * wait for the next interrupt to clean
					 * up dma resources
					 */
					ATRACE(audio_4231_recintr, 'wolf',
					    cmdp);
					return;
				}
			} else {
				/*
				 * This is the Eb2 DMA engine's end of record
				 * condition.  goto done with int_active
				 * set to not active.
				 */

				int_active = Interrupt;
				unitp->input.active = B_FALSE;
				unitp->input.error = B_TRUE;
				unitp->input.cmdptr = NULL;
				goto done;
			}
#elif defined(i386) || defined(__ppc)

/* test PPC case fornow..... */
			AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
			    "last cmdp? %x input.error SET\n", cmdp));
			int_active = Interrupt;
			unitp->input.active = B_FALSE;
			unitp->input.error = B_TRUE;
			unitp->input.cmdptr = NULL;
			goto done;
#else
#error One of sparc, i386 or ppc must be defined.
#endif
		}
	}

	if (unitp->input.active) {
		cmdp = unitp->input.cmdptr;

		/*
		 * Ignore null and non-data buffers
		 */
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
		    "cmdp 0x%x\tcmdp->skip 0x%x\tcmdp->done 0x%x\tcmdp->next "
		    "0x%x\n", cmdp, cmdp->skip, cmdp->done, cmdp->next));

		while (cmdp != NULL && (cmdp->skip || cmdp->done)) {
			cmdp->done = B_TRUE;
			cmdp = cmdp->next;


			/*
			 * if no commands available and the byte count is
			 * not 0, then just wait for another interrupt
			 */
			AUD_DMA_GET_COUNT(unitp, 0, &ltmpval);
			AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
			    "8237 pending transfer count %x\n", ltmpval));
			if (!cmdp && ltmpval) {
				ATRACE(audio_4231_recintr, 'WOLF',
					unitp->input.cmdptr);
				return;
			}
			unitp->input.cmdptr = cmdp;
		}

		/*
		 * Check for flow error EOF??
		 */
		if (cmdp == NULL) {		/* Flow error condition */
			AUD_ERRPRINT(AUD_EP_L1, AUD_EM_RCRD, ("4231_recintr: "
			    "flow error, input.error SET\n"));
			unitp->input.error = B_TRUE;
			unitp->input.active = B_FALSE;
			unitp->input.cmdptr = NULL;
			int_active |= Interrupt;
			ATRACE(audio_4231_recintr, 'LLUN', cmdp);
			AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: "
			    "null cmdp\n"));
			goto done;
		}

		/*
		 * Setup for DMA transfers to the buffer from the device
		 */
		length = cmdp->enddata - cmdp->data;
		AUD_ERRPRINT(AUD_EP_L2, AUD_EM_RCRD, ("4231_recintr: length "
		    "%d\tcmdp 0x%x\n", length, cmdp));

		if (cmdp->data == NULL || length == NULL ||
		    (length > MAX(AUD_CS4231_BSIZE, ds->as.info.buffer_size))) {
			AUD_ERRPRINT(AUD_EP_L2, AUD_EM_RCRD, ("4231_recintr: "
			    "no data recorded.\n"));
			cmdp->skip = B_TRUE;
			goto done;
		}

		(void) AUD_DMA_SETUP(unitp, RECORD, cmdp, length);

		if (DMA_RC_HANDLE) {
EB2JUMP:
			AUD_DMA_REC_NEXT(unitp);

			if (jumpflag && unitp->eb2dma) {
				return;
			}
			audio_4231_insert(cmdp, DMA_RC_HANDLE,
			    unitp->recordcount,
			    (aud_dma_list_t *)&dma_recorded_list, unitp);
			if (unitp->recordcount < 1)
				cmdp->data = cmdp->enddata;
			unitp->recordcount++;
			int_active |= Active;
			if (cmdp->next != NULL) {
				unitp->input.cmdptr = cmdp->next;
				AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD,
				    ("4231_recintr: updating input cmdptr %x\n",
				    cmdp->next));
			} else {
				cmdp->skip = B_TRUE;
			}

			/*
			 * If we have written a valid cmd buf and
			 * at the time of writing that cmd buf to the
			 * eb2 engine we had underflowed, (indicated
			 * by the EB2_DMA_ON == 0) we need to try and
			 * write the next address again to re-enable
			 * chaining in the engine. Else we will
			 * proceed to run in non-chained mode.
			 * XXX this may not be the optimal way to
			 * do this.
			 */
			if (unitp->eb2dma) {
				eb2intr = ddi_get32(unitp->cnf_handle_eb2record,
						(uint32_t *)EB2_REC_CSR);
				if (!(eb2intr & EB2_NADDR_LOADED) &&
				    (eb2intr & EB2_EN_NEXT)) {
						jumpflag = 0;
						goto DONEXT;
				}
			}
		} else {
			cmn_err(CE_WARN, "audiocs: NULL DMA handle!");
		}

	} /* END OF RECORD */


done:
	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: done\n"));

	/*
	 * If no IO is active, shut down device interrupts and
	 * dma from the dma engine.
	 */
	if ((int_active & Active)) {
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: done"
		    " but still active\n"));
		audio_4231_remove(unitp->recordcount,
		    (aud_dma_list_t *)&dma_recorded_list);
		audio_process_input(&unitp->input.as);
	} else {
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: done,"
		    " cleaning up\n"));
		AUD_DMA_REC_CLEANUP(unitp);

		REG_SELECT(INTERFACE_CR);
		AND_SET_BYTE_R(handle, CS4231_IDR, CEN_DISABLE);
		AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recintr: done,"
		    " calling recordend\n"));
		audio_4231_recordend(unitp, dma_recorded_list);
	}

} /* END OF RECORD */

void
audio_4231_initlist(aud_dma_list_t *dma_list, cs_unit_t *unitp)
{
	int i;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_STRT,
	    ("4231_initlist: dma_listp 0x%x\n", dma_list));
	ATRACE(audio_4231_initlist, 'EREH', dma_list);
	for (i = 0; i < DMA_LIST_SIZE; i++) {
#ifdef AUDIOTRACE
		if (dma_list[i].cmdp || dma_list[i].buf_dma_handle)
			ATRACE(audio_4231_initlist, 'LIST', dma_list[i].cmdp);
#endif
		audio_4231_remove(i, dma_list);
	}

	if (dma_list == dma_recorded_list) {
		unitp->recordcount = 0;
	} else {
		unitp->playcount = 0;
	}
}

void
audio_4231_insert(aud_cmd_t *cmdp, ddi_dma_handle_t buf_dma_handle,
		    uint_t count, aud_dma_list_t *dma_list, cs_unit_t *unitp)
{
	count %= DMA_LIST_SIZE;
	ATRACE(audio_4231_insert, ' tnC', count);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_INTR, ("4231_insert: count %d\n",
	    count));

	if (dma_list[count].cmdp || dma_list[count].buf_dma_handle) {
		cmn_err(CE_WARN, "audio_4231_insert: dma_list not cleared!");
		ATRACE(audio_4231_insert, 'futs', count);
	}
	if (dma_list[count].cmdp == (aud_cmd_t *)NULL) {
		ATRACE(audio_4231_insert, 'mdpC', cmdp);
		dma_list[count].cmdp = cmdp;
		dma_list[count].buf_dma_handle = buf_dma_handle;
#ifdef AUDIOTRACE
		if (dma_list == dma_recorded_list) {
			ATRACE(audio_4231_insert, ' pac', dma_list);
			unitp->recordlastent = count;
		} else {
			ATRACE(audio_4231_insert, 'yalp', dma_list);
		}
#else
		if (dma_list == dma_recorded_list) {
			ATRACE(audio_4231_insert, ' pac', dma_list);
			unitp->recordlastent = count;
		}
#endif
	} else {
		cmn_err(CE_WARN, "audiocs: insert dma handle failed!");
	}
}

void
audio_4231_remove(uint_t count, aud_dma_list_t *dma_list)
{

	count = ((count - 3) % DMA_LIST_SIZE);
	ATRACE(audio_4231_remove, ' tnc', count);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_INTR, ("4231_remove: count %d\n",
	    count));

	if (dma_list[count].cmdp != (aud_cmd_t *)NULL) {
		if (dma_list == dma_recorded_list)  {
			dma_list[count].cmdp->data =
			    dma_list[count].cmdp->enddata;
		}
		dma_list[count].cmdp->done = B_TRUE;
		ATRACE(audio_4231_remove, 'dmcr', dma_list[count].cmdp);

#if defined(sparc) || defined(__sparc)
		if (dma_list[count].buf_dma_handle != NULL) {
			ddi_dma_unbind_handle(dma_list[count].buf_dma_handle);
			ddi_dma_free_handle(&dma_list[count].buf_dma_handle);
		} else
			cmn_err(CE_WARN,
				"audio_4231_remove: NULL buf_dma_handle");
#elif defined(i386) || defined(__ppc)
		if (dma_list[count].buf_dma_handle == NULL)
			cmn_err(CE_WARN,
				"audio_4231_remove: NULL buf_dma_handle");

#else
#error One of sparc, i386 or ppc must be defined.
#endif
		dma_list[count].buf_dma_handle = NULL;
		dma_list[count].cmdp = (aud_cmd_t *)NULL;
	}
}

/*
 * Called on a stop condition to free up all of the dma queued
 */
void
audio_4231_clear(aud_dma_list_t *dma_list, cs_unit_t *unitp)
{
	int i;

	ATRACE(audio_4231_clear, 'RLCu', dma_list);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_INTR, ("4231_clear: dma_listp 0x%x\n",
	    dma_list));

	for (i = 3; i < (DMA_LIST_SIZE + 3); i++) {
		audio_4231_remove(i, dma_list);
	}
	if (dma_list == dma_recorded_list) {
		unitp->recordcount = 0;
	} else {
		unitp->playcount = 0;
	}
}

void
audio_4231_pollready()
{
	cs_unit_t *unitp;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	uchar_t iar, idr;
	register uint_t x = 0;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_CINT, ("4231_pollready:\n"));
	/*
	 * The Calibration-completed condition is detected by reading the
	 * Index address register; the value returned will be 0x80 while the
	 * chip is calibrating, and should return 0x00 when calibration is
	 * complete.  However, some of the older revisions of the CS4231
	 * would not immediately go to 0x80, so we must wait a bit
	 * before starting the poll loop to check for completion.
	 */
	drv_usecwait(100);

	/*
	 * Use the timeout routine for the older rev parts as these
	 * codecs may spin upto 15 secs blocking all other threads
	 */
/* XXXMERGE - can replace with codec_model check */
	if (!CS4231_reva) {
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_CINT, ("4231_pollready: "
		    "CS4231-older part\n"));
		audio_4231_timeout();
		return;
	}

	unitp = &cs_units[0];
	handle = CS4231_HANDLE;
	chip = unitp->chip;
	ddi_putb(handle, CS4231_IAR, (uchar_t)IAR_MCD);

	/*
	 * Wait to see if chip is out of mode change enable
	 */
	iar = ddi_getb(handle, CS4231_IAR);

	while (iar == IAR_NOTREADY && x <= CS_TIMEOUT) {

		iar = ddi_getb(handle, CS4231_IAR);
		x++;
	}

	x = 0;

	/*
	 * Wait to see if chip has done the autocalibrate
	 */
	REG_SELECT(TEST_IR);

	idr = ddi_getb(handle, CS4231_IDR);

	while (idr == AUTOCAL_INPROGRESS && x <= CS_TIMEOUT) {

			idr = ddi_getb(handle, CS4231_IDR);
			x++;
	}

	drv_usecwait(1000);
}

void
audio_4231_samplecalc(cs_unit_t *unitp, uint_t dma_len, uint_t direction)
{
	uint_t samples, ncount, ccount = 0;
	uint32_t ltmpval, lntmpval = 0;

	ATRACE(audio_4231_samplecalc, 'HERE', unitp);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_INTR, ("4231_samplecalc: dmalen 0x%x\n",
	    dma_len));

	switch (direction) {
	case RECORD:
		dma_len = audio_4231_sampleconv(&unitp->input, dma_len);
		samples = unitp->input.samples;
		AUD_DMA_GET_COUNT(unitp, (uint32_t *)0, &ltmpval);
		AUD_DMA_GET_NCOUNT(unitp, direction, &lntmpval);

		ncount = audio_4231_sampleconv(&unitp->input, lntmpval);
		ccount = audio_4231_sampleconv(&unitp->input, ltmpval);
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_INTR, ("4231_samplecalc: "
		    "pcount: 0x%x pncount: 0x%x\n", ccount, ncount));
		if (ccount != 0) {
			unitp->input.samples =
			    ((samples - ncount) + (dma_len - ccount));
		}
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_INTR, ("4231_samplecalc: "
		    "RC samples: 0x%x\n", unitp->input.samples));
		break;

	case PLAYBACK:
	default:
		dma_len = audio_4231_sampleconv(&unitp->output, dma_len);
		samples = unitp->output.samples;
		AUD_DMA_GET_COUNT(unitp, &ltmpval, (uint32_t *)0);
		AUD_DMA_GET_NCOUNT(unitp, direction, &lntmpval);

		ncount = audio_4231_sampleconv(&unitp->output, lntmpval);
		ccount = audio_4231_sampleconv(&unitp->output, ltmpval);
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_INTR, ("4231_samplecalc: "
		    "pcount: 0x%x pncount: 0x%x\n", ccount, ncount));
		if (ccount != 0) {
			unitp->output.samples =
			    ((samples - ncount) + (dma_len - ccount));
		}
		AUD_ERRPRINT(AUD_EP_L1, AUD_EM_INTR, ("4231_samplecalc: "
		    "PB samples: 0x%x\n", unitp->output.samples));
	}
}

/*
 * Converts byte counts to sample counts
 */
uint_t
audio_4231_sampleconv(cs_stream_t *stream, uint_t length)
{

	uint_t samples;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_INTR, ("4231_sampleconv: length %d\n",
	    length));

	if (stream->as.info.channels == 2) {
		samples = (length/2);
	} else {
		samples = length;
	}
	if (stream->as.info.encoding == AUDIO_ENCODING_LINEAR) {
			samples = samples/2;
	}

	AUD_ERRPRINT(AUD_EP_L1, AUD_EM_INTR, ("4231_sampleconv: samples %d\n",
	    samples));
	return (samples);
}

/*
 * This routine is used to adjust the ending record cmdp->data
 * since it is set in the intr routine we need to look it up
 * in the circular buffer, mark it as done and adjust the
 * cmdp->data point based on the current count in the capture
 * count. Once this is done call audio_process_input() and
 * also call audio_4231_clear() to free up all of the dma_handles.
 */
void
audio_4231_recordend(cs_unit_t *unitp, aud_dma_list_t *dma_list)
{

	uint32_t count, capcount, recend;
	int i;

	count = (uint32_t)unitp->recordlastent;
	ATRACE(audio_4231_recordend, 'STAL', count);
	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD,
	    ("4231_recordend: dma_listp 0x%x\n", dma_list));

	AUD_DMA_GET_COUNT(unitp, (uint32_t *)0, &capcount);
	AUD_ERRPRINT(AUD_EP_L3, AUD_EM_RCRD, ("4231_recordend: recordlastent "
	    "%d  capcount %x\n", count, capcount));

	recend = capcount;

	if (count > (uint32_t)0x00 && count < DMA_LIST_SIZE)
		count--;
	else if (unitp->recordlastent == unitp->recordcount)
		count = 0;
	else
		count = DMA_LIST_SIZE - 1;

	if (dma_list[count].cmdp != (aud_cmd_t *)NULL) {
		dma_list[count].cmdp->data =
		    (dma_list[count].cmdp->enddata - recend);
		dma_list[count].cmdp->done = B_TRUE;
		if (count != 0) {
			audio_4231_initcmdp(dma_list[count].cmdp,
			    unitp->input.as.info.encoding);
		}
	}


	for (i = 0; i < DMA_LIST_SIZE; i++) {
		if (dma_list[i].cmdp) {
			if (!dma_list[i].buf_dma_handle)
				cmn_err(CE_WARN, "audio_4231_recordend: "
				    "NULL buf_dma_handle");
			/*
			 * mark all freed buffers as done
			 */
			dma_list[i].cmdp->done = B_TRUE;
			ATRACE(audio_4231_recordend, ' tnc', i);
			ATRACE(audio_4231_recordend, 'pdmc', dma_list[i].cmdp);

#if defined(sparc) || defined(__sparc)
			if (dma_list[i].buf_dma_handle != NULL) {
				ddi_dma_unbind_handle(dma_list[i].
				    buf_dma_handle);
				ddi_dma_free_handle(&dma_list[i].
				    buf_dma_handle);
			} else {
				cmn_err(CE_WARN,
				    "audio_4231_remove: NULL buf_dma_handle");
			}
#elif defined(i386) || defined(__ppc)
			if (dma_list[i].buf_dma_handle == NULL)
				cmn_err(CE_WARN,
				    "audio_4231_remove: NULL buf_dma_handle");
#else
#error One of sparc, i386 or ppc must be defined.
#endif
			dma_list[i].cmdp = (aud_cmd_t *)NULL;
			dma_list[i].buf_dma_handle = NULL;
		}
	}

	unitp->recordcount = 0;

	/*
	 * on PowerPC and x86, the DMA buffers are never freed.
	 */
#if defined(sparc) || defined(__sparc)
	DMA_RC_NCOOKIES = 0;
#endif
	/*
	 * look for more buffers to process
	 */
	audio_process_input(&unitp->input.as);
}

/*
 * XXXXX this is a way gross hack to prevent the static from
 * being played at the end of the record. Until the *real*
 * cause can be found this will at least silence the
 * extra data.
 */
void
audio_4231_initcmdp(aud_cmd_t *cmdp, uint_t format)
{

	uint_t zerosample = 0;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_RCRD,
	    ("4231_initcmdp: cmdp 0x%x, format %d\n", cmdp, format));

	switch (format) {
	case AUDIO_ENCODING_ULAW:
		zerosample = 0xff;	/* silence for ulaw format */
		break;
	case AUDIO_ENCODING_ALAW:
		zerosample = 0xd5;	/* zerosample for alaw */
		break;
	case AUDIO_ENCODING_LINEAR:
		zerosample = 0x00;	/* zerosample for linear */
		break;
	}
	ATRACE(audio_4231_initcmdp, 'PDMC', cmdp);
	ATRACE(audio_4231_initcmdp, 'TAMF', format);

	for (; cmdp->data < cmdp->enddata; ) {
		*cmdp->data++ = zerosample;
	}
}

void
audio_4231_workaround(cs_unit_t *unitp)
{

	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	handle = CS4231_HANDLE;
	chip = unitp->chip;
	/*
	 * This workaround is so that the 4231 will run a logical
	 * zero sample through the DAC when playback is disabled.
	 * Otherwise there can be a "zipper" noise when adjusting
	 * the play gain at idle.
	 */
	if (audio_4231_acal) {
		/*
		 * turn off auto-calibrate before
		 * running the zero sample thru
		 */
		REG_SELECT(IAR_MCE | INTERFACE_CR);
		AND_SET_BYTE_R(handle, CS4231_IDR, ACAL_DISABLE);
		audio_4231_pollready();
	}

	if (!CS4231_reva) {
		REG_SELECT(IAR_MCE);
		drv_usecwait(100);
		REG_SELECT(IAR_MCD);
		drv_usecwait(100);
		audio_4231_pollready();
		drv_usecwait(1000);
	}

	if (audio_4231_acal) {
		/*
		 * re-enable the auto-calibrate
		 */
		REG_SELECT(IAR_MCE | INTERFACE_CR);
		OR_SET_BYTE_R(handle, CS4231_IDR, CHIP_INACTIVE);
		audio_4231_pollready();
	}
}

/*
 * audio_4231_config_queue - Set the high and low water marks for a queue
 *
 */
void
audio_4231_config_queue(aud_stream_t *as)
{
	long hiwater, lowater;
	long onesec;

	ASSERT(as != NULL);
	ASSERT_ASLOCKED(as);
	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_SETI, ("4231_config_queue: as 0x%x\n",
	    as));

	/*
	 * Configure an output stream
	 */
	if (as == as->output_as && !(UNITP(as)->output.active)) {
		/*
		 * If the write queue is not open, then just return
		 */
		if (as->writeq == NULL)
			return;

		onesec = (as->info.sample_rate * as->info.channels *
		    as->info.precision) / 8;

		hiwater = onesec * 3;
		hiwater = MIN(hiwater, 80000);
		lowater = hiwater * 2 / 3;

		/*
		 * Set the play stream hi and lowater marks based
		 * upon tunable variables in /etc/system is they
		 * have been set. For the future these should
		 * be ioctls().
		 */


		if (audio_4231_play_hiwater != 0) {
			hiwater = audio_4231_play_hiwater;
		}

		if (audio_4231_play_lowater != 0) {
			lowater = audio_4231_play_lowater;
		}

		/*
		 * Tweak the high and low water marks based on throughput
		 * expectations.
		 */
		freezestr(as->writeq);
		strqset(as->writeq, QHIWAT, 0, hiwater);
		strqset(as->writeq, QLOWAT, 0, lowater);
		unfreezestr(as->writeq);
	}
}

/*
 * Use the timeout routine to allow other threads to run if the chip
 * is doing a long auto-calibrate. This is a workaround for the CS4231
 * which can spin for CS_TIMEOUT ~ 15 secs. This is fixed in the
 * CS4231A part.
 */
static void
audio_4231_timeout()
{
	cs_unit_t *unitp;
	register uint_t x = 0;
	static	int	timeout_count = 0;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	AUD_ERRPRINT(AUD_EP_L0, AUD_EM_CINT, ("4231_timeout:\n"));

	unitp = &cs_units[0];
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	ddi_putb(handle, &unitp->chip->pioregs.iar, (u_char)IAR_MCD);

	/*
	 * Wait to see if chip is out of mode change enable
	 */
	while ((ddi_getb(handle, CS4231_IAR)) == IAR_NOTREADY &&
	    x <= CS_TIMEOUT) {
		x++;
	}

	/*
	 * Wait to see if chip has done the autocalibrate
	 */
	REG_SELECT(TEST_IR);
	if ((ddi_getb(handle, CS4231_IDR) == AUTOCAL_INPROGRESS)) {
		if (++timeout_count < CS_TIMEOUT)
			timeout(audio_4231_timeout, (caddr_t)NULL, 100);
		else {
			timeout_count = 0;
			cmn_err(CE_WARN,
			    "audio_4231_timeout: codec not ready\n");
		}
	} else
		timeout_count = 0;
}

#if defined(DEBUG)
void
audio_4231_dumpregs()
{
#define	NREGS 32
	cs_unit_t *unitp;
	uint_t x = 0;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	unitp = &cs_units[0];
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	for (x = 0; x < NREGS; x++) {
		REG_SELECT(x);
		prom_printf("%d: 0x%x%c", x, ddi_getb(handle, CS4231_IDR),
		    ((x + 1) % 4) ? '\t' : '\n');
	}
	prom_printf("<< Press any key to continue --- >>>\n");
	(void) prom_getchar();
}

void
audio_4231_int_status()
{
	cs_unit_t *unitp;
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;

	unitp = &cs_units[0];
	handle = CS4231_HANDLE;
	chip = unitp->chip;

	REG_SELECT(ALT_FEAT_STATR);
	prom_printf("status(832): 0x%x estatus(24) 0x%x\n",
	    ddi_getb(handle, CS4231_STATUS), ddi_getb(handle, CS4231_IDR));

	REG_SELECT(PLAY_DATA_FR)
	prom_printf("PB format(8): 0x%x ", ddi_getb(handle, CS4231_IDR));

	REG_SELECT(PIN_CR);
	prom_printf("int on/off(10): 0x%x\n", ddi_getb(handle, CS4231_IDR));

	REG_SELECT(INTERFACE_CR);
	prom_printf("P/R enabled(9): 0x%x ", ddi_getb(handle, CS4231_IDR));

	REG_SELECT(TEST_IR);
	prom_printf("errstat(11): 0x%x\n", ddi_getb(handle, CS4231_IDR));
}

#endif	/* defined(DEBUG) */

/*
 * Device-independent routine to mute a channel
 * The mechanism used to mute a particular output line seems to be
 * vendor/platform specific.  Various OEM's use external control
 * lines, board registers, POS registers, etc.
 */

static void
audio_4231_mute_channel(cs_unit_t *unitp, int channel)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	uchar_t oldvol;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	/*
	 * There is no way to avoid popping by lowering volume incrementally;
	 * the output muting must be applied.  Changing the attenuation by
	 * any amount without muting will always produce a pop.
	 */
	REG_SELECT(channel);
	oldvol = ddi_getb(handle, CS4231_IDR);
	ddi_putb(handle, CS4231_IDR, MUTE_ON(oldvol));
}

static void
audio_4231_unmute_channel(cs_unit_t *unitp, int channel)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	uchar_t oldvol;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	REG_SELECT(channel);
	oldvol = ddi_getb(handle, CS4231_IDR);
	ddi_putb(handle, CS4231_IDR, MUTE_OFF(oldvol));
}

static void
audio_4231_change_sample_rate(cs_unit_t *unitp, uchar_t dfr_mask)
{
	ddi_acc_handle_t handle;
	struct aud_4231_chip *chip;
	uchar_t tmp_bits;

	handle = CS4231_HANDLE;
	chip = unitp->chip;

	REG_SELECT(IAR_MCE | PLAY_DATA_FR);
	tmp_bits = ddi_getb(handle, CS4231_IDR);
	ddi_putb(handle, CS4231_IDR,
	    (uchar_t)CHANGE_DFR(tmp_bits, dfr_mask));

	audio_4231_pollready();
}

void
audio_4231_dma_errprt(int rc)
{
	switch (rc) {
	case DDI_DMA_PARTIAL_MAP:
		DPRINTF(("partial mapping done.\n"));
					/* post no breaks! */
	/*FALLTHRU*/
	case DDI_DMA_MAPPED:
		DPRINTF(("DMA buffers were mapped\n"));
		break;

	case DDI_DMA_INUSE:
		cmn_err(CE_WARN, "audiocs: DMA handle in use.");
		break;

	case DDI_DMA_NORESOURCES:
		cmn_err(CE_WARN, "audiocs: Insufficient DMA resources.");
		break;

	case DDI_DMA_NOMAPPING:
		cmn_err(CE_WARN, "audiocs: Could not map requested buffer.");
		break;

	case DDI_DMA_TOOBIG:
		cmn_err(CE_WARN, "audiocs: I/O buffer too big.");
		break;

	default:
		cmn_err(CE_WARN, "audiocs: Unknown alloc error 0x%x.", rc);
		break;
	}
}
