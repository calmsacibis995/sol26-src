/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fpathconf.s	1.3	94/07/04 SMI"

	.file	"fpathconf.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fpathconf - get configurable pathname variables
 *
 *   Syntax:	long fpathconf(int fildes, int name);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpathconf,function)

#include "SYS.h"

	ENTRY(fpathconf)

	SYSTRAP(fpathconf)
	SYSCERROR

	RET

	SET_SIZE(fpathconf)
