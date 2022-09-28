/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_send.s	1.4	96/06/14 SMI"

	.file	"_so_send.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_send)

	SYSTRAP(send)
	SYSRESTART(.restart__so_send)

	RET

	SET_SIZE(_so_send)
