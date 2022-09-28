/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1996, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#ident	"@(#)mkstemp.c	1.7	96/10/04 SMI"	/* SVr4.0 1.1	*/

/*
******************************************************************


		PROPRIETARY NOTICE(Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice

Notice of copyright on this source code product does not indicate
publication.

	(c) 1986, 1987,	1988, 1989  Sun Microsystems, Inc
	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
		All rights reserved.
******************************************************************* */

#if _FILE_OFFSET_BITS == 64
#pragma weak mkstemp64 = _mkstemp64
#else /* _FILE_OFFSET_BITS == 64 */
#pragma weak mkstemp = _mkstemp
#endif

#include "synonyms.h"
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>

int
_mkstemp(char *as)
{
	int	fd;
	char	*tstr, *str, *mkret;

	if (as == NULL || *as == NULL)
		return (-1);

	tstr = alloca(strlen(as) + 1);
	strcpy(tstr, as);

	str = tstr + (strlen(tstr) - 1);

	/*
	 * The following for() loop is doing work.  mktemp() will generate
	 * a different name each time through the loop.  So if the first
	 * name is used then keep trying until you find a free filename.
	 */

	for (; ; ) {
		if (*str == 'X') { /* If no trailing X's don't call mktemp. */
			mkret = mktemp(as);
			if (*mkret == '\0') {
				return (-1);
			}
		}

		if ((fd = open(as, O_CREAT|O_EXCL|O_RDWR, 0600)) != -1) {
			return (fd);
		}

		/*
		 * If the error condition is other than EEXIST or if the
		 * file exists and there are no X's in the string
		 * return -1.
		 */

		if ((errno != EEXIST) || (*str != 'X')) {
			return (-1);
		}
		strcpy(as, tstr);
	}
}
