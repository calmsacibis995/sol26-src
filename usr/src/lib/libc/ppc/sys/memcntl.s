/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)memcntl.s	1.3	94/07/04 SMI"

	.file	"memcntl.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	memcntl - memory management control
 *
 *   Syntax:	int memcntl(caddr_t addr, size_t len, int cmd, caddr_t arg,
 *		int attr, int mask);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memcntl,function)

#include "SYS.h"

	ENTRY(memcntl)

	SYSTRAP(memcntl)
	SYSCERROR

	RETZ

	SET_SIZE(memcntl)
