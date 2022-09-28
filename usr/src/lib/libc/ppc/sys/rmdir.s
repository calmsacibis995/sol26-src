/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)rmdir.s	1.3	94/07/04 SMI"

	.file	"rmdir.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	rmdir - remove a directory
 *
 *   Syntax:	int rmdir(const char *path);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(rmdir,function)

#include "SYS.h"

	ENTRY(rmdir)

	SYSTRAP(rmdir)
	SYSCERROR

	RET

	SET_SIZE(rmdir)
