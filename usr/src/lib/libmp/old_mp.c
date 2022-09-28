/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
/* 	Portions Copyright(c) 1996, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

/* fix for bugid 1240660 redefine old libmp interfaces to go to the new */
/* mp_*() interfaces */

#ident	"@(#)old_mp.c	1.2	96/04/05 SMI"	/* SVr4.0 1.1	*/

/* LINTLIBRARY */

#include <mp.h>

void gcd(a, b, c) MINT *a, *b, *c; { mp_gcd(a, b, c); }

void madd(a, b, c) MINT *a, *b, *c; { mp_madd(a, b, c); }

void msub(a, b, c) MINT *a, *b, *c; { mp_msub(a, b, c); }

void mdiv(a, b, q, r) MINT *a, *b, *q, *r; { mp_mdiv(a, b, q, r); }

void sdiv(a, n, q, r) MINT *a; short n; MINT *q; short *r;
{ mp_sdiv(a, n, q, r); }

int min(a) MINT *a; { return (mp_min(a)); }

void mout(a) MINT *a; { mp_mout(a); }

int msqrt(a, b, r) MINT *a, *b, *r; { return (mp_msqrt(a, b, r)); }

void mult(a, b, c) MINT *a, *b, *c; { mp_mult(a, b, c); }

void pow(a, b, c, d) MINT *a, *b, *c, *d; { mp_pow(a, b, c, d); }

void rpow(a, n, b) MINT *a; int n; MINT *b; { mp_rpow(a, n, b); }

MINT *itom(n) int n; { return (mp_itom(n)); }

int mcmp(a, b) MINT *a, *b; { return (mp_mcmp(a, b)); }

MINT *xtom(key) char *key; { return (mp_xtom(key)); }

char *mtox(key) MINT *key; { return (mp_mtox(key)); }

void mfree(a) MINT *a; { mp_mfree(a); }

extern short *_mp_xalloc();

/* VARARGS */
short *xalloc(nint, s) int nint; char *s; { return (_mp_xalloc(nint, s)); }

void xfree(c) MINT *c; { _mp_xfree(c); }
