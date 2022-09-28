/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_create.s	1.3	94/07/04 SMI"

	.file	"_lwp_create.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_create - create a new light-weight process (LWP)
 *
 *   Syntax:	int _lwp_create(ucontext_t *contextp, unsigned long flags,
 *		lwp_id_t *new_lwp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_create,function)

#include "SYS.h"

	ENTRY(_lwp_create)

	SYSTRAP(lwp_create)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_create)
