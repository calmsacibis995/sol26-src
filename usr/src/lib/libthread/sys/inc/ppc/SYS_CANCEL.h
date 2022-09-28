/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

	.ident	"@(#)SYS_CANCEL.h	1.8	96/03/11 SMI"

#ifndef _LIBTHREAD_CANCEL_SYS_H
#define	_LIBTHREAD_CANCEL_SYS_H

#include <sys/asm_linkage.h>
#include <sys/stack.h>
#include <assym.s>

/*
 * This wrapper provides cancellation point to calling function.
 * It is assumed that function "name" is interposbale and has
 * "newname" defined in respective library, for example libc defines
 * "read" as weak as well as "_read" as strong symbols.
 *
 * This wrapper turns on cancelabilty before calling "-" version of
 * call, restore the the cancelabilty type after return.
 *
 * 	name:
 *		if (cancellation is DISBALED)
 *			go to newname; /returns directly to caller
 *		save previous cancel type
 *		make cancel type = ASYNC (-1) /both ops atomic
 *		if (cancel pending)
 *			_thr_exit(PTHREAD_CANCELED);
 *		newname;
 *		retore previous type;
 *		return;
 *
 * For sparc, we need temp storage to save previous type and %o7.
 * We use 6 words of callee's stack which are used to save arguments
 * in case address is required. These words start from ARGPUSH (stack.h).
 * FIRST_ARG is used by libc for restartable system calls. We will leave
 * SECOND_ARG for future use by libc.
 */

#define	THREAD_REG	%r2
#define	FRAMESIZE	SA(MINFRAME+4)

#define	SYSCALL_CANCELPOINT(name, newname) \
	ENTRY_NP(name); \
	lbz	%r0, T_CANSTATE(THREAD_REG); \
	cmpi	%r0, TC_DISABLE & 0xff	/* if (t->t_can_state == DISABLE) */; \
	beq	.goto/**/name		/*	go to newname		*/; \
	li	%r11,TC_ASYNCHRONOUS; \
	lbz	%r0, T_CANTYPE(THREAD_REG) /* type = t->t_can_type	*/; \
	stb	%r11, T_CANTYPE(THREAD_REG) /* t->t_can_type = ASYNC(-1) */; \
	eieio				/* make store visible		*/; \
	/* store saved "type" byte onto stack (3rd arg) */ \
	lbz	%r0, T_CANPENDING(THREAD_REG); \
	cmpi	%r0, TC_PENDING & 0xff	/* if (t->t_can_pending is not set)*/; \
	beq	.exit/**/name		/* 	go to 1f		*/; \
	stwu	%r1, -FRAMESIZE(%r1)	/* create frame to save type	*/; \
	mflr	%r11			/* get return address		*/; \
	stw	%r0, MINFRAME(%r1)	/* save type			*/; \
	stw	%r11, FRAMESIZE+4(%r1)	/* save return address	*/; \
	bl	newname@PLT		/* call newname with arguments	*/; \
	lwz	%r11, FRAMESIZE+4(%r1)	/* fetch return address	*/; \
	lwz	%r0, MINFRAME(%r1)	/* fetch type			*/; \
	mtlr	%r11			/* set return address		*/; \
	addi	%r1, %r1, FRAMESIZE	/* destroy frame		*/; \
	stb	%r0, T_CANTYPE(THREAD_REG) /* restore t->t_can_type	*/; \
	blr				/* return			*/; \
.exit/**/name:;\
	li	%r3, PTHREAD_CANCELED	/* *status = PTHREAD_CANCELED	*/; \
	bl	_pthread_exit@PLT	/* exit thread with status	*/; \
.goto/**/name: ;\
	b	newname@PLT		/*	go to newname		*/; \
	SET_SIZE(name)



/*
 * Macro to declare a weak symbol alias.  This is similar to
 *	#pragma weak wsym = sym
 */

#define	PRAGMA_WEAK(wsym, sym) \
	.weak	wsym; \
	.set	wsym,sym

#endif	/* _LIBTHREAD_CANCEL_SYS_H */
