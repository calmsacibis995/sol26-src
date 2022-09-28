/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_kill.s	1.3	94/07/04 SMI"

	.file	"_lwp_kill.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_kill - send a signal to a LWP
 *
 *   Syntax:	int _lwp_kill(lwp_id_t target_lwp, int sig);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_kill,function)

#include "SYS.h"

	ENTRY(_lwp_kill)

	SYSTRAP(lwp_kill)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_kill)
