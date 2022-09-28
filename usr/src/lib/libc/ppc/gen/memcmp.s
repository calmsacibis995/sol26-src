/*
 *   Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *   All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *
 *   Syntax:	
 *
 */

.ident "@(#)memcmp.s 1.7      95/09/27 SMI"

/*
 * memcmp(s1, s2, len)
 *
 * Compare n bytes:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 *
 * Fast assembler language version of the following C-program for memcmp
 * which represents the `standard' for the C-library.
 *
 *	int
 *	memcmp(s1, s2, n)
 *	const void *s1;
 *	const void *s2;
 *	register size_t n;
 *	{
 *		if (s1 != s2 && n != 0) {
 *			register const char *ps1 = s1;
 *			register const char *ps2 = s2;
 *			do {
 *				if (*ps1++ != *ps2++)
 *					return(ps1[-1] - ps2[-1]);
 *			} while (--n != 0);
 *		}
 *		return (NULL);
 *	}
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memcmp,function)

#include "synonyms.h"

	ENTRY(memcmp)		! XXXPPC inefficient version for now

	cmpl	%r3,%r4
	beq	.ret0
	cmpi	%r5,0
	beq	.ret0

	addi	%r3, %r3, -1
	addi	%r4, %r4, -1
	mtctr	%r5

.cmploop:
	lbzu	%r12,1(%r3)
	lbzu	%r11,1(%r4)
	cmp	%r12,%r11
	bne	.retdiff
	bdnz	.cmploop

.ret0:
	li	%r3, 0
	blr

.retdiff:
	subf	%r3,%r11,%r12
	blr

	SET_SIZE(memcmp)
