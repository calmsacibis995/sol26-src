/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)llseek.s	1.4	94/07/04 SMI"

	.file	"llseek.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	llseek - move extended read/write file pointer
 *
 *   Syntax:	offset_t llseek(int fildes, offset_t offset, int whence);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(llseek,function)

#include "SYS.h"

	ENTRY(llseek)

	SYSTRAP(llseek)
	SYSCERROR64

	RET

	SET_SIZE(llseek)
