/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pcontrol.c	1.6	96/07/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include "pcontrol.h"
#include "ramdata.h"

/* Process Management */

/* This module is carefully coded to contain only read-only data */
/* All read/write data is defined in ramdata.c (see also ramdata.h) */

/*
 * Function prototypes for static routines in this module.
 */
static	void	deadcheck(process_t *);
static	int	execute(process_t *, int);
static	int	checksyscall(process_t *);

static int
dupfd(int fd, int dfd)
{
	/*
	 * Make sure fd not one of 0, 1, or 2.
	 * This allows the program to work when spawned by init(1m).
	 * Also, if dfd is non-zero, dup the fd to be dfd.
	 */
	if (dfd > 0 || (0 <= fd && fd <= 2)) {
		if (dfd <= 0)
			dfd = 3;
		dfd = fcntl(fd, F_DUPFD, dfd);
		(void) close(fd);
		fd = dfd;
	}
	/*
	 * Mark it close-on-exec so any created process doesn't inherit it.
	 */
	if (fd >= 0)
		(void) fcntl(fd, F_SETFD, 1);
	return (fd);
}

/* create new controlled process */
int
Pcreate(process_t *P,
	char **args)	/* argument array, including the command name */
{
	register int i;
	register pid_t pid;
	register int fd;
	char procname[100];
	char *fname;

	pid = fork();

	if (pid == 0) {			/* child process */
		(void) pause();		/* wait for PRSABORT from parent */

		/* if running setuid or setgid, reset credentials to normal */
		if ((i = getgid()) != getegid())
			(void) setgid(i);
		if ((i = getuid()) != geteuid())
			(void) setuid(i);

		(void) execvp(*args, args);	/* execute the command */
		_exit(127);
	}

	if (pid == -1) {		/* failure */
		perror("Pcreate fork()");
		return (-1);
	}

	/* initialize the process structure */
	(void) memset(P, 0, sizeof (*P));
	P->cntrl = TRUE;
	P->child = TRUE;
	P->state = PS_RUN;
	P->pid   = pid;
	P->asfd = -1;
	P->ctlfd = -1;
	P->statfd = -1;
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;

	/* open the /proc/pid files */
	(void) sprintf(procname, "%s/%ld/", procdir, pid);
	fname = procname + strlen(procname);

	/* exclusive write open prevents others from interfering */
	(void) strcpy(fname, "as");
	if ((fd = open(procname, (O_RDWR|O_EXCL))) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		perror("Pcreate open(\"as\")");
		(void) kill(pid, SIGKILL);
		return (-1);
	}
	P->asfd = fd;

	(void) strcpy(fname, "status");
	if ((fd = open(procname, O_RDONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		perror("Pcreate open(\"status\")");
		(void) kill(pid, SIGKILL);
		(void) close(P->asfd);
		P->asfd = -1;
		return (-1);
	}
	P->statfd = fd;

	(void) strcpy(fname, "ctl");
	if ((fd = open(procname, O_WRONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		perror("Pcreate open(\"ctl\")");
		(void) kill(pid, SIGKILL);
		(void) close(P->asfd);
		(void) close(P->statfd);
		P->asfd = -1;
		P->statfd = -1;
		return (-1);
	}
	P->ctlfd = fd;

	(void) Pstop(P, 0);	/* stop the controlled process */

	/* set run-on-last-close so process runs even if we die on a signal */
	if (Psetflags(P, PR_RLC) != 0)
		perror("Pcreate PR_RLC");

	for (;;) {		/* wait for process to sleep in pause() */
		if (P->state == PS_STOP &&
		    P->why.pr_lwp.pr_why == PR_REQUESTED &&
		    (P->why.pr_flags & PR_ASLEEP) &&
		    Pgetsysnum(P) == SYS_pause)
			break;

		if (P->state != PS_STOP ||	/* interrupt or process died */
		    Psetrun(P, 0, PRSTOP, 0) != 0) {	/* can't restart */
			long ctl[2];

			ctl[0] = PCKILL;
			ctl[1] = SIGKILL;
			(void) write(P->ctlfd, ctl, 2*sizeof (long));
			(void) kill(pid, SIGKILL);	/* overkill? */
			(void) close(P->ctlfd);
			(void) close(P->asfd);
			(void) close(P->statfd);
			P->ctlfd = -1;
			P->asfd = -1;
			P->statfd = -1;
			P->state = PS_DEAD;
			return (-1);
		}
		(void) Pwait(P, 0);
		/* dumpwhy(P, 0); */
	}

	(void) Psysentry(P, SYS_exit, 1);	/* catch these sys calls */
	(void) Psysentry(P, SYS_exec, 1);
	(void) Psysentry(P, SYS_execve, 1);

	/* kick it off the pause() */
	if (Psetrun(P, 0, PRSABORT, 0) == -1) {
		long ctl[2];

		perror("Pcreate PCRUN");
		ctl[0] = PCKILL;
		ctl[1] = SIGKILL;
		(void) write(P->ctlfd, ctl, 2*sizeof (long));
		(void) kill(pid, SIGKILL);	/* overkill? */
		(void) close(P->ctlfd);
		(void) close(P->asfd);
		(void) close(P->statfd);
		P->ctlfd = -1;
		P->asfd = -1;
		P->statfd = -1;
		P->state = PS_DEAD;
		return (-1);
	}

	(void) Pwait(P, 0);	/* wait for exec() or exit() */

	return (0);
}

/* grab existing process */
int
Pgrab(process_t *P,
	pid_t pid,		/* UNIX process ID */
	int force)		/* if TRUE, grab regardless */
{
	register int fd;
	uid_t ruid;
	struct prcred prcred;
	char procname[100];
	char *fname;
	int rc = 0;

	P->asfd = -1;
	P->ctlfd = -1;
	P->statfd = -1;
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;

again:	/* Come back here if we lose it in the Window of Vulnerability */
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	(void) memset(P, 0, sizeof (*P));
	P->ctlfd = -1;
	P->asfd = -1;
	P->statfd = -1;
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;

	/* open the /proc/pid files */
	(void) sprintf(procname, "%s/%ld/", procdir, pid);
	fname = procname + strlen(procname);

	/*
	 * Request exclusive open to avoid grabbing someone else's
	 * process and to prevent others from interfering afterwards.
	 * If this fails and the 'force' flag is set, attempt to
	 * open non-exclusively.
	 */
	(void) strcpy(fname, "as");
	if (((fd = open(procname, (O_RDWR|O_EXCL))) < 0 &&
	    (fd = (force? open(procname, O_RDWR) : -1)) < 0) ||
	    (fd = dupfd(fd, 0)) < 0) {
		switch (errno) {
		case ENOENT:
			rc = G_NOPROC;
			break;
		case EACCES:
		case EPERM:
			rc = G_PERM;
			break;
		case EBUSY:
			if (!force || geteuid() != 0) {
				rc = G_BUSY;
				break;
			}
			/* FALLTHROUGH */
		default:
			perror("Pgrab open(\"as\")");
			rc = G_STRANGE;
			break;
		}
		goto err;
	}
	P->asfd = fd;

	(void) strcpy(fname, "status");
	if ((fd = open(procname, O_RDONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		switch (errno) {
		case ENOENT:
			rc = G_NOPROC;
			break;
		default:
			perror("Pgrab open(\"status\")");
			rc = G_STRANGE;
			break;
		}
		goto err;
	}
	P->statfd = fd;

	(void) strcpy(fname, "ctl");
	if ((fd = open(procname, O_WRONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		switch (errno) {
		case ENOENT:
			rc = G_NOPROC;
			break;
		default:
			perror("Pgrab open(\"ctl\")");
			rc = G_STRANGE;
			break;
		}
		goto err;
	}
	P->ctlfd = fd;

	P->cntrl = TRUE;
	P->child = FALSE;
	P->state = PS_RUN;
	P->pid   = pid;

	/* ---------------------------------------------------- */
	/* We are now in the Window of Vulnerability (WoV).	*/
	/* The process may exec() a setuid/setgid or unreadable	*/
	/* object file between the open() and the PCSTOP.	*/
	/* We will get EAGAIN in this case and must start over.	*/
	/* ---------------------------------------------------- */

	/*
	 * If the process is a system process, we can't control it
	 * even if we are super-user.  First get the status structure.
	 */
	if (Pstatus(P, PCNULL, 0) != 0) {
		if (P->state == PS_LOST)	/* WoV */
			goto again;
		if (P->state == PS_DEAD)    /* Don't complain about zombies */
			rc = G_NOPROC;
		else {
			perror("Pgrab read(\"status\")");
			rc = G_STRANGE;
		}
		goto err;
	}
	if (P->why.pr_flags & PR_ISSYS) {
		rc = G_SYS;
		goto err;
	}

	/*
	 * Verify process credentials in case we are running setuid root.
	 * We only verify that our real uid matches the process's real uid.
	 * This means that the user really did create the process, even
	 * if using a different group id (via newgrp(1) for example).
	 */
	if (Pgetcred(P, &prcred) < 0) {
		if (errno == EAGAIN)	/* WoV */
			goto again;
		if (errno == ENOENT)	/* Don't complain about zombies */
			rc = G_NOPROC;
		else {
			perror("Pgrab credentials");
			rc = G_STRANGE;
		}
		goto err;
	}
	if ((ruid = getuid()) != 0 &&	/* super-user allowed anything */
	    ruid != prcred.pr_ruid) {	/* credentials check failed */
		errno = EACCES;
		rc = G_PERM;
		goto err;
	}

	/* get the ps information, even if it is a system process or ourself */
	if (Pgetpsinfo(P, &P->psinfo) < 0) {
		if (errno == EAGAIN)	/* WoV */
			goto again;
		if (errno == ENOENT)	/* Don't complain about zombies */
			rc = G_NOPROC;
		else {
			perror("Pgrab psinfo");
			rc = G_STRANGE;
		}
		goto err;
	}

	/* before stopping the process, make sure it's not ourself */
	if (pid == getpid()) {
		/*
		 * Verify that the process is really ourself:
		 * Set a magic number, read it through the
		 * /proc file and see if the results match.
		 */
		long magic1 = 0;
		long magic2 = 2;

		errno = 0;

		if (Pread(P, (long)&magic1, (char *)&magic2, sizeof (magic2))
		    == sizeof (magic2) &&
		    magic2 == 0 &&
		    (magic1 = 0xfeedbeef) &&
		    Pread(P, (long)&magic1, (char *)&magic2, sizeof (magic2))
		    == sizeof (magic2) &&
		    magic2 == 0xfeedbeef) {
			rc = G_SELF;
			goto err;
		}
	}

	/*
	 * If the process is already stopped or has been directed
	 * to stop via /proc, do not set run-on-last-close.
	 */
	if (P->state == PS_STOP ||
	    (P->why.pr_lwp.pr_flags & (PR_ISTOP|PR_DSTOP)))
		goto out;

	/*
	 * Mark the process run-on-last-close so
	 * it runs even if we die from SIGKILL.
	 */
	if (Psetflags(P, PR_RLC) != 0) {
		if (errno == EAGAIN)	/* WoV */
			goto again;
		if (errno == ENOENT)	/* Don't complain about zombies */
			rc = G_ZOMB;
		else {
			perror("Pgrab set PR_RLC");
			rc = G_STRANGE;
		}
		goto err;
	}

	/* Stop the process, get its status and its signal/syscall masks. */
	if (Pstop(P, 2) != 0) {
		if (P->state == PS_LOST)	/* WoV */
			goto again;
		if ((errno != EINTR && errno != ERESTART) ||
		    (P->state != PS_STOP && !(P->why.pr_flags & PR_DSTOP))) {
			if (P->state != PS_RUN && errno != ENOENT) {
				perror("Pgrab PCSTOP");
				rc = G_STRANGE;
			} else {
				rc = G_ZOMB;
			}
			goto err;
		}
	}

out:
	/*
	 * Process should either be stopped via /proc or
	 * there should be an outstanding stop directive.
	 */
	if (P->state != PS_STOP &&
	    (P->why.pr_flags & (PR_ISTOP|PR_DSTOP)) == 0) {
		(void) fprintf(stderr, "Pgrab: process is not stopped\n");
		rc = G_STRANGE;
		goto err;
	}

	P->sigmask = P->why.pr_sigtrace;
	P->faultmask = P->why.pr_flttrace;
	P->sysentry = P->why.pr_sysentry;
	P->sysexit = P->why.pr_sysexit;

	return (0);

err:
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	(void) memset(P, 0, sizeof (*P));
	P->ctlfd = -1;
	P->asfd = -1;
	P->statfd = -1;
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;
	return (rc);
}

/*
 * Ensure that the lwp's signal mask and the
 * lwp registers are flushed to the lwp.
 */
static int
Psync(process_t *P)
{
	int ctlfd = (P->lwpctlfd >= 0)? P->lwpctlfd : P->ctlfd;
	int rc = 0;
	long cmd[2];
	iovec_t iov[4];
	int n = 0;

	if (P->sethold) {
		cmd[0] = PCSHOLD;
		iov[n].iov_base = (caddr_t)&cmd[0];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->why.pr_lwp.pr_lwphold;
		iov[n++].iov_len = sizeof (P->why.pr_lwp.pr_lwphold);
	}
	if (P->setregs) {
		cmd[1] = PCSREG;
		iov[n].iov_base = (caddr_t)&cmd[1];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->REG[0];
		iov[n++].iov_len = sizeof (P->REG);
	}
	if (n > 0 && writev(ctlfd, iov, n) < 0)
		rc = -1;

	P->sethold  = FALSE;
	P->setregs  = FALSE;
	return (rc);
}

/* choose an lwp for further operations */
int
Pchoose(process_t *P)
{
	int fd;
	char pathname[100];
	char *dirname;
	char *fname;
	DIR *dirp;
	struct dirent *dentp;

	Punchoose(P);

	(void) sprintf(pathname, "%s/%ld/lwp", procdir, P->pid);
	if ((dirp = opendir(pathname)) == NULL) {
		perror(pathname);
		return (-1);
	}
	dirname = pathname + strlen(pathname);
	*dirname++ = '/';

	/* search for an lwp --- */
	while (dentp = readdir(dirp)) {
		/* skip . and .. */
		if (dentp->d_name[0] == '.')
			continue;

		(void) strcpy(dirname, dentp->d_name);
		fname = dirname + strlen(dirname);
		*fname++ = '/';

		/* open the lwp files */
		(void) strcpy(fname, "lwpstatus");
		if ((fd = open(pathname, O_RDONLY)) < 0 ||
		    (fd = dupfd(fd, 0)) < 0)
			continue;
		P->lwpstatfd = fd;

		(void) strcpy(fname, "lwpctl");
		if ((fd = open(pathname, O_WRONLY)) < 0 ||
		    (fd = dupfd(fd, 0)) < 0) {
			(void) close(P->lwpstatfd);
			P->lwpstatfd = -1;
			continue;
		}
		P->lwpctlfd = fd;

		/* get the lwp status */
		if (Pstatus(P, PCNULL, 0) != 0)
			break;

		/* avoid the aslwp, if possible */
		if ((P->why.pr_lwp.pr_flags & PR_ASLWP) && P->why.pr_nlwp > 1) {
			Punchoose(P);
			continue;
		}

		/* we have the lwp we want */
		(void) closedir(dirp);
		return (0);
	}
	(void) closedir(dirp);

	Punchoose(P);
	return (-1);
}

/* undo Pchoose() */
void
Punchoose(process_t *P)
{
	(void) Psync(P);

	if (P->lwpctlfd >= 0)
		(void) close(P->lwpctlfd);
	if (P->lwpstatfd >= 0)
		(void) close(P->lwpstatfd);
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;

	/* refresh the process status */
	(void) Pstatus(P, PCNULL, 0);
}

/* reopen the /proc file (after PS_LOST) */
int
Preopen(process_t *P)
{
	register int fd;
	char procname[100];
	char *fname;

	if (P->lwpctlfd >= 0)
		(void) close(P->lwpctlfd);
	if (P->lwpstatfd >= 0)
		(void) close(P->lwpstatfd);
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;

	(void) sprintf(procname, "%s/%ld/", procdir, P->pid);
	fname = procname + strlen(procname);

	(void) strcpy(fname, "as");
	if ((fd = open(procname, O_RDWR)) < 0 ||
	    close(P->asfd) < 0 ||
	    (fd = dupfd(fd, P->asfd)) != P->asfd) {
		if (debugflag)
			perror("Preopen open(\"as\")");
		if (fd >= 0)
			(void) close(fd);
		goto err;
	}
	P->asfd = fd;

	(void) strcpy(fname, "status");
	if ((fd = open(procname, O_RDONLY)) < 0 ||
	    close(P->statfd) < 0 ||
	    (fd = dupfd(fd, P->statfd)) != P->statfd) {
		if (debugflag)
			perror("Preopen open(\"status\")");
		if (fd >= 0)
			(void) close(fd);
		goto err;
	}
	P->statfd = fd;

	(void) strcpy(fname, "ctl");
	if ((fd = open(procname, O_WRONLY)) < 0 ||
	    close(P->ctlfd) < 0 ||
	    (fd = dupfd(fd, P->ctlfd)) != P->ctlfd) {
		if (debugflag)
			perror("Preopen open(\"ctl\")");
		if (fd >= 0)
			(void) close(fd);
		goto err;
	}
	P->ctlfd = fd;

	/* process should be stopped on exec (REQUESTED) */
	/* or else should be stopped on exit from exec() (SYSEXIT) */
	P->state = PS_RUN;
	if (Pwait(P, 0) == 0 &&
	    P->state == PS_STOP &&
	    (P->why.pr_lwp.pr_why == PR_REQUESTED ||
	    (P->why.pr_lwp.pr_why == PR_SYSEXIT &&
	    (P->why.pr_lwp.pr_what == SYS_exec ||
	    P->why.pr_lwp.pr_what == SYS_execve)))) {
		/* fake up stop-on-exit-from-execve */
		if (P->why.pr_lwp.pr_why == PR_REQUESTED) {
			P->why.pr_lwp.pr_why = PR_SYSEXIT;
			P->why.pr_lwp.pr_what = SYS_execve;
		}
	} else {
		(void) fprintf(stderr,
		"Preopen: expected REQUESTED or SYSEXIT(SYS_execve) stop\n");
	}

	return (0);
err:
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	(void) memset(P, 0, sizeof (*P));
	P->ctlfd = -1;
	P->asfd = -1;
	P->statfd = -1;
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;
	return (-1);
}

/* release process to run freely */
int
Prelease(process_t *P)
{
	if (debugflag)
		(void) fprintf(stderr, "Prelease: releasing pid # %ld\n",
			P->pid);

	/* attempt to stop it if we have to reset its registers */
	if (P->sethold || P->setregs || P->jsig) {
		register int count;
		for (count = 10;
		    count > 0 && (P->state == PS_RUN || P->state == PS_STEP);
		    count--)
			(void) Pstop(P, 2);
	}

	/*
	 * If we lost control, all we can do is close the files.
	 */
	if (P->state == PS_STOP ||
	    (P->why.pr_lwp.pr_flags & (PR_ISTOP|PR_DSTOP))) {
		/*
		 * We didn't lose control; we do more.
		 */
		long ctl[2];

		if (Psync(P) != 0)
			perror("Prelease() set lwphold / set registers");

		(void) Psetflags(P, PR_RLC);
		(void) Presetflags(P, PR_FORK);

		if (P->jsig) {
			ctl[0] = PCKILL;
			ctl[1] = P->jsig;
			if (write(P->ctlfd, ctl, 2*sizeof (long)) !=
			    2*sizeof (long))
				perror("Prelease post signal");
		}
	}

	/* last close sets the process running */
	if (P->lwpstatfd >= 0)
		(void) close(P->lwpstatfd);
	if (P->lwpctlfd >= 0)
		(void) close(P->lwpctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);

	(void) memset(P, 0, sizeof (*P));
	P->asfd = -1;
	P->statfd = -1;
	P->ctlfd = -1;
	P->lwpstatfd = -1;
	P->lwpctlfd = -1;

	return (0);
}

/* release process but leave it stopped */
int
Phang(process_t *P)
{
	register int count;

	if (debugflag)
		(void) fprintf(stderr, "Phang: releasing pid # %ld\n",
			P->pid);

	/* attempt to stop it */
	for (count = 10;
	    count > 0 && (P->state == PS_RUN || P->state == PS_STEP);
	    count--)
		(void) Pstop(P, 2);

	/*
	 * If we lost control, all we can do is close the file.
	 */
	if (P->state == PS_STOP ||
	    (P->why.pr_lwp.pr_flags & (PR_ISTOP|PR_DSTOP))) {
		/*
		 * We didn't lose control; we do more.
		 */
		sysset_t sysset;
		sigset_t sigset;
		fltset_t fltset;
		long cmd;
		iovec_t iov[2];

		iov[0].iov_base = (caddr_t)&cmd;
		iov[0].iov_len = sizeof (cmd);

		if (Psync(P) != 0)
			perror("Phang() set lwphold / set registers");

		/* no run-on-last-close or kill-on-last-close */
		if (Presetflags(P, PR_RLC|PR_KLC) != 0)
			perror("Phang PCUNSET");

		/* no signal tracing */
		premptyset(&sigset);
		cmd = PCSTRACE;
		iov[1].iov_base = (caddr_t)&sigset;
		iov[1].iov_len = sizeof (sigset);
		if (writev(P->ctlfd, iov, 2) < 0)
			perror("Phang PCSTRACE");

		/* no fault tracing */
		premptyset(&fltset);
		cmd = PCSFAULT;
		iov[1].iov_base = (caddr_t)&fltset;
		iov[1].iov_len = sizeof (fltset);
		if (writev(P->ctlfd, iov, 2) < 0)
			perror("Phang PCSFAULT");

		/* no syscall tracing */
		premptyset(&sysset);
		cmd = PCSENTRY;
		iov[1].iov_base = (caddr_t)&sysset;
		iov[1].iov_len = sizeof (sysset);
		if (writev(P->ctlfd, iov, 2) < 0)
			perror("Phang PCSENTRY");

		premptyset(&sysset);
		cmd = PCSEXIT;
		iov[1].iov_base = (caddr_t)&sysset;
		iov[1].iov_len = sizeof (sysset);
		if (writev(P->ctlfd, iov, 2) < 0)
			perror("Phang PCSEXIT");
	}

	if (P->lwpstatfd >= 0)
		(void) close(P->lwpstatfd);
	if (P->lwpctlfd >= 0)
		(void) close(P->lwpctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);

	(void) memset(P, 0, sizeof (*P));
	P->asfd = -1;
	P->statfd = -1;
	P->ctlfd = -1;
	P->lwpstatfd = -1;
	P->lwpctlfd = -1;

	return (0);
}

/* debugging */
static void
prdump(process_t *P)
{
	long bits;

	if (P->why.pr_lwp.pr_cursig)
		(void) fprintf(stderr, "  p_cursig  = %d",
			P->why.pr_lwp.pr_cursig);
	bits = *((long *)&P->why.pr_sigpend);
	if (bits)
		(void) fprintf(stderr, "  pr_sigpend = 0x%.8lX", bits);
	bits = *((long *)&P->why.pr_lwp.pr_lwppend);
	if (bits)
		(void) fprintf(stderr, "  pr_lwppend = 0x%.8lX", bits);
	(void) fputc('\n', stderr);
}

/* wait for process to stop for any reason */
int
Pwait(process_t *P, unsigned sec)
{
	return (Pstatus(P, PCWSTOP, sec));
}

/* direct process to stop; wait for it to stop */
int
Pstop(process_t *P, unsigned sec)
{
	return (Pstatus(P, PCSTOP, sec));
}

/* wait for specified process to stop or terminate */
int
Pstatus(process_t *P,
	int request,		/* PCNULL, PCSTOP, PCWSTOP */
	unsigned sec)		/* if non-zero, alarm timeout in seconds */
{
	int ctlfd;
	long ctl[3];
	int rc;
	int err;

	switch (P->state) {
	case PS_NULL:
	case PS_LOST:
	case PS_DEAD:
		return (-1);
	case PS_STOP:
		if (request != PCNULL)
			return (0);
	}

	switch (request) {
	default:
		/* programming error */
		(void) fprintf(stderr, "Pstatus: illegal request\n");
		return (-1);
	case PCSTOP:
	case PCWSTOP:
		ctlfd = (P->lwpctlfd >= 0)? P->lwpctlfd : P->ctlfd;
		ctl[0] = PCDSTOP;
		ctl[1] = PCTWSTOP;
		ctl[2] = 1000*sec;	/* timeout */
		if (request == PCSTOP)
			rc = write(ctlfd, &ctl[0], 3*sizeof (long));
		else
			rc = write(ctlfd, &ctl[1], 2*sizeof (long));
		err = (rc < 0)? errno : 0;
		if (err == 0 && interrupt)
			err = EINTR;
		break;
	case PCNULL:
		err = 0;
		break;
	}

	if (P->lwpstatfd < 0) {
		if (pread(P->statfd, &P->why,
		    sizeof (P->why), (off_t)0) < 0)
			err = errno;
	} else {
		if (pread(P->lwpstatfd, &P->why.pr_lwp,
		    sizeof (P->why.pr_lwp), (off_t)0) < 0)
			err = errno;
		P->why.pr_flags = P->why.pr_lwp.pr_flags;
	}

	if (err) {
		switch (err) {
		case EINTR:		/* user typed ctl-C */
		case ERESTART:
			if (debugflag)
				(void) fprintf(stderr, "Pstatus: EINTR\n");
			break;
		case EAGAIN:		/* we lost control of the the process */
			if (debugflag)
				(void) fprintf(stderr, "Pstatus: EAGAIN\n");
			P->state = PS_LOST;
			break;
		default:		/* check for dead process */
			if (debugflag || err != ENOENT) {
				const char *errstr;

				switch (request) {
				case PCNULL:
					errstr = "Pstatus PCNULL"; break;
				case PCSTOP:
					errstr = "Pstatus PCSTOP"; break;
				case PCWSTOP:
					errstr = "Pstatus PCWSTOP"; break;
				default:
					errstr = "Pstatus PC???"; break;
				}
				perror(errstr);
			}
			deadcheck(P);
			break;
		}
		if (err != EINTR && err != ERESTART) {
			errno = err;
			return (-1);
		}
	}

	if (!(P->why.pr_flags&PR_STOPPED)) {
		P->state = PS_RUN;
		if (request == PCNULL || sec != 0)
			return (0);
		if (debugflag)
			(void) fprintf(stderr,
				"Pstatus: process is not stopped\n");
		return (-1);
	}

	P->state = PS_STOP;

	switch (P->why.pr_lwp.pr_why) {
	case PR_REQUESTED:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: REQUESTED");
			prdump(P);
		}
		break;
	case PR_SIGNALLED:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: SIGNALLED %s",
				signame(P->why.pr_lwp.pr_what));
			prdump(P);
		}
		break;
	case PR_FAULTED:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: FAULTED %s",
				fltname(P->why.pr_lwp.pr_what));
			prdump(P);
		}
		break;
	case PR_SYSENTRY:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: SYSENTRY %s",
				sysname(P->why.pr_lwp.pr_what));
			prdump(P);
		}
		break;
	case PR_SYSEXIT:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: SYSEXIT %s",
				sysname(P->why.pr_lwp.pr_what));
			prdump(P);
		}
		break;
	case PR_JOBCONTROL:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: JOBCONTROL %s",
				signame(P->why.pr_lwp.pr_what));
			prdump(P);
		}
		break;
	case PR_SUSPENDED:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: SUSPENDED");
			prdump(P);
		}
		break;
	default:
		if (debugflag) {
			(void) fprintf(stderr, "Pstatus: why: Unknown");
			prdump(P);
		}
		return (-1);
	}

	return (0);
}

