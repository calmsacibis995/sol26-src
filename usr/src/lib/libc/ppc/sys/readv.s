/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)readv.s	1.3	94/07/04 SMI"

	.file	"readv.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	readv - read from file
 *
 *   Syntax:	ssize_t readv(int fildes, struct iovec *iov, int iovcnt);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(readv,function)

#include "SYS.h"

	SYSREENTRY(readv)

	SYSTRAP(readv)
	SYSRESTART(.restart_readv)

	RET

	SET_SIZE(readv)
