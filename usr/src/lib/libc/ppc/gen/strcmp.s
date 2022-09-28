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

.ident "@(#)strcmp.s 1.7      94/09/09 SMI"

/* strcmp(s1, s2)
 *
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 *
 * Fast assembler language version of the following C-program for strcmp
 * which represents the `standard' for the C-library.
 *
 *	int
 *	strcmp(s1, s2)
 *	register const char *s1;
 *	register const char *s2;
 *	{
 *	
 *		if(s1 == s2)
 *			return(0);
 *		while(*s1 == *s2++)
 *			if(*s1++ == '\0')
 *				return(0);
 *		return(*s1 - s2[-1]);
 *	}
 */

#include <sys/asm_linkage.h>

	ENTRY(strcmp)
	cmpl	%r3,%r4	
	bne	.compare
	li	%r3, 0
	blr

.compare:
	addi	%r3, %r3, -1
	addi	%r4, %r4, -1

.cmploop:
	lbzu	%r12,1(%r4)
	lbzu	%r11,1(%r3)
	cmpl	%r11,%r12
	bne	.notequal
	cmpi	%r11, 0
	bne	.cmploop
	li	%r3, 0
	blr

.notequal:
	subf	%r3,%r12,%r11
	blr
	SET_SIZE(strcmp)
