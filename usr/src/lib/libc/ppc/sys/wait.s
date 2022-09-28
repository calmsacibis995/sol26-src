/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)wait.s	1.5	96/05/02 SMI"

	.file	"wait.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	wait - wait for child process to stop or terminate
 *
 *   Syntax:	pid_t wait(int *stat_loc);
 *
 */

#include <sys/asm_linkage.h>

	.weak	_libc_wait;
	.type	_libc_wait, @function
	_libc_wait = _wait

#include "SYS.h"

	SYSREENTRY(wait)

	SYSTRAP(wait)
	SYSRESTART(.restart_wait)
	cmpwi	%r10, 0
	beqlr+

	stw	%r4, 0(%r10)
	RET

	SET_SIZE(wait)
