/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)mknod.s	1.3	94/07/04 SMI"

	.file	"mknod.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	mknod - make a directory, or a special or ordinary file
 *
 *   Syntax:	int mknod(const char *path, mode_t mode, dev_t dev);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mknod,function)

#include "SYS.h"

	ENTRY(mknod)

	SYSTRAP(mknod)
	SYSCERROR

	RETZ

	SET_SIZE(mknod)
