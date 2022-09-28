/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)mount.s	1.3	94/07/04 SMI"

	.file	"mount.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	mount - mount a file system
 *
 *   Syntax:	int mount(const char *spec, const char *dir, int mflag,
 *			  int fstyp, const char *dataptr, size_t datalen,  ...);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mount,function)

#include "SYS.h"

	ENTRY(mount)

	SYSTRAP(mount)
	SYSCERROR

	RETZ

	SET_SIZE(mount)
