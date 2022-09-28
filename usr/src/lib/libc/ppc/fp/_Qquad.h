/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)_Qquad.h 1.5	94/09/09 SMI"

/*
 * Header file for long double == quadruple-precision run-time support. C
 * "long double" and Fortran "real*16" are implemented identically on all
 * architectures.
 *
 * Thus the quad run-time support is intentionally coded as C-callable
 * routines for portability.
 *
 * Mixed-case identifiers with leading _ are intentionally chosen to
 * minimize conflicts with user-defined C and Fortran identifiers.
 */

#ifndef _QUAD_INCLUDED_
#define	_QUAD_INCLUDED_		/* Render harmless multiple inclusions. */

#include <sys/types.h>		/* _BIG or _LITTLE_ENDIAN */
#include <math.h>		/* to get float macros */
				/* note that quadruple is defined in  */
				/* floatingpoint.h which is included in */
				/* math.h  */

/* *****
	Quad support: C run-time in libc/crt
**** */

extern quadruple _q_neg(/* quadruple x */);	/* returns -x */
extern quadruple _q_add(/* quadruple x, y */);	/* returns x + y */
extern quadruple _q_sub(/* quadruple x, y */);	/* returns x - y */
extern quadruple _q_mul(/* quadruple x, y */);	/* returns x * y */
extern quadruple _q_div(/* quadruple x, y */);	/* returns x / y */
extern quadruple _q_sqrt(/* quadruple x */);	/* return sqrt(x) */
extern enum fcc_type
	_q_cmp(/* quadruple x, y */);	/* x compare y , exception on sNaN */
extern enum fcc_type
	_q_cmpe(/* quadruple x, y */);	/* x compare y , exception on sqNaN */
extern int   _q_feq(/* quadruple x, y */);	/* return TRUE if x == y */
extern int   _q_fne(/* quadruple x, y */);	/* return TRUE if x != y */
extern int   _q_fgt(/* quadruple x, y */);	/* return TRUE if x >  y */
extern int   _q_fge(/* quadruple x, y */);	/* return TRUE if x >= y */
extern int   _q_flt(/* quadruple x, y */);	/* return TRUE if x <  y */
extern int   _q_fle(/* quadruple x, y */);	/* return TRUE if x <= y */

	/* Conversion routines are pretty straightforward. */

extern quadruple	_q_stoq(float s);
extern float		_q_qtos(const quadruple *x);
extern quadruple	_q_dtoq(/* double d */);
extern quadruple	_q_itoq(/* int i */);
extern quadruple	_q_utoq(/* unsigned u */);
extern double		_q_qtod(/* quadruple x */);
extern int		_q_qtoi(/* quadruple x */);
extern unsigned		_q_qtou(/* quadruple x */);

/* *****
	Quad support: scanf/printf support in libm/libc:
	decimal_to_quadruple() is in libm/libc/decimal_bin.c
	quadruple_to_decimal() is in libm/libc/float_decim.c
**** */

extern void
decimal_to_quadruple(/* quadruple *px; decimal_mode *pm;
			decimal_record *pd;
			fp_exception_field_type *ps; */);

extern void
quadruple_to_decimal(/* quadruple *px; decimal_mode *pm;
			decimal_record *pd;
			fp_exception_field_type *ps; */);

enum fcc_type		/* relationships for loading into cc */
	{
	fcc_equal	= 0,
	fcc_less	= 1,
	fcc_greater	= 2,
	fcc_unordered	= 3
	};


typedef			/* FPU register viewed as single components. */
	struct
	{
#ifdef	_LITTLE_ENDIAN
	unsigned significand :	23;
	unsigned exponent :	 8;
	unsigned sign :		 1;
#else	/* _BIG_ENDIAN */
	unsigned sign :		 1;
	unsigned exponent :	 8;
	unsigned significand :	23;
#endif	/* _LITTLE_ENDIAN */
	}
	single_type;


typedef			/* FPU register viewed as double components. */
	struct
	{
#ifdef	_LITTLE_ENDIAN
	unsigned significand :	20;
	unsigned exponent :	11;
	unsigned sign :		 1;
#else	/* _BIG_ENDIAN */
	unsigned sign :		 1;
	unsigned exponent :	11;
	unsigned significand :	20;
#endif	/* _LITTLE_ENDIAN */
	}
	double_type;


typedef			/* FPU register viewed as extended components. */
	struct
	{
#ifdef	_LITTLE_ENDIAN
	unsigned significand :	16;
	unsigned exponent :	15;
	unsigned sign :		 1;
#else	/* _BIG_ENDIAN */
	unsigned sign :		 1;
	unsigned exponent :	15;
	unsigned significand :	16;
#endif	/* _LITTLE_ENDIAN */
	}
	extended_type;

enum fp_op_type		/* Type specifiers in FPU instructions. */
	{
	fp_op_integer	= 0, /* Not in hardware, but convenient to define. */
	fp_op_single	= 1,
	fp_op_double	= 2,
	fp_op_extended	= 3,
	fp_op_longlong	= 4
	};

#endif				/* QUAD_INCLUDED */
