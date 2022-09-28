/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)mincore.s	1.3	94/07/04 SMI"

	.file	"mincore.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	mincore - determine residency of memory pages
 *
 *   Syntax:	int mincore(caddr_t addr, size_t len, char *vec);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mincore,function)

#include "SYS.h"

	ENTRY(mincore)

	SYSTRAP(mincore)
	SYSCERROR

	RETZ

	SET_SIZE(mincore)
