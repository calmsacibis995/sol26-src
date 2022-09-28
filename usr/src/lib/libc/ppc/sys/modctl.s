/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)modctl.s	1.2	94/07/04 SMI"

	.file	"modctl.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	module management routines
 *
 *   Syntax:	int modctl(int opcode, char *arg);
 *
 */

#include "SYS.h"

	ENTRY(modctl)

	SYSTRAP(modctl)
	SYSCERROR

	RET

	SET_SIZE(modctl)
