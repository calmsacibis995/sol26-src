/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pcred.c	1.5	96/10/15 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>

static	int	look(char *);
static	int	perr(char *);

static 	int	all = 0;
static	char	procname[64];

main(int argc, char **argv)
{
	int rc = 0;

	if (argc <= 1) {
		char *cmd = strrchr(argv[0], '/');

		if (cmd++ == NULL)
			cmd = argv[0];
		(void) fprintf(stderr, "usage:  %s pid ...\n", cmd);
		(void) fprintf(stderr, "  (report process credentials)\n");
		return (2);
	}

	if (argc > 1 && strcmp(argv[1], "-a") == 0) {
		all = 1;
		argc--;
		argv++;
	}

	while (--argc > 0)
		rc += look(*++argv);

	return (rc);
}


static int
look(char *arg)
{
	char credfile[100];
	register int cfd;
	u_int size;
	char *pidp;
	prcred_t *prcred;
	int grp;
	unsigned guess = NGROUPS_MAX;	/* initial guess at number of groups */

	if (strchr(arg, '/') != NULL)
		(void) strncpy(procname, arg, sizeof (procname));
	else {
		(void) strcpy(procname, "/proc/");
		(void) strncat(procname, arg, sizeof (procname)-6);
	}
	pidp = strrchr(procname, '/')+1;
	while (*pidp == '0' && *(pidp+1) != '\0')
		pidp++;

	(void) strcpy(credfile, procname);
	(void) strcat(credfile, "/cred");
	if ((cfd = open(credfile, O_RDONLY)) < 0)
		return (perr(NULL));
	for (;;) {
		size = sizeof (prcred_t) + guess*sizeof (gid_t);
		prcred = malloc(size);
		if (pread(cfd, prcred, size, (off_t)0) < 0) {
			free(prcred);
			(void) close(cfd);
			return (perr(credfile));
		}
		if (guess >= prcred->pr_ngroups)    /* got all the groups */
			break;
		/* reallocate and try again */
		free(prcred);
		guess = prcred->pr_ngroups;
	}
	(void) close(cfd);

	(void) printf("%s:\t", pidp);

	if (!all &&
	    prcred->pr_euid == prcred->pr_ruid &&
	    prcred->pr_ruid == prcred->pr_suid)
		(void) printf("e/r/suid=%ld  ",
			prcred->pr_euid);
	else
		(void) printf("euid=%ld ruid=%ld suid=%ld  ",
			prcred->pr_euid,
			prcred->pr_ruid,
			prcred->pr_suid);

	if (!all &&
	    prcred->pr_egid == prcred->pr_rgid &&
	    prcred->pr_rgid == prcred->pr_sgid)
		(void) printf("e/r/sgid=%ld\n",
			prcred->pr_egid);
	else
		(void) printf("egid=%ld rgid=%ld sgid=%ld\n",
			prcred->pr_egid,
			prcred->pr_rgid,
			prcred->pr_sgid);

	if (prcred->pr_ngroups != 0 &&
	    (all || prcred->pr_ngroups != 1 ||
	    prcred->pr_groups[0] != prcred->pr_rgid)) {
		(void) printf("\tgroups:");
		for (grp = 0; grp < prcred->pr_ngroups; grp++)
			(void) printf(" %ld", prcred->pr_groups[grp]);
		(void) printf("\n");
	}

	free(prcred);

	return (0);
}

static int
perr(char *s)
{
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}
