/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_AUDIO_4231_DEBUG_H
#define	_SYS_AUDIO_4231_DEBUG_H

#pragma ident	"@(#)audio_4231_debug.h	1.6	95/09/29 SMI"

/*
 * This file contains debug definitions for the CS4231 driver.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * flags/masks for error printing.
 * the levels are for severity
 */
#define	AUD_EP_L0	0	/* chatty as can be - for debug! */
#define	AUD_EP_L1	1	/* best for debug */
#define	AUD_EP_L2	2	/* minor errors - retries, etc. */
#define	AUD_EP_L3	3	/* major errors */
#define	AUD_EP_L4	4	/* catastrophic errors, don't mask! */
#define	AUD_EP_LMAX	4	/* catastrophic errors, don't mask! */

#ifdef DEBUG
#define	AUD_ERRPRINT(l, m, args)	\
	{ if (((l) >= aud_errlevel) && ((m) & aud_errmask)) prom_printf args; }
#define	AUD_ERRDUMPREGS(l, m) \
	{ if (((l) >= aud_errlevel) && ((m) & aud_errmask)) \
	    audio_4231_dumpregs(); }
#define	DPRINTF(x)	if (audiocs_debug) (void)prom_printf x
#else
#define	AUD_ERRPRINT(l, m, args)	{ }
#define	AUD_ERRDUMPREGS(l, m)	{ }
#define	DPRINTF(x)
#endif /* DEBUG */

/*
 * for each function, we can mask off its printing by clearing its bit in
 * the fderrmask.  Some functions (attach, ident) share a mask bit
 */
#define	AUD_EM_IDEN 0x00000001	/* audio_4231_identify */
#define	AUD_EM_ATTA 0x00000001	/* audio_4231_attach, audio_4231_8237_mapregs */
#define	AUD_EM_DETA 0x00000002	/* audio_4231_detach */
				/* audio_4231_8237_unmapregs */
#define	AUD_EM_OPEN 0x00000004	/* audio_4231_open */
#define	AUD_EM_GETI 0x00000008	/* audio_4231_getinfo */
#define	AUD_EM_CLOS 0x00000010	/* audio_4231_close */
#define	AUD_EM_MPRO 0x00000020	/* audio_4231_mproto */
#define	AUD_EM_STRT 0x00000040	/* audio_4231_start, audio_4231_initlist */
#define	AUD_EM_STOP 0x00000080	/* audio_4231_stop */
#define	AUD_EM_SETF 0x00000100	/* audio_4231_setflag */
#define	AUD_EM_SETI 0x00000200	/* audio_4231_setinfo */
				/* audio_4231_config_queue */
#define	AUD_EM_QCMD 0x00000400	/* audio_4231_queuecmd */
#define	AUD_EM_FLSH 0x00000800	/* audio_4231_flushcmd */
#define	AUD_EM_CINT 0x00001000	/* audio_4231_chipinit, audio_4231_pollready */
				/* audio_4231_timeout, audio_4231_8237_reset */
#define	AUD_EM_IOCT 0x00002000	/* audio_4231_ioctl, audio_4231_8237_version */
#define	AUD_EM_HWOP 0x00004000	/* audio_4231_output_muted, audio_4231_inport */
				/* audio_4231_outport, audio_4231_record_gain */
				/* audio_4231_play_gain, */
				/* audio_4231_monitor_gain */
#define	AUD_EM_PROP 0x00008000	/* audio_4231_prop_ops */
#define	AUD_EM_PLAY 0x00010000	/* audio_4231_playintr */
				/* audio_4231_8237_start_output */
				/* audio_4231_8237_stop_output */
#define	AUD_EM_RCRD 0x00020000	/* audio_4231_recintr, audio_4231_recordend */
				/* audio_4231_8237_stop_input, */
				/* audio_4231_initcmdp */
				/* audio_4231_8237_start_input */
#define	AUD_EM_INTR 0x00040000	/* audio_4231_insert, audio_4231_remove */
				/* audio_4231_clear, audio_4231_samplecalc */
				/* audio_4231_sampleconv */
				/* audio_4231_8237_dma_setup */
#define	AUD_EM_STAT 0x00080000	/* audio_4231_sampleconv */
#define	AUD_EM_GCNT 0x00100000	/* audio_4231_8237_get_count */
				/* audio_4231_8237_get_acchandle */
#define	AUD_EM_ALL  0xFFFFFFFF	/* all */

#ifdef	__cplusplus
}
#endif

#endif	/* !_SYS_AUDIO_4231_DEBUG_H */
