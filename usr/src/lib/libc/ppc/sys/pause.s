/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)pause.s	1.4	96/05/02 SMI"

	.file	"pause.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	pause - suspend process until signal
 *
 *   Syntax:	int pause(void);
 *
 */

#include <sys/asm_linkage.h>


	.weak	_libc_pause;
	.type	_libc_pause, @function
	_libc_pause = _pause

#include "SYS.h"

	ENTRY(pause)

	SYSTRAP(pause)
	SYSCERROR

	RET

	SET_SIZE(pause)
