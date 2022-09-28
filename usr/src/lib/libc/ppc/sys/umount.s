/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)umount.s	1.3	94/07/04 SMI"

	.file	"umount.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	umount - unmount a file system
 *
 *   Syntax:	int umount(const char *file);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(umount,function)

#include "SYS.h"

	ENTRY(umount)

	SYSTRAP(umount)
	SYSCERROR

	RETZ

	SET_SIZE(umount)
