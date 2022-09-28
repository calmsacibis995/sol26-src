/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_wcsdup.c 1.1	96/01/17 SMI"

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
static char rcsID[] = "$Id: m_wcsdup.c 1.5 1994/08/17 15:32:51 jeffhe Exp $";
#endif
#endif

#include <mks.h>
#include <string.h>
#include <stdlib.h>

/*f
 * Return a wide copy of the wide string `s', or NULL
 */
LDEFN wchar_t *
m_wcsdup(s)
const wchar_t *s;
{
	wchar_t *cp;
	int len;
	extern char *_cmdname;

	cp = (wchar_t *)malloc(len = (wcslen(s) + 1) * sizeof(wchar_t));
	if (cp == (wchar_t *)NULL) {
		return((wchar_t *)0);
	}
	return ((wchar_t *)memcpy((char *)cp, s, len));
}
