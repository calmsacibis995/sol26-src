/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_exit.s	1.3	94/07/04 SMI"

	.file	"_lwp_exit.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_exit - terminate the calling LWP.
 *
 *   Syntax:	void _lwp_exit(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_exit,function)

#include "SYS.h"

	ENTRY(_lwp_exit)

	SYSTRAP(lwp_exit)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_exit)
