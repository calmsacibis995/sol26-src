/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_AUDIO_4231_H
#define	_SYS_AUDIO_4231_H

#pragma ident	"@(#)audio_4231.h	1.17	96/10/15 SMI"

#include <sys/audio_4231_dma.h>

/*
 * This file describes the Crystal 4231 CODEC chip and declares
 * parameters and data structures used by the audio driver.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * Macros for distinguishing between minor devices
 */
#define	CS_UNITMASK	(0x0f)
#define	CS_UNIT(dev)	((geteminor(dev)) & CS_UNITMASK)
#define	CS_ISCTL(dev)	(((geteminor(dev)) & CS_MINOR_CTL) != 0)
#define	CS_CLONE_BIT	(0x10)
#define	CS_MINOR_RW	(0x20)
#define	CS_MINOR_RO	(0x40)
#define	CS_MINOR_CTL	(0x80)

/*
 * Default Driver constants for the 4231
 */
#define	AUD_CS4231_PRECISION	(8)		/* Bits per sample unit */
#define	AUD_CS4231_CHANNELS	(1)		/* Channels per sample frame */
#define	AUD_CS4231_SAMPLERATE	(8000)		/* Sample frames per second */
#define	AUD_CS4231_SAMPR16000	(16000)
#define	AUD_CS4231_SAMPR11025	(11025)
#define	AUD_CS4231_SAMPR18900	(18900)
#define	AUD_CS4231_SAMPR22050	(22050)
#define	AUD_CS4231_SAMPR32000	(32000)
#define	AUD_CS4231_SAMPR37800	(37800)
#define	AUD_CS4231_SAMPR44100	(44100)
#define	AUD_CS4231_SAMPR48000	(48000)
#define	AUD_CS4231_SAMPR9600	(9600)
#define	AUD_CS4231_SAMPR8000	AUD_CS4231_SAMPLERATE

/*
 * This is the default size of the STREAMS buffers we send down the
 * read side and the maximum record buffer size that can be specified
 * by the user.
 */
#ifdef MULTI_DEBUG
#define	AUD_CS4231_BSIZE		(1024 * 3)	/* Record buffer size */
#else
#define	AUD_CS4231_BSIZE		(8180)		/* Record buffer size */
#endif
#define	AUD_CS4231_MAX_BSIZE	(65536)		/* Maximum buffer_size */

/*
 * Buffer allocation
 */
#define	AUD_CS4231_CMDPOOL	(100)	/* total command block buffer pool */
#define	AUD_CS4231_RECBUFS	(50)	/* number of record command blocks */
#define	AUD_CS4231_PLAYBUFS	(AUD_CS4231_CMDPOOL - AUD_CS4231_RECBUFS)

/*
 * Driver constants
 */
#define	AUD_CS4231_IDNUM	(0x6175)
#define	AUD_CS4231_NAME		"audiocs"
#define	AUD_CS4231_MINPACKET	(0)

#ifdef MULTI_DEBUG
#define	AUD_CS4231_MAXPACKET	(1024 * 3)
#else
#define	AUD_CS4231_MAXPACKET	(8180)
#endif

#define	AUD_CS4231_HIWATER	(57000)
#define	AUD_CS4231_LOWATER	(32000)

/*
 * Default gain settings
 */
#define	AUD_CS4231_DEFAULT_PLAYGAIN	(132)	/* play gain initialization */
#define	AUD_CS4231_DEFAULT_RECGAIN	(126)	/* gain initialization */

#define	AUD_CS4231_MIN_ATEN	(0)	/* Minimum attenuation */
#define	AUD_CS4231_MAX_ATEN	(31)	/* Maximum usable attenuation */
#define	AUD_CS4231_MAX_DEV_ATEN	(63)	/* Maximum device attenuation */
#define	AUD_CS4231_MIN_GAIN	(0)
#define	AUD_CS4231_MAX_GAIN	(15)

/*
 * Monitor Gain settings
 */
#define	AUD_CS4231_MON_MIN_ATEN		(0)
#define	AUD_CS4231_MON_MAX_ATEN		(63)


/*
 * Values returned by the AUDIO_GETDEV ioctl
 */
