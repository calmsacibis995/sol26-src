/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getpgrp.s	1.3	94/07/04 SMI"

	.file	"getpgrp.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getpgrp - get process group ID of the calling process.
 *
 *   Syntax:	pid_t getpgrp(void);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_getpgrp	0

	ANSI_PRAGMA_WEAK(getpgrp,function)

#include "SYS.h"

	ENTRY(getpgrp)
	li	%r3, SUBSYS_getpgrp

	SYSTRAP(pgrpsys)
	SYSCERROR

	RET

	SET_SIZE(getpgrp)
