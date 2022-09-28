/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)priocntlset.s	1.4	94/07/04 SMI"

	.file	"priocntlset.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	priocntlset - generalized process scheduler control
 *
 *   Syntax:	long priocntlset(procset_t *psp, int cmd, ...);
 *
 */

#include "SYS.h"

	ENTRY(__priocntlset)

	SYSTRAP(priocntlsys)
	SYSCERROR

	RET

	SET_SIZE(__priocntlset)
