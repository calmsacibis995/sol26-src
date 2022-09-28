/*
 * Copyright (c) 1989, 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)_Q_fcc.c	1.6	94/09/02 SMI"

/* integer function _q_feq, _q_fne, _q_fgt, _q_fge, _q_flt, _q_fle */

#include "synonyms.h"
#include "_Qquad.h"

int _q_feq(const quadruple *x, const quadruple *y)
{
	enum fcc_type	fcc;
	fcc = _q_cmp(x, y);
	return (fcc_equal == fcc);
}

int _q_fne(const quadruple *x, const quadruple *y)
{
	enum fcc_type	fcc;
	fcc = _q_cmp(x, y);
	return (fcc_equal != fcc);
}

int _q_fgt(const quadruple *x, const quadruple *y)
{
	enum fcc_type	fcc;
	fcc = _q_cmpe(x, y);
	return (fcc_greater == fcc);
}

int _q_fge(const quadruple *x, const quadruple *y)
{
	enum fcc_type	fcc;
	fcc = _q_cmpe(x, y);
	return (fcc_greater == fcc || fcc_equal == fcc);
}

int _q_flt(const quadruple *x, const quadruple *y)
{
	enum fcc_type	fcc;
	fcc = _q_cmpe(x, y);
	return (fcc_less == fcc);
}

int _q_fle(const quadruple *x, const quadruple *y)
{
	enum fcc_type	fcc;
	fcc = _q_cmpe(x, y);
	return (fcc_less == fcc || fcc_equal == fcc);
}
