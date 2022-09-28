/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_sendmsg.s	1.4	96/06/14 SMI"

	.file	"_so_sendmsg.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_sendmsg)

	SYSTRAP(sendmsg)
	SYSRESTART(.restart__so_sendmsg)

	RET

	SET_SIZE(_so_sendmsg)