/* determine which syscall number we are at */
int
Pgetsysnum(process_t *P)
{
	return (P->why.pr_lwp.pr_syscall);
}

/* we are at a syscall trap, prepare to issue syscall */
int
Psetsysnum(process_t *P, int syscall)
{
#if i386
	P->REG[EAX] = syscall;
	if (Pputareg(P, EAX))
		syscall = -1;
#endif
#if __ppc
	P->REG[R_R0] = syscall;
	if (Pputareg(P, R_R0))
		syscall = -1;
#endif
#if sparc
	P->REG[R_G1] = syscall;
	if (Pputareg(P, R_G1))
		syscall = -1;
#endif
	return (syscall);
}

static void
deadcheck(process_t *P)
{
	int fd;
	char *buf;
	size_t size;

	if (P->statfd < 0)
		P->state = PS_DEAD;
	else {
		if (P->lwpstatfd < 0) {
			fd = P->statfd;
			buf = (char *)&P->why;
			size = sizeof (P->why);
		} else {
			fd = P->lwpstatfd;
			buf = (char *)&P->why.pr_lwp;
			size = sizeof (P->why.pr_lwp);
		}
		while (pread(fd, buf, size, (off_t)0) != size) {
			switch (errno) {
			default:
				/* process or lwp is dead */
				P->state = PS_DEAD;
				break;
			case EINTR:
			case ERESTART:
				continue;
			case EAGAIN:
				P->state = PS_LOST;
				break;
			}
			break;
		}
		P->why.pr_flags = P->why.pr_lwp.pr_flags;
	}
}

