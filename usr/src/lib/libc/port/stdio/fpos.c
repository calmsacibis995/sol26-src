/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fpos.c	1.14	96/01/04 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

int
fgetpos(FILE *stream, fpos_t *pos)
{
	if ((*pos = (fpos_t)ftello(stream)) == (fpos_t)-1)
		return (-1);
	return (0);
}

int
fsetpos(FILE *stream, const fpos_t *pos)
{
	if (fseeko(stream, (off_t)*pos, SEEK_SET) != 0)
		return (-1);
	return (0);
}
