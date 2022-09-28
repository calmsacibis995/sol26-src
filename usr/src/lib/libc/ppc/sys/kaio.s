/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident "@(#)kaio.s	1.1	94/11/10 SMI"

	.file	"kaio.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	kaio - 
 *
 *   Syntax:	int kaio(...);
 *	 subcodes:
 *		aioread(...)   :: kaio(AIOREAD, ...)
 *		aiowrite(...)  :: kaio(AIOWRITE, ...)
 *		aiowait(...)   :: kaio(AIOWAIT, ...)
 *		aiocancel(...) :: kaio(AIOCANCEL, ...)
 *		aionotify()    :: kaio(AIONOTIFY)
 *		aioinit()      :: kaio(AIOINIT)
 *		aiostart()     :: kaio(AIOSTART)
 *		see <sys/aio.h>
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(kaio,function)

#include "SYS.h"

	ENTRY(kaio)

	SYSTRAP(kaio)
	SYSCERROR

	RET

	SET_SIZE(kaio)
