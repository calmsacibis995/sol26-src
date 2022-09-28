/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)open.s	1.6	96/05/24 SMI"

	.file	"open.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	open - open for reading or writing
 *
 *   Syntax:	int open(const char *path, int oflag, ...);
 *
 */

#include <sys/asm_linkage.h>

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	.weak	_libc_open;
	.type	_libc_open, @function
	_libc_open = open
	
	ENTRY(open)

	SYSTRAP(open)
	SYSCERROR

	RET

	SET_SIZE(open)

/* 
 * C library -- open64 - transitional API				
 * int open64 (const char *path, int oflag, [ mode_t mode ] )	
 */

#else
	.weak	_libc_open64;
	.type	_libc_open64, @function
	_libc_open64 = open64

	ENTRY(open64)
	
	SYSTRAP(open64)
	SYSCERROR
	RET

	SET_SIZE(open64)
#endif
