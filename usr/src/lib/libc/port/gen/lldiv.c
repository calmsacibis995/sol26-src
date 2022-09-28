/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 */

#ident	"@(#)lldiv.c	1.3	94/04/08 SMI"

/*LINTLIBRARY*/

#if !defined(_NO_LONGLONG)

#ifdef __STDC__
#pragma weak lldiv = _lldiv
#endif

#include "synonyms.h"
#include <stdlib.h>
#include <sys/types.h>


#ifdef __STDC__
lldiv_t
lldiv(register long long numer, register long long denom)
#else
lldiv_t
lldiv(numer, denom)
register long long numer;
register long long denom;
#endif
{
	lldiv_t	sd;

	if (numer >= 0 && denom < 0) {
		numer = -numer;
		sd.quot = -(numer / denom);
		sd.rem  = -(numer % denom);
	} else if (numer < 0 && denom > 0) {
		denom = -denom;
		sd.quot = -(numer / denom);
		sd.rem  = numer % denom;
	} else {
		sd.quot = numer / denom;
		sd.rem  = numer % denom;
	}
	return (sd);
}

#endif	/* _NO_LONGLONG */
