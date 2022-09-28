/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 */

#ident	"@(#)llabs.c	1.2	94/04/06 SMI"

/*LINTLIBRARY*/

#ifdef __STDC__
#pragma weak llabs = _llabs
#endif

#include "synonyms.h"
#include <stdlib.h>
#include <sys/types.h>

longlong_t
llabs(arg)
register longlong_t arg;
{
	return (arg >= 0 ? arg : -arg);
}
