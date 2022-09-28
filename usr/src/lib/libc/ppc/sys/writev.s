/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)writev.s	1.3	94/07/04 SMI"

	.file	"writev.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	writev - write on a file
 *
 *   Syntax:	int writev(int fildes, const struct iovec *iov, int iovcnt);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(writev,function)

#include "SYS.h"

	SYSREENTRY(writev)

	SYSTRAP(writev)
	SYSRESTART(.restart_writev)

	RET

	SET_SIZE(writev)
