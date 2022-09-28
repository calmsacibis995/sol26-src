/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)statvfs.s	1.5	96/02/26 SMI"

	.file	"statvfs.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	statvfs - get file system information
 *
 *   Syntax:	int statvfs(const char *path, struct statvfs *buf);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(statvfs,function)
#else
	ANSI_PRAGMA_WEAK(statvfs64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(statvfs)

	SYSTRAP(statvfs)
	SYSCERROR

	RETZ

	SET_SIZE(statvfs)

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	statvfs64 - get file system information
 *
 *   Syntax:	int statvfs64(const char *path, struct statvfs64 *buf);
 *
 */

#else

	ENTRY(statvfs64)

	SYSTRAP(statvfs64)
	SYSCERROR

	RETZ

	SET_SIZE(statvfs64)
	
#endif
