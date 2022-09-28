/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sync.s	1.3	94/07/04 SMI"

	.file	"sync.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sync - update super block
 *
 *   Syntax:	void sync(void);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sync,function)

#include "SYS.h"

	ENTRY(sync)

	SYSTRAP(sync)

	RET

	SET_SIZE(sync)
