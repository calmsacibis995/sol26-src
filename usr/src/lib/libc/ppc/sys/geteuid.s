/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)geteuid.s	1.4	94/07/04 SMI"

	.file	"geteuid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	geteuid - get effective user ID
 *
 *   Syntax:	uid_t geteuid(void);
 *
 *   Return values from kernel:		(shared syscall: getuid/geteuid)
 *   			%r3 = uid
 *			%r4 = euid
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(geteuid,function)

#include "SYS.h"

	ENTRY(geteuid)

	SYSTRAP(getuid)
	mr	%r3, %r4

	RET

	SET_SIZE(geteuid)
