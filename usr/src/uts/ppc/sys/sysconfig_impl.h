/*
 *	Copyright (c) 1995 Sun Microsystems, Inc.
 */
/*
 * sysconfig_impl.h:
 *
 * platform-specific variables for the SUN private sysconfig syscall
 *
 */

#ifndef _SYS_SYSCONFIG_IMPL_H
#define	_SYS_SYSCONFIG_IMPL_H

#pragma ident	"@(#)sysconfig_impl.h	1.4	95/08/03 SMI"

#include <sys/mmu.h>		/* for dcache size etc */

#ifdef	__cplusplus
extern "C" {
#endif

extern int dcache_sets;		/* associativity  1,2,3 etc */
extern int icache_sets;		/* associativity  1,2,3 etc */
extern int timebase_frequency;
extern int lwarx_size;		/* bytes */
extern int coherency_size;	/* bytes */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSCONFIG_IMPL_H */
