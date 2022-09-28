/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_AUDIO_4231_DMA_H
#define	_SYS_AUDIO_4231_DMA_H

#pragma ident	"@(#)audio_4231_dma.h	1.6	95/09/29 SMI"

/*
 * This file contains platform-specific definitions for the various
 * DMA controllers used to support the CS4231 on SPARC/PPC/x86 platforms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * These are the registers for the APC DMA channel interface to the
 * 4231.
 */

struct apc_dma {
	u_long 	dmacsr;		/* APC CSR */
	u_long	lpad[3];	/* PAD */
	u_long 	dmacva;		/* Captue Virtual Address */
	u_long 	dmacc;		/* Capture Count */
	u_long 	dmacnva;	/* Capture Next VAddress */
	u_long 	dmacnc;		/* Capture next count */
	u_long 	dmapva;		/* Playback Virtual Address */
	u_long 	dmapc;		/* Playback Count */
	u_long 	dmapnva;	/* Playback Next VAddress */
	u_long 	dmapnc;		/* Playback Next Count */
};


/*
 * APC CSR Register bit definitions
 */

#define	APC_IP		0x800000	/* Interrupt Pending */
#define	APC_PI		0x400000	/* Playback interrupt */
#define	APC_CI		0x200000	/* Capture interrupt */
#define	APC_EI		0x100000	/* General interrupt */
#define	APC_IE		0x80000		/* General ext int. enable */
#define	APC_PIE		0x40000		/* Playback ext intr */
#define	APC_CIE		0x20000		/* Capture ext intr */
#define	APC_EIE		0x10000		/* Error ext intr */
#define	APC_PMI		0x8000		/* Pipe empty interrupt */
#define	APC_PM		0x4000		/* Play pipe empty */
#define	APC_PD		0x2000		/* Playback NVA dirty */
#define	APC_PMIE	0x1000		/* play pipe empty Int enable */
#define	APC_CM		0x800		/* Cap data dropped on floor */
#define	APC_CD		0x400		/* Capture NVA dirty */
#define	APC_CMI		0x200		/* Capture pipe empty interrupt */
#define	APC_CMIE	0x100		/* Cap. pipe empty int enable */
#define	APC_PPAUSE	0x80		/* Pause the play DMA */
#define	APC_CPAUSE	0x40		/* Pause the capture DMA */
#define	APC_CODEC_PDN   0x20		/* CODEC RESET */
#define	PDMA_GO		0x08
#define	CDMA_GO		0x04		/* bit 2 of the csr */
#define	APC_RESET	0x01		/* Reset the chip */
#define	PLAY_SETUP	(APC_EI  | APC_IE | APC_PIE | \
			    APC_EIE | PDMA_GO | APC_PMIE)

#define	CAP_SETUP	(APC_EI | APC_IE | APC_CIE | \
			    APC_EIE | CDMA_GO)

#define	PLAY_UNPAUSE	(~(APC_PPAUSE | APC_IP | APC_PI | APC_CI | APC_EI | \
			    APC_PMI | APC_PMIE | APC_CMI | APC_CMIE))

#define	CAP_UNPAUSE	(~(APC_CPAUSE | APC_IP | APC_PI | APC_CI | APC_EI | \
			    APC_PMI | APC_PMIE | APC_CMI | APC_CMIE))

/*
 * macro does the ~ of this val
 */
#define	APC_INTR_MASK	(APC_IP | APC_PI | APC_CI | APC_EI |\
			    APC_PMI | APC_CMI)

/*
 * These are the registers for the EBUS2 DMA channel interface to the
 * 4231. 1 struct per channel for playback and record.
 */

struct eb2_dmar {
	u_long 	eb2csr;		/* Ebus 2 csr */
	u_long 	eb2acr;		/* ebus 2 Addrs */
	u_long 	eb2bcr;		/* ebus 2 counts */
};

/*
 * EBUS 2 CSR definitions
 */