#define	CS_DEV_NAME		"SUNW,CS4231"
#define	CS_DEV_VERSION		"a"	/* SS5		*/
#define	CS_DEV_VERSION_B	"b"	/* Electron - internal loopback	*/
#define	CS_DEV_VERSION_C	"c"	/* Positron	*/
#define	CS_DEV_VERSION_D	"d"	/* PowerPC	*/
#define	CS_DEV_VERSION_E	"e"	/* x86		*/
#define	CS_DEV_VERSION_F	"f"	/* Tazmo	*/
#define	CS_DEV_VERSION_G	"g"	/* Quark	*/
#define	CS_DEV_CONFIG_ONBRD1	"onboard1"


/*
 * These are the registers for the Crystal Semiconductor 4231 chip.
 */

#if defined(sparc) || defined(__sparc)

struct aud_4231_pioregs {
	u_char iar;		/* Index Address Register */
	u_char pad[3];		/* PAD  */
	u_char idr;		/* Indexed Data Register */
	u_char pad1[3];	/* PAD */
	u_char statr;		/* Status Register */
	u_char pad2[3];	/* PAD */
	u_char piodr;		/* PIO Data Register I/O */
	u_char pad3[3];	/* PAD */
};

#elif defined(i386) || defined(__ppc)

struct aud_4231_pioregs {
	u_char iar;		/* Index Address Register */
	u_char idr;		/* Indexed Data Register */
	u_char statr;		/* Status Register */
	u_char piodr;		/* PIO Data Register I/O */
};

#else
#error One of sparc, i86 or ppc must be defined.
#endif

struct aud_4231_chip {
	struct aud_4231_pioregs pioregs;
	struct apc_dma dmaregs;
	struct eb2_dmar *eb2_record_dmar;
	struct eb2_dmar *eb2_play_dmar;
};

#define	DMA_LIST_SIZE 8

typedef struct aud_dma_list {
	aud_cmd_t *cmdp;
	ddi_dma_handle_t buf_dma_handle;
} aud_dma_list_t;


/*
 * Device-dependent audio stream which encapsulates the generic audio
 * stream
 */
typedef struct cs_stream {
	/*
	 * Generic audio stream.  This MUST be the first structure member
	 */
	aud_stream_t as;

	/* DD Audio */
	aud_cmd_t *cmdptr;		/* current command pointer */

	/*
	 * Current statistics
	 */
	uint_t samples;
	uchar_t active;
	uchar_t error;
} cs_stream_t;


/*
 * This is the control structure used by the CS4231-specific driver
 * routines.
 */
typedef struct {
	/*
	 * Device-independent state--- MUST be first structure member
	 */
	aud_state_t distate;

	/*
	 * Address of the unit's registers
	 */

	struct aud_4231_chip *chip;

	/*
	 * ops vector table for DMA-engine support routines
	 */
	struct audio_4231_dma_ops *opsp;

/* #if defined(sparc) || defined(__sparc) */
	ulong_t audio_auxio;
	struct eb2_dmar *eb2_record_dmar;
	struct eb2_dmar *eb2_play_dmar;

/* #elif defined(i386) || defined(__ppc) */
	struct i8237_dma physdma;
/* #else						*/
/* #error One of sparc, i386 or ppc must be defined.	*/
/* #endif						*/

	/*
	 * Device-dependent audio stream strucutures
	 */
	cs_stream_t input;
	cs_stream_t output;
	cs_stream_t control;

	/*
	 * Pointers to per-unit allocated memory so we can free it
	 * at detach time
	 */
	caddr_t *allocated_memory;	/* kernel */
	size_t allocated_size;		/* kernel */

	/*
	 * PM and CPR support
	 */
	boolean_t suspended;	/* TRUE if driver suspended */

	/*
	 * Driver state info.
	 */
	uint_t playcount;
	uint_t typ_playlength;
	uint_t playlastaddr;
	uint_t typ_reclength;
	uint_t recordcount;
	uint_t recordlastent;
	uint_t dma_engine;		/* APC, Eb2, or 8237 (dmae) */
	uint_t intl_spkr_mute;		/* method used to mute intl spkr */
	uint_t cd_input_line;		/* AUX1, AUX2, or none */
	uint_t rec_intr_flag;
	uint_t play_intr_flag;
	boolean_t hw_output_inited;
	boolean_t hw_input_inited;
	boolean_t eb2dma;		/* Ebus or APC dma */
	uint_t	samecmdp;		/* same played command as last */
	boolean_t module_type;		/* For tazmo/quark  */

	/*
	 * OS-dependent info
	 */
	dev_info_t *dip;
	kmutex_t lock;			/* lolevel lock */

	ddi_dma_attr_t	*dma_attrp;
	ddi_acc_handle_t cnf_handle; 	/* a void* */
	ddi_acc_handle_t cnf_handle_auxio; /* a void* */
	ddi_acc_handle_t cnf_handle_eb2record;
	ddi_acc_handle_t cnf_handle_eb2play;

	uint_t play_dma_cookie_count;
	ddi_dma_cookie_t play_dma_cookie; /* I/O playback buffer DMA cookie */
	ddi_dma_handle_t play_dma_handle; /* I/O playback buffer DMA handle */
	ddi_acc_handle_t play_acc_handle; /* I/O playback buffer access handl */

	uint_t cap_dma_cookie_count;
	ddi_dma_cookie_t cap_dma_cookie; /* I/O record buffer DMA cookie */
	ddi_dma_handle_t cap_dma_handle; /* I/O record buffer DMA handle */
	ddi_acc_handle_t cap_acc_handle; /* I/O record buffer access handle */

	/*
	 * The loopback ioctl sets the device to a funny state, so we need
	 * to know if we have to re-initialize the chip when the user closes
	 * the device.
	 */
	boolean_t init_on_close;

} cs_unit_t;


