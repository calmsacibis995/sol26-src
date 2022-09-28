/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)atof.c 1.4	94/09/09 SMI"

#include	"synonyms.h"
#include	<stdio.h>

extern	double
strtod();

double
atof(char *cp)
{
	return (strtod(cp, (char **) NULL));
}
