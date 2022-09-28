/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_wait.s	1.3	94/07/04 SMI"

	.file	"_lwp_wait.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_wait - wait for a LWP to terminate.
 *
 *   Syntax:	int _lwp_wait(lwp_id_t wait_for, lwp_id_t *departed_lwp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_wait,function)

#include "SYS.h"

	ENTRY(_lwp_wait)

	SYSTRAP(lwp_wait)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_wait)
