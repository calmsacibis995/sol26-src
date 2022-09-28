/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)open.s	1.10	96/05/24 SMI"	/* SVr4.0 1.8	*/

	.file	"open.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
/* C library -- open						*/
/* int open (const char *path, int oflag, [ mode_t mode ] )	*/

	.weak	_libc_open;
	.type	_libc_open, #function
	_libc_open = open

	SYSCALL(open)
	RET

	SET_SIZE(open)

#else /* _FILE_OFFSET_BITS == 64 */
/* 
 * C library -- open64 - transitional API				
 * int open64 (const char *path, int oflag, [ mode_t mode ] )	
 */

	.weak	_libc_open64;
	.type	_libc_open64, #function
	_libc_open64 = open64

	SYSCALL(open64)
	RET

	SET_SIZE(open64)
#endif
