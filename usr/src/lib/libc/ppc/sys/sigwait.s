/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

	.ident "@(#)sigwait.s	1.5	96/03/08 SMI"

	.file	"sigwait.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_sigwait)

	li %r4, 0
	li %r5, 0
	SYSTRAP(sigtimedwait)
	SYSCERROR

	RET

	SET_SIZE(_libc_sigwait)
