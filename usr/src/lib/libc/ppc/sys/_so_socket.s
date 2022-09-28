/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_so_socket.s	1.4	96/06/13 SMI"

	.file	"_so_socket.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_so_socket)

	SYSTRAP(so_socket)
	SYSCERROR

	RET

	SET_SIZE(_so_socket)
