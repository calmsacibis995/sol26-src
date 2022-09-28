/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)strtod.c 1.6	94/09/09 SMI"
#include "synonyms.h"
#include <errno.h>
#include <stdio.h>
#include <values.h>
#include <floatingpoint.h>

extern enum fp_direction_type _QgetRD();

double
strtod(const char *cp, char **ptr)
{
	double		x;
	decimal_mode	mr;
	decimal_record	dr;
	fp_exception_field_type fs;
	enum decimal_string_form form;
	char		*pechar;

	string_to_decimal((char **) &cp, MAXINT, 0, &dr, &form, &pechar);
	if (ptr != (char **) NULL)
		*ptr = (char *) cp;
	if (form == invalid_form)
		return (0.0);	/* Shameful kluge for SVID's sake. */
	mr.rd = _QgetRD();
	decimal_to_double(&x, &mr, &dr, &fs);
	if (fs & (1 << fp_overflow)) {	/* Overflow. */
		errno = ERANGE;
	}
	if (fs & (1 << fp_underflow)) {	/* underflow */
		errno = ERANGE;
	}
	return (x);
}
