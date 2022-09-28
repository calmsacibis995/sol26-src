/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_continue.s	1.3	94/07/04 SMI"

	.file	"_lwp_continue.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_continue - continue LWP execution
 *
 *   Syntax:	int _lwp_continue(lwp_id_t target_lwp);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_continue,function)

#include "SYS.h"

	ENTRY(_lwp_continue)

	SYSTRAP(lwp_continue)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_continue)
