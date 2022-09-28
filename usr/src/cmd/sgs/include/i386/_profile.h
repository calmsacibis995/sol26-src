/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_profile.h	1.3	96/06/18 SMI"

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

/*
 * Note that the registers eax, ecx, & edx can be changed accross a function
 * call - all others 'must' be preserved by the function being called.  Because
 * of this we preserve these three functions by pushing/popping them onto and
 * off of the stack.
 */
#define	PRF_MCOUNT(index, func) \
	if (profile_rtld) { \
		asm("	pushl	%eax"); \
		asm("	pushl	%ecx"); \
		asm("	pushl	%edx"); \
		asm("	pushl	"#func"@GOT(%ebx)"); \
		asm("	pushl	4(%ebp)"); \
		asm("	pushl	$"#index); \
		asm("	call	plt_cg_interp"); \
		asm("	addl	$0xc, %esp"); \
		asm("	popl	%edx"); \
		asm("	popl	%ecx"); \
		asm("	popl	%eax"); \
	}
#else
#define	PRF_MCOUNT(index, func)
#endif
#else
#define	PRF_MCOUNT(index, func)
#endif

#endif
