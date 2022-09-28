/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)globals.c	1.24	96/02/28 SMI"

/* LINTLIBRARY */

/*
 * Global variables
 */
#include	<stdio.h>
#include	"_libld.h"

/*
 * List of allocated blocks for link-edit dynamic allocations
 */
Ld_heap *	ld_heap;
