/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_getsockname.s	1.3	96/06/13 SMI"

	.file	"_so_getsockname.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_so_getsockname)

	SYSTRAP(getsockname)
	SYSCERROR

	RET

	SET_SIZE(_so_getsockname)
