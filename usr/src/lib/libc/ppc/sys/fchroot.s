/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fchroot.s	1.3	94/07/04 SMI"

	.file	"fchroot.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fchroot - change root directory
 *
 *   Syntax:	int fchroot(int fildes);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fchroot,function)

#include "SYS.h"

	ENTRY(fchroot)

	SYSTRAP(fchroot)
	SYSCERROR

	RET

	SET_SIZE(fchroot)
