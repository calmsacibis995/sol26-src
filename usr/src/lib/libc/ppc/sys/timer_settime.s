/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)timer_settime.s	1.2	94/07/04 SMI"

	.file	"timer_settime.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	timer_settime - sets expiration time for specified
 *				high resolution timer.
 *
 *   Syntax:	int timer_settime(timer_t timerid, int flags,
 *				const struct itimerspec *value,
 *				struct itimerspec *ovalue);
 *
 *   called internally by timer_settime(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__timer_settime)

	SYSTRAP(timer_settime)
	SYSCERROR

	RET

	SET_SIZE(__timer_settime)
