/*
 * Copyright (c) 1993 Sun Microsystems, Inc.
 */

#ifndef	_SYS_PSM_DEFS_H
#define	_SYS_PSM_DEFS_H

#pragma ident	"@(#)psm_defs.h	1.2	93/11/15 SMI"

/*
 * Platform Specific Module Definitions
 */

#include <sys/pic.h>
#include <sys/xc_levels.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__STDC__
typedef	void *	opaque_t;
#else	/* __STDC__ */
typedef	char *	opaque_t;
#endif	/* __STDC__ */

/*
 *	External Kernel Interface
 */

extern void picsetup(void);	/* isp initialization 			*/
extern u_longlong_t mul32(ulong a, ulong b); /* u_long_long = ulong x ulong */

/*
 *	External Kernel Reference Data
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSM_DEFS_H */
