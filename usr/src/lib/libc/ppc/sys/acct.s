/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)acct.s	1.3	94/07/04 SMI"

	.file	"acct.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	acct - enable or disable process accounting
 *
 *   Syntax:	int acct(const char *path);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(acct,function)

#include "SYS.h"

	ENTRY(acct)

	SYSTRAP(acct)
	SYSCERROR

	RETZ

	SET_SIZE(acct)
