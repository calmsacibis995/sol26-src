/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)nanosleep.s	1.3	96/03/11 SMI"

	.file	"nanosleep.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	nanosleep - high resolution sleep
 *
 *   Syntax:	int nanosleep(const struct timespec *rqtp,
 *				struct timespec *rmtp);
 *
 *   called internally by nanosleep(), (libposix4/common/clock_timer.c)
 */

#include "SYS.h"

	ENTRY(_libc_nanosleep)

	SYSTRAP(nanosleep)
	SYSCERROR

	RET

	SET_SIZE(_libc_nanosleep)
