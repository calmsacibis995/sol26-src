/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_cond_wait.s	1.3	94/07/04 SMI"

	.file	"_lwp_cond_wait.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_cond_wait - wait on a condition variable
 *
 *   Syntax:	int _lwp_cond_wait(lwp_cond_t *cvp, lwp_mutex_t *mp,
 *				timestruct_t *ts);
 *
 *   called internally by _lwp_cond_wait(), (port/sys/lwp_cond.c)
 */

#include <sys/asm_linkage.h>

	ENTRY(___lwp_cond_wait)

#include "SYS.h"

	SYSTRAP(lwp_cond_wait)
	SYSLWPERR

	RET

	SET_SIZE(___lwp_cond_wait)
