/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sysinfo.s	1.3	94/07/04 SMI"

	.file	"sysinfo.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sysinfo - get and set system information strings
 *
 *   Syntax:	long sysinfo(int command, char *buf, long count);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sysinfo,function)

#include "SYS.h"

	ENTRY(sysinfo)

	SYSTRAP(systeminfo)
	SYSCERROR

	RET

	SET_SIZE(sysinfo)
