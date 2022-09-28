/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *
 *   Syntax:	
 *
 */

.ident "@(#)strncmp.s 1.7      94/09/09 SMI"

/*
 * strncmp(s1, s2, n)
 *
 * Compare strings (at most n bytes):  s1>s2: >0  s1==s2: 0  s1<s2: <0
 *
 * Fast assembler language version of the following C-program for strncmp
 * which represents the `standard' for the C-library.
 *
 *	int
 *	strncmp(s1, s2, n)
 *	register const char *s1, *s2;
 *	register size_t n;
 *	{
 *		n++;
 *		if(s1 == s2)
 *			return(0);
 *		while(--n != 0 && *s1 == *s2++)
 *			if(*s1++ == '\0')
 *				return(0);
 *		return((n == 0)? 0: (*s1 - s2[-1]));
 *	}
 */

#include <sys/asm_linkage.h>

	ENTRY(strncmp)

	cmpl	%r3,%r4
	beq	.ret0

	cmpi	%r5, 0
	mtctr	%r5
	addi	%r4, %r4, -1
	addi	%r3, %r3, -1
	beq	.ret0

.cmploop:
	lbzu	%r12,+1(%r4)	
	lbzu	%r11,+1(%r3)
	cmpl	%r11,%r12
	bne	.retdiff
	cmpi	%r11,0
	beq	.ret0
	bdnz	.cmploop

.ret0:
	li	%r3, 0
	blr

.retdiff:
	subf	%r3,%r12,%r11
	blr

	SET_SIZE(strncmp)
