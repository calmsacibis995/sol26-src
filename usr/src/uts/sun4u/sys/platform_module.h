
/*
 * Copyright (c) 1987-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PLATFORM_MODULE_H
#define	_SYS_PLATFORM_MODULE_H

#pragma ident	"@(#)platform_module.h	1.1	96/10/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef _KERNEL

/*
 * The are functions that are expected of the platform modules.
 */

extern void set_platform_defaults(void);
extern void load_platform_drivers(void);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PLATFORM_MODULE_H */
