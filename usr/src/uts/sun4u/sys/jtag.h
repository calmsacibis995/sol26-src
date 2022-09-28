/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_JTAG_H
#define	_SYS_JTAG_H

#pragma ident	"@(#)jtag.h	1.5	96/01/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

enum board_type jtag_get_board_type(volatile u_int *, int);
int jtag_powerdown_board(volatile u_int *, int, enum board_type,
	u_int *, u_int *);
int jtag_integrity_scan(volatile u_int *, int);
int jtag_get_board_info(volatile u_int *, struct bd_info *);
int jtag_init_disk_board(volatile u_int *, int, int, u_int *, u_int *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_JTAG_H */
