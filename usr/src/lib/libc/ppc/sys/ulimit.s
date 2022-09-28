/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)ulimit.s	1.4	94/07/04 SMI"

	.file	"ulimit.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	ulimit - get and set process limits
 *
 *   Syntax:	long ulimit(int cmd, ...);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ulimit,function)

#include "SYS.h"

	ENTRY(ulimit)

	SYSTRAP(ulimit)
	SYSCERROR

	RET

	SET_SIZE(ulimit)
