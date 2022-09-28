/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)readlink.s	1.3	94/07/04 SMI"

	.file	"readlink.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	readlink - read the value of a symbolic link
 *
 *   Syntax:	int readlink(const char *path, void *buf, size_t bufsiz);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(readlink,function)

#include "SYS.h"

	ENTRY(readlink)

	SYSTRAP(readlink)
	SYSCERROR

	RET

	SET_SIZE(readlink)
