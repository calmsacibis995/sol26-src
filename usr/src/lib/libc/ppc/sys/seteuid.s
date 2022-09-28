/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)seteuid.s	1.3	94/07/04 SMI"

	.file	"seteuid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	seteuid - set effective user id
 *
 *   Syntax:	int seteuid(uid_t uid);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(seteuid,function)

#include "SYS.h"

	ENTRY(seteuid)

	SYSTRAP(seteuid)
	SYSCERROR

	RET

	SET_SIZE(seteuid)
