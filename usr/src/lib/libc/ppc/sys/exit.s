/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)exit.s	1.3	94/07/04 SMI"

	.file	"exit.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	exit - terminate process
 *
 *   Syntax:	void exit(int status);
 *
 */

#include "SYS.h"

	ENTRY(_exit)

	SYSTRAP(exit)

	SET_SIZE(_exit)
