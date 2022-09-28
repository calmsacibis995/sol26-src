/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setreid.s	1.2	95/11/05 SMI"

	.file	"setreid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setreuid,function)
	ANSI_PRAGMA_WEAK(setregid,function)
	
#include "SYS.h"

	ENTRY(setreuid)
	SYSTRAP(setreuid)
	SYSCERROR
	RET
	SET_SIZE(setreuid)

	ENTRY(setregid)
	SYSTRAP(setregid)
	SYSCERROR
	RET
	SET_SIZE(setregid)
