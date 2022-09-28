/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)prun.c	1.3	96/06/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>

static	int	start(char *);
static	int	perr(char *);

static	char	procname[64];

main(int argc, char **argv)
{
	int rc = 0;

	if (argc <= 1) {
		char *cmd = strrchr(argv[0], '/');

		if (cmd++ == NULL)
			cmd = argv[0];
		(void) fprintf(stderr, "usage:  %s pid ...\n", cmd);
		(void) fprintf(stderr, "  (set stopped processes running)\n");
		return (2);
	}

	while (--argc > 0)
		rc += start(*++argv);

	return (rc);
}

static int
start(char *arg)
{
	char ctlfile[100];
	int ctlfd;
	long ctl[4];
	char *pidp;

	if (strchr(arg, '/') != NULL)
		(void) strncpy(procname, arg, sizeof (procname));
	else {
		(void) strcpy(procname, "/proc/");
		(void) strncat(procname, arg, sizeof (procname)-6);
	}
	pidp = strrchr(procname, '/')+1;
	while (*pidp == '0' && *(pidp+1) != '\0')
		pidp++;

	(void) strcpy(ctlfile, procname);
	(void) strcat(ctlfile, "/ctl");
	errno = 0;
	if ((ctlfd = open(ctlfile, O_WRONLY|O_EXCL)) >= 0) {
		ctl[0] = PCKILL;
		ctl[1] = SIGCONT;
		ctl[2] = PCRUN;
		ctl[3] = 0;
		(void) write(ctlfd, (char *)ctl, 4*sizeof (long));
		(void) close(ctlfd);
	}

	return (perr(NULL));
}

static int
perr(char *s)
{
	if (errno == 0 || errno == EBUSY)
		return (0);
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}
