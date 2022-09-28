/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)vsscanf.c	1.6	92/07/14 SMI"	/* SVr4.0 1.7	*/

#include "curses_inc.h"

#ifdef __STDC__
#include	<stdarg.h>
#else
#include <varargs.h>
#endif

/*
 *	This routine implements _vsscanf (nonportably) until such time
 *	as one is available in the system (if ever).
 */

/*VARARGS2*/
_vsscanf(buf, fmt, ap)
char	*buf;
char	*fmt;
va_list	ap;
{
	FILE	junk;
	extern int _doscan();

#ifdef SYSV
	junk._flag = _IOREAD;
	junk._file = -1;
	junk._base = junk._ptr = (unsigned char *) buf;
#else
	junk._flag = _IOREAD|_IOSTRG;
	junk._base = junk._ptr = buf;
#endif
	junk._cnt = strlen(buf);
	return (_doscan(&junk, fmt, ap));
}
