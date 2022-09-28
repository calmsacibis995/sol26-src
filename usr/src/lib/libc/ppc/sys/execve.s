/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)execve.s	1.3	94/07/04 SMI"

	.file	"execve.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	execve - execute a file
 *
 *   Syntax:	int execve (const char *path, char *const argv[],
 *		char *const envp[]);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(execve,function)

#include "SYS.h"

	ENTRY(execve)

	SYSTRAP(execve)
	SYSCERROR

	SET_SIZE(execve)