/*
 * Define's for supported DMA controllers
 */
#define	APC_DMA		0x01		/* SPARC */
#define	EB2_DMA		0x02		/* SPARC */
#define	i8237_DMA	0x03		/* PowerPC/x86 */

/*
 * Define's for generic DMA data structures
 */
#define	DMA_PB_HANDLE	(unitp->play_dma_handle)
#define	DMA_RC_HANDLE	(unitp->cap_dma_handle)
#define	ACC_PB_HANDLE	(unitp->play_acc_handle)
#define	ACC_RC_HANDLE	(unitp->cap_acc_handle)
#define	DMA_PB_COOKIE	(unitp->play_dma_cookie)
#define	DMA_RC_COOKIE	(unitp->cap_dma_cookie)
#define	DMA_PB_NCOOKIES	(unitp->play_dma_cookie_count)
#define	DMA_RC_NCOOKIES	(unitp->cap_dma_cookie_count)

/*
 * Define's for various mechanisms to mute internal speaker
 */
#define	XCTL0_OFF	0x01	/* new PPC 6015 (i.e., == speaker enable) */
#define	XCTL0_ON	0x02	/* old PPC 6015, SPARC */
#define	REG_83E		0x03	/* mobile systems */
#define	REG_83E_MASK	0x10	/* mask used to enable internal speaker */

/*
 * ops vectors for DMA-engine support routines
 */
typedef struct audio_4231_dma_ops {
	char	*dma_device;
	void	(*audio_4231_dma_reset)(cs_unit_t *unitp,
		ddi_acc_handle_t handle);
	int	(*audio_4231_map_regs)(dev_info_t *dip, cs_unit_t *unitp);
	void	(*audio_4231_unmap_regs)(cs_unit_t *unitp);
	int	(*audio_4231_add_intr)(dev_info_t *dip, cs_unit_t *unitp);
	void	(*audio_4231_version)(cs_unit_t *unitp, caddr_t verp);
	void	(*audio_4231_start_input)(cs_unit_t *unitp);
	void	(*audio_4231_start_output)(cs_unit_t *unitp);
	void	(*audio_4231_stop_input)(cs_unit_t *unitp);
	void	(*audio_4231_stop_output)(cs_unit_t *unitp);
	void	(*audio_4231_get_count)(cs_unit_t *unitp, uint32_t *pcount,
		uint32_t *ccount);
	void	(*audio_4231_get_ncount)(cs_unit_t *unitp, int direction,
		uint32_t *ncount);
	ddi_acc_handle_t (*audio_4231_get_acchandle)(cs_unit_t *unitp,
		uint32_t *eb2intr, int direction);
	uint_t	(*audio_4231_dma_setup)(cs_unit_t *unitp, uint_t direction,
		aud_cmd_t *cmdp, size_t length);
	uint32_t	(*audio_4231_play_last)(cs_unit_t *unitp,
			ddi_acc_handle_t handle, uint32_t eb2intr);
	uint32_t	(*audio_4231_play_cleanup)(cs_unit_t *unitp,
			ddi_acc_handle_t handle, uint32_t eb2intr);
	void	(*audio_4231_play_next)(cs_unit_t *unitp,
		ddi_acc_handle_t handle);
	void	(*audio_4231_rec_next)(cs_unit_t *unitp);
	void	(*audio_4231_rec_cleanup)(cs_unit_t *unitp);
} ops_t;

