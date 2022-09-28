/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_getprivate.s	1.3	94/07/04 SMI"

	.file	"_lwp_getprivate.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_getprivate - get LWP specific storage
 *
 *   Syntax:	void *_lwp_getprivate(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_getprivate,function)

#include "SYS.h"

	ENTRY(_lwp_getprivate)

	SYSTRAP(lwp_getprivate)
	SYSCERROR

	RET

	SET_SIZE(_lwp_getprivate)
