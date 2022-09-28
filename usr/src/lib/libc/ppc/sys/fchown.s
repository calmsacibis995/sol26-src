/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fchown.s	1.3	94/07/04 SMI"

	.file	"fchown.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fchown - change owner and group of a file
 *
 *   Syntax:	int fchown(int fildes, uid_t owner, gid_t group);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fchown,function)

#include "SYS.h"

	ENTRY(fchown)

	SYSTRAP(fchown)
	SYSCERROR

	RETZ

	SET_SIZE(fchown)
