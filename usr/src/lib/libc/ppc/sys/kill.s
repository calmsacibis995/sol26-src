/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)kill.s	1.3	94/07/04 SMI"

	.file	"kill.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	kill - send a signal to a process or a group of processes
 *
 *   Syntax:	int kill(pid_t pid, int sig);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(kill,function)

#include "SYS.h"

	ENTRY(kill)

	SYSTRAP(kill)
	SYSCERROR

	RETZ

	SET_SIZE(kill)
