/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_profile.h	1.4	96/07/03 SMI"

#ifndef	_PROFILE_DOT_H
#define	_PROFILE_DOT_H

/*
 * Size of a dynamic PLT entry, used for profiling of shared objects.
 */
#define	M_DYN_PLT_ENT	(5 * sizeof (long))


#ifdef	PRF_RTLD
/*
 * Define MCOUNT macros that allow functions within ld.so.1 to collect
 * call count information.  Each function must supply a unique index.
 */
#ifndef	_ASM

#define	PRF_MCOUNT(index, func) \
	if (profile_rtld) { 						\
		asm("mflr  %r0");					\
		asm("stw   %r0, 4(%r1)");				\
		asm("stwu  %r1, -48(%r1)");				\
		asm("stw   %r3, 8(%r1)");				\
		asm("stw   %r4, 12(%r1)");				\
		asm("stw   %r5, 16(%r1)");				\
		asm("stw   %r6, 20(%r1)");				\
		asm("stw   %r7, 24(%r1)");				\
		asm("stw   %r8, 28(%r1)");				\
		asm("stw   %r9, 32(%r1)");				\
		asm("stw   %r10, 36(%r1)");				\
		asm("mfcr  %r3");					\
		asm("stw   %r3, 40(%r1)");				\
		asm("bl	   _GLOBAL_OFFSET_TABLE_@local - 4");		\
		asm("mflr  %r3");					\
		asm("lwz   %r5, "#func"@got(%r3)");			\
		asm("mr    %r4, %r0");					\
		asm("li    %r3, "#index);				\
		asm("bl    plt_cg_interp");				\
		asm("mtctr %r3");					\
		asm("lwz  %r3, 8(%r1)");				\
		asm("lwz  %r4, 12(%r1)");				\
		asm("lwz  %r5, 16(%r1)");				\
		asm("lwz  %r6, 20(%r1)");				\
		asm("lwz  %r7, 24(%r1)");				\
		asm("lwz  %r8, 28(%r1)");				\
		asm("lwz  %r9, 32(%r1)");				\
		asm("lwz  %r10, 36(%r1)");				\
		asm("lwz  %r0, 40(%r1)");				\
		asm("mtcrf  0xff,%r0");					\
		asm("addi  %r1, %r1, 48");				\
		asm("lwz  %r0, 4(%r1)");				\
		asm("mtlr  %r0");					\
	}
#else
#define	PRF_MCOUNT(index, func)
#endif
#else
#define	PRF_MCOUNT(index, func)
#endif

#endif
