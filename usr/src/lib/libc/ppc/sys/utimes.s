/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)utimes.s	1.3	94/07/04 SMI"

	.file	"utimes.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	utimes - set file times
 *
 *   Syntax:	int utimes(const char *file, struct timeval *tvp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(utimes,function)

#include "SYS.h"

	ENTRY(utimes)

	SYSTRAP(utimes)
	SYSCERROR

	RET

	SET_SIZE(utimes)
