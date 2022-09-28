/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)acctwtmp.c	1.9	96/08/13 SMI"	/* SVr4.0 1.9	*/
/*
 *	acctwtmp reason /var/adm/wtmp
 *	writes utmp.h record (with current time) to specific file
 *	acctwtmp `uname` /var/adm/wtmp as part of startup
 *	acctwtmp pm /var/adm/wtmp  (taken down for pm, for example)
 */
#include <stdio.h>
#include <sys/types.h>
#include "acctdef.h"
#include <utmp.h>

struct	utmp	wb;
char	*strncpy();

main(argc, argv)
char **argv;
{

	register struct utmp *p;

	if (argc < 3)
		fprintf(stderr, "Usage: %s reason wtmp_file\n",
			argv[0]), exit(1);

	strncpy(wb.ut_line, argv[1], sizeof(wb.ut_line));
	wb.ut_line[11] = NULL;
	wb.ut_type = ACCOUNTING;
	time(&wb.ut_time);
	utmpname(argv[2]);

	if (pututline(&wb) == (struct utmp *)NULL)
		printf("acctwtmp - pututline failed\n");
}
