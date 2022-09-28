/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */


#pragma ident	"@(#)_setjmp.s	1.2	94/12/21 SMI"

#if defined(lint)

#include <sys/debug/debugger.h>

#endif	/* lint */

#include <sys/asm_linkage.h>

/*
 * _setjmp( buf_ptr )
 * buf_ptr points to a 23 word array (typedef struct frame in frame.h).
 *
 *		+----------------+
 *   0->	|      r1        |  0
 *		+----------------+
 *		|      pc        |  4
 *		+----------------+
 *		|      cr        |  8
 *		+----------------+
 *		|      r2        | 12
 *		+----------------+
 *		|      r13       | 16
 *		+----------------+
 *		|      ...       |
 *		+----------------+
 *		|      r31       | 88
 *		+----------------+------  (sizeof)jmp_buf = 92 bytes
 */

#if defined(lint)

/* ARGSUSED */
int
_setjmp(struct frame *buf_ptr)
{ return (0); }

#else	/* lint */

	.text

	ENTRY(_setjmp)
	stw	%r1,0(%r3)
	mflr	%r5
	stw	%r5,4(%r3)
	mfcr	%r5
	stw	%r5,8(%r3)
	stw	%r2,12(%r3)
	stw	%r13,16(%r3)
	stw	%r14,20(%r3)
	stw	%r15,24(%r3)
	stw	%r16,28(%r3)
	stw	%r17,32(%r3)
	stw	%r18,36(%r3)
	stw	%r19,40(%r3)
	stw	%r20,44(%r3)
	stw	%r21,48(%r3)
	stw	%r22,52(%r3)
	stw	%r23,56(%r3)
	stw	%r24,60(%r3)
	stw	%r25,64(%r3)
	stw	%r26,68(%r3)
	stw	%r27,72(%r3)
	stw	%r28,76(%r3)
	stw	%r29,80(%r3)
	stw	%r30,84(%r3)
	stw	%r31,88(%r3)
	li	%r3,0			! retval is 0
	blr
	SET_SIZE(_setjmp)

#endif	/* lint */

/*
 * _longjmp ( buf_ptr , val)
 * buf_ptr points to an array which has been initialized by _setjmp.
 */

#if defined(lint)

/* ARGSUSED */
void
_longjmp(struct frame *buf_ptr, int val)
{}

#else	/* lint */

	ENTRY(_longjmp)
	lwz	%r1,0(%r3)
	lwz	%r5,4(%r3)
	mtlr	%r5
	lwz	%r5,8(%r3)
	mtcrf	0xff,%r5
	lwz	%r2,12(%r3)
	lwz	%r13,16(%r3)
	lwz	%r14,20(%r3)
	lwz	%r15,24(%r3)
	lwz	%r16,28(%r3)
	lwz	%r17,32(%r3)
	lwz	%r18,36(%r3)
	lwz	%r19,40(%r3)
	lwz	%r20,44(%r3)
	lwz	%r21,48(%r3)
	lwz	%r22,52(%r3)
	lwz	%r23,56(%r3)
	lwz	%r24,60(%r3)
	lwz	%r25,64(%r3)
	lwz	%r26,68(%r3)
	lwz	%r27,72(%r3)
	lwz	%r28,76(%r3)
	lwz	%r29,80(%r3)
	lwz	%r30,84(%r3)
	lwz	%r31,88(%r3)
	mr	%r3,%r4			! retval is the 2nd argument
	blr
	SET_SIZE(_longjmp)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
_set_pc(struct frame *buf_ptr, func_t val)
{}

#else	/* lint */
#if 1
	ENTRY(_set_pc)
	stw	%r4,4(%r3)	! store the function's entry point
	blr
	SET_SIZE(_set_pc)
#else
	.globl	_set_pc
_set_pc:
	ret
#endif

#endif	/* lint */
