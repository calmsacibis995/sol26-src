/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_strmsg.c 1.1	96/01/17 SMI"

/*
 * MKS interface to XPG message internationalization routines.
 * Copyright 1989, 1992 by Mortice Kern Systems Inc.  All rights reserved.
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
** Written by Trevor John Thompson
 */
#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/i18n/rcs/m_strmsg.c 1.5 1992/07/16 19:08:40 tj Exp $";
#endif
#endif

#define	I18N	1	/* InternationalizatioN on */

#include <mks.h>
#include <stdlib.h>

LDEFN char*
m_strmsg(str)
const char* str;
{
	char* cp;
	int id = (int)strtol(str, &cp, 0);

	if (cp[0]!='#' || cp[1]!='#')	/* no "##" delimiter */
		return ((char *)str);
	else
		return (m_textmsg(id, &cp[2], ""));
}
