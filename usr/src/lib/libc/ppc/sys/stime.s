/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)stime.s	1.4	94/07/04 SMI"

	.file	"stime.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	stime - set system time and date
 *
 *   Syntax:	int stime(const time_t *tp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(stime,function)

#include "SYS.h"

	ENTRY(stime)
	lwz	%r3, 0(%r3)

	SYSTRAP(stime)
	SYSCERROR

	RETZ

	SET_SIZE(stime)
