/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)_ftoll.c	1.7	95/11/06 SMI"

#include "synonyms.h"
#include <floatingpoint.h>
#include <limits.h>

#if !defined(_NO_LONGLONG)

/*
 * Convert a double precision floating point number into a 64-bit int.
 */
extern _Q_set_exception();

long long
__dtoll(double d)
{
	register int exp;	/* exponent */
	register int i0, m0;	/* most significant word of mantissa */
	register unsigned m1;	/* least sig. word of mantissa */
	volatile double e = 1.0;
	volatile long i;

	if (*((unsigned *)&e) != 0) {
		i0 = m0 = *(int *)&d;
		m1 = *(1+(int *)&d);
	} else {
		i0 = m0 = *(1+(int *)&d);
		m1 = *(int *)&d;
	}

	/*
	 * Extract the exponent and check boundary conditions.
	 * Notice that the exponent is equal to the bit number where
	 * we want the most significant bit to live.
	 */
	exp = ((m0 >> 20) & 0x7ff) - 0x3ff;
	if (exp < 31) {
		/* d is in the range of long, so convert to long instead */
		i = d;
		return ((long long) i);
	} else if (exp > 62) {
		/* fp_invalid NOT raised if d == LLONG_MIN */
		if (d != LLONG_MIN) {
			/* d is large, Inf, or Nan; signal invalid conversion */
			i = d;
		}
		if (i0 < 0)
			return (LLONG_MIN);
		else
			return (LLONG_MAX);
	}

	/* Extract the mantissa. */
	m0 = 0x40000000 | ((m0 << 10) & 0x3ffffc00) | ((m1 >> 22) & 0x3ff);
	m1 = m1 << 10;

	/*
	 * The most significant bit of the mantissa is now in bit 62 of m0:m1.
	 * Shift right by (62 - exp) bits.
	 */
	if ((m1 & ((1 << (62 - exp)) - 1)) != 0) {
		/* signal inexact */
		e += 1.0e100;
	}
	m1 >>= (62 - exp);
	m1 |= m0 << (exp - 30);
	m0 >>= (62 - exp);
	if (i0 < 0) {
		m0 = ~m0;
		m1 = ~m1;
		if (++m1 == 0)
			m0++;
	}
	return ((long long)(((unsigned long long)m0 << 32) | m1));
}

/*
 * Convert a floating point number into a 64-bit int.
 */
long long
__ftoll(float f)
{
	return (__dtoll((double) f));
}

/*
 * Convert an extended precision floating point number into a 64-bit int.
 */
/*ARGSUSED*/
long long
_q_qtoll(long double *d)
{
	register int exp;	/* exponent */
	register int i0, m0;	/* most significant word of mantissa */
	register unsigned m1;	/* next most significant word of mantissa */
	register unsigned m2;	/* third most sig. word of mantissa */
	register unsigned m3;	/* fourth most sig. word of mantissa */
	volatile double e = 1.0;
	volatile long i;

	if (*((unsigned *)&e) != 0) {
		i0 = m0 = *(int *)d;
		m1 = *(1+(unsigned *)d);
		m2 = *(2+(unsigned *)d);
		m3 = *(3+(unsigned *)d);
	} else {
		i0 = m0 = *(3+(int *)d);
		m1 = *(2+(unsigned *)d);
		m2 = *(1+(unsigned *)d);
		m3 = *(unsigned *)d;
	}

	/*
	 * Extract the exponent and check boundary conditions.
	 * Notice that the exponent is equal to the bit number where
	 * we want the most significant bit to live.
	 */
	exp = ((m0 >> 16) & 0x7fff) - 0x3fff;
	if (exp < 31) {
		/* d is in the range of long, so convert to long instead */
		i = *d;
		return ((long long) i);
	} else if (exp > 62) {
		/* fp_invalid NOT raised if *d trunc to 64 bits == LLONG_MIN */
		if ((m0 - 0xc03f0000) | m1 | (m2 & 0xfffe0000)) {
			/* d is large, Inf, or Nan; signal invalid conversion */
			i = *d;
		} else if ((m2 & 0x1ffff) | m3) {
			/* signal inexact */
			e += 1.0e100;
		}
		if (i0 < 0)
			return (LLONG_MIN);
		else
			return (LLONG_MAX);
	}

	/* Extract the mantissa. */
	m0 = 0x40000000 | ((m0 << 14) & 0x3fffc000) | ((m1 >> 18) & 0x3fff);
	m1 = (m1 << 14) | ((m2 >> 18) & 0x3fff);

	/*
	 * The most significant bit of the mantissa is now in bit 62 of m0:m1.
	 * Shift right by (62 - exp) bits.
	 */
	if (((m1 & ((1 << (62 - exp)) - 1)) | (m2 & 0x3ffff) | m3) != 0) {
		/* signal inexact */
		e += 1.0e100;
	}
	m1 >>= (62 - exp);
	m1 |= m0 << (exp - 30);
	m0 >>= (62 - exp);
	if (i0 < 0) {
		m0 = ~m0;
		m1 = ~m1;
		if (++m1 == 0)
			m0++;
	}
	return ((long long)(((unsigned long long)m0 << 32) | m1));
}

#endif /* _NO_LONGLONG */
