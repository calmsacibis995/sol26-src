/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)close.s	1.4	96/05/02 SMI"

	.file	"close.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	close - close a file descriptor
 *
 *   Syntax:	int close(int fildes);
 *
 */

#include <sys/asm_linkage.h>

	.weak	_libc_close;
	.type	_libc_close, @function
	_libc_close = _close

#include "SYS.h"

	ENTRY(close)

	SYSTRAP(close)
	SYSCERROR

	RETZ

	SET_SIZE(close)
