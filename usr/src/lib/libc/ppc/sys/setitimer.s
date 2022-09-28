/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident "@(#)setitimer.s	1.5	96/03/08 SMI"

	.file	"setitimer.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setitimer - set value of interval timer
 *
 *   Syntax:	int setitimer(int which, const struct itimerval *value,
 *				struct itimerval *ovalue);
 *
 */

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_setitimer)

	SYSTRAP(setitimer)
	SYSCERROR

	RET

	SET_SIZE(_libc_setitimer)
