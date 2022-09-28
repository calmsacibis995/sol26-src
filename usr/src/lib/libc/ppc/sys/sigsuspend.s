/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sigsuspend.s	1.4	96/05/02 SMI"

	.file	"sigsuspend.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigsuspend - install a signal mask and suspend process
 *		until signal
 *
 *   Syntax:	int sigsuspend(const sigset_t *set);
 *
 */

#include <sys/asm_linkage.h>

	.weak	_libc_sigsuspend;
	.type	_libc_sigsuspend, @function
	_libc_sigsuspend = _sigsuspend

#include "SYS.h"

	ENTRY(sigsuspend)

	SYSTRAP(sigsuspend)
	SYSCERROR

	RET

	SET_SIZE(sigsuspend)
