/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setgroups.s	1.3	94/07/04 SMI"

	.file	"setgroups.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setgroups - set supplementary group access list IDs
 *
 *   Syntax:	int setgroups(int ngroups, const gid_t *grouplist);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setgroups,function)

#include "SYS.h"

	ENTRY(setgroups)

	SYSTRAP(setgroups)
	SYSCERROR

	RETZ

	SET_SIZE(setgroups)
