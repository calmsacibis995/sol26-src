/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sigfillset.s	1.3	94/07/04 SMI"

	.file	"sigfillset.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigfillset - initialize signal set to include all
 *				signals defined by the system.
 *
 *   Syntax:	int sigfillset(sigset_t *set);
 *
 */

#include "SYS.h"

#define	SUBSYS_sigfillset	2

	ENTRY(__sigfillset)
	mr	%r4, %r3
	li	%r3, SUBSYS_sigfillset

	SYSTRAP(sigpending)
	SYSCERROR

	RET

	SET_SIZE(__sigfillset)