#define	EB2_INT_PEND		0x01		/* RD ONLY */
#define	EB2_ERR_PEND		0x02		/* RD ONLY */
#define	EB2_DRAINING		0x04		/* RD ONLY */
#define	EB2_INT_EN		0x10		/* R/W 1 = on */
#define	EB2_RESET		0x80		/* R/W 0 clears */
#define	EB2_WRITE		0x100		/* 4231->memory */
#define	EB2_EN_DMA		0x200		/* DMA ON */
#define	EB2_CYC_PENDING		0x400		/* no clr REST with 1 */
#define	EB2_DIAG_RD_DONE	0x800		/* diag DMA RD done */
#define	EB2_DIAG_WR_DONE	0x1000		/* diag WR done */
#define	EB2_BYTE_CNT_EN		0x2000		/* use EB2 byte cntr */
#define	EB2_TC			0x4000		/* TC occurred */
#define	EB2_DISAB_CSR_DRN	0x10000		/* R/W 1= off */
#define	EB2_SIXTEEN		~(0xC0000) 	/* 19,18 == 0,0 */
#define	EB2_THIRTY2		0x40000		/* 19,18 == 0,1 */
#define	EB2_FOUR		0x80000		/* 19,18 == 1,0 */
#define	EB2_SIXTY4		0xC0000		/* 19,18 == 1,1 */
#define	EB2_DIAG_EN		0x100000	/* R/W */
#define	EB2_TC_INT_EN		0x800000	/* R/W 1=no intr */
#define	EB2_EN_NEXT		0x1000000	/* R/W 1= chaining */
#define	EB2_DMA_ON		0x2000000	/* R/O 1= on */
#define	EB2_ADDR_LOADED		0x4000000	/* R/O 1= addr loaded */
#define	EB2_NADDR_LOADED	0x8000000	/* R/O 1= naddr loaded */

#define	EB2_REC_CSR	&unitp->eb2_record_dmar->eb2csr
#define	EB2_PLAY_CSR	&unitp->eb2_play_dmar->eb2csr
#define	EB2_REC_ACR	&unitp->eb2_record_dmar->eb2acr
#define	EB2_REC_BCR	&unitp->eb2_record_dmar->eb2bcr
#define	EB2_PLAY_ACR	&unitp->eb2_play_dmar->eb2acr
#define	EB2_PLAY_BCR	&unitp->eb2_play_dmar->eb2bcr

#ifdef MULTI_DEBUG
#define	EB2_BURST		EB2_THIRTY2
#else
#define	EB2_BURST		0xC0000
#endif

#define	EB2_CAP_SETUP		(audio_4231_cap_burstsize | EB2_INT_EN | \
				    EB2_EN_DMA | EB2_BYTE_CNT_EN | \
				    EB2_EN_NEXT | EB2_WRITE)

#define	EB2_PLAY_SETUP		(audio_4231_play_burstsize | EB2_INT_EN | \
				    EB2_EN_DMA  | EB2_BYTE_CNT_EN | \
				    EB2_EN_NEXT)
/*
 * These are the data structure for the Intel 8237A DMA controller, typically
 * used as the system DMA engine for both the x86 and PowerPC platforms.
 */

struct i8237_dma {
	u_long	pb_chan;		/* Playback DMA channel */
	u_long	rc_chan;		/* Record DMA channel */
	uint_t	buflim;			/*  */

	uchar_t	*PBbuffer[2];		/* pointers to PB I/O buffers */
	int	PBlength[2];		/* length of data in PB buffers */
	uint_t	PBbuflen;		/* length of allocated PB buffers */
	uint_t	PBbufnum;		/* buffer being transferred */
	aud_cmd_t *PB_paused_buf;	/* buffer paused while playing */
	caddr_t	PB_paused_ptr;		/* pointer to next data to load */
	uchar_t *RCbuffer[2];		/* pointers to RC I/O buffers */
	int	RClength[2];		/* length of data in RC buffers */
	uint_t	RCbuflen;		/* length of allocated RC buffers */
	int	bytecount[2];		/* transfer byte count */
	int	samplecount[2];		/* transfer sample count */

	u_short	flags;			/* used for DMA double-buffering */
};

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AUDIO_4231_DMA_H */
