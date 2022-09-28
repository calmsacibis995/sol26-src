/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_DEBUG_DEBUG_H
#define	_SYS_DEBUG_DEBUG_H

#pragma ident	"@(#)debug.h	1.1	94/01/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef int (*func_t)();

/*
 * The debugger gets one megabyte virtual address range in which it
 * can reside.  It is hoped that this space is large enough to accommodate
 * the largest kernel debugger that would be needed but not too large to
 * cramp the kernel's virtual address space.
 */
#define	DEBUGSIZE	0x100000
#define	DEBUGSTART	0xFFE00000

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUG_DEBUG_H */
