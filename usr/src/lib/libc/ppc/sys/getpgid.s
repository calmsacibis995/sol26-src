/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getpgid.s	1.3	94/07/04 SMI"

	.file	"getpgid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getpgid - get process group ID of specified process.
 *
 *   Syntax:	pid_t getpgid(pid_t pid);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_getpgid	4

	ANSI_PRAGMA_WEAK(getpgid,function)

#include "SYS.h"

	ENTRY(getpgid)
	mr	%r4, %r3
	li	%r3, SUBSYS_getpgid

	SYSTRAP(pgrpsys)
	SYSCERROR

	RET

	SET_SIZE(getpgid)
