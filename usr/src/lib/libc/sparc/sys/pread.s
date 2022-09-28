/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)pread.s	1.4	96/02/26 SMI"	/* SVr4.0 1.9	*/

/* C library -- pread						*/
/* int pread (int fildes, void *buf, unsigned nbyte, off_t offset);	*/

	.file	"pread.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(pread,function)
#else
	ANSI_PRAGMA_WEAK(pread64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)

	SYSCALL_RESTART(pread)
	RET

	SET_SIZE(pread)

#else
/* C library -- pread64 transitional large file API	*/
/* ssize_t pread(int, void *, size_t, off64_t);		*/

	SYSCALL_RESTART(pread64)
	RET

	SET_SIZE(pread64)

#endif
