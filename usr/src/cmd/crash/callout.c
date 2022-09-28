#ident	"@(#)callout.c	1.9	95/11/13 SMI"		/* SVr4.0 1.8 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1986,1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * This file contains code for the crash function callout.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/callo.h>
#include <sys/elf.h>
#include "crash.h"


static int prcallout(char *);


/* get arguments for callout function */
int
getcallout()
{
	int    c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind])
		longjmp(syn, 0);

	fprintf(fp, "FUNCTION        ARGUMENT        TIME  ID\n");

	prcallout("callout_state");
	prcallout("rt_callout_state");
}

/* Print a callout table. */
int
prcallout(char *callout_tbl_name)
{
	callout_state_t *callout_tbl_addr = sym2addr(callout_tbl_name);
	callout_state_t	 cs;   /* copy of the callout table */
	callout_t	 c;    /* copy of a callout entry */
	callout_t	*cp;   /* address of callout entry in kernel mem */
	int		 i;
	char		*name;

	/* Get a copy of the callout table. */
	GET_VAR(cs, callout_tbl_addr, "callout table");

	/* For each callout bucket ... */
	for (i = 0; i < CALLOUT_BUCKETS; i++) {

		/* For each callout in that bucket (end points to bucket) ... */
		cp = cs.cs_bucket[i].b_first;
		while (cp != (callout_t *)&callout_tbl_addr->cs_bucket[i]) {

			/* Get the symbol name for that callout. */
			GET_VAR(c, cp, "callout table entry");
			name = addr2sym(c.c_func);

			/* Print the callout entry. */
			fprintf(fp, "%-15s %08lx  %10u  %08lx\n",
				name,
				c.c_arg,
				c.c_runtime,
				c.c_xid);

			/* Advance to the next callout entry in that bucket. */
			cp = c.c_next;
		}
	}
}
