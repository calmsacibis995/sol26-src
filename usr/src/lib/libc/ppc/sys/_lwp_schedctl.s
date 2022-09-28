/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
        .ident  "@(#)_lwp_schedctl.s 1.1     96/05/20 SMI"

	.file	"_lwp_schedctl.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_schedctl - set up scheduler control functions
 *
 *   Syntax:	int _lwp_schedctl(unsigned int flags, int upcall_did,
 *				sc_shared_t **addrp);
 *
 *   Called by libthread and libsched.
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_schedctl,function)

#include "SYS.h"

	ENTRY(_lwp_schedctl)

	SYSTRAP(schedctl)
	SYSCERROR

	RET

	SET_SIZE(_lwp_schedctl)
