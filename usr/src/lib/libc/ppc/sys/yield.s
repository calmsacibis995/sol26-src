/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)yield.s	1.3	94/07/04 SMI"

	.file	"yield.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	yield - yield execution to another lightweight process
 *
 *   Syntax:	void yield(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(yield,function)

#include "SYS.h"

	ENTRY(yield)

	SYSTRAP(yield)
	RET

	SET_SIZE(yield)
