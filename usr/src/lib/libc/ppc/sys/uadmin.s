/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)uadmin.s	1.3	94/07/04 SMI"

	.file	"uadmin.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	uadmin - administrative control
 *
 *   Syntax:	int uadmin(int cmd, int fcn, int mdep);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(uadmin,function)

#include "SYS.h"

	ENTRY(uadmin)

	SYSTRAP(uadmin)
	SYSCERROR

	RET

	SET_SIZE(uadmin)
