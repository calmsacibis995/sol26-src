/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)fdsync.s	1.4	94/07/04 SMI"

	.file	"fdsync.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fdsync - synchronize a file's in-memory state with that
 *		on the physical medium
 *
 *   Syntax:	int fdsync(int fildes, int flag);
 *
 *   called internally by fsync(); (libc/port/sys/fsync.c),
 *   also POSIX fdatasync().
 */

#include "SYS.h"

	ENTRY(__fdsync)

	SYSTRAP(fdsync)
	SYSCERROR

	RETZ

	SET_SIZE(__fdsync)
