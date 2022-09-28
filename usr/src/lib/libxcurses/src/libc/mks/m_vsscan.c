/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_vsscan.c 1.1	96/01/17 SMI"

/*
 *	vsscanf.c
 *
 *	Copyright 1985, 1994 by Mortice Kern Systems Inc.  All rights reserved.
 *	
 *	This Software is unpublished, valuable, confidential property of
 *	Mortice Kern Systems Inc.  Use is authorized only in accordance
 *	with the terms and conditions of the source licence agreement
 *	protecting this Software.  Any unauthorized use or disclosure of
 *	this Software is strictly prohibited and will result in the
 *	termination of the licence agreement.
 *	
 *	If you have any questions, please consult your supervisor.
 */

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/mks/rcs/m_vsscan.c 1.2 1994/06/17 18:19:41 ant Exp $";
#endif
#endif

#include <mks.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

extern int vfscanf ANSI((FILE *, char *, va_list));

LDEFN int
m_vsscanf(buf, fmt, vp)
char *buf, *fmt;
va_list vp;
{
	static FILE *fp = NULL;

	/* Either open or reuse a temporary file.  Note temporary files
	 * opened by tmpfile() will be automatically closed and removed 
	 * when the program terminates (so says ANSI C).
	 */
	if (fp == NULL && (fp = tmpfile()) == NULL)
		return -1;
	else
		(void) rewind(fp);

	/* Write out the contents of the buffer to the temporary file. */
	(void) fputs(buf, fp);

	/* Rewind in preparation for reading. */
	(void) rewind(fp);

	return vfscanf(fp, fmt, vp);
}
