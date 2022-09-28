/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)install_utrap.s	1.1	95/11/05 SMI"

	.file	"install_utrap.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(install_utrap,function)

#include "SYS.h"

	ENTRY(install_utrap)
	SYSTRAP(install_utrap)
	SYSCERROR
	RET
	SET_SIZE(install_utrap)
