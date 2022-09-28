/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fstatvfs.s	1.5	96/02/26 SMI"

	.file	"fstatvfs.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fstatvfs - get file system information
 *
 *   Syntax:	int fstatvfs(int fildes, struct statvfs *buf);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(fstatvfs,function)
#else
	ANSI_PRAGMA_WEAK(fstatvfs64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(fstatvfs)

	SYSTRAP(fstatvfs)
	SYSCERROR

	RETZ

	SET_SIZE(fstatvfs)

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fstatvfs64 - get file system information
 *
 *   Syntax:	int fstatvfs64(int fildes, struct statvfs64 *buf);
 *
 */

#else

	ENTRY(fstatvfs64)

	SYSTRAP(fstatvfs64)
	SYSCERROR

	RETZ

	SET_SIZE(fstatvfs64)
	
#endif
