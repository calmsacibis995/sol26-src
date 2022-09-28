/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_sema_post.s	1.3	94/07/04 SMI"

	.file	"_lwp_sema_post.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_sema_post - semaphore operations
 *
 *   Syntax:	int _lwp_sema_post(_lwp_sema_t *sema);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_sema_post,function)

#include "SYS.h"

	ENTRY(_lwp_sema_post)

	SYSTRAP(lwp_sema_post)
	SYSLWPERR

	RET

	SET_SIZE(_lwp_sema_post)
