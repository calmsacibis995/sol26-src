/*
 *	Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)_profile.h	1.5	96/06/18 SMI"

#ifndef	_PROFILE_DOT_H
#define	_PROFILE_DOT_H

/*
 * Size of a dynamic PLT entry, used for profiling of shared objects.
 */
#define	M_DYN_PLT_ENT	M_PLT_ENTSIZE


#ifdef	PRF_RTLD
/*
 * Define MCOUNT macros that allow functions within ld.so.1 to collect
 * call count information.  Each function must supply a unique index.
 */
#ifndef	_ASM
#define	PRF_MCOUNT(index, func) \
	if (profile_rtld) \
	    (void) plt_cg_interp(index, (caddr_t)caller(), (caddr_t)&func);
#else
#define	PRF_MCOUNT(index, func)
#endif
#else
#define	PRF_MCOUNT(index, func)
#endif

#endif
