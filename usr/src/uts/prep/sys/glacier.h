/*
 * Copyright 1988-1989, Sun Microsystems, Inc.
 */

#ifndef	_SYS_GLACIER_H
#define	_SYS_GLACIER_H

#pragma ident	"@(#)glacier.h	1.13	95/02/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct glacier_ramdac {
	unsigned long	palette_write_index;
	unsigned long	palette_data;
	unsigned long	pixel_mask;
	unsigned long	palette_read_index;
	unsigned long	index_low;
	unsigned long	index_high;
	unsigned long	index_data;
	unsigned long	index_control;
};

#define	GLACIER_RAMDAC(r)	\
	(*(struct glacier_ramdac *)(r)->p9100_control_regs[4].p9100_cr_ramdac)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_GLACIER_H */
