/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)psig.c	1.4	96/08/21 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>

extern	char	*signame(int);

static	char	*sigflags(int, int);
static	int	look(char *);
static	int	perr(char *);

static	int	all = 0;
static	char	procname[64];
static	char	*command;

main(int argc, char **argv)
{
	int rc = 0;

	command = strrchr(argv[0], '/');
	if (command++ == NULL)
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr, "usage:  %s pid ...\n", command);
		(void) fprintf(stderr, "  (report process signal actions)\n");
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
	char pathname[100];
	struct stat statb;
	int fd;
	int sig;
	char *pidp;
	sigset_t holdmask;
	int maxsig;
	struct sigaction *action;
	pstatus_t pstatus;
	psinfo_t psinfo;

	if (strchr(arg, '/') != NULL)
		(void) strncpy(procname, arg, sizeof (procname));
	else {
		(void) strcpy(procname, "/proc/");
		(void) strncat(procname, arg, sizeof (procname)-6);
	}
	pidp = strrchr(procname, '/')+1;
	while (*pidp == '0' && *(pidp+1) != '\0')
		pidp++;

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/status");
	if ((fd = open(pathname, O_RDONLY)) < 0)
		return (perr(NULL));
	if (read(fd, (char *)&pstatus, sizeof (pstatus)) != sizeof (pstatus)) {
		(void) close(fd);
		return (perr("read status"));
	}
	(void) close(fd);
	holdmask = pstatus.pr_lwp.pr_lwphold;

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/psinfo");
	if ((fd = open(pathname, O_RDONLY)) < 0)
		return (perr("open psinfo"));
	if (read(fd, (char *)&psinfo, sizeof (psinfo)) != sizeof (psinfo)) {
		(void) close(fd);
		return (perr("read psinfo"));
	}
	(void) close(fd);

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/sigact");
	if ((fd = open(pathname, O_RDONLY)) < 0)
		return (perr("open sigact"));
	if (fstat(fd, &statb) != 0) {
		(void) close(fd);
		return (perr("fstat sigact"));
	}
	maxsig = statb.st_size / sizeof (struct sigaction);
	action = malloc(maxsig * sizeof (struct sigaction));
	if (action == NULL) {
		(void) fprintf(stderr,
		"%s: cannot malloc() space for %d sigaction structures\n",
			command, maxsig);
		(void) close(fd);
		return (1);
	}
	if (read(fd, (char *)action, maxsig * sizeof (struct sigaction)) !=
	    maxsig * sizeof (struct sigaction)) {
		(void) close(fd);
		free((char *)action);
		return (perr("read sigact"));
	}
	(void) close(fd);

	(void) printf("%s:\t%.70s\n", pidp, psinfo.pr_psargs);

	for (sig = 1; sig <= maxsig; sig++) {
		struct sigaction *sp = &action[sig-1];
		int caught = 0;

		/* signame() returns "SIG..."; skip the "SIG" part */
		(void) printf("%s\t", signame(sig)+3);

		if (prismember(&holdmask, sig))
			(void) printf("blocked,");

		if (sp->sa_handler == SIG_DFL)
			(void) printf("default");
		else if (sp->sa_handler == SIG_IGN)
			(void) printf("ignored");
		else {
			caught = 1;
			(void) printf("caught");
		}

		if (caught || all) {
			int anyb = 0;
			int bsig;
			char *s = sigflags(sig, sp->sa_flags);

			(void) printf("%s", (*s != '\0')? s : "\t0");
			for (bsig = 1; bsig <= maxsig; bsig++) {
				if (prismember(&sp->sa_mask, bsig)) {
					(void) printf(anyb++? "," : "\t");
					(void) printf("%s", signame(bsig)+3);
				}
			}
		} else if (sig == SIGCLD) {
			(void) printf("%s", sigflags(sig,
			    sp->sa_flags & (SA_NOCLDWAIT|SA_NOCLDSTOP)));
		} else if (sig == SIGWAITING) {
			(void) printf("%s", sigflags(sig,
			    sp->sa_flags & SA_WAITSIG));
		}
		(void) printf("\n");
	}

	free((char *)action);
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

static char *
sigflags(int sig, int flags)
{
	static char code_buf[100];
	char *str = code_buf;
	int flagmask =
		(SA_ONSTACK|SA_RESETHAND|SA_RESTART|SA_SIGINFO|SA_NODEFER);

	switch (sig) {
	case SIGCLD:
		flagmask |= (SA_NOCLDSTOP|SA_NOCLDWAIT);
		break;
	case SIGWAITING:
		flagmask |= SA_WAITSIG;
		break;
	}

	*str = '\0';
	if (flags & ~flagmask)
		(void) sprintf(str, ",0x%x,", flags & ~flagmask);
	else if (flags == 0)
		return (str);

	if (flags & SA_RESTART)
		(void) strcat(str, ",RESTART");
	if (flags & SA_RESETHAND)
		(void) strcat(str, ",RESETHAND");
	if (flags & SA_ONSTACK)
		(void) strcat(str, ",ONSTACK");
	if (flags & SA_SIGINFO)
		(void) strcat(str, ",SIGINFO");
	if (flags & SA_NODEFER)
		(void) strcat(str, ",NODEFER");

	switch (sig) {
	case SIGCLD:
		if (flags & SA_NOCLDWAIT)
			(void) strcat(str, ",NOCLDWAIT");
		if (flags & SA_NOCLDSTOP)
			(void) strcat(str, ",NOCLDSTOP");
		break;
	case SIGWAITING:
		if (flags & SA_WAITSIG)
			(void) strcat(str, ",WAITSIG");
		break;
	}

	*str = '\t';

	return (str);
}
