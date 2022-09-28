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

.ident "@(#)memmove.s 1.13      95/09/27 SMI"

/*
 * memmove(s1, s2, len)
 * Copy s2 to s1, always copy n bytes.
 * For overlapped copies it does the right thing.
 */
/*
 * void * 
 * memmove(s, s0, n )
 * void *s;
 * const void *s0;
 * register size_t n;
 * {
 *	if (n != 0) {
 *   		register char *s1 = s;
 *		register const char *s2 = s0;
 *		if (s1 <= s2) {
 *			do {
 *				*s1++ = *s2++;
 *			} while (--n != 0);
 *		} else {
 *			s2 += n;
 *			s1 += n;
 *			do {
 *				*--s1 = *--s2;
 *			} while (--n != 0);
 *		}
 *	}
 *	return ( s );
 * }
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memmove,function)

#include "synonyms.h"


/*
 *	WARNING : This code assumes NBPW == 4
 */

	ENTRY(memmove)

	cmpi	%r5,0			! Anything to do?
	beqlr				! if not, return.

	mr	%r6, %r3		! Use r6 for pointer to dest to
					! preserve r3 for return value.
	cmpl	%r6,%r4
	bge	.backward

					! (s1 < s2) so do straight cpy s2 -> s1

					! Test buffer alignment
	or	%r11,%r6,%r4
	andi.	%r11,%r11,3
	bnz	.fbytecopy

					! Buffers are both aligned.

	srwi.	%r11,%r5,2		! number of words
	bz	.fbytecopy		! if less than 1 word, byte copy
	mtctr	%r11			! into counter

					! Word copy leading whole words.

	addi	%r6, %r6, -4		! set up for pre-inc "u" instructions
	addi	%r4, %r4, -4

.fwordloop:
	lwzu	%r11,4(%r4)
	stwu	%r11,4(%r6)
	bdnz	.fwordloop

	andi.	%r5,%r5,3		! Remaining bytes
	beqlr				! If none, return.

	addi	%r6, %r6, 4
	addi	%r4, %r4, 4

					! fall through to...

.fbytecopy:				! Byte copy
	addi	%r6, %r6, -1		! set up for pre-inc "u" instructions
	addi	%r4, %r4, -1
	mtctr	%r5

.fbyteloop:
	lbzu	%r11,1(%r4)
	stbu	%r11,1(%r6)
	bdnz	.fbyteloop

	blr				! Done!

.backward:				! (s1 >= s2) backward cpy
	beqlr				! If copy to self, return.

	add	%r6,%r6,%r5		! r6 points to after dest
	add	%r4,%r4,%r5		! r4 points to after src

					! Test alignment of buffer tails
	or	%r11,%r6,%r4
	andi.	%r11,%r11,3
	bnz	.rbytecopy
					! Tails of both buffers are aligned

	srwi.	%r11,%r5,2		! number of words
	bz	.rbytecopy		! if less than 1 word, byte copy
	mtctr	%r11			! into counter

					! Word copy trailing whole words
					! Note no adjustment needed for
					! pre-inc 'u' instructions because
					! we're already pointing after the
					! buffers.
.rwordloop:
	lwzu	%r11,-4(%r4)
	stwu	%r11,-4(%r6)
	bdnz	.rwordloop

	andi.	%r5,%r5,3		! remaining bytes
	beqlr				! if none, return.

.rbytecopy:
	mtctr	%r5
					! Once again, no adjustment for
					! 'u' instructions because the pointers
					! are already pointing after the
					! areas to be copied.
.rbyteloop:
	lbzu	%r11,-1(%r4)
	stbu	%r11,-1(%r6)
	bdnz	.rbyteloop

	blr				! done!

	SET_SIZE(memmove)
