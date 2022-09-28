/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

	.ident "@(#)signotifywait.s	1.1	95/04/05 SMI"

	.file	"signotifywait.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_signotifywait,function)

#include "SYS.h"

	ENTRY(_signotifywait)

	SYSTRAP(signotifywait)
	SYSCERROR

	RET

	SET_SIZE(_signotifywait)
