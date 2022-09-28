/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)timer_gettime.s	1.2	94/07/04 SMI"

	.file	"timer_gettime.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	timer_gettime - retrieves the time until expiration
 *				and interval reload for specified
 *				high resolution timer.
 *
 *   Syntax:	int timer_gettime(timer_t timerid,
 *				struct itimerspec *value);
 *
 *   called internally by timer_gettime(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__timer_gettime)

	SYSTRAP(timer_gettime)
	SYSCERROR

	RET

	SET_SIZE(__timer_gettime)
