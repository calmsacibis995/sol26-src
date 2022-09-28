/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_recv.s	1.4	96/06/14 SMI"

	.file	"_so_recv.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_recv)

	SYSTRAP(recv)
	SYSRESTART(.restart__so_recv)

	RET

	SET_SIZE(_so_recv)
