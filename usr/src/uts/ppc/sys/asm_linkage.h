/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#pragma ident	"@(#)asm_linkage.h	1.17	95/11/13 SMI"

#include <sys/stack.h>
#include <sys/trap.h>

#ifdef	__cplusplus
extern "C" {
#endif

#undef BUG_1163010		/* "##" macro concatenation bug */
#undef BUG_1163156		/* (symbol)@attribute bug */

#if defined(_ASM) /* The remainder of this file is only for assembly files */

/*
 * Sign extend a 16-bit hex value to make it appropriate for use in a
 * lis instruction.
 *
 * For example,
 *	lis %r1, 0xabcd
 * causes a warning, because the assembler cannot tell whether the 64-bit
 * value of 0xffffffffabcd0000 or 0x00000000abcd0000 is intended.
 * The former is possible, but the latter is not.  Using
 *	lis %r1, 0xffffffffffffabcd
 * is awkward (and still not correct), so we use
 *	lis %r1, EXT16(0xabcd)
 * instead.
 *
 * This is defined so as to work on 32-bit and 64-bit assemblers.
 */
#define	EXT16(x)	(((0 - (1 & ((x) >> 15))) & ~0xffff) | ((x) & 0xffff))

/*
 * Symbolic section definitions.
 */
#define	RODATA	".rodata"

/*
 * Macro to define weak symbol aliases. These are similar to the ANSI-C
 *      #pragma weak name = _name
 * except a compiler can determine type. The assembler must be told. Hence,
 * the second parameter must be the type of the symbol (i.e.: function,...)
 */
#if defined(__STDC__) && defined(BUG_1163010)

#define	ANSI_PRAGMA_WEAK(sym, stype)    \
	.weak	sym; \
	.type sym, @stype; \
sym	= _ ## sym

#else	/* !defined(__STDC__) */

#define	ANSI_PRAGMA_WEAK(sym, stype)    \
	.weak	sym; \
	.type sym, @stype; \
/* CSTYLED */ \
sym	= _/**/sym

#endif	/* defined(__STDC__ */

/*
 * A couple of macro definitions for creating/restoring
 * a minimal PowerPC stack frame.
 *
 */
#define	MINSTACK_SETUP \
	mflr	%r0; \
	stwu	%r1, -SA(MINFRAME)(%r1); \
	stw	%r0, (SA(MINFRAME)+4)(%r1)

#define	MINSTACK_RESTORE \
	lwz	%r0, +(SA(MINFRAME)+4)(%r1); \
	addi	%r1, %r1, +SA(MINFRAME+4); \
	mtlr	%r0

/*
 * A couple of macros for position-independent function references.
 */
#if defined(PIC)

#if defined(__STDC__) && defined(BUG_1163010)

#define	POTENTIAL_FAR_CALL(x)	bl	x ## @plt
#define	POTENTIAL_FAR_BRANCH(x)	b	x ## @plt

#else	/* !defined(__STDC__) */

/* CSTYLED */
#define	POTENTIAL_FAR_CALL(x)	bl	x/**/@plt
/* CSTYLED */
#define	POTENTIAL_FAR_BRANCH(x)	b	x/**/@plt

#endif

#else	/* !defined(PIC) */

#define	POTENTIAL_FAR_CALL(x)	bl	x
#define	POTENTIAL_FAR_BRANCH(x)	b	x

#endif	/* defined(PIC) */

/*
 * A couple of macros for position-independent data references.
 */

/*
 * This macro returns a pointer to the specified data item in the
 * desired register.  Returns pointer to requested data item in second
 * parameter, "regX".
 */
#if defined(PIC)

#if defined(__STDC__) && defined(BUG_1163156)

#define	POTENTIAL_NONLOCAL_DATA_REF(x, regX, GOTreg) \
	PIC_SETUP(); \
	mflr	%GOTreg; \
	addis	%regX, %GOTreg, (x)@got@ha; \
	lwz	%regX, (x)@got@l(%regX)

#else	/* ! defined(__STDC__) */

#define	POTENTIAL_NONLOCAL_DATA_REF(x, regX, GOTreg) \
	PIC_SETUP(); \
	mflr	%GOTreg; \
	addis	%regX, %GOTreg, x@got@ha; \
	lwz	%regX, x@got@l(%regX)

#endif	/* defined(__STDC__) */

#else	/* !defined(PIC) */

#if defined(__STDC__) && defined(BUG_1163156)

#define	POTENTIAL_NONLOCAL_DATA_REF(x, regX, GOTreg) \
	lis	%regX, (x)@ha; \
	la	%regX, (x)@l(%regX)

#else	/* ! defined(__STDC__) */

#define	POTENTIAL_NONLOCAL_DATA_REF(x, regX, GOTreg) \
	lis	%regX, x@ha; \
	la	%regX, x@l(%regX)

#endif	/* defined(__STDC__) */

#endif	/* defined(PIC) */

/*
 * profiling causes definitions of the MCOUNT and RTMCOUNT
 * particular to the type
 */
#if defined(GPROF)

#define	MCOUNT(x) \
	mflr	%r0;	\
	stwu	%r1,	-SA(MINFRAME)(%r1);	\
	stw	%r0,	4(%r1);			\
	POTENTIAL_FAR_CALL(_mcount);		\
	lwz	%r0,	4(%r1);			\
	addi	%r1, %r1, SA(MINFRAME);		\
	mtlr	%r0

#endif /* defined(GPROF) */

#if defined(PROF)

#if defined(__STDC__) && defined(BUG_1163010)

#define	MCOUNT(x) \
	lis	%r11, (.L_ ## x ## 1)@h; \
	ori	%r11, %r11, (.L_ ## x ## 1)@l; \
	POTENTIAL_FAR_CALL(_mcount); \
	/* CSTYLED */ \
	.common .L_/**/x ## 1, 4, 2

#else	/* !defined(__STDC__) */

#define	MCOUNT(x) \
	/* CSTYLED */ \
	lis	%r11, (.L_/**/x/**/1)@h; \
	/* CSTYLED */ \
	ori	%r11, %r11, (.L_/**/x/**/1)@l; \
	POTENTIAL_FAR_CALL(_mcount); \
	/* CSTYLED */ \
	.common .L_/**/x/**/1, 4, 2

#endif	/* defined(__STDC__) */

#endif /* defined(PROF) */

/*
 * if we are not profiling, MCOUNT should be defined to nothing
 */
#if !defined(PROF) && !defined(GPROF)
#define	MCOUNT(x)
#endif /* !defined(PROF) && !defined(GPROF) */

#define	RTMCOUNT(x)	MCOUNT(x)

/*
 * ENTRY provides the standard procedure entry code and an easy way to
 * insert the calls to mcount for profiling. ENTRY_NP is identical, but
 * never calls mcount.
 */
#define	ENTRY(x) \
	.text; \
	.global	x; \
	.align	2; \
	.type	x, @function; \
x:	MCOUNT(x)

#define	ENTRY_NP(x) \
	.text; \
	.global	x; \
	.align	2; \
	.type	x, @function; \
x:

#define	RTENTRY(x) \
	.text; \
	.global	x; \
	.align	2; \
	.type	x, @function; \
x:	RTMCOUNT(x)

/*
 * ENTRY2 is identical to ENTRY but provides two labels for the entry point.
 */
#define	ENTRY2(x, y) \
	.text; \
	.global	x, y; \
	.align	2; \
	.type	x, @function; \
	.type	y, @function; \
	/* CSTYLED */ \
x: ; \
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y) \
	.text; \
	.global	x, y; \
	.align	2; \
	.type	x, @function; \
	.type	y, @function; \
	/* CSTYLED */ \
x: ; \
y:


/*
 * ALTENTRY provides for additional entry points.
 */
#define	ALTENTRY(x) \
	.global	x; \
	.type	x, @function; \
x:

/*
 * DGDEF and DGDEF2 provide global data declarations.
 */
#define	DGDEF2(name, sz) \
	.data; \
	.global name; \
	.type	name, @object; \
	.size	name, sz; \
name:

#define	DGDEF(name)	DGDEF2(name, 4)

/*
 * SET_SIZE trails a function and set the size for the ELF symbol table.
 */
#define	SET_SIZE(x) \
	.size	x, (.-x)

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_LINKAGE_H */
