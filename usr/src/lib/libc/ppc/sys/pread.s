/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)pread.s	1.5	96/02/26 SMI"

	.file	"pread.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	pread - read from a file.
 *
 *   Syntax:	ssize_t pread(int filedes, void *buf, size_t nbyte,
 *		off_t offset);
 *
 */

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(pread,function)
#else
	ANSI_PRAGMA_WEAK(pread64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSREENTRY(pread)

	SYSTRAP(pread)
	SYSRESTART(.restart_pread)

	RET

	SET_SIZE(pread)

#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	pread64 - read from a file.
 *
 *   Syntax:	ssize_t pread64(int filedes, void *buf, size_t nbyte,
 *		off64_t offset);
 */
	
	SYSREENTRY(pread64)

	SYSTRAP(pread64)
	SYSRESTART(.restart_pread64)

	RET

	SET_SIZE(pread64)

#endif
