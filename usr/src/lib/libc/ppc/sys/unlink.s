/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)unlink.s	1.3	94/07/04 SMI"

	.file	"unlink.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	unlink - remove directory entry
 *
 *   Syntax:	int unlink(const char *path);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(unlink,function)

#include "SYS.h"

	ENTRY(unlink)

	SYSTRAP(unlink)
	SYSCERROR

	RETZ

	SET_SIZE(unlink)
