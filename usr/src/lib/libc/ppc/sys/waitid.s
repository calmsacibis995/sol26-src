/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)waitid.s	1.3	94/07/04 SMI"

	.file	"waitid.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	waitid - wait for child process to change state
 *
 *   Syntax:	int waitid(idtype_t idtype, id_t id, siginfo_t *infop,
 *		int options);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(waitid,function)

#include "SYS.h"

	SYSREENTRY(waitid)

	SYSTRAP(waitsys)
	SYSRESTART(.restart_waitid)

	RET

	SET_SIZE(waitid)
