/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)umask.s	1.3	94/07/04 SMI"

	.file	"umask.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	umask - set and get file creation mask
 *
 *   Syntax:	mode_t umask(mode_t cmask);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(umask,function)

#include "SYS.h"

	ENTRY(umask)

	SYSTRAP(umask)
	SYSCERROR

	RET

	SET_SIZE(umask)
