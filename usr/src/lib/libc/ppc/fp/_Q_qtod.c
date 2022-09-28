/*
 * Copyright (c) 1988, 1989, 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)_Q_qtod.c	1.6	94/09/02 SMI"

#include "synonyms.h"
#include "_Qquad.h"
#include "_Qglobals.h"

extern  _Q_set_exception();

double
_q_qtod(const quadruple *x)
{
	unpacked	px;
	double		d;
	unsigned	cexc = 0;
	_fp_unpack(&px, x, fp_op_extended, &cexc);
	_fp_pack(&px, &d, fp_op_double, &cexc);
	_Q_set_exception(cexc);
	return (d);
}
