/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_sendto.s	1.4	96/06/14 SMI"

	.file	"_so_sendto.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_sendto)

	SYSTRAP(sendto)
	SYSRESTART(.restart__so_sendto)

	RET

	SET_SIZE(_so_sendto)
