/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_setprivate.s	1.3	94/07/04 SMI"

	.file	"_lwp_setprivate.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_setprivate - set LWP specific storage.
 *
 *   Syntax:	void _lwp_setprivate(void *buffer);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_setprivate,function)

#include "SYS.h"

	ENTRY(_lwp_setprivate)

	SYSTRAP(lwp_setprivate)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_setprivate)
