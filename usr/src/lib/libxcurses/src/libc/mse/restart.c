/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)restart.c 1.1	96/01/17 SMI"

/*
 * Copyright 1992 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 * 
 */
#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/mse/RCS/restart.c,v 1.1 1992/12/15 10:59:56 alex Exp alex $";
#endif /*lint*/
#endif /*M_RCSID*/

#include <mks.h>
#include <stdlib.h>
#include <wchar.h>

/*f
 * Restartable multibyte routines, for a system without state.
 */
int
mbrtowc(wchar_t *wc, const char *mb, size_t n, mbstate_t *ps)
{
	return mbtowc(wc, mb, n);
}

int
wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
	return wctomb(s, wc);
}
