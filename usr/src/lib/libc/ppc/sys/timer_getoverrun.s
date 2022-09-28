/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)timer_getoverrun.s	1.2	94/07/04 SMI"

	.file	"timer_getoverrun.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	timer_getoverrun - get the timer expiration overrun
 *			count for the specified high resolution timer.
 *
 *   Syntax:	int timer_getoverrun(timer_t timerid);
 *
 *   called internally by timer_getoverrun(),
 *					(libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__timer_getoverrun)

	SYSTRAP(timer_getoverrun)
	SYSCERROR

	RET

	SET_SIZE(__timer_getoverrun)
