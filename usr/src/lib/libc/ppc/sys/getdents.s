/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getdents.s	1.5	96/02/26 SMI"

	.file	"getdents.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getdents - read directory entries and put in a 
 *		file system independent format
 *
 *   Syntax:	int getdents(int fildes, struct dirent *buf, size_t nbyte);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(getdents,function)
#else
	ANSI_PRAGMA_WEAK(getdents64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(getdents)

	SYSTRAP(getdents)
	SYSCERROR

	RET

	SET_SIZE(getdents)

#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getdents64 - read directory entries and put in a 
 *		file system independent format
 *		transitional large file interface
 *
 *   Syntax:	int getdents64(int fildes, struct dirent64 *buf, size_t nbyte);
 *
 */

	ENTRY(getdents64)

	SYSTRAP(getdents64)
	SYSCERROR

	RET

	SET_SIZE(getdents64)
#endif
