/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)lchown.s	1.3	94/07/04 SMI"

	.file	"lchown.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	lchown - change owner and group of a file
 *
 *   Syntax:	int lchown(const char *path, uid_t owner, gid_t group);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lchown,function)

#include "SYS.h"

	ENTRY(lchown)

	SYSTRAP(lchown)
	SYSCERROR

	RETZ

	SET_SIZE(lchown)
