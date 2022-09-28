/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_suspend.s	1.3	94/07/04 SMI"

	.file	"_lwp_suspend.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_suspend - suspend LWP execution
 *
 *   Syntax:	int _lwp_suspend(lwp_id_t target_lwp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_suspend,function)

#include "SYS.h"

	ENTRY(_lwp_suspend)

	SYSTRAP(lwp_suspend)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_suspend)
