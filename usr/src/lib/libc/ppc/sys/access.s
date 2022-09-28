/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)access.s	1.3	94/07/04 SMI"

	.file	"access.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	access - determine accessibility of a file
 *
 *   Syntax:	int access(const char *path, int amode);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(access,function)

#include "SYS.h"

	ENTRY(access)

	SYSTRAP(access)
	SYSCERROR

	RET

	SET_SIZE(access)
