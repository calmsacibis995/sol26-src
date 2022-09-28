/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sigpending.s	1.5	96/05/02 SMI"

	.file	"sigpending.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigpending - examine signals that are blocked and pending
 *
 *   Syntax:	int sigpending(sigset_t *set);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_sigpending	1

/*
 * The following flag and the __mt_sigpending entry point represents a
 * private interface between libthread and libc. Only libthread should
 * use this entry point and flag. Note that the value of 2 for the SUBSYS
 * flag is used by the __sigfillset() entry point. So __mt_sigpending() use
 * the value 3 below.
 */

#define	SUBSYS_mt_sigpending	3

	.weak	_libc_sigpending;
	.type	_libc_sigpending, @function
	_libc_sigpending = _sigpending

#include "SYS.h"


	ENTRY(sigpending)
	mr	%r4, %r3
	li	%r3, SUBSYS_sigpending

	SYSTRAP(sigpending)
	SYSCERROR

	RET

	SET_SIZE(sigpending)

	ENTRY(__mt_sigpending)
	mr	%r4, %r3
	li	%r3, SUBSYS_mt_sigpending

	SYSTRAP(sigpending)
	SYSCERROR

	RET

	SET_SIZE(__mt_sigpending)
