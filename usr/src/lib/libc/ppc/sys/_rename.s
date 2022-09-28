/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_rename.s	1.3	94/07/04 SMI"

	.file	"_rename.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_rename - change the name of a file
 *
 *   Syntax:	int _rename(const char *old, const char *new);
 *
 */

#include "SYS.h"

	ENTRY(_rename)

	SYSTRAP(rename)
	SYSCERROR

	RETZ

	SET_SIZE(_rename)
