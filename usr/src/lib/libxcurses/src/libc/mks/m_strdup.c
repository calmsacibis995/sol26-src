/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_strdup.c 1.1	96/01/17 SMI"

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
static char rcsID[] = "$Id: m_strdup.c 1.8 1993/05/28 07:43:26 scott Exp $";
#endif
#endif

#include <mks.h>
#include <string.h>
#include <stdlib.h>

/*f
 * Return a copy of the string `s', issue an error and return NULL.
 */
LDEFN char *
m_strdup(s)
const char *s;
{
	char *cp;
	int len;

	if ((cp = m_malloc(len = strlen(s)+1)) == NULL) {
		m_error(m_textmsg(3581, "!memory allocation failure", "E"));
		return NULL;
	}
	return (memcpy(cp, s, len));
}
