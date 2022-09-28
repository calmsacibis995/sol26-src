/*
 * Copyright 1988-1989, Sun Microsystems, Inc.
 */

#ifndef	_SYS_P9000REG_H
#define	_SYS_P9000REG_H

#pragma ident	"@(#)fairway.h	1.12	95/02/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct fairway {
	/* 0x000000 */
	char pad0[0x080000];
	struct {
		unsigned long	palette_write_index;
		unsigned long	palette_data;
		unsigned long	pixel_mask;
		unsigned long	palette_read_index;
		unsigned long	cursor_write_index;
		unsigned long	cursor_data;
		unsigned long	command_0;
		unsigned long	cursor_read_index;
		unsigned long	command_1;
		unsigned long	command_2;
		unsigned long	status;
		unsigned long	cursor_array_data;
		unsigned long	cursor_x_low;
		unsigned long	cursor_x_high;
		unsigned long	cursor_y_low;
		unsigned long	cursor_y_high;
	} ramdac;
	/* 0x080040 */
	char pad1[0x100000 - 0x080040];
};

#define	FAIRWAY_REGS(r)	((struct fairway *)(r)->p9000_nonpower9000)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_P9000REG_H */
