/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)_ftoull.c	1.1	95/11/06 SMI"

#include "synonyms.h"
#include <limits.h>

#if !defined(_NO_LONGLONG)

/*
 * Convert a double precision floating point # into a 64-bit unsigned int.
 */
unsigned long long
__dtoull(double d)
{
	double l;
	long long i;

	l = LLONG_MIN;
	if (d >= -l)
		i = (1ll << 63) | (long long) (d + l);
	else
		i = (long long) d;
	return ((unsigned long long) i);
}

unsigned long long
__ftoull(float f)
{
	return (__dtoull((double) f));
}

/*
 * Convert a quadruple precision floating point # into a 64-bit unsigned int.
 */
unsigned long long
_q_qtoull(long double *d)
{
	long double l;
	long long i;

	l = LLONG_MIN;
	if (*d >= -l)
		i = (1ll << 63) | (long long) (*d + l);
	else
		i = (long long) *d;
	return ((unsigned long long) i);
}

#endif /* _NO_LONGLONG */
