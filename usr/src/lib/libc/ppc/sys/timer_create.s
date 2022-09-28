/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)timer_create.s	1.2	94/07/04 SMI"

	.file	"timer_create.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	timer_create - creates a high resolution timer per-LWP.
 *
 *   Syntax:	int timer_create(clockid_t clock_id, struct sigevent *evp,
 *				timer_t *timerid);
 *
 *   called internally by timer_create(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(__timer_create)

	SYSTRAP(timer_create)
	SYSCERROR

	RET

	SET_SIZE(__timer_create)
