/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_recvfrom.s	1.4	96/06/14 SMI"

	.file	"_so_recvfrom.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_recvfrom)

	SYSTRAP(recvfrom)
	SYSRESTART(.restart__so_recvfrom)

	RET

	SET_SIZE(_so_recvfrom)
