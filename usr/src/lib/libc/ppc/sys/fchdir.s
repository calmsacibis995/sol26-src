/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fchdir.s	1.3	94/07/04 SMI"

	.file	"fchdir.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fchdir - change working directory
 *
 *   Syntax:	int fchdir(int fildes);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fchdir,function)

#include "SYS.h"

	ENTRY(fchdir)

	SYSTRAP(fchdir)
	SYSCERROR

	RETZ

	SET_SIZE(fchdir)
