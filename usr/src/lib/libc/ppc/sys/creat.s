/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)creat.s	1.6	96/05/24 SMI"

	.file	"creat.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	creat - create a new file or rewrite an existing one
 *
 *   Syntax:	int creat(const char *path, mode_t mode);
 *
 */

#include <sys/asm_linkage.h>

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	.weak	_libc_creat;
	.type	_libc_creat, @function
	_libc_creat = creat
	
	ENTRY(creat)

	SYSTRAP(creat)
	SYSCERROR

	RET

	SET_SIZE(creat)

#else
/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	creat64 - create a new file or rewrite an existing one
 *			transitional large file interface
 *
 *   Syntax:	int creat64(const char *path, mode_t mode);
 *
 */
	.weak	_libc_creat64;
	.type	_libc_creat64, @function
	_libc_creat64 = creat64

	ENTRY(creat64)

	SYSTRAP(creat64)
	SYSCERROR

	RET

	SET_SIZE(creat64)

#endif
