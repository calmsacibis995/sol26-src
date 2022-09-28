/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setuid.s	1.3	94/07/04 SMI"

	.file	"setuid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setuid - set user ID
 *
 *   Syntax:	int setuid(uid_t uid);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setuid,function)

#include "SYS.h"

	ENTRY(setuid)

	SYSTRAP(setuid)
	SYSCERROR

	RETZ

	SET_SIZE(setuid)
