/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ident	"@(#)mktemp.c	1.11	96/09/05 SMI"	/* SVr4.0 1.16	*/

/*LINTLIBRARY*/
/*
***************************************************************
*	Routine expects a string with six trailing 'X's.
*	These will be overlaid with letters, digits and symbols
*	from the portable filename character set. If every combination
*	thus inserted leads to an existing file name, the
*	string is shortened to length zero and a pointer to a null
*	string is returned. The routine was modified for use in a
*	multi-threaded program.
*
*	lrand48() was used to get a wider distribution of random numbers.
*	A mask is generated depending on the number of trailing X's
*	in the original string. This was done to improve the
*	performance of the search.
*	For example, if there was only one X to replace the search
*	would be limited to 64 possibilities instead of 64^n where
*	n stands for number of X's.
*	Once the number is generated from l64a routine, only the X's
*	are replaced. If the resulting string contains blanks or '/'
*	they are replaced with 0's and '_' respectively. The file
*	status is obtained and if lstat fails and the error condition
*	is file does not exist, the string is returned. Otherwise
*	it goes through the loop and obtains another file name.

************************************************************** */
#define	XCNT  6
#ifdef __STDC__
#pragma weak mktemp = _mktemp
#endif
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "synonyms.h"
#include "shlib.h"
#include <thread.h>
#include <synch.h>
#include "mtlib.h"
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

char *
mktemp(char *as)
{
	static int		seeded = 0;
	char			*num, *first_x, *s;
	unsigned		xcnt;	/* keeps track of number of X's seen */
	struct stat64		buf;
	int			first_try, current_try, len;
	unsigned long		mask;

	if (as == NULL || *as == NULL) { /* If the string passed is null then */
		return (as);	/* a pointer to a null string is returned. */
	}

	if (seeded == 0) {
		seeded = 1;
		srand48((long)getpid());
	}

	xcnt = 0;
	len = strlen(as);
	s = as + (len - 1);
	while ((len != 0) && (xcnt < XCNT) && (*s == 'X')) {
			xcnt++;
			len--;
			--s;
	}

	first_x = s + 1; /* Adjust pointer to first X in buffer. */
	first_try = current_try = lrand48();

	/*
	 * A mask is generated depending on the number of trailing X's.
	 * Instead of searching for all possibilities of 64^6, the mask
	 * allows only the needed search. This could have been done in
	 * one step like  mask = ~(-1 << (xcnt *6)) except for the
	 * compiler problem.
	 */

	mask = -1;
	if (xcnt < 6) {
		mask <<= (xcnt * 6); /* This was done in two steps to avoid */
		mask = ~mask;		/* a compiler problem. */
	}

	first_try &= mask;

	do {
		num = (char *) l64a(current_try);
		s = first_x;
		if (xcnt != 0) {
			sprintf(s, "%*.*s", xcnt, xcnt, num);
		}

		while (*s == ' ')	/* If the resulting string contains */
			*(s++) = '0';	/* blanks replace with zero's. */

		while (*s != '\0') {
			if (*s == '/')	/* If the resulting string contains */
				*s = '_'; /* '/' replace with '_' */
			s++;
		}

		if (lstat64(as, &buf) == -1) {
			if (errno == ENOENT) {
				return (as);
			}
		}
		current_try &= mask;
	} while ((*first_x != '\0') && (++current_try != first_try));
	*as = '\0';
	return (as);
}
