/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fltname.c	1.3	96/06/18 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <sys/fault.h>

static char flt_name[20];

/* return the name of the fault */
/* return NULL if unknown fault */
char *
rawfltname(int flt)
{
	register char *name;

	switch (flt) {
	case FLTILL:	name = "FLTILL";	break;
	case FLTPRIV:	name = "FLTPRIV";	break;
	case FLTBPT:	name = "FLTBPT";	break;
	case FLTTRACE:	name = "FLTTRACE";	break;
	case FLTACCESS:	name = "FLTACCESS";	break;
	case FLTBOUNDS:	name = "FLTBOUNDS";	break;
	case FLTIOVF:	name = "FLTIOVF";	break;
	case FLTIZDIV:	name = "FLTIZDIV";	break;
	case FLTFPE:	name = "FLTFPE";	break;
	case FLTSTACK:	name = "FLTSTACK";	break;
	case FLTPAGE:	name = "FLTPAGE";	break;
	default:	name = NULL;		break;
	}

	return (name);
}

/* return the name of the fault */
/* manufacture a name if fault unknown */
char *
fltname(int flt)
{
	register char *name = rawfltname(flt);

	if (name == NULL) {			/* manufacture a name */
		(void) sprintf(flt_name, "FLT#%d", flt);
		name = flt_name;
	}

	return (name);
}
