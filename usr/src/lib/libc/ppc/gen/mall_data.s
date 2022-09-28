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

.ident "@(#)mall_data.s 1.3	94/12/24 SMI"

/*
 * This file contains the definition of the
 *  imported beginning of the malloc arena _allocs
 * 
 * union store *allocs[2] = { 0, 0 } ; /* if it were possible */
 */

	.globl	_allocs

	.data
	.align	4
_dgdef_(_allocs):	
	.long	0
	.long	0
