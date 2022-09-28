/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ticerror.c 1.1	96/01/17 SMI"

/*
 *	ticerror.c		Terminal Information Compiler
 *
 *	Copyright 1990, 1992 by Mortice Kern Systems Inc.  All rights reserved.
 *
 *	This Software is unpublished, valuable, confidential property of
 *	Mortice Kern Systems Inc.  Use is authorized only in accordance
 *	with the terms and conditions of the source licence agreement
 *	protecting this Software.  Any unauthorized use or disclosure of
 *	this Software is strictly prohibited and will result in the
 *	termination of the licence agreement.
 *
 *	If you have any questions, please consult your supervisor.
 *
 *	Portions of this code Copyright 1982 by Pavel Curtis.
 *
 */

#ifdef M_RCSID
#ifndef lint
static char const rcsID[] = "$Header: /rd/src/tic/rcs/ticerror.c 1.14 1995/06/22 18:11:44 ant Exp $";
#endif
#endif

#include "tic.h"
#include <stdarg.h>

int warnings = 0;

/*f
 *	Display warning message.
 */
void
warning (char const *f, ...)
{
	va_list ap;
	char *fmt = m_msgdup((char *) f);

	va_start(ap, f);

	(void) fprintf(
		stderr, m_textmsg(3101, "%s: Warning in \"%s\" line %u,\n", "W _ filename line_num"),
		_cmdname, source_file, curr_line
	);

	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fputc('\n', stderr);

	m_msgfree(fmt);
	warnings++;
	return;
}

/*f
 *	Display error message.
 */
void
err_abort (char const *f, ...)
{
	va_list ap;
	char *fmt = m_msgdup((char *) f);

	va_start(ap, f);

	(void) fprintf(
		stderr, m_textmsg(3102, "%s: Error in \"%s\" line %u,\n", "E _ filename line_num"), 
		_cmdname, source_file, curr_line
	);

	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fputc('\n', stderr);

	m_msgfree(fmt);
	exit(1);
}

