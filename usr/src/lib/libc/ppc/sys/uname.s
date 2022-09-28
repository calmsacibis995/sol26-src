/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)uname.s	1.3	94/07/04 SMI"

	.file	"uname.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	uname - get name of current operating system
 *
 *   Syntax:	int uname(struct utsname *name);
 *
 *   (was formerly called nuname().)
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(uname,function)

#include "SYS.h"

	ENTRY(uname)

	SYSTRAP(uname)
	SYSCERROR

	RET

	SET_SIZE(uname)
