/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)vhangup.s	1.3	94/07/04 SMI"

	.file	"vhangup.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	vhangup - virtually "hangup" the current controlling
 *		terminal
 *
 *   Syntax:	void vhangup(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(vhangup,function)

#include "SYS.h"

	ENTRY(vhangup)

	SYSTRAP(vhangup)
	SYSCERROR

	RET

	SET_SIZE(vhangup)
