/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getgid.s	1.4	94/07/04 SMI"

	.file	"getgid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	get real group ID
 *
 *   Syntax:	gid_t getgid(void);
 *
 *   Return values from kernel:		(shared syscall: getgid/getegid)
 *   			%r3 = gid
 *			%r4 = egid
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getgid,function)

#include "SYS.h"

	ENTRY(getgid)

	SYSTRAP(getgid)

	RET

	SET_SIZE(getgid)
