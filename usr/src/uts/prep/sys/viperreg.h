/*
 * Copyright 1993 by Sun Microsystems, Inc.
 */

#ifndef	_VIPERREG_H
#define	_VIPERREG_H

#pragma ident	"@(#)viperreg.h	1.3	95/11/10 SMI"

/*
 * The followin describes the viper specific registers
 */

#ifdef	__cplusplus
extern "C" {
#endif

	/* Definitions of the vga miscellaneous registers */

#define	VIPER_MISCOUT			0x03c2
#define	VIPER_MISCIN			0x03cc

	/* Definitions of the vga sequence registers */

#define	VIPER_SEQ_INDEX_PORT		0x03c4  /* extended ref index */
#define	VIPER_SEQ_DATA_PORT		0x03c5  /* extended regs access here */

	/* Viper specific values in the vga miscellaneous register */

#define	VIPER_MISC_VSYNC_POLARITY	0x80
#define	VIPER_MISC_HSYNC_POLARITY	0x40
#define	VIPER_MISC_DATA			0x08    /* ic designs data bit */
#define	VIPER_MISC_CLOCK		0x04    /* clock bit */

	/* Viper specific indexes in the vga sequencer registers */

#define	VIPER_SEQ_MISC_INDEX		0x11    /* misc reg index */
#define	VIPER_SEQ_OUTCTRL_INDEX		0x12    /* output control index */
#define	VIPER_SEQ_MASK			0x0f	/* bits that can be read back */

	/* Values of the viper miscellaneous sequencer register */

#define	VIPER_MISC_CRLOCK		0x20

	/* Values of the viper output control sequencer register */

#define	VIPER_OUTCTRL_MEM_DISABLED		0x00
#define	VIPER_OUTCTRL_MEM_A0000000		0x01
#define	VIPER_OUTCTRL_MEM_20000000		0x02
#define	VIPER_OUTCTRL_MEM_80000000		0x03
#define	VIPER_OUTCTRL_MEM_BITS			0x03
#define	VIPER_OUTCTRL_RESERVED_BITS		0x0c
#define	VIPER_OUTCTRL_P9000_VIDEO_ENABLE	0x10
#define	VIPER_OUTCTRL_P9000_HSYNC_POLARITY	0x20
#define	VIPER_OUTCTRL_P9000_VSYNC_POLARITY	0x40
#define	VIPER_OUTCTRL_5186_VIDEO_ENABLE		0x80

#ifdef	__cplusplus
}
#endif

#endif	/* !_VIPERREG_H */