extern ops_t audio_4231_apcdma_ops;
extern ops_t audio_4231_eb2dma_ops;
extern ops_t audio_4231_8237dma_ops;

extern ddi_dma_attr_t aud_apcdma_attr;
extern ddi_dma_attr_t aud_eb2dma_attr;
extern ddi_dma_attr_t aud_8237dma_attr;

#endif /* _KERNEL */

/*
 * Macros to derive control struct and chip addresses from the audio struct
 */
#define	UNITP(as)	((cs_unit_t *)((as)->distate->ddstate))
#define	CSR(as)		(UNITP(as)->chip)
#define	ASTOCS(as) 	((cs_stream_t *)(as))
#define	CSTOAS(cs) 	((aud_stream_t *)(cs))


/*
 * Macros for frequently used variables
 */
#define	CS4231_IAR	&chip->pioregs.iar	/* Index Address Register */
#define	CS4231_IDR	&chip->pioregs.idr	/* Indexed Data Register */
#define	CS4231_STATUS	&chip->pioregs.statr	/* Status Register */
#define	CS4231_PIOR	&chip->pioregs.piodr	/* PIO Data Register I/O */
#define	CS4231_HANDLE	(unitp->cnf_handle)
#define	REG_SELECT(r)	ddi_put8(handle, &chip->pioregs.iar, (uint8_t)(r));

#define	LOCK_UNITP(unitp)	mutex_enter(&(unitp)->lock)
#define	UNLOCK_UNITP(unitp)	mutex_exit(&(unitp)->lock)
#define	ASSERT_UNITLOCKED(unitp) \
				ASSERT(MUTEX_HELD(&(unitp)->lock))

/*
 * Useful bit twiddlers
 *
 */
#define	OR_SET_BYTE_R(handle, addr, val) {		\
	    register uint8_t tmpval;			\
	    tmpval = ddi_get8(handle, (uint8_t *)addr);	\
	    tmpval |= val; 				\
	    ddi_put8(handle, (uint8_t *)addr, tmpval);	\
	}
#define	OR_SET_LONG_R(handle, addr, val) {			\
	    register uint32_t tmpval;				\
	    tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	    tmpval |= val; 					\
	    ddi_put32(handle, (uint32_t *)addr, tmpval);	\
	}
#define	NOR_SET_LONG_R(handle, addr, val, mask) {		\
	    register uint32_t tmpval;				\
	    tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	    tmpval &= ~(mask);					\
	    tmpval |= val; 					\
	    ddi_put32(handle, (uint32_t *)addr, tmpval);	\
	}

#define	AND_SET_BYTE_R(handle, addr, val) {		\
	    register uint8_t tmpval;			\
	    tmpval = ddi_get8(handle, (uint8_t *)addr);	\
	    tmpval &= val; 				\
	    ddi_put8(handle, (uint8_t *)addr, tmpval);	\
	}
#define	AND_SET_LONG_R(handle, addr, val) {			\
	    register uint32_t tmpval;				\
	    tmpval = ddi_get32(handle, (uint32_t *)addr);	\
	    tmpval &= val; 					\
	    ddi_put32(handle, (uint32_t *)addr, tmpval);	\
	}

#define	GET_INT_PROP(devi, pname, pval, plen) \
		(ddi_prop_op(DDI_DEV_T_ANY, (devi), PROP_LEN_AND_VAL_BUF, \
		    DDI_PROP_DONTPASS, (pname), (caddr_t)(pval), (plen)))


/* Shared define's for gains, muting etc.. */
#define	GAIN_SET(var, gain)	((var & ~(0x3f)) | gain)
#if defined(sparc) || defined(__sparc)
#define	RECGAIN_SET(var, gain)	((var & ~(0x1f)) | gain)
#elif defined(i386) || defined(__ppc)
#define	RECGAIN_SET(var, gain)	((var & ~(0x3f)) | gain)
#else
#error One of sparc, i386 or ppc must be defined.
#endif
#define	CHANGE_MUTE(var, val)	((var & ~(0x80)) | val)
#define	MUTE_ON(var)		(var | 0x80)
#define	MUTE_OFF(var)		(var & 0x7f)

