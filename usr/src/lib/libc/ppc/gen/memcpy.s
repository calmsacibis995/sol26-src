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

.ident "@(#)memcpy.s 1.12      95/09/27 SMI"

/*
 * memcpy(s1, s2, len)
 *
 * Copy s2 to s1, always copy n bytes.
 * Note: this does not work for overlapped copies, bcopy() does
 *
 * Fast assembler language version of the following C-program for memcpy
 * which represents the `standard' for the C-library.
 *
 *	void * 
 *	memcpy(s, s0, n)
 *	void *s;
 *	const void *s0;
 *	register size_t n;
 *	{
 *		if (n != 0) {
 *	   	    register char *s1 = s;
 *		    register const char *s2 = s0;
 *		    do {
 *			*s1++ = *s2++;
 *		    } while (--n != 0);
 *		}
 *		return ( s );
 *	}
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memcpy,function)

#include "synonyms.h"

/*
 *	WARNING : This code assumes NBPW == 4
 */

	ENTRY(memcpy)

	cmpi	%r5,0
	beqlr

	mr	%r9,%r3


	andi.	%r11,%r9,3
	andi.	%r12,%r4,3
	or.	%r12,%r12,%r11
	bz	.wordcpy		! if both word-aligned wordcpy

	mtctr	%r5
	addi	%r4, %r4, -1
	addi	%r9, %r9, -1

.bytecpy:
	lbzu	%r11,1(%r4)
	stbu	%r11,1(%r9)
	bdnz	.bytecpy

	blr

.wordcpy:

	addi	%r4, %r4, -4
	addi	%r9, %r9, -4

	andi.	%r12,%r5,3		! r12 = remainder-bytes
	srwi.	%r10,%r5,2		! r10 = nwords
	bz	.bytes

	mtctr	%r10

.wordloop:
	lwzu	%r11,4(%r4)
	stwu	%r11,4(%r9)		! *(r9) = *(r4)
	bdnz	.wordloop

.bytes:
	cmpi	%r12,0
	beqlr				! if remain_bytes==0 return

	mtctr	%r12
	addi	%r4, %r4, +3
	addi	%r9, %r9, +3

.byteloop:
	lbzu	%r11,1(%r4)
	stbu	%r11,1(%r9)
	bdnz	.byteloop

	blr

	SET_SIZE(memcpy)
