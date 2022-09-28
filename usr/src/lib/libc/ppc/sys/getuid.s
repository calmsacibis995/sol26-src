/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getuid.s	1.4	94/07/04 SMI"

	.file	"getuid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getuid - get real user ID
 *
 *   Syntax:	uid_t getuid(void);
 *
 *   Return values from kernel:		(shared syscall: getuid/geteuid)
 *   			%r3 = uid
 *			%r4 = euid
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getuid,function)

#include "SYS.h"

	ENTRY(getuid)

	SYSTRAP(getuid)

	RET

	SET_SIZE(getuid)
