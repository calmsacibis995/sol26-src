/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)chdir.s	1.3	94/07/04 SMI"

	.file	"chdir.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	chdir - change working directory
 *
 *   Syntax:	int chdir(const char *path);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(chdir,function)

#include "SYS.h"

	ENTRY(chdir)

	SYSTRAP(chdir)
	SYSCERROR

	RETZ

	SET_SIZE(chdir)