/* get values of registers from stopped process */
int
Pgetregs(process_t *P)
{
	if (P->state != PS_STOP)
		return (-1);
	return (0);		/* registers are always available */
}

/* get the value of one register from stopped process */
int
Pgetareg(process_t *P, int reg)
{
	if (reg < 0 || reg >= NGREG) {
		(void) fprintf(stderr,
			"Pgetareg(): invalid register number, %d\n", reg);
		return (-1);
	}
	if (P->state != PS_STOP)
		return (-1);
	return (0);		/* registers are always available */
}

/* put values of registers into stopped process */
int
Pputregs(process_t *P)
{
	if (P->state != PS_STOP)
		return (-1);
	P->setregs = TRUE;	/* set registers before continuing */
	return (0);
}

/* put value of one register into stopped process */
int
Pputareg(process_t *P, int reg)
{
	if (reg < 0 || reg >= NGREG) {
		(void) fprintf(stderr,
			"Pputareg(): invalid register number, %d\n", reg);
		return (-1);
	}
	if (P->state != PS_STOP)
		return (-1);
	P->setregs = TRUE;	/* set registers before continuing */
	return (0);
}

int
Pstart(process_t *P, int sig)
{
	return (Psetrun(P, sig, 0, 0));
}

int
Psetrun(process_t *P,
	int sig,	/* signal to pass to process */
	int flags,	/* PRSTEP|PRSABORT|PRSTOP|PRCSIG|PRCFAULT|PRWAIT */
	int sec)	/* timeout for PRWAIT, seconds */
{
	int ctlfd;
	long ctl[1 +					/* PCCFAULT	*/
		1 + sizeof (siginfo_t)/sizeof (long) +	/* PCSSIG/PCCSIG */
		1 + sizeof (sigset_t)/sizeof (long) +	/* PCSTRACE	*/
		1 + sizeof (sigset_t)/sizeof (long) +	/* PCSHOLD	*/
		1 + sizeof (fltset_t)/sizeof (long) +	/* PCSFAULT	*/
		1 + sizeof (sysset_t)/sizeof (long) +	/* PCSENTRY	*/
		1 + sizeof (sysset_t)/sizeof (long) +	/* PCSEXIT	*/
		1 + sizeof (prgregset_t)/sizeof (long) + /* PCSREG	*/
		2 +					/* PCRUN	*/
		2 ];					/* PCTWSTOP	*/
	register long *ctlp = ctl;
	register unsigned size;
	register int why = P->why.pr_lwp.pr_why;

	if (P->state != PS_STOP)
		return (-1);

	if (flags & PRCFAULT) {		/* clear current fault */
		*ctlp++ = PCCFAULT;
		flags &= ~PRCFAULT;
	}

	if (flags & PRCSIG) {		/* clear current signal */
		*ctlp++ = PCCSIG;
		flags &= ~PRCSIG;
	} else if (sig && sig != P->why.pr_lwp.pr_cursig) {
		/* make current signal */
		register siginfo_t *infop;

		*ctlp++ = PCSSIG;
		infop = (siginfo_t *)ctlp;
		(void) memset(infop, 0, sizeof (*infop));
		infop->si_signo = sig;
		ctlp += sizeof (siginfo_t) / sizeof (long);
	}

	if (P->setsig) {
		*ctlp++ = PCSTRACE;
		*(sigset_t *)ctlp = P->sigmask;
		ctlp += sizeof (sigset_t) / sizeof (long);
	}
	if (P->sethold) {
		*ctlp++ = PCSHOLD;
		*(sigset_t *)ctlp = P->why.pr_lwp.pr_lwphold;
		ctlp += sizeof (sigset_t) / sizeof (long);
	}
	if (P->setfault) {
		*ctlp++ = PCSFAULT;
		*(fltset_t *)ctlp = P->faultmask;
		ctlp += sizeof (fltset_t) / sizeof (long);
	}
	if (P->setentry) {
		*ctlp++ = PCSENTRY;
		*(sysset_t *)ctlp = P->sysentry;
		ctlp += sizeof (sysset_t) / sizeof (long);
	}
	if (P->setexit) {
		*ctlp++ = PCSEXIT;
		*(sysset_t *)ctlp = P->sysexit;
		ctlp += sizeof (sysset_t) / sizeof (long);
	}
	if (P->setregs) {
		*ctlp++ = PCSREG;
		(void) memcpy((char *)ctlp, (char *)&P->REG[0],
		    sizeof (prgregset_t));
		ctlp += sizeof (prgregset_t) / sizeof (long);
	}

	*ctlp++ = PCRUN;
	*ctlp++ = (flags & ~PRWAIT);
	if (flags & PRWAIT) {
		*ctlp++ = PCTWSTOP;
		*ctlp++ = 1000*sec;		/* timeout */
	}
	size = (char *)ctlp - (char *)ctl;

	ctlfd = (P->lwpctlfd >= 0)? P->lwpctlfd : P->ctlfd;
	if (write(ctlfd, ctl, size) != size) {
		if (errno != EAGAIN || Preopen(P) != 0) {
			if ((why != PR_SIGNALLED && why != PR_JOBCONTROL) ||
			    errno != EBUSY)
				goto bad;
			/* ptrace()ed or jobcontrol stop -- back off */
			goto out;
		}
	}

	P->setsig   = FALSE;
	P->sethold  = FALSE;
	P->setfault = FALSE;
	P->setentry = FALSE;
	P->setexit  = FALSE;
	P->setregs  = FALSE;
out:
	P->state = (flags&PRSTEP)? PS_STEP : PS_RUN;
	return ((flags & PRWAIT)? Pstatus(P, PCNULL, 0) : 0);

bad:
	if (errno == ENOENT)
		goto out;
	perror("Psetrun");
	return (-1);
}