#define	LINEMUTE_ON		0x80
#define	LINEMUTE_OFF		(~0x80)

/* slot zero of both the record and play is the max attenuation */

#define	CS4231_MAX_DEV_ATEN_SLOT	(0)

/*
 * CS4231 Register Set Definitions
 *
 */
/* Index Register values */
#define	IAR_AUTOCAL_BEGIN	0x40
#define	IAR_AUTOCAL_END		~(0x40)
#define	IAR_MCE			0x40	/* Mode change Enable */
#define	IAR_MCD			IAR_AUTOCAL_END
#define	IAR_NOTREADY		0x80	/* 80h not ready CODEC state */

/* (Direct) Status Register */
#define	INT_ACTIVE	0x01
#define	PB_READY	0x02
#define	SAMPLE_ERR	0x10
#define	RC_READY	0x20

/*
#define	CHANGE_INPUT(var, val)		((var & ~(0xC0)) | val)
*/
#define	CHANGE_INPUT(var, val)		((var & ~(0x0f)) | val)

/* Left input control (Index 0) Modes 1&2 */
#define	L_INPUT_CR		0x0
#define	L_INPUTCR_AUX1		0x40
#define	MIC_ENABLE(var)		((var & 0x2f) | 0x80)
#define	LINE_ENABLE(var)	(var & 0x2f)
/* NOTE: This definition only works if the CD is connected to AUX1. */
#define	CDROM_ENABLE(var)	((var & 0x2f) | 0x40)
#define	OUTPUTLOOP_ENABLE(var)	((var & 0x2f) | 0xC0)

/* CD input line definitions */
#define	NO_INTERNAL_CD		0x0
#define	INTERNAL_CD_ON_AUX1	0x1
#define	INTERNAL_CD_ON_AUX2	0x2
#define	AUX_INIT_VALUE		0x88	/* unity gain */

#if defined(sparc) || defined(__sparc)
#define	AUX1_LOOPBACK_TEST	0x80	/* special factory loopback test */
#define	CODEC_ANALOG_LOOPBACK	0x40	/* factory internal codec looback */
#endif			/* defined(sparc) || defined(__sparc) */

/*
#define	LINE_ENABLE		(~0xC0)
*/

/* Right Input Control (Index 1) Modes 1&2 */
#define	R_INPUT_CR		0x1
#define	R_INPUTCR_AUX1		L_INPUTCR_AUX1

/* Left Aux. 1 Input Control (Index 2) Modes 1&2 */
#define	L_AUX1_INPUT_CR		0x2

/* Right Aux. 1 Input Control (Index 3) Modes 1&2 */
#define	R_AUX1_INPUT_CR		0x3

/* Left Aux. 2 Input Control (Index 4) Modes 1&2 */
#define	L_AUX2_INPUT_CR		0x4

/* Right Aux. 1 Input Control (Index 5) Modes 1&2 */
#define	R_AUX2_INPUT_CR		0x5

/* Left Output Control (Index 6) Modes 1&2 */
#define	L_OUTPUT_CR		0x6
#define	OUTCR_MUTE		0x80
#define	OUTCR_UNMUTE		~0x80

/* Right Output Control (Index 7) Modes 1&2 */
#define	R_OUTPUT_CR		0x7

/* Playback Data Format Register (Index 8) Mode 2 only */
#define	PLAY_DATA_FR		0x08

#define	CHANGE_DFR(var, val)		((var & ~(0xF)) | val)
#define	CHANGE_ENCODING(var, val)	((var & ~(0xe0)) | val)
#define	DEFAULT_DATA_FMAT		0x20
#define	CS4231_DFR_8000			0x00	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_9600			0x0e	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_11025		0x03	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_16000		0x02	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_18900		0x05	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_22050		0x07	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_32000		0x06	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_37800		0x09	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_44100		0x0b	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_48000		0x0c	/* CSF Bits and XTAL bits */
#define	CS4231_DFR_LINEAR8		0x00	/* Linear 8 bit unsigned */
#define	CS4231_DFR_ULAW 		0x20	/* Mu law 8 bit companded */
#define	CS4231_DFR_ALAW			0x60	/* Alaw 8 bit companded */
#define	CS4231_DFR_ADPCM		0xa0	/* ADPCM 4 bit IMA */
#ifdef MULTI_DEBUG
#define	CS4231_DFR_LINEARBE		0x40	/* Linear 16 bit Little E */
#else
#define	CS4231_DFR_LINEARBE		0xc0	/* Linear 16 bit 2's Big E */
#endif
#define	CS4231_DFR_LINEARLE		0x40	/* Linear 16 bit 2's Little E */


