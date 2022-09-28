/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)dup.s	1.3	94/07/04 SMI"

	.file	"dup.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	dup - duplicate an open file descriptor
 *
 *   Syntax:	int dup(int fildes);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(dup,function)

#include "SYS.h"

	ENTRY(dup)

	SYSTRAP(dup)
	SYSCERROR

	RET

	SET_SIZE(dup)
