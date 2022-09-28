/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident	"@(#)_sockconfig.s	1.4	96/06/13 SMI"

	.file	"_sockconfig.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_sockconfig)

	SYSTRAP(sockconfig)
	SYSCERROR

	RET

	SET_SIZE(_sockconfig)
