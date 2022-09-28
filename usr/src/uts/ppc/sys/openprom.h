/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_OPENPROM_H
#define	_SYS_OPENPROM_H

#pragma ident	"@(#)openprom.h	1.2	95/07/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structure in to which OpenFirmware's memory list information is
 * saved.
 */

struct prom_memlist {
	struct prom_memlist	*next;
	u_int			address;
	u_int			size;
};

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_OPENPROM_H */
