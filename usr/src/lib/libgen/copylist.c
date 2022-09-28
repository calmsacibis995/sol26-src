/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)copylist.c	1.10	96/04/18 SMI"	/* SVr4.0 1.1.3.2	*/

/*
	copylist copies a file into a block of memory, replacing newlines
	with null characters, and returns a pointer to the copy.
*/

#pragma weak copylist64 = _copylist64
#pragma weak copylist = _copylist

#include "synonyms.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

static char *
common_copylist(const char *filenm, off64_t size)
{
	FILE		*strm;
	register int	c;
	register char	*ptr, *p;

	if (size > SSIZE_MAX) {
		errno = EOVERFLOW;
		return (NULL);
	}

	/* get block of memory */
	if ((ptr = malloc((unsigned) size)) == NULL) {
		return (NULL);
	}

	/* copy contents of file into memory block, replacing newlines */
	/* with null characters */
	if ((strm = fopen(filenm, "r")) == NULL) {
		return (NULL);
	}
	for (p = ptr; p < ptr + size && (c = getc(strm)) != EOF; p++) {
		if (c == '\n')
			*p = '\0';
		else
			*p = c;
	}
	(void) fclose(strm);

	return (ptr);
}


char *
_copylist64(const char *filenm, off64_t *szptr)
{
	struct	stat64	stbuf;

	/* get size of file */
	if (stat64(filenm, &stbuf) == -1) {
		return (NULL);
	}
	*szptr = stbuf.st_size;

	return (common_copylist(filenm, stbuf.st_size));
}


char *
_copylist(const char *filenm, off_t *szptr)
{
	struct	stat64	stbuf;

	/* get size of file */
	if (stat64(filenm, &stbuf) == -1) {
		return (NULL);
	}

	if (stbuf.st_size > LONG_MAX) {
		errno = EOVERFLOW;
		return (NULL);
	}

	*szptr = (off_t) stbuf.st_size;

	return (common_copylist(filenm, stbuf.st_size));
}
