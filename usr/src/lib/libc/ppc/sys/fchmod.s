/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fchmod.s	1.3	94/07/04 SMI"

	.file	"fchmod.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fchmod - change access permission mode of file
 *
 *   Syntax:	int fchmod(int fildes, mode_t mode);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fchmod,function)

#include "SYS.h"

	ENTRY(fchmod)

	SYSTRAP(fchmod)
	SYSCERROR

	RETZ

	SET_SIZE(fchmod)
