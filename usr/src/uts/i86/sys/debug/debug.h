/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ifndef _SYS_DEBUG_DEBUG_H
#define	_SYS_DEBUG_DEBUG_H

#pragma ident	"@(#)debug.h	1.5	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef int (*func_t)();

/*
 * The debugger gets four megabytes virtual address range in which it
 * can reside.  It is hoped that this space is large enough to accommodate
 * the largest kernel debugger that would be needed but not too large to
 * cramp the kernel's virtual address space.
 */
#define	DEBUGSIZE	0x400000
#define	DEBUGSTART	0xFF800000

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUG_DEBUG_H */
