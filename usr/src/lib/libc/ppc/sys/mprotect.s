/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)mprotect.s	1.3	94/07/04 SMI"

	.file	"mprotect.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	mprotect - set protection of memory mapping
 *
 *   Syntax:	int mprotect(caddr_t addr, size_t len, int prot);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mprotect,function)

#include "SYS.h"

	ENTRY(mprotect)

	SYSTRAP(mprotect)
	SYSCERROR

	RETZ

	SET_SIZE(mprotect)
