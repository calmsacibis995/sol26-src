/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)signame.c	1.3	96/06/18 SMI"

#include <stdio.h>
#include <string.h>
#include <sys/signal.h>

static char sig_name[20];

/* return the name of the signal */
/* return NULL if unknown signal */
char *
rawsigname(int sig)
{
	/* belongs in some header file */
	extern int sig2str(int, char *);

	/*
	 * The C library function sig2str() omits the leading "SIG".
	 */
	(void) strcpy(sig_name, "SIG");

	if (sig > 0 && sig2str(sig, sig_name+3) == 0)
		return (sig_name);

	return (NULL);
}

/* return the name of the signal */
/* manufacture a name for unknown signal */
char *
signame(int sig)
{
	register char *name = rawsigname(sig);

	if (name == NULL) {			/* manufacture a name */
		(void) sprintf(sig_name, "SIG#%d", sig);
		name = sig_name;
	}

	return (name);
}
