/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sigaltstk.s	1.3	94/07/04 SMI"

	.file	"sigaltstk.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sigaltstack - set or get signal alternate stack context.
 *
 *   Syntax:	int sigaltstack(const ck_t *ss, stack_t *oss);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sigaltstack,function)

#include "SYS.h"

	ENTRY(sigaltstack)

	SYSTRAP(sigaltstack)
	SYSCERROR

	RET

	SET_SIZE(sigaltstack)
