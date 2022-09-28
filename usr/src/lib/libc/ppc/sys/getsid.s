/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getsid.s	1.3	94/07/04 SMI"

	.file	"getsid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getsid - get session ID
 *
 *   Syntax:	pid_t getsid(pid_t pid);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_getsid	2

	ANSI_PRAGMA_WEAK(getsid,function)

#include "SYS.h"

	ENTRY(getsid)
	mr	%r4, %r3
	li	%r3, SUBSYS_getsid

	SYSTRAP(pgrpsys)
	SYSCERROR

	RET

	SET_SIZE(getsid)
