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

.ident "@(#)strncpy.s 1.5      95/11/11 SMI"

/*
 * strncpy(s1, s2)
 *
 * Copy string s2 to s1, truncating or null-padding to always copy n bytes
 * return s1.
 *
 * Fast assembler language version of the following C-program for strncpy
 * which represents the `standard' for the C-library.
 *
 *	char *
 *	strncpy(s1, s2, n)
 *	register char *s1;
 *	register const char *s2;
 *	register size_t n;
 *	{
 *		register char *os1 = s1;
 *	
 *		n++;				
 *		while ((--n != 0) &&  ((*s1++ = *s2++) != '\0'))
 *			;
 *		if (n != 0)
 *			while (--n != 0)
 *				*s1++ = '\0';
 *		return (os1);
 *	}
 */

#include <sys/asm_linkage.h>

#define	NBPW	4
#define	SMALL_CPY_SIZE	8

	ENTRY(strncpy)

	li	%r10, 0			! set count to zero
	cmpi	%r5, SMALL_CPY_SIZE
	ble-	.str_byteloop
	!
	! Check if both From and To are on the same byte alignment. If the
	! alignment is not the same then check if they are half word
	! aligned and if so do half word copy, otherwise do byte copy.
	!
	xor	%r0, %r3, %r4		! xor from and to
	andi.	%r0, %r0, NBPW-1	! if lower two bits zero
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
	cmpwi	%r7, 0			! null byte
	beq-	.strdone
	cmpl	%r5, %r10
	beqlr-				! n bytes copied, return
	andi.	%r0, %r9, NBPW-1	! is source address word aligned?
	bne-	.str_loop1
	subi	%r10, %r10, 1

	!
	! Word copy loop.
	!
.strwcopy:
	subf	%r12, %r10, %r5		! %r12 = %r5 - %r10
	srwi	%r12, %r12, 2		! word count in %r12
	slwi	%r12, %r12, 2		! back to byte count in %r12
	! use trick to check if any read bytes of a word are zero
	! the following two constants will generate "byte carries"
	! and check if any bit in a byte is set, if all characters
	! are 7bits (unsigned) this always works, otherwise
	! there is a special case that rarely happens, see below
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
	cmpl	%r12, %r10
	bgt	.str_loop2		! more words to copy
	b	.str_byteloop		! copy remaining bytes

	! check for the zero byte and increment the count appropriately
	! some information (the carry bit) is lost if bit 31
	! was set (very rare), if this is the rare condition,
	! return to the main loop again
.stralign:
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
#ifdef __LITTLE_ENDIAN
	li	%r6, 0xff		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.stralgn1
	stbx	%r8, %r3, %r10		! write the null byte
	addi	%r10, %r10, 1		! count += 1
	b	.strdone
.stralgn1:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.stralgn2
	stbx	%r8, %r3, %r10		! write the first byte 
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	srwi	%r8, %r8, 8
	stbx	%r8, %r3, %r10		! write the null byte 
	addi	%r10, %r10, 1		! count += 1
	b	.strdone
.stralgn2:
	slwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.stralgn3
	stbx	%r8, %r3, %r10		! write first byte
	li	%r0, 0
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	srwi	%r8, %r8, 8
	stbx	%r8, %r3, %r10		! write second byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	srwi	%r8, %r8, 8
	stbx	%r0, %r3, %r10		! write null byte
	addi	%r10, %r10, 1		! count += 1
	b	.strdone
.stralgn3:

	stbx	%r8, %r3, %r10		! write the first byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return

	srwi	%r8, %r8, 8
	stbx	%r8, %r3, %r10		! write the second byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return

	srwi	%r8, %r8, 8
	stbx	%r8, %r3, %r10		! write the third byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return

	srwi	%r8, %r8, 8
	stbx	%r8, %r3, %r10		! write the fourth byte
	addi	%r10, %r10, 1		! count += 1

	li	%r6, 0xff
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.str_loop2		! if not continue the loop
	b	.strdone
#else	/* _BIG_ENDIAN */
	lis	%r6, 0xff00		! mask used to test for terminator
	and.	%r0, %r6, %r8		! check if first byte was zero
	bnz-	.stralgn1
	li	%r0, 0
	stbx	%r0, %r3, %r10		! write the null byte
	addi	%r10, %r10, 1		! count += 1
	b	.strdone
.stralgn1:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if second byte was zero
	bnz-	.stralgn2
	srwi	%r8, %r8, 16		! get to the second byte
	srwi	%r11, %r8, 8
	stbx	%r11, %r3, %r10		! write the first byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	stbx	%r8, %r3, %r10		! write the second byte
	addi	%r10, %r10, 1		! count += 1
	b	.strdone
.stralgn2:
	srwi	%r6, %r6, 8
	and.	%r0, %r6, %r8		! check if third byte was zero
	bnz-	.stralgn3
	srwi	%r8, %r8, 16		! get to the second byte
	srwi	%r11, %r8, 8
	stbx	%r11, %r3, %r10		! write first byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	stbx	%r8, %r3, %r10		! write second byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	li	%r0, 0
	stbx	%r0, %r3, %r10		! write null byte
	addi	%r10, %r10, 1		! count += 1
	b	.strdone
.stralgn3:
	srwi	%r11, %r8, 24
	stbx	%r11, %r3, %r10		! write the first byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	srwi	%r11, %r8, 16
	stbx	%r11, %r3, %r10		! write the second byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	srwi	%r11, %r8, 8
	stbx	%r11, %r3, %r10		! write the third byte
	addi	%r10, %r10, 1		! count += 1
	cmpl	%r5, %r10
	beqlr				! if n bytes copied return
	stbx	%r8, %r3, %r10		! write the null byte
	addi	%r10, %r10, 1		! count += 1
	li	%r6, 0xff
	and.	%r0, %r6, %r8		! check if last byte was zero
	bnz-	.str_loop2		! if not continue the loop
	b	.strdone
#endif	/* __LITTLE_ENDIAN */
	!
	! Byte copy loop.
	!
.str_byteloop:
	cmpl	%r5,%r10
	blelr-
	lbzx	%r7, %r4, %r10		! read the byte
	stbx	%r7, %r3, %r10		! store the byte
	addi	%r10, %r10, 1		! count += 1
	cmpwi	%r7, 0
	bne+	.str_byteloop
.strdone:
	cmpl	%r5, %r10
	blelr-
	li	%r7, 0			! fill remainder with '\0'
	subf	%r5, %r10, %r5
	mtctr	%r5
.strzero:
	stbx	%r7, %r3, %r10
	addi	%r10, %r10, 1
	bdnz	.strzero
	blr

	SET_SIZE(strncpy)