int
Pterm(process_t *P)
{
	long ctl[2];

	if (debugflag)
		(void) fprintf(stderr,
			"Pterm: terminating pid # %ld\n", P->pid);
	if (P->state == PS_STOP)
		(void) Pstart(P, SIGKILL);
	ctl[0] = PCKILL;				/* make sure */
	ctl[1] = SIGKILL;
	(void) write(P->ctlfd, ctl, 2*sizeof (long));
	(void) kill((int)P->pid, SIGKILL);		/* make double sure */

	if (P->lwpctlfd >= 0)
		(void) close(P->lwpctlfd);
	if (P->lwpstatfd >= 0)
		(void) close(P->lwpstatfd);
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	(void) memset(P, 0, sizeof (*P));
	P->asfd = -1;
	P->ctlfd = -1;
	P->statfd = -1;
	P->lwpctlfd = -1;
	P->lwpstatfd = -1;

	return (0);
}

int
Pread(process_t *P,
	uintptr_t address,	/* address in process */
	void *buf,		/* caller's buffer */
	int nbyte)		/* number of bytes to read */
{
	if (nbyte <= 0)
		return (0);
	return (pread(P->asfd, buf, (unsigned)nbyte, (off_t)address));
}

int
Pwrite(process_t *P,
	uintptr_t address,	/* address in process */
	const void *buf,	/* caller's buffer */
	int nbyte)		/* number of bytes to write */
{
	if (nbyte <= 0)
		return (0);
	return (pwrite(P->asfd, buf, (unsigned)nbyte, (off_t)address));
}

