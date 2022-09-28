/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)gtty.s	1.3	94/07/04 SMI"

	.file	"gtty.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	gtty - get terminal state (defunct)
 *
 *   Syntax:	int gtty(int fd, struct sgttyb *buf);
 *
 *   NOTE: This system call is obsoleted by "ioctl(fd, TIOCGETP, buf)"
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(gtty,function)

#include "SYS.h"

	ENTRY(gtty)

	SYSTRAP(gtty)
	SYSCERROR

	RET

	SET_SIZE(gtty)
