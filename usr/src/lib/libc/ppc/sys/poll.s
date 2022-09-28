/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)poll.s	1.4	94/07/04 SMI"

	.file	"poll.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	poll - input/output multiplexing
 *
 *   Syntax:	int poll(struct poll *fds, size_t nfds, int timeout);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(poll,function)

#include "SYS.h"

	ENTRY(poll)

	SYSTRAP(poll)
	SYSCERROR

	RET

	SET_SIZE(poll)
