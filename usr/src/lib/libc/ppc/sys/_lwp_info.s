/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_info.s	1.3	94/07/04 SMI"

	.file	"_lwp_info.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_info - retrieve lightweight process information
 *
 *   Syntax:	int _lwp_info(struct lwpinfo *infop);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_info,function)

#include "SYS.h"

	ENTRY(_lwp_info)

	SYSTRAP(lwp_info)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_info)
