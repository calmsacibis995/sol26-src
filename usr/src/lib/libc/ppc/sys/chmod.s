/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)chmod.s	1.3	94/07/04 SMI"

	.file	"chmod.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	chmod - change access permission mode of file
 *
 *   Syntax:	int chmod(const char *path, mode_t mode);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(chmod,function)

#include "SYS.h"

	ENTRY(chmod)

	SYSTRAP(chmod)
	SYSCERROR

	RETZ

	SET_SIZE(chmod)
