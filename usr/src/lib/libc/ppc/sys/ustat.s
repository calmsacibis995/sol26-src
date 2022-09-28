/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)ustat.s	1.3	94/07/04 SMI"

	.file	"ustat.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	ustat - get file system statistics
 *
 *   Syntax:	int ustat(dev_t dev, struct ustat *buf);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_ustat	2

	ANSI_PRAGMA_WEAK(ustat,function)

#include "SYS.h"

	ENTRY(ustat)
	mr	%r5, %r3	/* reverse argument order for utssys */
	mr	%r3, %r4
	mr	%r4, %r5
	li	%r5, SUBSYS_ustat

	SYSTRAP(utssys)
	SYSCERROR

	RETZ

	SET_SIZE(ustat)