int
Pgetcred(process_t *P, prcred_t *credp)
{
	register int fd;
	char credname[100];

	/* open the /proc/<pid>/cred file */
	(void) sprintf(credname, "%s/%ld/cred", procdir, P->pid);

	if ((fd = open(credname, O_RDONLY)) < 0 ||
	    read(fd, credp, sizeof (*credp)) < 0) {
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	(void) close(fd);
	return (0);
}

int
Pgetpsinfo(process_t *P, psinfo_t *psp)
{
	register int fd;
	char infoname[100];

	/* open the /proc/<pid>/psinfo file */
	(void) sprintf(infoname, "%s/%ld/psinfo", procdir, P->pid);

	if ((fd = open(infoname, O_RDONLY)) < 0 ||
	    read(fd, psp, sizeof (*psp)) != sizeof (*psp)) {
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	(void) close(fd);
	return (0);
}

int
Psetflags(process_t *P, long flags)
{
	register int rc;
	long ctl[2];

	ctl[0] = PCSET;
	ctl[1] = flags;

	if (write(P->ctlfd, ctl, 2*sizeof (long)) != 2*sizeof (long)) {
		perror("Psetflags");
		rc = -1;
	} else {
		P->why.pr_flags |= flags;
		P->why.pr_lwp.pr_flags |= flags;
		rc = 0;
	}

	return (rc);
}

int
Presetflags(process_t *P, long flags)
{
	register int rc;
	long ctl[2];

	ctl[0] = PCUNSET;
	ctl[1] = flags;

	if (write(P->ctlfd, ctl, 2*sizeof (long)) != 2*sizeof (long)) {
		perror("Presetflags");
		rc = -1;
	} else {
		P->why.pr_flags &= ~flags;
		P->why.pr_lwp.pr_flags &= ~flags;
		rc = 0;
	}

	return (rc);
}

/* action on specified signal */
int
Psignal(process_t *P,
	int which,		/* signal number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	register int oldval;

	if (which <= 0 || which > PRMAXSIG || (which == SIGKILL && stop))
		return (-1);

	oldval = prismember(&P->sigmask, which)? TRUE : FALSE;

	if (stop) {	/* stop process on receipt of signal */
		if (!oldval) {
			praddset(&P->sigmask, which);
			P->setsig = TRUE;
		}
	} else {	/* let process continue on receipt of signal */
		if (oldval) {
			prdelset(&P->sigmask, which);
			P->setsig = TRUE;
		}
	}

	return (oldval);
}

/* action on specified fault */
int
Pfault(process_t *P,
	int which,		/* fault number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	register int oldval;

	if (which <= 0 || which > PRMAXFAULT)
		return (-1);

	oldval = prismember(&P->faultmask, which)? TRUE : FALSE;

	if (stop) {	/* stop process on receipt of fault */
		if (!oldval) {
			praddset(&P->faultmask, which);
			P->setfault = TRUE;
		}
	} else {	/* let process continue on receipt of fault */
		if (oldval) {
			prdelset(&P->faultmask, which);
			P->setfault = TRUE;
		}
	}

	return (oldval);
}

