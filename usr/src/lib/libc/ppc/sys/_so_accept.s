/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_accept.s	1.4	96/06/14 SMI"

	.file	"_so_accept.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_accept)

	SYSTRAP(accept)
	SYSRESTART(.restart__so_accept)

	RET

	SET_SIZE(_so_accept)
