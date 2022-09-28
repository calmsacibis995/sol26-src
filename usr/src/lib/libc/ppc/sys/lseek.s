/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)lseek.s	1.6	96/02/26 SMI"

	.file	"lseek.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	lseek - move read/write file pointer
 *
 *   Syntax:	off_t lseek(int fildes, off_t offset, int whence);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(lseek,function)
#else
	ANSI_PRAGMA_WEAK(lseek64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	ENTRY(lseek)

	SYSTRAP(lseek)
	SYSCERROR

	RET

	SET_SIZE(lseek)
#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	lseek - move read/write file pointer
 *			transitional large file API
 *
 *   Syntax:	off64_t lseek64(int fildes, off64_t offset, int whence);
 *
 */
	
	ENTRY(lseek64)

	SYSTRAP(llseek)
	SYSCERROR64

	RET

	SET_SIZE(lseek64)
#endif
