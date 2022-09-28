/*
 * Copyright (c) 1988, 1989, 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)_Q_qtos.c	1.6	94/09/02 SMI"

#include "synonyms.h"
#include "_Qquad.h"
#include "_Qglobals.h"

extern  _Q_set_exception();

float _q_qtos(const quadruple *x)
{
	unpacked	px;
	float		s;
	unsigned	cexc = 0;
	_fp_unpack(&px, x, fp_op_extended, &cexc);
	_fp_pack(&px, &s, fp_op_single, &cexc);
	_Q_set_exception(cexc);
	return (s);
}
