/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pldd.c	1.3	96/06/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <link.h>
#include <libelf.h>
#include <procfs.h>
#include "dynlib.h"

static	int	look(char *);
static	void	perr(char *);

static	char	procname[64];
static	char	*command;

main(int argc, char **argv)
{
	int rc = 0;

	command = strrchr(argv[0], '/');
	if (command++ == NULL)
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr,
			"usage:  %s pid ...\n", command);
		(void) fprintf(stderr,
			"  (report process dynamic libraries)\n");
		return (2);
	}

	while (--argc > 0)
		rc += look(*++argv);

	return (rc);
}

static int
look(char *arg)
{
	char		pathname[100];
	int 		i;
	int		asfd;
	int		psfd;
	psinfo_t	psinfo;
	pid_t		pid;
	char		*pidp;
	char		*name;

	/*
	 * Generate the /proc filename and a pointer to the pid portion.
	 */
	if (strchr(arg, '/') != NULL)
		(void) strncpy(procname, arg, sizeof (procname));
	else {
		(void) strcpy(procname, "/proc/");
		(void) strncat(procname, arg, sizeof (procname)-6);
	}
	pidp = strrchr(procname, '/')+1;
	while (*pidp == '0' && *(pidp+1) != '\0')
		pidp++;
	pid = atol(pidp);

	/*
	 * Open the address space of the process to be examined.
	 */
	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/as");
	if ((asfd = open(pathname, O_RDONLY)) < 0) {
		perr(pathname);
		return (1);
	}

	/*
	 * Read its psinfo structure.
	 */
	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/psinfo");
	if ((psfd = open(pathname, O_RDONLY)) < 0 ||
	    read(psfd, &psinfo, sizeof (psinfo)) != sizeof (psinfo)) {
		perr(pathname);
		if (psfd >= 0)
			(void) close(psfd);
		(void) close(asfd);
		return (1);
	}
	(void) close(psfd);

	load_ldd_names(asfd, pid);
	(void) close(asfd);

	(void) printf("%s:\t%.70s\n", pidp, psinfo.pr_psargs);
	for (i = 0; (name = index_name(i)) != NULL; i++)
		(void) printf("%s\n", name);

	clear_names();

	return (0);
}

static void
perr(char *s)
{
	char message[100];

	if (s)
		(void) sprintf(message, "%s: %s: %s", command, procname, s);
	else
		(void) sprintf(message, "%s: %s", command, procname);
	perror(message);
}
