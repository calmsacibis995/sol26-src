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

	.ident "@(#)strcpy.s 1.11      95/11/11 SMI"

#include <sys/asm_linkage.h>

#define	 NBPW	4


	ENTRY(strcpy)

	li	%r10, 0			! set count to zero
	!
	! Check if both From and To are on the same byte alignment. If the
	! alignment is not the same then check if they are half word
	! aligned and if so do half word copy, otherwise do byte copy.
	!
	xor	%r0, %r3, %r4		! xor from and to
	andi.	%r0, %r0, 3		! if lower two bits zero
	bnz-	.str_byteloop		! alignment is not the same
	andi.	%r0, %r4, NBPW-1	! is address word aligned?
	beq+	.strwcopy		! do copy from word-aligned source

	!
	! copy bytes until source & dest are word aligned 
	!
	mr	%r9, %r4
	subi	%r9, %r9, 1
.str_loop1:
	lbzu	%r7, 1(%r9)		! load a byte
	stbx	%r7, %r3, %r10		! store a byte
	addi	%r10, %r10, 1		! count += 1
	cmpi	%r7, 0			! null byte
	beq-	.strdone
	andi.	%r0, %r9, NBPW-1	! is source address word aligned?
	bne-	.str_loop1
	subi	%r10, %r10, 1
	!
	! Word copy loop.
	!
.strwcopy:
	! use trick to check if any read bytes of a word are zero
	! the following two constants will generate "byte carries"
	! and check if any bit in a byte is set, if all characters
	! are 7bits (unsigned) this allways works, otherwise
	! there is a specil case that rarely happens, see below
	lis	%r6, 0x7efe
	ori	%r6, %r6, 0xfeff	! %r6 = 0x7efefeff
	lis	%r7, EXT16(0x8101)
	ori	%r7, %r7, 0x0100	! %r7 = 0x81010100
.str_loop2:				! main loop
	lwzx	%r8, %r4, %r10		! read the word
	add	%r9, %r8, %r6		! generate byte-carries
	xor	%r9, %r8, %r9		! see if original bits set
	and	%r9, %r7, %r9
	cmpl	%r9, %r7		! if ==, no zero bytes
	bne-	.stralign
	stwx	%r8, %r3, %r10		! store the word
	addi	%r10, %r10, 4		! count += 4
	b	.str_loop2

	! check for the zero byte and increment the count appropriately
	! some information (the carry bit) is lost if bit 31
	! was set (very rare), if this is the rare condition,
	! return to the main loop again
.stralign:
#ifdef __LITTLE_ENDIAN
	li	%r6, 0xff		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.stralgn1
	stbx	%r8, %r3, %r10		! write the null byte
	blr
.stralgn1:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.stralgn2
	sthx	%r8, %r3, %r10		! write the two bytes (including null)
	blr
.stralgn2:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.stralgn3
	sthx	%r8, %r3, %r10		! write first two bytes
	li	%r0, 0
	addi	%r10, %r10, 2		! count += 2
	stbx	%r0, %r3, %r10		! write null byte
	blr
.stralgn3:
	stwx	%r8, %r3, %r10		! write the word
	addi	%r10, %r10, 4		! count += 4
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.str_loop2		! if not continue the loop
	blr
#else	/* _BIG_ENDIAN */
	lis	%r6, 0xff00		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.stralgn1
	li	%r0, 0
	stbx	%r0, %r3, %r10		! write the null byte
	blr
.stralgn1:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.stralgn2
	srwi	%r8, %r8, 16		! get to the second byte
	sthx	%r8, %r3, %r10		! write the two bytes (including null)
	blr
.stralgn2:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.stralgn3
	srwi	%r8, %r8, 16		! get to the second byte
	sthx	%r8, %r3, %r10		! write first two bytes
	li	%r0, 0
	addi	%r10, %r10, 2		! count += 2
	stbx	%r0, %r3, %r10		! write null byte
	blr
.stralgn3:
	stwx	%r8, %r3, %r10		! write the word
	addi	%r10, %r10, 4		! count += 4
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.str_loop2		! if not continue the loop
	blr
#endif	/* __LITTLE_ENDIAN */
	!
	! Byte copy loop.
	!
.str_byteloop:
	lbzx	%r7, %r4, %r10		! read the byte
	stbx	%r7, %r3, %r10		! store the byte
	addi	%r10, %r10, 1		! count += 1
	cmpwi	%r7, 0
	bne	.str_byteloop
.strdone:
	blr

	SET_SIZE(strcpy)
