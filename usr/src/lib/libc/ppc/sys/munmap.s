/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)munmap.s	1.3	94/07/04 SMI"

	.file	"munmap.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	munmap - unmap pages of memory
 *
 *   Syntax:	int munmap(caddr_t addr, size_t len);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(munmap,function)

#include "SYS.h"

	ENTRY(munmap)

	SYSTRAP(munmap)
	SYSCERROR

	RETZ

	SET_SIZE(munmap)
