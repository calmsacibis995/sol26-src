/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)link.s	1.3	94/07/04 SMI"

	.file	"link.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	link - link to a file
 *
 *   Syntax:	int link(const char *existing, const char *new);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(link,function)

#include "SYS.h"

	ENTRY(link)

	SYSTRAP(link)
	SYSCERROR

	RETZ

	SET_SIZE(link)
