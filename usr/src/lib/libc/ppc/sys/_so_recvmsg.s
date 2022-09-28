/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_recvmsg.s	1.4	96/06/14 SMI"

	.file	"_so_recvmsg.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSREENTRY(_so_recvmsg)

	SYSTRAP(recvmsg)
	SYSRESTART(.restart__so_recvmsg)

	RET

	SET_SIZE(_so_recvmsg)
