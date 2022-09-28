/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setegid.s	1.3	94/07/04 SMI"

	.file	"setegid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setegid - set effective group id
 *
 *   Syntax:	int setegid(gid_t gid);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setegid,function)

#include "SYS.h"

	ENTRY(setegid)

	SYSTRAP(setegid)
	SYSCERROR

	RET

	SET_SIZE(setegid)
