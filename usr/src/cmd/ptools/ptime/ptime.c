/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ptime.c	1.3	96/06/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>

static	int	look(char *);
static	void	hr_min_sec(char *, long);
static	void	prtime(char *, timestruc_t *);
static	int	perr(const char *);

static	void	tsadd(timestruc_t *result, timestruc_t *a, timestruc_t *b);
#if SOMEDAY
static	void	tssub(timestruc_t *result, timestruc_t *a, timestruc_t *b);
static	void	tszero(timestruc_t *);
static	int	tsiszero(timestruc_t *);
static	int	tscmp(timestruc_t *a, timestruc_t *b);
#endif

static	char	procname[100];

main(int argc, char **argv)
{
	char *cmd = strrchr(argv[0], '/');
	int rc = 0;
	int ctlfd;
	long ctl[2];
	pid_t pid;
	char cpid[8];
	struct siginfo info;

	if (argc <= 1) {
		if (cmd++ == NULL)
			cmd = argv[0];
		(void) fprintf(stderr,
			"usage:\t%s command [ args ... ]\n", cmd);
		(void) fprintf(stderr,
			"  (time a command using microstate accounting)\n");
		return (2);
	}

	switch (pid = fork()) {
	case -1:
		(void) fprintf(stderr, "%s: cannot fork\n", cmd);
		return (2);
	case 0:
		/* open the /proc ctl file and turn on microstate accounting */
		(void) sprintf(procname, "/proc/%ld/ctl", getpid());
		ctlfd = open(procname, O_WRONLY);
		ctl[0] = PCSET;
		ctl[1] = PR_MSACCT;
		(void) write(ctlfd, ctl, 2*sizeof (long));
		(void) close(ctlfd);
		(void) execvp(argv[1], &argv[1]);
		(void) fprintf(stderr, "%s: exec failed\n", cmd);
		_exit(2);
	}

	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) waitid(P_PID, pid, &info, WEXITED | WNOWAIT);

	(void) sprintf(cpid, "%ld", pid);
	(void) look(cpid);

	(void) waitid(P_PID, pid, &info, WEXITED);

	return (rc);
}

static int
look(char *arg)
{
	char pathname[100];
	int rval = 0;
	int fd;
	char *pidp;
	psinfo_t psinfo;
	prusage_t prusage;
	timestruc_t real, user, sys;
	register prusage_t *pup = &prusage;

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
	(void) strcat(pathname, "/psinfo");
	if ((fd = open(pathname, O_RDONLY)) < 0)
		return (perr(NULL));
	else if (read(fd, &psinfo, sizeof (psinfo)) != sizeof (psinfo))
		rval = perr("read psinfo");
	(void) close(fd);

	(void) strcpy(pathname, procname);
	(void) strcat(pathname, "/usage");
	if ((fd = open(pathname, O_RDONLY)) < 0)
		return (perr(NULL));
	else if (read(fd, &prusage, sizeof (prusage)) != sizeof (prusage))
		rval = perr("read usage");
	(void) close(fd);

	if (rval) {
		if (errno == ENOENT) {
			(void) printf("%s\t<defunct>\n", pidp);
			return (0);
		}
		return (rval);
	}

	real = pup->pr_rtime;
	user = pup->pr_utime;
	sys = pup->pr_stime;
	tsadd(&sys, &sys, &pup->pr_ttime);

	(void) fprintf(stderr, "\n");
	prtime("real", &real);
	prtime("user", &user);
	prtime("sys", &sys);

	return (0);
}

static void
hr_min_sec(char *buf, long sec)
{
	if (sec >= 3600)
		(void) sprintf(buf, "%ld:%.2ld:%.2ld",
			sec / 3600, (sec % 3600) / 60, sec % 60);
	else if (sec >= 60)
		(void) sprintf(buf, "%ld:%.2ld",
			sec / 60, sec % 60);
	else {
		(void) sprintf(buf, "%ld", sec);
	}
}

static void
prtime(char *name, timestruc_t *ts)
{
	char buf[32];

	hr_min_sec(buf, ts->tv_sec);
	(void) fprintf(stderr, "%-4s %8s.%.3lu\n",
		name, buf, ts->tv_nsec/1000000);
}

static int
perr(const char *s)
{
	if (s == NULL || errno != ENOENT) {
		if (s)
			(void) fprintf(stderr, "%s: ", procname);
		else
			s = procname;
		perror(s);
	}
	return (1);
}

static	void
tsadd(timestruc_t *result, timestruc_t *a, timestruc_t *b)
{
	result->tv_sec = a->tv_sec + b->tv_sec;
	if ((result->tv_nsec = a->tv_nsec + b->tv_nsec) >= 1000000000) {
		result->tv_nsec -= 1000000000;
		result->tv_sec += 1;
	}
}

#if SOMEDAY

static	void
tssub(timestruc_t *result, timestruc_t *a, timestruc_t *b)
{
	result->tv_sec = a->tv_sec - b->tv_sec;
	if ((result->tv_nsec = a->tv_nsec - b->tv_nsec) < 0) {
		result->tv_nsec += 1000000000;
		result->tv_sec -= 1;
	}
}

static	void
tszero(timestruc_t *a)
{
	a->tv_sec = 0;
	a->tv_nsec = 0;
}

static	int
tsiszero(timestruc_t *a)
{
	return (a->tv_sec == 0 && a->tv_nsec == 0);
}

static	int
tscmp(timestruc_t *a, timestruc_t *b)
{
	if (a->tv_sec > b->tv_sec)
		return (1);
	if (a->tv_sec < b->tv_sec)
		return (-1);
	if (a->tv_nsec > b->tv_nsec)
		return (1);
	if (a->tv_nsec < b->tv_nsec)
		return (-1);
	return (0);
}

#endif	/* SOMEDAY */
