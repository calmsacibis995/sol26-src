/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_socketpair.s	1.4	96/06/13 SMI"

	.file	"_so_socketpair.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_so_socketpair)

	SYSTRAP(so_socketpair)
	SYSCERROR

	RET

	SET_SIZE(_so_socketpair)
