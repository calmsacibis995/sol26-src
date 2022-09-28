/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_cond_broadcast.s	1.3	94/07/04 SMI"

	.file	"_lwp_cond_broadcast.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_cond_broadcast - signal a condition variable
 *
 *   Syntax:	int _lwp_cond_broadcast(lwp_cond_t *cvp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_cond_broadcast,function)

#include "SYS.h"

	ENTRY(_lwp_cond_broadcast)

	SYSTRAP(lwp_cond_broadcast)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_cond_broadcast)
