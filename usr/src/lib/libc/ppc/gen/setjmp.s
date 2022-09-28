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

.ident "@(#)setjmp.s 1.10      96/05/06 SMI"

/*
 * Setjmp and longjmp implement non-local gotos using state vectors
 * type label_t.
 *
 * setjmp(lp)
 * label_t *lp;
 *
 * The saved state consists of:
 *
 *              +----------------+  0
 *              |      r1 (sp)   |
 *              +----------------+  4
 *              | pc (ret LR )   |
 *              +----------------+  8
 *              |      r2        |
 *              +----------------+  12
 *              |      r13       |
 *              +----------------+  16
 *              |      ...       |
 *              +----------------+  84
 *              |      r31       |
 *              +----------------+  88
 *		|      cr	 |
 *		+----------------+  92
 *		|      fpscr	 |
 *		+----------------+  96
 *		|      f13	 |
 *		+----------------+  104
 *		|      ...	 |
 *		+----------------+  240
 *		|      f31	 |
 *		+----------------+<----- _JBLEN = 62 words = 248 bytes
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setjmp,function)
	ANSI_PRAGMA_WEAK(longjmp,function)

#include "synonyms.h"

#include <sys/trap.h>

/*
 * setjmp(buf_ptr)
 * buf_ptr points to a above array (jmp_buf)
 */
	ENTRY(setjmp)

        mflr    %r4			! get LR
        stw     %r1, 0(%r3)		! save SP while waiting on LR
        mfcr    %r5			! get CR
        stw     %r2,+8(%r3)		! save R2 while waiting on CR

        stw     %r4,+4(%r3)		! save LR
        stw     %r5,+88(%r3)		! save CR

	mffs	%f0			! get fpscr in %fr0

        stw     %r13,+12(%r3)		! save r13 - r31	
        stw     %r14,+16(%r3)	
        stw     %r15,+20(%r3)

	stfd	%f0,+96(%r3)		! store 64 bit status

        stw     %r16,+24(%r3)
        stw     %r17,+28(%r3)

#ifdef	__LITTLE_ENDIAN			! get 32 bit status
	lwz	%r6,+96(%r3)
#else
	lwz	%r6,+100(%r3)
#endif

        stw     %r18,+32(%r3)
        stw     %r19,+36(%r3)
        stw     %r20,+40(%r3)

	stw	%r6,+92(%r3)		! save 32 bit fpscr

        stw     %r21,+44(%r3)
        stw     %r22,+48(%r3)
        stw     %r23,+52(%r3)
        stw     %r24,+56(%r3)
        stw     %r25,+60(%r3)
        stw     %r26,+64(%r3)
        stw     %r27,+68(%r3)
        stw     %r28,+72(%r3)
        stw     %r29,+76(%r3)
        stw     %r30,+80(%r3)
        stw     %r31,+84(%r3)

	stfd	%f13,+96(%r3)		! save f13 - f31
	stfd	%f14,+104(%r3)
	stfd	%f15,+112(%r3)
	stfd	%f16,+120(%r3)
	stfd	%f17,+128(%r3)
	stfd	%f18,+136(%r3)
	stfd	%f19,+144(%r3)
	stfd	%f20,+152(%r3)
	stfd	%f21,+160(%r3)
	stfd	%f22,+168(%r3)
	stfd	%f23,+176(%r3)
	stfd	%f24,+184(%r3)
	stfd	%f25,+192(%r3)
	stfd	%f26,+200(%r3)
	stfd	%f27,+208(%r3)
	stfd	%f28,+216(%r3)
	stfd	%f29,+224(%r3)
	stfd	%f30,+232(%r3)
	stfd	%f31,+240(%r3)

        li      %r3,0                   ! retval is 0
        blr

	SET_SIZE(setjmp)

/*
 * longjmp(buf_ptr, val)
 * buf_ptr points to a jmpbuf which has been initialized by setjmp.
 * val is the value we wish to return to setjmp's caller
 *
 * sp, fp, and %r2, the caller's return address, are all restored
 * to the values they had at the time of the call to setjmp().  All
 * other locals, ins and outs are set to potentially random values
 * (as per the man page).  This is sufficient to permit the correct
 * operation of normal code.
 *
 * Actually, the above description is not quite correct.  If the routine
 * that called setjmp() has not altered the sp value of their frame we
 * will restore the remaining locals and ins to the values these
 * registers had in the this frame at the time of the call to longjmp()
 * (not setjmp()!).  This is intended to help compilers, typically not
 * C compilers, that have some registers assigned to fixed purposes,
 * and that only alter the values of these registers on function entry
 * and exit.
 *
 * Since a C routine could call setjmp() followed by alloca() and thus
 * alter the sp this feature will typically not be helpful for a C
 * compiler.
 *
 */
	ENTRY(longjmp)


        lwz     %r7, 4(%r3)		! get LR
        lwz     %r5,+88(%r3)		! get CR

        lwz     %r1, 0(%r3)		! restore SP
	lwz	%r2,+8(%r3)		! restore R2
	lwz	%r6,+92(%r3)		! get fpscr
	stwu	%r1, -16(%r1)		! create temporary frame

        mtlr    %r7			! restore LR
        mtcrf   0xff, %r5		! restore CR

#ifdef	__LITTLE_ENDIAN			! temporarily store at double algn
	stw	%r6, 0(%r1)
#else
	stw	%r6, 4(%r1)
#endif

        lwz     %r13, 12(%r3)		! restore r13 - r31
        lwz     %r14, 16(%r3)
        lwz     %r15, 20(%r3)

	lfd	%f0, 0(%r1)		! get 64 bit status

	addi	%r1, %r1, 16		! undo temporary frame

        lwz     %r16, 24(%r3)
        lwz     %r17, 28(%r3)
        lwz     %r18, 32(%r3)

	mtfsf	0xff, %f0		! restore fpscr

        lwz     %r19, 36(%r3)
        lwz     %r20, 40(%r3)
        lwz     %r21, 44(%r3)
        lwz     %r22, 48(%r3)
        lwz     %r23, 52(%r3)
        lwz     %r24, 56(%r3)
        lwz     %r25, 60(%r3)
        lwz     %r26, 64(%r3)
        lwz     %r27, 68(%r3)
        lwz     %r28, 72(%r3)
        lwz     %r29, 76(%r3)
        lwz     %r30, 80(%r3)
        lwz     %r31, 84(%r3)

	lfd	%f13,+96(%r3)		! restore f13 - f31
	lfd	%f14,+104(%r3)
	lfd	%f15,+112(%r3)
	lfd	%f16,+120(%r3)
	lfd	%f17,+128(%r3)
	lfd	%f18,+136(%r3)
	lfd	%f19,+144(%r3)
	lfd	%f20,+152(%r3)
	lfd	%f21,+160(%r3)
	lfd	%f22,+168(%r3)
	lfd	%f23,+176(%r3)
	lfd	%f24,+184(%r3)
	lfd	%f25,+192(%r3)
	lfd	%f26,+200(%r3)
	lfd	%f27,+208(%r3)
	lfd	%f28,+216(%r3)
	lfd	%f29,+224(%r3)
	lfd	%f30,+232(%r3)
	lfd	%f31,+240(%r3)

	mr	%r3, %r4
	cmpi	%r3, 0
	bnelr				! return if user's value is not 1, else
        li      %r3, 1                  ! return 1
        blr

	SET_SIZE(longjmp)
