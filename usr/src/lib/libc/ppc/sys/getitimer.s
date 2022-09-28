/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getitimer.s	1.3	94/07/04 SMI"

	.file	"getitimer.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getitimer - get value of interval timer
 *
 *   Syntax:	int getitimer(int which, struct itimerval *value);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getitimer,function)

#include "SYS.h"

	ENTRY(getitimer)

	SYSTRAP(getitimer)
	SYSCERROR

	RET

	SET_SIZE(getitimer)
