/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)symlink.s	1.3	94/07/04 SMI"

	.file	"symlink.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	symlink - make a symbolic link to a file
 *
 *   Syntax:	int symlink(const char *name1, const char *name2);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(symlink,function)

#include "SYS.h"

	ENTRY(symlink)

	SYSTRAP(symlink)
	SYSCERROR

	RETZ

	SET_SIZE(symlink)
