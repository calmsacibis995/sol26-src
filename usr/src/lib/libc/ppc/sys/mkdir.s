/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)mkdir.s	1.3	94/07/04 SMI"

	.file	"mkdir.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	mkdir - make a directory
 *
 *   Syntax:	int mkdir(const char *path, mode_t mode);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mkdir,function)

#include "SYS.h"

	ENTRY(mkdir)

	SYSTRAP(mkdir)
	SYSCERROR

	RET

	SET_SIZE(mkdir)
