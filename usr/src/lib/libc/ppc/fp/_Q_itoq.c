/*
 * Copyright (c) 1988, 1989, 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)_Q_itoq.c	1.6	94/09/02 SMI"

#include "synonyms.h"
#include "_Qquad.h"
#include "_Qglobals.h"

quadruple
_q_itoq(x)
	int x;
{
	unpacked	px;
	quadruple	q;
	unsigned	cexc = 0;
	_fp_unpack(&px, &x, fp_op_integer, &cexc);
	_fp_pack(&px, &q, fp_op_extended, &cexc);
	_Q_set_exception(0);		/* clear cexc */
	return (q);
}
