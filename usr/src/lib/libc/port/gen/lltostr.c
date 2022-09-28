/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 */

#ident	"@(#)lltostr.c	1.2	94/04/06 SMI"

/*
 *	lltostr -- convert long long to decimal string
 *
 *	char *
 *	lltostr(value, ptr)
 *	long long value;
 *	char *ptr;
 *
 *	Ptr is assumed to point to the byte following a storage area
 *	into which the decimal representation of "value" is to be
 *	placed as a string.  Lltostr converts "value" to decimal and
 *	produces the string, and returns a pointer to the beginning
 *	of the string.  No leading zeroes are produced, and no
 *	terminating null is produced.  The low-order digit of the
 *	result always occupies memory position ptr-1.
 *	Lltostr's behavior is undefined if "value" is negative.  A single
 *	zero digit is produced if "value" is zero.
 */

#ifdef __STDC__
#pragma weak lltostr = _lltostr
#pragma weak ulltostr = _ulltostr
#endif

#include "synonyms.h"
#include <sys/types.h>

#ifdef __STDC__
char *
lltostr(longlong_t value, char *ptr)
#else
char *
lltostr(value, ptr)
longlong_t value;
char *ptr;
#endif
{
	longlong_t t;

	do {
		*--ptr = '0' + value - 10 * (t = value / 10);
	} while ((value = t) != 0);

	return (ptr);
}

#ifdef __STDC__
char *
ulltostr(u_longlong_t value, char *ptr)
#else
char *
ulltostr(value, ptr)
u_longlong_t value;
char *ptr;
#endif
{
	u_longlong_t t;

	do {
		*--ptr = '0' + value - 10 * (t = value / 10);
	} while ((value = t) != 0);

	return (ptr);
}
