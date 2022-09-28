/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)strlen.c	1.7	92/07/14 SMI"	/* SVr4.0 1.7	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * Returns the number of
 * non-NULL bytes in string argument.
 */

#include "synonyms.h"
#include <string.h>

size_t
strlen(s)
#ifdef __STDC__
register const char *s;
{
	register const char *s0 = s + 1;
#else
register char *s;
{
	register char *s0 = s + 1;
#endif

	while (*s++ != '\0')
		;
	return (s - s0);
}
