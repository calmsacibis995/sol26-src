/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fstat.s	1.5	96/02/26 SMI"

	.file	"fstat.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fstat - get file status
 *
 *   Syntax:	int fstat(int fildes, struct stat *buf);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(fstat,function)
#else
	ANSI_PRAGMA_WEAK(fstat64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	ENTRY(fstat)

	SYSTRAP(fstat)
	SYSCERROR

	RETZ

	SET_SIZE(fstat)
#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fstat64 - get file status
 *			transitional large files API
 *	
 *   Syntax:	int fstat64(int fildes, struct stat64 *buf);
 *
 */

	ENTRY(fstat64)

	SYSTRAP(fstat64)
	SYSCERROR

	RETZ

	SET_SIZE(fstat64)

#endif
