
/*
 * Copyright (c) 1993-1994 by Sun Microsystems, Inc.
 */

#ifndef	_MENU_FDISK_H
#define	_MENU_FDISK_H

#pragma ident	"@(#)menu_fdisk.h	1.4	94/08/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI
 */
int get_solaris_part(int fd, struct ipart *ipart);
int copy_solaris_part(struct ipart *ipart);

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_FDISK_H */
