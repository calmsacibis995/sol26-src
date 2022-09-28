/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)timer_delete.s	1.2	94/07/04 SMI"

	.file	"timer_delete.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	timer_delete - deletes the specified high resolution timer.
 *
 *   Syntax:	int timer_delete(timer_t timerid);
 *
 *   called internally by timer_delete(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__timer_delete)

	SYSTRAP(timer_delete)
	SYSCERROR

	RET

	SET_SIZE(__timer_delete)
