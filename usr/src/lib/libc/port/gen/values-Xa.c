/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)values-Xa.c	1.2	92/07/14 SMI"	/* SVr4.0 1.3	*/

#include "synonyms.h"
#include <math.h>

/* variables which differ depending on the
 * compilation mode
 *
 * ANSI conforming mode
 * This file is linked into the a.out immediately following
 * the startup routine if the -Xa compilation mode is selected
 */

 const enum version _lib_version = ansi_1;
