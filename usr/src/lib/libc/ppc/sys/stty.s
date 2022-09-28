/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)stty.s	1.3	94/07/04 SMI"

	.file	"stty.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	stty - set terminal state (defunct)
 *
 *   Syntax:	int stty(int fd, struct sgttyb *buf);
 *
 *   NOTE: This system call is obsoleted by "ioctl(fd, TIOCSETP, buf)"
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(stty,function)

#include "SYS.h"

	ENTRY(stty)

	SYSTRAP(stty)
	SYSCERROR

	RET

	SET_SIZE(stty)