#define	CS4231_STEREO_ON(val)		(val | 0x10)
#define	CS4231_MONO_ON(val)		(val & ~0x10)

/* Interface Configuration Register (Index 9) */
#define	INTERFACE_CR		0x09
#define	CHIP_INACTIVE		0x08
#define	PEN_ENABLE		(0x01)
#define	PEN_DISABLE		(~0x01)
#define	CEN_ENABLE		(0x02)
#define	CEN_DISABLE		(~0x02)
#define	ACAL_DISABLE		(~0x08)

/* Calibration types */
#define	NO_CALIBRATION		(0x00)
#define	CONV_CALIBRATION	(0x08)
#define	DAC_CALIBRATION		(0x10)
#define	FULL_CALIBRATION	(0x18)

/*
 * Enable = playback, capture, Dual DMA Channels, Autocal, Playback DMA
 * only, capture DMA only.
 */
#define	ICR_AUTOCAL_INIT	0x01 /* PLAY ONLY FOR NOW XXXXX */

/* Pin Control Register (Index 10) Modes 1&2  For Interrupt enable */
#define	PIN_CR			0x0a
#define	XCTL0			0x40
#define	XCTL1			0x80
#if defined(sparc) || defined(__sparc)
/*
 * XCTL0 (0x40) and XCTL1 (0x80) are general-purpose external control lines.
 */
#define	INTR_ON			0x82
#define	INTR_OFF		0x80
#define	PINCR_LINE_MUTE		0x40	/* XCTL0 */
#define	PINCR_HDPH_MUTE		0x80	/* XCTL1 */

#elif defined(i386) || defined(__ppc)
#define	INTR_ON			0x02
#define	INTR_OFF		0x00

/*
 * this is an implementation-specific external control line, and so the
 * functionality must checked for each platform.
 */
#else
#error One of sparc, i386 or ppc must be defined.
#endif

/* Test and Initialization Register (Index 11) Modes 1&2 */
#define	TEST_IR			0x0b
#define	DRQ_STAT		0x10
#define	AUTOCAL_INPROGRESS	0x20

/* Misc. Information Register (Index 12) Modes 1&2 */

#define	MISC_IR			0x0c
#define	MISC_IR_MODE2		0x40
#define	TIAD65			0x20	/* TI AD65 CODEC */
#define	CODEC_ID_MASK		0x0F
#define	CODEC_SIGNATURE		0x0A	/* AD1848K, CS423x */

/* Loopback Control Register (Index 13) Modes 1&2 */
#define	LOOPB_CR		0x0d
#define	LOOPB_ON		0x01
#define	LOOPB_OFF		0x00

/* Upper base Register (Index 14) M1 only  Not used */
/* Lower base Register (Index 15) M1 only  Not used */

/* Playback Upper Base Register (Index 14) Mode 2 only */
#define	PLAYB_UBR		0x0e

/* Playback Lower Base Register (Index 15) Mode 2 only */
#define	PLAYB_LBR		0x0f

/* All of the following registers only apply to MODE 2 Operations */

/* Alternate Feature Enable I (Index 16) */
#define	ALT_FEA_EN1R		0x10
#define	OLB_ENABLE		0x80	/* Output Level Bit */
#define	TIMER_ENABLE		0x40
#define	DACZ_ON			0x01

/* Alternate Feature Enable II (Index 17) */
#define	ALT_FEA_EN2R		0x11
#define	HPF_ON			0x01
#define	XTALE_ON		0x20

/* Left Line Input Gain (Index 18) */
#define	L_LINE_INGAIN_R		0x12
#define	L_LINE_MUTE		0x80

/* Right Line Input Gain (Index 19) */
#define	R_LINE_INGAIN_R		0x13
#define	R_LINE_MUTE		0x80

/* Timer Hi Byte (Index 20) */
#define	TIMER_HIB_R		0x14

