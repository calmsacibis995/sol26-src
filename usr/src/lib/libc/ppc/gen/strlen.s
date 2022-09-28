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

	.ident "@(#)strlen.s 1.7      95/11/11 SMI"

#include <sys/asm_linkage.h>

#define	NBPW	4


	ENTRY(strlen)

	mr	%r4, %r3
	li	%r3, 0			! length of non zero bytes
	andi.	%r0, %r4, NBPW-1	! is src word aligned
	bz-	.algnd

	addi	%r4, %r4, -1
.algn_loop0:
	lbzu	%r5, 1(%r4)
	cmpi	%r5, 0			! null byte?
	bz-	.strlen_done
	andi.	%r0, %r4, NBPW-1	! word aligned now?
	bz-	.algnd
	addi	%r3, %r3, 1
	b	.algn_loop0

.algnd:
	! use trick to check if any read bytes of a word are zero
	! the following two constants will generate "byte carries"
	! and check if any bit in a byte is set, if all characters
	! are 7bits (unsigned) this allways works, otherwise
	! there is a specil case that rarely happens, see below
	lis	%r6, 0x7efe
	ori	%r6, %r6, 0xfeff	! %r6 = 0x7efefeff
	lis	%r7, EXT16(0x8101)
	ori	%r7, %r7, 0x0100	! %r7 = 0x81010100
	subi	%r4, %r4, 4
	subi	%r3, %r3, 4
.algn_loop1:				! main loop
	addi	%r3, %r3, 4
	lwzu	%r8, 4(%r4)
	add	%r9, %r8, %r6		! generate byte-carries
	xor	%r9, %r8, %r9		! see if original bits set
	and	%r9, %r7, %r9
	cmpl	%r9, %r7		! if ==, no zero bytes
	beq	.algn_loop1

	! check for the zero byte and increment the count appropriately
	! some information (the carry bit) is lost if bit 31
	! was set (very rare), if this is the rare condition,
	! return to the main loop again
#ifdef __LITTLE_ENDIAN
	li	%r6, 0xff		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.algn1
	blr
.algn1:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.algn2
	addi	%r3, %r3, 1
	blr
.algn2:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.algn3
	addi	%r3, %r3, 2
	blr
.algn3:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.algn_loop1
	addi	%r3, %r3, 3
#else
	lis	%r6, 0xff00		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.algn1
	blr
.algn1:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.algn2
	addi	%r3, %r3, 1
	blr
.algn2:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.algn3
	addi	%r3, %r3, 2
	blr
.algn3:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.algn_loop1
	addi	%r3, %r3, 3
#endif	/* __LITTLE_ENDIAN */
.strlen_done:
	blr

	SET_SIZE(strlen)
