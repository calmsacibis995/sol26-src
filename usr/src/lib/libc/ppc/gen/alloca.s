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

.ident "@(#)alloca.s 1.5      94/09/09 SMI"

#include <sys/asm_linkage.h>

	ENTRY(__builtin_alloca)

	lwz	%r12, 0(%r1)		! old back_chain
	addi	%r11, %r3, 16
	rlwinm	%r3, %r11, 0, 0, 27 
	neg	%r3, %r3
	stwux	%r12, %r1, %r3
	mr	%r3, %r1
	blr

	SET_SIZE(__builtin_alloca)
