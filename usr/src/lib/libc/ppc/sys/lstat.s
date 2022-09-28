/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)lstat.s	1.5	96/02/26 SMI"

	.file	"lstat.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	lstat - get file status.
 *
 *   Syntax:	int lstat(const char *path, struct stat *buf);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(lstat,function)
#else
	ANSI_PRAGMA_WEAK(lstat64,function)
#endif
	
#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(lstat)

	SYSTRAP(lstat)
	SYSCERROR

	RETZ

	SET_SIZE(lstat)
#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	lstat64 - get file status.
 *			transitional large file API
 *
 *   Syntax:	int lstat64(const char *path, struct stat64 *buf);
 *
 */
	
	ENTRY(lstat64)

	SYSTRAP(lstat64)
	SYSCERROR

	RETZ

	SET_SIZE(lstat64)

#endif
