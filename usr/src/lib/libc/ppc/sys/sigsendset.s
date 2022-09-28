/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sigsendset.s	1.3	94/07/04 SMI"

	.file	"sigsendset.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigsendset - send a signal to a process or a group of
 *				processes
 *
 *   Syntax:	int sigsendset(procset_t *psp, int sig); *
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sigsendset,function)

#include "SYS.h"

	ENTRY(sigsendset)

	SYSTRAP(sigsendsys)
	SYSCERROR

	RET

	SET_SIZE(sigsendset)
