/*
 * Copyright 1988-1989, Sun Microsystems, Inc.
 */

#ifndef	_SYS_SDACREG_H
#define	_SYS_SDACREG_H

#pragma ident	"@(#)SDACreg.h	1.3	95/04/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * S3 SDAC video DAC hardware definitions.
 */

#define	SDAC_PALETTE_WRITE_ADDR	0x00
#define	SDAC_PALETTE_DATA	0x01
#define	SDAC_MASK		0x02
#define	SDAC_PALETTE_READ_ADDR	0x03
#define	SDAC_PLL_WRITE_ADDR	0x04
#define	SDAC_PLL_DATA		0x05
#define	SDAC_ENH_CMD		0x06
#define	SDAC_PLL_READ_ADDR	0x07

#define	SDAC_CMAP_ENTRIES	256

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SDACREG_H */
