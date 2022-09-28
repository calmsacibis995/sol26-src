/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fstatfs.s	1.3	94/07/04 SMI"

	.file	"fstatfs.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fstatfs - get file system information.
 *
 *   Syntax:	int fstatfs(int fildes, struct statfs *sbp, int len,
 *			int fstyp);
 *
 *   NOTE: This system call is obsoleted by fstatvfs().
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fstatfs,function)

#include "SYS.h"

	ENTRY(fstatfs)

	SYSTRAP(fstatfs)
	SYSCERROR

	RETZ

	SET_SIZE(fstatfs)
