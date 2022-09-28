/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident "@(#)sigtimedwait.s	1.3	95/09/25 SMI"

	.file	"sigtimedwait.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigtimedwait - wait for queued signals
 *
 *   Syntax:	int __sigtimedwait(const sigset_t *set, siginto_t *info,
 *				const struct timespec *timeout);
 *
 *   called internally by sigtimedwait(), sigwaitinfo(),
 *					(libposix4/common/sigrt.c)
 */

#include "SYS.h"

	ENTRY(_libc_sigtimedwait)

	SYSTRAP(sigtimedwait)
	SYSCERROR

	RET

	SET_SIZE(_libc_sigtimedwait)
