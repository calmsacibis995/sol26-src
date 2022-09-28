/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_getsp - returns the stack pointer
 *
 *   Syntax:	
 *
 */

.ident "@(#)_getsp.s 1.6    94/09/09 SMI"

#include "SYS.h"

/*
 * Return the stack pointer
 */
	ENTRY(_getsp)
	mr	%r3, %r1
	SET_SIZE(_getsp)
