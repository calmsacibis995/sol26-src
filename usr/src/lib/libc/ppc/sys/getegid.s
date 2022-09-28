/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getegid.s	1.4	94/07/04 SMI"

	.file	"getegid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getegid - get effective group ID
 *
 *   Syntax:	gid_t getegid(void);
 *
 *   Return values from kernel:		(shared syscall: getgid/getegid)
 *   			%r3 = gid
 *			%r4 = egid
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getegid,function)

#include "SYS.h"

	ENTRY(getegid)

	SYSTRAP(getgid)
	mr	%r3, %r4

	RET

	SET_SIZE(getegid)
