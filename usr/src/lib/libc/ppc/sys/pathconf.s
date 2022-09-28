/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)pathconf.s	1.3	94/07/04 SMI"

	.file	"pathconf.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	pathconf - get configurable pathname variables
 *
 *   Syntax:	long pathconf(const char *path, int name);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pathconf,function)

#include "SYS.h"

	ENTRY(pathconf)

	SYSTRAP(pathconf)
	SYSCERROR

	RET

	SET_SIZE(pathconf)
