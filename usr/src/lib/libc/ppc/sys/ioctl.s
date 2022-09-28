/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)ioctl.s	1.3	94/07/04 SMI"

	.file	"ioctl.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	ioctl - control device
 *
 *   Syntax:	int ioctl(int fildes, int request, arg);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ioctl,function)

#include "SYS.h"

	SYSREENTRY(ioctl)

	SYSTRAP(ioctl)
	SYSRESTART(.restart_ioctl)

	RET

	SET_SIZE(ioctl)
