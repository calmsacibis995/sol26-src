/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setsid.s	1.3	94/07/04 SMI"

	.file	"setsid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setsid - set session ID.
 *
 *   Syntax:	pid_t setsid(void);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_setsid	3

	ANSI_PRAGMA_WEAK(setsid,function)

#include "SYS.h"

	ENTRY(setsid)
	li	%r3, SUBSYS_setsid

	SYSTRAP(pgrpsys)
	SYSCERROR

	RET

	SET_SIZE(setsid)
