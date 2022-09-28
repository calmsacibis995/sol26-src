/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)adjtime.s	1.3	94/07/04 SMI"

	.file	"adjtime.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	adjtime - correct the time to allow synchronization
 *			of the system clock
 *
 *   Syntax:	int adjtime(const struct timeval *delta,
 *		struct timeval *olddelta);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(adjtime,function)

#include "SYS.h"

	ENTRY(adjtime)

	SYSTRAP(adjtime)
	SYSCERROR

	RETZ

	SET_SIZE(adjtime)
