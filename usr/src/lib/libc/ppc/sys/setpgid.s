/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setpgid.s	1.3	94/07/04 SMI"

	.file	"setpgid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setpgid - set process group ID.
 *
 *   Syntax:	int setpgid(pid_t pid, pid_t pgid);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_setpgid	5

	ANSI_PRAGMA_WEAK(setpgid,function)

#include "SYS.h"

	ENTRY(setpgid)
	mr	%r5, %r4
	mr	%r4, %r3
	li	%r3, SUBSYS_setpgid

	SYSTRAP(pgrpsys)
	SYSCERROR

	RET

	SET_SIZE(setpgid)
