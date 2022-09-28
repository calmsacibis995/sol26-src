/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)utime.s	1.3	94/07/04 SMI"

	.file	"utime.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	utime - set file access and modification times
 *
 *   Syntax:	int utime(const char *path, const struct utimbuf *times);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(utime,function)

#include "SYS.h"

	ENTRY(utime)

	SYSTRAP(utime)
	SYSCERROR

	RET

	SET_SIZE(utime)
