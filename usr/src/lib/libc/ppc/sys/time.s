/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)time.s	1.6	95/01/03 SMI"

	.file	"time.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	time - get time
 *
 *   Syntax:	time_t time(time_t *tloc);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(time,function)

#include "SYS.h"

	ENTRY(time)

	mr	%r5,%r3		! save pointer
	SYSTRAP(time)
	cmpwi	%r5, 0
	beq	.no_store
	stw	%r3,0(%r5)
.no_store:

	RET

	SET_SIZE(time)
