/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setgid.s	1.3	94/07/04 SMI"

	.file	"setgid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setgid - set group ID
 *
 *   Syntax:	int setgid(uid_t uid);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setgid,function)

#include "SYS.h"

	ENTRY(setgid)

	SYSTRAP(setgid)
	SYSCERROR

	RETZ

	SET_SIZE(setgid)
