/*
 * Copyright 1988-1989, Sun Microsystems, Inc.
 */

#ifndef	_SYS_WD90C24A2REG_H
#define	_SYS_WD90C24A2REG_H

#pragma ident	"@(#)wd90c24a2reg.h	1.12	95/10/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * WD90C24A2 frame buffer hardware definitions.
 */

#define	WD90C24A2_DEPTH	8

#define	WD90C24A2_DAC_BASE	0x06
#define	WD90C24A2_CRTC_ADR	0x14
#define	WD90C24A2_CRTC_DATA	0x15

#define	WD90C24A2_FB_OFFSET	0xf00000
#define	WD90C24A2_VGAREG_OFFSET	0x3c0
#define	WD90C24A2_VGAREG_SIZE	0x20

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_WD90C24A2REG_H */
