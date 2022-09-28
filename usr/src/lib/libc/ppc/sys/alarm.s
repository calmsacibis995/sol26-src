/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident "@(#)alarm.s	1.5	96/03/08 SMI"

	.file	"alarm.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	alarm - set a process alarm clock
 *
 *   Syntax:	unsigned alarm(unsigned sec);
 *
 */

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_alarm)

	SYSTRAP(alarm)

	RET

	SET_SIZE(_libc_alarm)
