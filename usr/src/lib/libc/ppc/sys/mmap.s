/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)mmap.s	1.5	96/02/26 SMI"

	.file	"mmap.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	mmap - map pages of memory
 *
 *   Syntax:	caddr_t mmap(caddr_t addr, size_t len, int prot,
 *		int flags, int fildes, off_t off);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(mmap,function)
#else
	ANSI_PRAGMA_WEAK(mmap64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(mmap)

	SYSTRAP(mmap)
	SYSCERROR

	RET

	SET_SIZE(mmap)

#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	mmap - map pages of memory
 *
 *   Syntax:	caddr_t mmap64(caddr_t addr, size_t len, int prot,
 *		int flags, int fildes, off64_t off);
 *
 */
		
	ENTRY(mmap64)

	SYSTRAP(mmap64)
	SYSCERROR

	RET

	SET_SIZE(mmap64)
#endif
