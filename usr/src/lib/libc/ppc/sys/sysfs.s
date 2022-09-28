/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)sysfs.s	1.3	94/07/04 SMI"

	.file	"sysfs.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	sysfs - get file system type information
 *
 *   Syntax:	int sysfs(int opcode, const char *fsname);
 *
 *		int sysfs(int opcode, int fs_index, char *buf);
 *
 *		int sysfs(int opcode);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sysfs,function)

#include "SYS.h"

	ENTRY(sysfs)

	SYSTRAP(sysfs)
	SYSCERROR

	RET

	SET_SIZE(sysfs)
