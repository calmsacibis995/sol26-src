/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *
 *   Syntax:	
 *
 */

.ident "@(#)abs.s 1.8      94/09/20 SMI"

#include "SYS.h"

	ENTRY2(abs, labs)

	cmpi	%r3, 0
	bgelr
	neg	%r3, %r3
	blr

	SET_SIZE(abs)
	SET_SIZE(labs)
