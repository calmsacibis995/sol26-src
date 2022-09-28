/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getgroups.s	1.3	94/07/04 SMI"

	.file	"getgroups.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getgroups - get supplementary group access list IDs
 *
 *   Syntax:	int getgroups(int gidsetsize, gid_t *grouplist);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getgroups,function)

#include "SYS.h"

	ENTRY(getgroups)

	SYSTRAP(getgroups)
	SYSCERROR

	RET

	SET_SIZE(getgroups)
