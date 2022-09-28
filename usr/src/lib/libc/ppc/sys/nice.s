/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)nice.s	1.3	94/07/04 SMI"

	.file	"nice.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	nice - change priority of a time-sharing process
 *
 *   Syntax:	int nice(int incr);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(nice,function)

#include "SYS.h"

	ENTRY(nice)

	SYSTRAP(nice)
	SYSCERROR

	RET

	SET_SIZE(nice)
