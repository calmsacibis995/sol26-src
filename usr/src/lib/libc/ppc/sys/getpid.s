/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getpid.s	1.3	94/07/04 SMI"

	.file	"getpid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getpid -  get  process ID
 *
 *   Syntax:	pid_t getpid(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getpid,function)

#include "SYS.h"

	ENTRY(getpid)

	SYSTRAP(getpid)

	RET

	SET_SIZE(getpid)
