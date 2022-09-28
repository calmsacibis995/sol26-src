/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)read.s	1.4	96/05/02 SMI"

	.file	"read.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	read - read from file
 *
 *   Syntax:	size_t read(int fildes, void *buf, size_t nbyte);
 *
 */

#include <sys/asm_linkage.h>


	.weak	_libc_read;
	.type	_libc_read, @function
	_libc_read = _read

#include "SYS.h"

	SYSREENTRY(read)

	SYSTRAP(read)
	SYSRESTART(.restart_read)

	RET

	SET_SIZE(read)
