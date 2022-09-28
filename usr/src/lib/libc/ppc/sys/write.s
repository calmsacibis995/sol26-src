/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)write.s	1.4	96/05/02 SMI"

	.file	"write.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	write - write on a file
 *
 *   Syntax:	ssize_t write(int fildes, const void *buf, size_t nbyte);
 *
 */

#include <sys/asm_linkage.h>

	.weak	_libc_write;
	.type	_libc_write, @function
	_libc_write = _write

#include "SYS.h"

	SYSREENTRY(write)

	SYSTRAP(write)
	SYSRESTART(.restart_write)

	RET

	SET_SIZE(write)
