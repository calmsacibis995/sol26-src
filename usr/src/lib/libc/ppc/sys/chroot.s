/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)chroot.s	1.3	94/07/04 SMI"

	.file	"chroot.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	chroot - change root directory
 *
 *   Syntax:	int chroot(const char *path);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(chroot,function)

#include "SYS.h"

	ENTRY(chroot)

	SYSTRAP(chroot)
	SYSCERROR

	RET

	SET_SIZE(chroot)
