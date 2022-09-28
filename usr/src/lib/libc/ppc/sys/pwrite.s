/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)pwrite.s	1.5	96/02/26 SMI"

	.file	"pwrite.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	pwrite - write on a file.
 *
 *   Syntax:	ssize_t pwrite(int fildes, const void *buf, size_t nbyte,
 *		off_t offset);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(pwrite,function)
#else
	ANSI_PRAGMA_WEAK(pwrite64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSREENTRY(pwrite)

	SYSTRAP(pwrite)
	SYSRESTART(.restart_pwrite)

	RET

	SET_SIZE(pwrite)

#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	pwrite64 - write on a file.
 *
 *   Syntax:	ssize_t pwrite64(int fildes, const void *buf, size_t nbyte,
 *		off64_t offset);
 */
	
	SYSREENTRY(pwrite64)

	SYSTRAP(pwrite64)
	SYSRESTART(.restart_pwrite64)

	RET

	SET_SIZE(pwrite64)

#endif
