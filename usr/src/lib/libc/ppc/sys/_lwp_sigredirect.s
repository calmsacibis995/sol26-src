/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident "@(#)_lwp_sigredirect.s	1.1	95/04/05 SMI"

	.file	"_lwp_sigredirect.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_sigredirect
 *
 *   Syntax:	int _lwp_sigredirect(int lwpid, int sig);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_sigredirect,function)

#include "SYS.h"

	ENTRY(_lwp_sigredirect)

	SYSTRAP(lwp_sigredirect)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_sigredirect)
