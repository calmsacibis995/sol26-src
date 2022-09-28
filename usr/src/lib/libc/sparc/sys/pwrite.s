/*	Copyright (c) 1989 by Sun Microsystems, Inc.	*/

.ident	"@(#)pwrite.s	1.4	96/02/26 SMI"	/* SVr4.0 1.9	*/

/* C library -- pwrite						*/
/* int pwrite (int fildes, const void *buf, unsigned nbyte, off_t offset);	*/

	.file	"pwrite.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(pwrite,function)
#else
	ANSI_PRAGMA_WEAK(pwrite64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)

	SYSCALL_RESTART(pwrite)
	RET

	SET_SIZE(pwrite)

#else
/* C library -- lseek64 transitional large file API		*/
/* ssize_t pwrite(int, void *, size_t, off64_t);	*/

	SYSCALL_RESTART(pwrite64)
	RET

	SET_SIZE(pwrite64)

#endif
