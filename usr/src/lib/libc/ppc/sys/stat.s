/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)stat.s	1.5	96/02/26 SMI"


	.file	"stat.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	stat - get file status
 *
 *   Syntax:	int stat(const char *path, struct stat *buf);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(stat,function)
#else
	ANSI_PRAGMA_WEAK(stat64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(stat)

	SYSTRAP(stat)
	SYSCERROR

	RETZ

	SET_SIZE(stat)

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	stat64 - get file status
 *
 *   Syntax:	int stat64(const char *path, struct stat64 *buf);
 *
 */

#else

	ENTRY(stat64)

	SYSTRAP(stat64)
	SYSCERROR

	RETZ

	SET_SIZE(stat64)

#endif
