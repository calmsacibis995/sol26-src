/*
 * Copyright (c) 1988, 1989, 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)_Q_utoq.c	1.6	94/09/02 SMI"

#include "synonyms.h"
#include "_Qquad.h"
#include "_Qglobals.h"

quadruple
_q_utoq(unsigned x)
{
	unpacked	px;
	double 		one = 1.0;
	quadruple	q, c;
	int 		*pc = (int*)&c;
	unsigned	cexc = 0;
	pc[0] = pc[1] = pc[2] = pc[3] = 0;	/* pc = 2^31 */
	if (*(int*)&one == 0)
		pc[3] = 0x401e0000;
	else
		pc[0] = 0x401e0000;

	if ((x & 0x80000000) != 0) {
		x ^= 0x80000000;
		_fp_unpack(&px, &x, fp_op_integer, &cexc);
		_fp_pack(&px, &q, fp_op_extended, &cexc);
		q = _q_add(q, c);
	} else {
		_fp_unpack(&px, &x, fp_op_integer, &cexc);
		_fp_pack(&px, &q, fp_op_extended, &cexc);
	}
	_Q_set_exception(0);		/* clear cexc */
	return (q);
}