/* action on specified system call entry */
int
Psysentry(process_t *P,
	int which,		/* system call number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	register int oldval;

	if (which <= 0 || which > PRMAXSYS)
		return (-1);

	oldval = prismember(&P->sysentry, which)? TRUE : FALSE;

	if (stop) {	/* stop process on sys call */
		if (!oldval) {
			praddset(&P->sysentry, which);
			P->setentry = TRUE;
		}
	} else {	/* don't stop process on sys call */
		if (oldval) {
			prdelset(&P->sysentry, which);
			P->setentry = TRUE;
		}
	}

	return (oldval);
}

/* action on specified system call exit */
int
Psysexit(process_t *P,
	int which,		/* system call number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	register int oldval;

	if (which <= 0 || which > PRMAXSYS)
		return (-1);

	oldval = prismember(&P->sysexit, which)? TRUE : FALSE;

	if (stop) {	/* stop process on sys call exit */
		if (!oldval) {
			praddset(&P->sysexit, which);
			P->setexit = TRUE;
		}
	} else {	/* don't stop process on sys call exit */
		if (oldval) {
			prdelset(&P->sysexit, which);
			P->setexit = TRUE;
		}
	}

	return (oldval);
}

/* execute the syscall instruction */
static int
execute(process_t *P, int sysindex)
{
	int ctlfd = (P->lwpctlfd >= 0)? P->lwpctlfd : P->ctlfd;
	int cursig;
	struct {
		int cmd;
		siginfo_t siginfo;
	} ctl;
	sigset_t hold;		/* mask of held signals */
	int sentry;		/* old value of stop-on-syscall-entry */

	sentry = Psysentry(P, sysindex, TRUE);	/* set stop-on-syscall-entry */
	hold = P->why.pr_lwp.pr_lwphold;	/* remember signal hold mask */
	if ((cursig = P->why.pr_lwp.pr_cursig) != 0) {	/* remember cursig */
		ctl.cmd = PCSSIG;
		ctl.siginfo = P->why.pr_lwp.pr_info;
	}
	prfillset(&P->why.pr_lwp.pr_lwphold);	/* hold all signals */
	P->sethold = TRUE;

	if (Psetrun(P, 0, PRCSIG, 0) == -1)
		goto bad;
	while (P->state == PS_RUN)
		(void) Pwait(P, 0);
	if (P->state != PS_STOP)
		goto bad;
	P->why.pr_lwp.pr_lwphold = hold;	/* restore hold mask */
	P->sethold = TRUE;
	(void) Psysentry(P, sysindex, sentry);	/* restore sysentry stop */
	if (cursig)				/* restore cursig */
		(void) write(ctlfd, &ctl, sizeof (ctl));
	if (P->why.pr_lwp.pr_why  == PR_SYSENTRY &&
	    P->why.pr_lwp.pr_what == sysindex)
		return (0);
bad:
	return (-1);
}


