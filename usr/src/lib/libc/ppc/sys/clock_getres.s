/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)clock_getres.s	1.2	94/07/04 SMI"

	.file	"clock_getres.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	clock_getres - returns the resolution of the specified
 *				clock
 *
 *   Syntax:	int clock_getres(clockid_t clock_id, struct timespec *res)
 *
 *   called internally by clock_getres(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__clock_getres)

	SYSTRAP(clock_getres)
	SYSCERROR

	RET

	SET_SIZE(__clock_getres)
