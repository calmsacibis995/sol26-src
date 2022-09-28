/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setpgrp.s	1.3	94/07/04 SMI"

	.file	"setpgrp.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setpgrp - set process group ID
 *
 *   Syntax:	pid_t setpgrp(void);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_setpgrp	1

	ANSI_PRAGMA_WEAK(setpgrp,function)

#include "SYS.h"

	ENTRY(setpgrp)
	li	%r3, SUBSYS_setpgrp

	SYSTRAP(pgrpsys)
	SYSCERROR

	RET

	SET_SIZE(setpgrp)