/* worst-case alignment for objects on the stack */
#if i386	/* stack grows down, non-aligned */
#define	ALIGN(sp)	(sp)
#define	ARGOFF	1
#endif
#if __ppc	/* stack grows down, 4-word-aligned */
#define	ALIGN(sp)	((sp) & ~(4*sizeof (int) - 1))
#define	ARGOFF	0
#endif
#if sparc	/* stack grows down, doubleword-aligned */
#define	ALIGN(sp)	((sp) & ~(2*sizeof (int) - 1))
#define	ARGOFF	0
#endif

/* perform system call in controlled process */
struct sysret
Psyscall(process_t *P,
	int sysindex,		/* system call index */
	register int nargs,	/* number of arguments to system call */
	struct argdes *argp)	/* argument descriptor array */
{
	pstatus_t save_pstatus;
	register struct argdes *adp;	/* pointer to argument descriptor */
	struct sysret rval;		/* return value */
	register int i;			/* general index value */
	register int Perr = 0;		/* local error number */
	int sexit;			/* old value of stop-on-syscall-exit */
	greg_t sp;			/* adjusted stack pointer */
	greg_t ap;			/* adjusted argument pointer */
	gregset_t savedreg;		/* remembered registers */
	int arglist[MAXARGS+2];		/* syscall arglist */
	int why;			/* reason for stopping */
	int what;			/* detailed reason (syscall,signal) */
	sigset_t block, unblock;
	int waschosen = FALSE;

	/* save P->why to restore on exit */
	save_pstatus = P->why;

	/* block (hold) all signals for the duration. */
	(void) sigfillset(&block);
	(void) sigemptyset(&unblock);
	(void) sigprocmask(SIG_BLOCK, &block, &unblock);

	rval.errno = 0;		/* initialize return value */
	rval.r0 = 0;
	rval.r1 = 0;

	/* if necessary, choose an lwp from the process to do all the work */
	if (P->lwpctlfd < 0) {
		if (Pchoose(P) != 0)
			goto bad8;
		waschosen = TRUE;
	}

	why = P->why.pr_lwp.pr_why;
	what = P->why.pr_lwp.pr_what;

	if (sysindex <= 0 || sysindex > PRMAXSYS ||	/* programming error */
	    nargs < 0 || nargs > MAXARGS)
		goto bad1;

	if (P->state != PS_STOP ||		/* check state of process */
	    (P->why.pr_flags & PR_ASLEEP) ||
	    Pgetregs(P) != 0)
		goto bad2;

	for (i = 0; i < NGREG; i++)		/* remember registers */
		savedreg[i] = P->REG[i];

	if (checksyscall(P))			/* bad text ? */
		goto bad3;


	/* validate arguments and compute the stack frame parameters --- */

	sp = savedreg[R_SP];	/* begin with the current stack pointer */
	sp = ALIGN(sp);
	/* for each argument */
	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		rval.r0 = i;		/* in case of error */
		switch (adp->type) {
		default:			/* programming error */
			goto bad4;
		case AT_BYVAL:			/* simple argument */
			break;
		case AT_BYREF:			/* must allocate space */
			switch (adp->inout) {
			case AI_INPUT:
			case AI_OUTPUT:
			case AI_INOUT:
				if (adp->object == NULL)
					goto bad5;	/* programming error */
				break;
			default:		/* programming error */
				goto bad6;
			}
			/* allocate stack space for BYREF argument */
			if (adp->len <= 0 || adp->len > MAXARGL)
				goto bad7;	/* programming error */
#if sparc || i386 || __ppc	/* downward stack growth */
			sp = ALIGN(sp - adp->len);
			adp->value = sp;	/* stack address for object */
#else				/* upward stack growth */
			adp->value = sp;	/* stack address for object */
			sp = ALIGN(sp + adp->len);
#endif
			break;
		}
	}
	rval.r0 = 0;			/* in case of error */
#if i386
	sp -= sizeof (int)*(nargs+2);	/* space for arg list + CALL parms */
	ap = sp;			/* address of arg list */
#endif
#if sparc
	sp -= (nargs > 6)? sizeof (int)*(16+1+nargs) : sizeof (int)*(16+1+6);
	sp = ALIGN(sp);
	ap = sp+(16+1)*sizeof (int);	/* address of arg dump area */
#endif
#if __ppc
	sp -= (nargs > 8)? sizeof (int)*(2 + (nargs - 8)) : sizeof (int)*(2);
	sp = ALIGN(sp);
	ap = sp+(2)*sizeof (int);    /* address of additional (> 8) arg list */
#endif

	/* point of no return */

	/* special treatment of stopped-on-syscall-entry */
	/* move the process to the stopped-on-syscall-exit state */
	if (why == PR_SYSENTRY) {
		/* arrange to reissue sys call */
#if i386 || __ppc
		savedreg[R_PC] -= sizeof (syscall_t);
#endif
		sexit = Psysexit(P, what, TRUE);  /* catch this syscall exit */

		if (Psetrun(P, 0, PRSABORT, 0) != 0 ||	/* abort sys call */
		    Pwait(P, 0) != 0 ||
		    P->state != PS_STOP ||
		    P->why.pr_lwp.pr_why != PR_SYSEXIT ||
		    P->why.pr_lwp.pr_what != what ||
		    Pgetareg(P, R_CCREG) != 0 ||
		    Pgetareg(P, R_RVAL0) != 0 ||
		    (P->REG[R_CCREG] & ERRBIT) == 0 ||
		    (P->REG[R_RVAL0] != EINTR && P->REG[R_RVAL0] != ERESTART)) {
			(void) fprintf(stderr,
				"Psyscall(): cannot abort sys call\n");
			(void) Psysexit(P, what, sexit);
			goto bad9;
		}

		(void) Psysexit(P, what, sexit); /* restore exit trap */
	}


	/* perform the system call entry, adjusting %sp */
	/* this moves the process to the stopped-on-syscall-entry state */
	/* just before the arguments to the sys call are fetched */

	(void) Psetsysnum(P, sysindex);
	P->REG[R_SP] = sp;
	P->REG[R_PC] = P->sysaddr;	/* address of syscall */
