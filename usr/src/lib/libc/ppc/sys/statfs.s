/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)statfs.s	1.3	94/07/04 SMI"

	.file	"statfs.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	statfs - get file system information.
 *
 *   Syntax:	int statfs(char *fname, struct statfs *sbp, int len, int fstyp);
 *
 *   NOTE: This system call is obsoleted by statvfs().
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(statfs,function)

#include "SYS.h"

	ENTRY(statfs)

	SYSTRAP(statfs)
	SYSCERROR

	RETZ

	SET_SIZE(statfs)
