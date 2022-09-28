/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)chown.s	1.3	94/07/04 SMI"

	.file	"chown.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	chown - change owner and group of a file
 *
 *   Syntax:	int chown(const char *path, uid_t owner, gid_t group);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(chown,function)

#include "SYS.h"

	ENTRY(chown)

	SYSTRAP(chown)
	SYSCERROR

	RETZ

	SET_SIZE(chown)
