/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_cond_signal.s	1.3	94/07/04 SMI"

	.file	"_lwp_cond_signal.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_cond_signal - signal a condition variable
 *
 *   Syntax:	int _lwp_cons_signal(lwp_cond_t cvp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_cond_signal,function)

#include "SYS.h"

	ENTRY(_lwp_cond_signal)

	SYSTRAP(lwp_cond_signal)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_cond_signal)
