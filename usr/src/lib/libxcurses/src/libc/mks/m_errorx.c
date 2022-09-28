/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_errorx.c 1.1	96/01/17 SMI"

/*
 * Copyright 1993 by Mortice Kern Systems Inc.  All rights reserved.
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
static char rcsID[] = "$Id: m_errorx.c 1.9 1995/02/08 15:03:16 rob Exp $";
#endif
#endif

#include <mks.h>
#include <errno.h>
#include <string.h>

#ifndef	ERRORFN

#define	ERRORFN	m_errorexit
#define	DONE	exit(1)

/* Default error msg routine in library */
M_ERROR(m_errorexit);

#endif	/* ERRORFN */

/*f
 * Print error message with command name and trailing newline.
 * Leading ! indicates format errno on the end.
 * The value of errno is restored on completion.
 */
void
ERRORFN(const char *fmt, va_list args)
{
	int saveerrno = errno;
	int syserr = 0;

	if (_cmdname != NULL)
		fprintf(stderr, "%s: ", _cmdname);
	fmt = m_strmsg(fmt);
	if (*fmt == '!') {
		fmt++;
		syserr++;
	}
	vfprintf(stderr, fmt, args);
	if (syserr) {
		char *str;

		/* Do eprintf-like stuff */
		str = strerror(saveerrno);
		if (*str == '\0')
			fprintf(stderr, ": errno = %d", saveerrno);
		else
			fprintf(stderr,": %s", str);
	}
	fputc('\n', stderr);
	errno = saveerrno;
	DONE;
}


