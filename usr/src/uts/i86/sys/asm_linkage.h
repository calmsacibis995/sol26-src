/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#pragma ident	"@(#)asm_linkage.h	1.8	95/02/24 SMI"

#include <sys/stack.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _ASM	/* The remainder of this file is only for assembly files */

/*
 * profiling causes defintions of the MCOUNT and RTMCOUNT
 * particular to the type
 */
#ifdef GPROF

#define	MCOUNT(x) \
	pushl	%ebp; \
	movl	%esp, %ebp; \
	call	_mcount; \
	popl	%ebp

#endif /* GPROF */

#ifdef PROF

#define	MCOUNT(x) \
/* CSTYLED */ \
	.lcomm .L_/**/x/**/1, 4, 4; \
	pushl	%ebp; \
	movl	%esp, %ebp; \
/* CSTYLED */ \
	movl	$.L_/**/x/**/1, %edx; \
	call	_mcount; \
	popl	%ebp

#endif /* PROF */

/*
 * if we are not profiling, MCOUNT should be defined to nothing
 */
#if !defined(PROF) && !defined(GPROF)
#define	MCOUNT(x)
#endif /* !defined(PROF) && !defined(GPROF) */

#define	RTMCOUNT(x)	MCOUNT(x)

/*
 * Macro to define weak symbol aliases. These are similar to the ANSI-C
 *	#pragma weak name = _name
 * except a compiler can determine type. The assembler must be told. Hence,
 * the second parameter must be the type of the symbol (i.e.: function,...)
 */
#define	ANSI_PRAGMA_WEAK(sym, stype)	\
	.weak	sym; \
	.type sym, @stype; \
/* CSTYLED */ \
sym	= _/**/sym

/*
 * ENTRY provides the standard procedure entry code and an easy way to
 * insert the calls to mcount for profiling. ENTRY_NP is identical, but
 * never calls mcount.
 */
#define	ENTRY(x) \
	.text; \
	.align	4; \
	.globl	x; \
	.type	x, @function; \
x:	MCOUNT(x)

#define	ENTRY_NP(x) \
	.text; \
	.align	4; \
	.globl	x; \
	.type	x, @function; \
x:

#define	RTENTRY(x) \
	.text; \
	.align	4; \
	.globl	x; \
	.type	x, @function; \
x:	RTMCOUNT(x)

/*
 * ENTRY2 is identical to ENTRY but provides two labels for the entry point.
 */
#define	ENTRY2(x, y) \
	.text; \
	.align	4; \
	.globl	x, y; \
	.type	x, @function; \
	.type	y, @function; \
/* CSTYLED */ \
x:	; \
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y) \
	.text; \
	.align	4; \
	.globl	x, y; \
	.type	x, @function; \
	.type	y, @function; \
/* CSTYLED */ \
x:	; \
y:


/*
 * ALTENTRY provides for additional entry points.
 */
#define	ALTENTRY(x) \
	.globl x; \
	.type	x, @function; \
x:

/*
 * DGDEF and DGDEF2 provide global data declarations.
 */
#define	DGDEF2(name, sz) \
	.data; \
	.globl name; \
	.type	name, @object; \
	.size	name, sz; \
name:

#define	DGDEF(name)	DGDEF2(name, 4)

/*
 * SET_SIZE trails a function and set the size for the ELF symbol table.
 */
#define	SET_SIZE(x) \
	.size	x, [.-x]

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_LINKAGE_H */
