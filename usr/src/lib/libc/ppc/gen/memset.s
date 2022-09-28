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

.ident "@(#)memset.s 1.10      95/09/27 SMI"

/*
 * char *memset(sp, c, n)
 *
 * Set an array of n chars starting at sp to the character c.
 * Return sp.
 *
 * Fast assembler language version of the following C-program for memset
 * which represents the `standard' for the C-library.
 *
 *	void *
 *	memset(sp1, c, n)
 *	void *sp1;
 *	register int c;
 *	register size_t n;
 *	{
 *	    if (n != 0) {
 *		register char *sp = sp1;
 *		do {
 *		    *sp++ = (char)c;
 *		} while (--n != 0);
 *	    }
 *	    return( sp1 );
 *	}
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memset,function)

#include "synonyms.h"

/*
 *	WARNING : This code assumes NBPW == 4
 */

	ENTRY(memset)	
	!
	! if number of bytes to clear is less than in a word
	! jump to tail clear
	!
	mr	%r7, %r3
	cmpwi	%r5, 4
	blt-	.setmtail
	andi.	%r6, %r3, 3		! is source address word aligned?
	beq+	.setmalign		! do copy from word-aligned source

	! set the bytes until the address is word aligned
.setm_loop1:
	stb	%r4, 0(%r3)
	subi	%r5, %r5, 1
	addi	%r3, %r3, 1
	andi.	%r6, %r3, 3
	bne	.setm_loop1

	cmpwi	%r5, 4
	blt-	.setmtail

	! %r3= word aligned source address
	! %r5 = byte count
.setmalign:
	srwi	%r12, %r5, 2		! convert to count of words to clear
	andi.	%r5, %r5, 3		! %r5 gets remainder byte count
	li	%r6, 0			! length copied = 0
	mtctr	%r12

	rlwinm	%r11, %r4, 8, 0, 23	! r11 = r4 << 8
	or	%r4, %r11, %r4		! r4 = r4 | r11
	rlwinm	%r11, %r4, 16, 0, 15	! r11 = r4 << 16
	or	%r4, %r11, %r4		! r4 = r4 | r11
					! cpy LS byte into remaining bytes of %r4

.setm_loop2:
	stwx	%r4, %r3, %r6
	addi	%r6, %r6, 4		! length_copied += 4
	bdnz	.setm_loop2

	add	%r3, %r3, %r6		! addr += length_copied

.setmtail:
	cmpwi	%r5, 0			! remainder byte count == 0 ?
	beq-	.setmdone		! done
	subi	%r3, %r3, 1
	mtctr	%r5

.setm_loop3:
	stbu	%r4, 1(%r3)
	bdnz	.setm_loop3

.setmdone:
	mr	%r3, %r7
	blr

	SET_SIZE(memset)
