/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)eprintf.c 1.1	96/01/17 SMI"

/*
 * MKS library
 * Copyright 1985, 1992 by Mortice Kern Systems Inc.  All rights reserved.
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
static char rcsID[] = "$Header: /rd/src/libc/gen/rcs/eprintf.c 1.17 1994/06/17 19:42:34 hilary Exp $";
#endif
#endif

#include <mks.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

char *_cmdname;

/*f
 * print error message followed by errno value.
 * The value of errno is guaranteed to be restored on exit.
 */
/* VARARGS0 */
LDEFN int
eprintf VARARG1(const char *, fmt)
{
	va_list args;
	register int saveerrno = errno;
	register int nprf = 0;
	char *str;

	if (_cmdname != NULL)
		nprf += fprintf(stderr, "%s: ", _cmdname);
	va_start(args, fmt);
	nprf += vfprintf(stderr, fmt, args);
	va_end(args);
	str = strerror(saveerrno);
	if (*str == '\0')
		nprf += fprintf(stderr, ": error %d\n", saveerrno);
	else
		nprf += fprintf(stderr,": %s\n", str);
	errno = saveerrno;
	return (nprf);
}
