/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getppid.s	1.3	94/07/04 SMI"

	.file	"getppid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getppid - get parent process ID
 *
 *   Syntax:	pid_t getppid(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getppid,function)

#include "SYS.h"

	ENTRY(getppid)

	SYSTRAP(getpid)
	mr	%r3, %r4

	RET

	SET_SIZE(getppid)