#if sparc
	P->REG[R_nPC] = P->sysaddr+sizeof (syscall_t);
#endif
	(void) Pputregs(P);

	/* execute the syscall instruction */
#if sparc
	if (execute(P, sysindex) != 0 ||
	    P->REG[R_PC] != P->sysaddr ||
	    P->REG[R_nPC] != P->sysaddr+sizeof (syscall_t))
#else
	if (execute(P, sysindex) != 0 ||
	    P->REG[R_PC] != P->sysaddr+sizeof (syscall_t))
#endif
		goto bad10;


	/* stopped at syscall entry; copy arguments to stack frame */
	/* for each argument */
	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		rval.r0 = i;		/* in case of error */
		if (adp->type != AT_BYVAL &&
		    adp->inout != AI_OUTPUT) {
			/* copy input byref parameter to process */
			if (Pwrite(P, (long)adp->value, adp->object, adp->len)
			    != adp->len)
				goto bad17;
		}
		arglist[ARGOFF+i] = adp->value;
#if __ppc
		if (i < 8) {
			P->REG[R_R3+i] = adp->value;
			(void) Pputareg(P, R_R3+i);
		}
#endif
#if sparc
		if (i < 6) {
			P->REG[R_O0+i] = adp->value;
			(void) Pputareg(P, R_O0+i);
		}
#endif
	}
	rval.r0 = 0;			/* in case of error */
#if i386
	arglist[0] = savedreg[R_PC];		/* CALL parameters */
	if (Pwrite(P, (long)ap, (char *)&arglist[0],
	    (int)sizeof (int)*(nargs+1)) != sizeof (int)*(nargs+1))
		goto bad18;
#endif
#if sparc
	if (nargs > 6 &&
	    Pwrite(P, (long)ap, (char *)&arglist[0],
	    (int)sizeof (int)*nargs) != sizeof (int)*nargs)
		goto bad18;
#endif
#if __ppc
	if (nargs > 8 &&
	    Pwrite(P, (long)ap, (char *)&arglist[8],
	    (int)sizeof (int)*(nargs-8)) != sizeof (int)*(nargs-8))
		goto bad18;
#endif

	/* complete the system call */
	/* this moves the process to the stopped-on-syscall-exit state */

	sexit = Psysexit(P, sysindex, TRUE);	/* catch this syscall exit */
	do {		/* allow process to receive signals in sys call */
		if (Psetrun(P, 0, 0, 0) == -1)
			goto bad21;
		while (P->state == PS_RUN)
			(void) Pwait(P, 0);
	} while (P->state == PS_STOP && P->why.pr_lwp.pr_why == PR_SIGNALLED);
	(void) Psysexit(P, sysindex, sexit);	/* restore original setting */

	if (P->state != PS_STOP ||
	    P->why.pr_lwp.pr_why  != PR_SYSEXIT)
		goto bad22;
	if (P->why.pr_lwp.pr_what != sysindex)
		goto bad23;
#if sparc
	if (P->REG[R_PC] != P->sysaddr+sizeof (syscall_t) ||
	    P->REG[R_nPC] != P->sysaddr+2*sizeof (syscall_t))
#else
	if (P->REG[R_PC] != P->sysaddr+sizeof (syscall_t))
#endif
		goto bad24;


	/* fetch output arguments back from process */
#if i386
	if (Pread(P, (long)ap, (char *)&arglist[0], (int)sizeof (int)*(nargs+1))
	    != sizeof (int)*(nargs+1))
		goto bad25;
#endif
	/* for each argument */
	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		rval.r0 = i;		/* in case of error */
		if (adp->type != AT_BYVAL &&
		    adp->inout != AI_INPUT) {
			/* copy output byref parameter from process */
			if (Pread(P, (long)adp->value, adp->object, adp->len)
			    != adp->len)
				goto bad26;
		}
		adp->value = arglist[ARGOFF+i];
	}


	/* get the return values from the syscall */

	if (P->REG[R_CCREG] & ERRBIT) {	/* error */
		rval.errno = P->REG[R_RVAL0];
		rval.r0 = -1;
	} else {			/* normal return */
		rval.r0 = P->REG[R_RVAL0];
		rval.r1 = P->REG[R_RVAL1];
	}

	goto good;

bad26:	Perr++;
bad25:	Perr++;
bad24:	Perr++;
bad23:	Perr++;
bad22:	Perr++;
bad21:	Perr++;
	Perr++;
	Perr++;
bad18:	Perr++;
bad17:	Perr++;
	Perr++;
	Perr++;
	Perr++;
	Perr++;
	Perr++;
	Perr++;
bad10:	Perr++;
bad9:	Perr++;
	Perr += 8;
	rval.errno = -Perr;	/* local errors are negative */

good:
	/* restore process to its previous state (almost) */

	for (i = 0; i < NGREG; i++)	/* restore remembered registers */
		P->REG[i] = savedreg[i];
	(void) Pputregs(P);

	if (why == PR_SYSENTRY &&	/* special treatment */
	    execute(P, what) != 0) {	/* get back to the syscall */
		(void) fprintf(stderr,
			"Psyscall(): cannot reissue sys call\n");
		if (Perr == 0)
			rval.errno = -27;
	}

	P->why.pr_lwp.pr_why = why;
	P->why.pr_lwp.pr_what = what;

	goto out;

bad8:	Perr++;
bad7:	Perr++;
bad6:	Perr++;
bad5:	Perr++;
bad4:	Perr++;
bad3:	Perr++;
bad2:	Perr++;
bad1:	Perr++;
	rval.errno = -Perr;	/* local errors are negative */

out:
	/* if we chose an lwp for the operation, unchoose it now */
	if (waschosen)
		Punchoose(P);

	/* unblock (release) all signals before returning */
	(void) sigprocmask(SIG_SETMASK, &unblock, (sigset_t *)NULL);

	/* restore P->why */
	P->why = save_pstatus;

	return (rval);
}

/* check syscall instruction in process */
static int
checksyscall(process_t *P)
{
	/* this should always succeed--we always have a good syscall address */
	syscall_t instr;		/* holds one syscall instruction */

	return (
#if i386
	    (Pread(P, P->sysaddr, (char *)&instr, sizeof (instr))
	    == sizeof (instr) && instr[0] == SYSCALL)?
#else
	    (Pread(P, P->sysaddr, (char *)&instr, sizeof (instr))
	    == sizeof (instr) && instr == (syscall_t)SYSCALL)?
#endif
	    0 : -1);
}
