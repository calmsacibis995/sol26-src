/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_shutdown.s	1.3	96/06/13 SMI"

	.file	"_so_shutdown.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_so_shutdown)

	SYSTRAP(shutdown)
	SYSCERROR

	RET

	SET_SIZE(_so_shutdown)
