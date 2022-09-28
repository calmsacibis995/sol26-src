/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sigqueue.s	1.2	94/07/04 SMI"

	.file	"sigqueue.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigqueue - queue a signal to a process
 *
 *   Syntax:	int sigqueue(pid_t pid, int signo,
 *				const union sigval value);
 *
 *   called internally by sigqueue(), (libposix4/common/sigrt.c)
 */

#include "SYS.h"

	ENTRY(__sigqueue)

	SYSTRAP(sigqueue)
	SYSCERROR

	RET

	SET_SIZE(__sigqueue)
