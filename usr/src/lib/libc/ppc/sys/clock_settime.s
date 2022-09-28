/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)clock_settime.s	1.2	94/07/04 SMI"

	.file	"clock_settime.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	clock_settime - sets the specified clock to the value
 *				specified by tp.
 *
 *   Syntax:	int clock_settime(clockid_t clock_id, timespec_t *tp);
 *
 *   called internally by clock_settime(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__clock_settime)

	SYSTRAP(clock_settime)
	SYSCERROR

	RET

	SET_SIZE(__clock_settime)
