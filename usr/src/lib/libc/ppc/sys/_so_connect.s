/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_connect.s	1.4	96/06/14 SMI"

	.file	"_so_connect.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_connect)

	SYSTRAP(connect)
	SYSRESTART(.restart__so_connect)

	RET

	SET_SIZE(_so_connect)
