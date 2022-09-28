/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved
 */
 
	.ident "@(#)sigprocmsk.s	1.5	96/03/08 SMI"

	.file	"sigprocmsk.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigprocmask - change and/or examine calling process's
 *		signal mask
 *
 *   Syntax:	int sigprocmask(int how, const sigset_t *set, sigset_t *oset);
 *
 */

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_sigprocmask)

	SYSTRAP(sigprocmask)
	SYSCERROR

	RET

	SET_SIZE(_libc_sigprocmask)
