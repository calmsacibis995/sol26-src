/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)brk.s	1.10	94/12/21 SMI"

	.file	"brk.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	brk - change the amount of space allocated for
 *		the calling process's data segment (no locking)
 *
 *   Syntax:	int brk_u(void *endds);
 *
 */

#include <sys/asm_linkage.h>

#include "SYS.h"
#include "PIC.h"

	ENTRY(_brk_u)

	SYSTRAP(brk)			! do it!
	SYSCERROR
	RET
	SET_SIZE(_brk_u)
