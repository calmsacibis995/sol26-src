/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)clock_gettime.s	1.2	94/07/04 SMI"

	.file	"clock_gettime.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	clock_gettime - returns the current time in tp for
 *				the specified clock.
 *
 *   Syntax:	int clock_gettime(clockid_t clock_id, timespec_t *tp);
 *
 *   called internally by clock_gettime(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__clock_gettime)

	SYSTRAP(clock_gettime)
	SYSCERROR

	RET

	SET_SIZE(__clock_gettime)
