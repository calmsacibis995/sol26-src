/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sigaction.s	1.2	94/07/04 SMI"

	.file	"sigaction.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	detailed signal management
 *
 *   Syntax:	int sigaction(int sig, const struct sigaction *nact,
 *			struct sigaction *oact);
 *
 *   called internally by sigaction(), (libc/ppc/sys/sigaction.c)
 */

#include "SYS.h"

	ENTRY(__sigaction)

	SYSTRAP(sigaction)
	SYSCERROR

	RET

	SET_SIZE(__sigaction)
