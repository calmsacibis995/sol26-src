/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)gettimeofday.s	1.3	94/07/04 SMI"

	.file	"gettimeofday.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	gettimeofday - get the date and time
 *
 *   Syntax:	int gettimeofday(struct timeval *tp, struct timezone *tzp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(gettimeofday,function)

#include "SYS.h"

	ENTRY(gettimeofday)

	SYSTRAP(gettimeofday)
	SYSCERROR

	RET

	SET_SIZE(gettimeofday)
