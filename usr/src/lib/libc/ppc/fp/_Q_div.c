/*
 * Copyright (c) 1988, 1989, 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)_Q_div.c	1.6	94/09/02 SMI"

#include "synonyms.h"
#include "_Qquad.h"
#include "_Qglobals.h"

extern  _Q_set_exception();

quadruple
_q_div(const quadruple *x, const quadruple *y)
{
	unpacked	px, py, pz;
	quadruple	z;
	unsigned	cexc = 0;
	_fp_unpack(&px, x, fp_op_extended, &cexc);
	_fp_unpack(&py, y, fp_op_extended, &cexc);
	_fp_div(&px, &py, &pz, &cexc);
	_fp_pack(&pz, &z, fp_op_extended, &cexc);
	_Q_set_exception(cexc);
	return (z);
}