/* Timer Lo Byte (Index 21) */
#define	TIMER_LOB_R		0x15

/* Index 22 and 23 are reserved */

/* Alternate Feature Status (Index 24) */
#define	ALT_FEAT_STATR		0x18
#define	CS_PU			0x01	/* Playback Underrun	*/
#define	CS_PO			0x02	/* Playback Overrun	*/
#define	CS_CO			0x04	/* Capture Overrun	*/
#define	CS_CU			0x08	/* Capture Underrun	*/
#define	CS_PI			0x10	/* Playback Interrupt	*/
#define	CS_CI			0x20	/* Capture Interrupt	*/
#define	CS_TI			0x40	/* Timer Interrupt	*/
#define	ALL_INTRS_MASK		CS_PI | CS_CI | CS_TI
#define	RESET_STATUS		0x00

/* Version / ID (Index 25) */

#define	VERSION_R	0x19
#define	CS4231A		0x20
#define	CS4231CDE	0x80

/* Mono Input and Output Control (Index 26) */
#define	MONO_IOCR		0x1a
#define	CHANGE_MONO_GAIN(val)	((val & ~(0xFF)) | val)
#define	MONO_SPKR_MUTE		0x40
#define	MONO_INPUT_MUTE		0x80

/*
 * Capture Data Format Register (Index 28)
 * The bit operations on this are the same as for the PLAY_DATA_FR
 * (Index *).
 */
#define	CAPTURE_DFR		0x1c


/* Capture Base Register Upper for DMA (Index 30) */
#define	CAPTURE_UBR		0x1e

/* Capture Base Register Lower for DMA (Index 30) */
#define	CAPTURE_LBR		0x1f


/*
 * Processing (which direction?) definitions
 */
#define	RECORD		0x1
#define	PLAYBACK	0x0

/*
 * chip-specific timing constants
 */
#define	CS_TIMEOUT	9000000
#define	CS_POLL_TIMEOUT	100000

/*
 * Sets of platform-specific definitions for various DMA wrapper routines.
 */

#define	AUD_DMA_RESET(P, h)	((P)->opsp->audio_4231_dma_reset)(P, h)
#define	AUD_DMA_MAP_REGS(d, P)	((P)->opsp->audio_4231_map_regs)(d, P)
#define	AUD_DMA_UNMAP_REGS(P)	((P)->opsp->audio_4231_unmap_regs)(P)
#define	AUD_DMA_ADD_INTR(d, P)	((P)->opsp->audio_4231_add_intr)(d, P)
#define	AUD_DMA_VERSION(P, sp)	((P)->opsp->audio_4231_version)(P, sp)
#define	AUD_DMA_START_INPUT(P)	((P)->opsp->audio_4231_start_input)(P)
#define	AUD_DMA_START_OUTPUT(P)	((P)->opsp->audio_4231_start_output)(P)
#define	AUD_DMA_STOP_INPUT(P)	((P)->opsp->audio_4231_stop_input)(P)
#define	AUD_DMA_STOP_OUTPUT(P)	((P)->opsp->audio_4231_stop_output)(P)
#define	AUD_DMA_GET_COUNT(P, pc, cc) \
			((P)->opsp->audio_4231_get_count)(P, pc, cc)
#define	AUD_DMA_GET_NCOUNT(P, d, nc) \
			((P)->opsp->audio_4231_get_ncount)(P, d, nc)
#define	AUD_DMA_GET_ACCHANDLE(P, e, d)	\
				((P)->opsp->audio_4231_get_acchandle)(P, e, d)
#define	AUD_DMA_SETUP(P, d, c, l)	\
			((P)->opsp->audio_4231_dma_setup)(P, d, c, l)
#define	AUD_DMA_PLAY_LAST(P, h, e)	\
				((P)->opsp->audio_4231_play_last)(P, h, e)
#define	AUD_DMA_PLAY_CLEANUP(P, h, e) \
				((P)->opsp->audio_4231_play_cleanup)(P, h, e)
#define	AUD_DMA_PLAY_NEXT(P, h)	\
				((P)->opsp->audio_4231_play_next)(P, h)
#define	AUD_DMA_REC_NEXT(P)	((P)->opsp->audio_4231_rec_next)(P)
#define	AUD_DMA_REC_CLEANUP(P)	((P)->opsp->audio_4231_rec_cleanup)(P)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AUDIO_4231_H */
