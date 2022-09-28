/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_sema_wait.s	1.4	96/01/04 SMI"

	.file	"_lwp_sema_wait.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_sema_wait - semaphore operations
 *
 *   Syntax:	int _lwp_sema_wait(_lwp_sema_t *sema);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_sema_wait,function)

#include "SYS.h"

	SYSREENTRY(_lwp_sema_wait)

	SYSTRAP(lwp_sema_wait)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_sema_wait)
