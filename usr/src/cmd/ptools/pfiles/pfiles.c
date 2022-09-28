/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pfiles.c	1.7	96/07/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/mkdev.h>

#include "../libproc/pcontrol.h"
#include "../libproc/ramdata.h"

static	int	grabit(process_t *, pid_t);

static	void	intr(int);
static	pid_t	getproc(char *, char **);
static	int	issignalled(process_t *);
static	void	dofcntl(process_t *, int, int);
static	void	show_files(process_t *);
static	void	show_fileflags(int);

main(int argc, char **argv)
{
	int retc = 0;
	int sys;
	int sig;
	int flt;
	int opt;
	int errflg = FALSE;
	register process_t *Pr = &Proc;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* allow all accesses for setuid version */
	(void) setuid((int)geteuid());

	/* options */
	while ((opt = getopt(argc, argv, "P:Fq")) != EOF) {
		switch (opt) {
		case 'P':		/* alternate /proc directory */
			procdir = optarg;
			break;
		case 'F':		/* force grabbing (no O_EXCL) */
			Fflag = TRUE;
			break;
		case 'q':		/* let QUIT give a core dump */
			qflag = TRUE;
			break;
		default:
			errflg = TRUE;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s pid ...\n",
			command);
		exit(2);
	}

	if (!isprocdir(procdir)) {
		(void) fprintf(stderr, "%s: %s is not a PROC directory\n",
			command, procdir);
		exit(2);
	}

	/* catch signals from terminal */
	if (sigset(SIGHUP, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGHUP, intr);
	if (sigset(SIGINT, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGINT, intr);
	if (sigset(SIGQUIT, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGQUIT, intr);
	(void) sigset(SIGALRM, intr);	/* always catch these */
	(void) sigset(SIGTERM, intr);
	if (qflag)		/* ensure death on SIGQUIT */
		(void) sigset(SIGQUIT, SIG_DFL);

	while (--argc >= 0 && !interrupt) {
		int reissue = 0;
		prgregset_t savedreg;
		int reg;
		sigset_t holdmask;
		pid_t pid;
		char *pdir;
		int gret;

		(void) fflush(stdout);	/* line-at-a-time */

		/* get the specified pid and its /proc directory */
		pid = getproc(*argv++, &pdir);

		gret = 0;
		if (pid < 0 || (gret = grabit(Pr, pid)) != 0) {
			switch (gret) {
			case 1:		/* system process */
				(void) printf("%ld:\t%.70s\n", pid,
					Pr->psinfo.pr_psargs);
				(void) printf("  [system process]\n");
				break;
			case 2:		/* attempt to grab self */
				(void) printf("%ld:\t%.70s\n", pid,
					Pr->psinfo.pr_psargs);
				show_files((process_t *)NULL);
				break;
			default:
				retc++;
				break;
			}
			continue;
		}

		if (scantext(Pr) != 0) {
			(void) fprintf(stderr, "%s: cannot find text in %ld\n",
				command, pid);
			(void) Prelease(Pr);
			retc++;
			continue;
		}

		for (sys = 1; sys <= PRMAXSYS; sys++) {	/* trace syscall exit */
			(void) Psysentry(Pr, sys, FALSE);
			(void) Psysexit(Pr, sys, TRUE);
		}
		for (sig = 1; sig <= PRMAXSIG; sig++)	/* trace no signals */
			(void) Psignal(Pr, sig, FALSE);
		for (flt = 1; flt <= PRMAXFAULT; flt++)	/* trace no faults */
			(void) Pfault(Pr, flt, FALSE);

		/* avoid waiting forever */
		(void) alarm(3);
		while ((Pr->why.pr_lwp.pr_why != PR_REQUESTED &&
		    Pr->why.pr_lwp.pr_why != PR_SYSEXIT) ||
		    issignalled(Pr)) {
			if (interrupt ||
			    Pstart(Pr, 0) != 0 ||
			    (msleep(20), Pstop(Pr, 0)) != 0 ||
			    Pr->state != PS_STOP) {
				(void) alarm(0);
				timeout = FALSE;
				(void) fprintf(stderr,
					"%s: cannot control process %ld\n",
					command, pid);
				retc++;
				goto out;
			}
		}
		(void) alarm(0);
		timeout = FALSE;

		/* choose one lwp for further operations */
		if (Pchoose(Pr) != 0) {
			(void) fprintf(stderr,
				"%s: cannot find an available lwp in %ld\n",
				command, pid);
			retc++;
			goto out;
		}

		reissue = 0;
		if (Pr->why.pr_lwp.pr_why == PR_REQUESTED &&
		    (Pr->why.pr_lwp.pr_flags & PR_ASLEEP)) {
			/* sleeping syscall */
			if ((reissue = Pgetsysnum(Pr)) <= 0) {
				(void) fprintf(stderr,
				    "%s: Cannot determine syscall number "
				    "in process %ld\n",
				    command, pid);
				retc++;
				goto out;
			}

			/* remember the registers before aborting the syscall */
			(void) Pgetregs(Pr);
			for (reg = 0; reg < NGREG; reg++)
				savedreg[reg] = Pr->REG[reg];

			/* move the process to SYSEXIT */
			(void) Psetrun(Pr, 0, PRSABORT, 0);
			(void) Pwait(Pr, 0);

			if (Pr->state != PS_STOP ||
			    Pr->why.pr_lwp.pr_why != PR_SYSEXIT) {
				(void) fprintf(stderr,
				"%s: expected process %ld to be stopped "
				"on PR_SYSEXIT, is %s(%d,%d)\n",
				    command, pid,
				    (Pr->why.pr_lwp.pr_flags & PR_ASLEEP)?
				    "ASLEEP" : "",
				    Pr->why.pr_lwp.pr_why,
				    Pr->why.pr_lwp.pr_what);
				retc++;
				goto out;
			}
		}

		holdmask = Pr->why.pr_lwp.pr_lwphold;
		prfillset(&Pr->why.pr_lwp.pr_lwphold);	/* hold all signals */
		Pr->sethold = TRUE;

		(void) Pgetregs(Pr);

		/* can't happen? */
		if ((sig = Pr->why.pr_lwp.pr_cursig) != 0) {
			long ctl[3];

			(void) fprintf(stderr,
				"%s: unexpected cursig: %d in process %ld\n",
				command, sig, pid);
			ctl[0] = PCCSIG;
			ctl[1] = PCKILL;
			ctl[2] = sig;
			(void) write(Pr->lwpctlfd, (char *)ctl,
				3*sizeof (long));
			Pr->why.pr_lwp.pr_cursig = 0;
		}

		if (!reissue) {
			/* remember the registers */
			for (reg = 0; reg < NGREG; reg++)
				savedreg[reg] = Pr->REG[reg];
		}

		for (sys = 1; sys <= PRMAXSYS; sys++)	/* no tracing */
			(void) Psysexit(Pr, sys, FALSE);

/* ------------- Insert code to be executed here ------------- */

		(void) printf("%ld:\t%.70s\n", pid, Pr->psinfo.pr_psargs);
		show_files(Pr);

/* ------------- End of code to be executed here ------------- */

		/* restore the registers */
		for (reg = 0; reg < NGREG; reg++)
			Pr->REG[reg] = savedreg[reg];
		(void) Pputregs(Pr);

		/* get back to the sleeping syscall */
		if (reissue) {
#if i386 || __ppc
			Pr->REG[R_PC] -= sizeof (syscall_t);
			(void) Pputareg(Pr, R_PC);
#endif
			(void) Psysentry(Pr, reissue, TRUE);
			if (Pstart(Pr, 0) != 0 ||
			    Pwait(Pr, 0) != 0 ||
			    Pr->why.pr_lwp.pr_why != PR_SYSENTRY ||
			    Pr->why.pr_lwp.pr_what != reissue)
				(void) fprintf(stderr,
					"%s: cannot reissue sys call\n",
					command);
			(void) Psysentry(Pr, reissue, FALSE);

			/* restore the registers again */
			for (reg = 0; reg < NGREG; reg++)
				Pr->REG[reg] = savedreg[reg];
			(void) Pputregs(Pr);
		}

		/* unblock pending signals */
		Pr->why.pr_lwp.pr_lwphold = holdmask;
		Pr->sethold = TRUE;

out:
		(void) Prelease(Pr);
	}

	if (interrupt)
		retc++;
	return (retc);
}

static int
issignalled(process_t *Pr)
{
	return (Pr->why.pr_lwp.pr_cursig);
}

/* get process id and /proc directory */
/* return pid on success, -1 on failure */
static pid_t
getproc(char *path,		/* number or /proc/nnn */
	char **pdirp)		/* points to /proc directory on success */
{
	register char *name;
	register pid_t pid;
	char *next;

	if ((name = strrchr(path, '/')) != NULL)	/* last component */
		*name++ = '\0';
	else {
		name = path;
		path = procdir;
	}

	if (strcmp(name, "-") == 0) {
		pid = getpid();
		next = name+1;
	} else {
		pid = strtol(name, &next, 10);
	}
	if ((isdigit(*name) || *name == '-') && pid >= 0 && *next == '\0') {
		if (strcmp(procdir, path) != 0 &&
		    !isprocdir(path)) {
			(void) fprintf(stderr,
				"%s: %s is not a PROC directory\n",
				command, path);
			pid = -1;
		}
	} else {
		(void) fprintf(stderr, "%s: invalid process id: %s\n",
			command, name);
		pid = -1;
	}

	if (pid >= 0)
		*pdirp = path;
	return (pid);
}

/* take control of an existing process */
static int
grabit(process_t *Pr, pid_t pid)
{
	int gcode;

	/* avoid waiting forever */
	(void) alarm(2);

	/* don't force the takeover unless the -F option was specified */
	gcode = Pgrab(Pr, pid, Fflag);

	(void) alarm(0);
	timeout = FALSE;

	/* don't force the takeover unless the -F option was specified */
	switch (gcode) {
	case 0:
		break;
	case G_BUSY:
		(void) fprintf(stderr,
			"%s: someone else is tracing process %ld\n",
			command, pid);
		return (-1);
	case G_SYS:		/* system process */
		return (1);
	case G_SELF:		/* attempt to grab self */
		return (2);
	default:
		goto bad;
	}

	if (Pr->state == PS_STOP &&
	    Pr->why.pr_lwp.pr_why == PR_JOBCONTROL) {
		long ctl[2];

		Pr->jsig = Pr->why.pr_lwp.pr_what;
		ctl[0] = PCKILL;
		ctl[1] = SIGCONT;
		if (write(Pr->ctlfd, (char *)ctl, 2*sizeof (long)) < 0) {
			perror("grabit(): PCKILL");
			(void) Prelease(Pr);
			goto bad;
		}
		Pr->state = PS_RUN;
		(void) Pwait(Pr, 0);
	}

	if (Pr->state != PS_STOP ||
	    Pr->why.pr_lwp.pr_why != PR_REQUESTED) {
		(void) fprintf(stderr,
			"%s: expected REQUESTED stop, pid# %ld (%d,%d)\n",
			command, pid,
			Pr->why.pr_lwp.pr_why,
			Pr->why.pr_lwp.pr_what);
		if (Pr->state != PS_STOP) {
			(void) Prelease(Pr);
			goto bad;
		}
	}

	return (0);

bad:
	if (gcode == G_NOPROC || gcode == G_ZOMB || errno == ENOENT)
		(void) fprintf(stderr, "%s: no such process: %ld\n",
			command, pid);
	else
		(void) fprintf(stderr, "%s: cannot control process: %ld\n",
			command, pid);
	return (-1);
}

static void
intr(int sig)
{
	if (sig == SIGALRM) {		/* reset alarm clock */
		timeout = TRUE;
		(void) alarm(1);
	} else {
		interrupt = TRUE;
	}
}

/* ------ begin specific code ------ */

static void
show_files(process_t *Pr)
{
	struct stat statb;
	struct rlimit rlim;
	register int fd;
	register int nfd = 64;
	char *s;

	if (prgetrlimit(Pr, RLIMIT_NOFILE, &rlim) == 0)
		nfd = rlim.rlim_cur;
	(void) printf("  Current rlimit: %d file descriptors\n", nfd);

	for (fd = 0; fd < nfd; fd++) {
		char unknown[12];
		dev_t rdev;

		if (prfstat(Pr, fd, &statb) == -1)
			continue;
		rdev = (dev_t)(-1);
		switch (statb.st_mode & S_IFMT) {
		case S_IFDIR: s = "S_IFDIR"; break;
		case S_IFCHR: s = "S_IFCHR"; rdev = statb.st_rdev; break;
		case S_IFBLK: s = "S_IFBLK"; rdev = statb.st_rdev; break;
		case S_IFREG: s = "S_IFREG"; break;
		case S_IFSOCK: s = "S_IFSOCK"; break;
		case S_IFIFO: s = "S_IFIFO"; break;
		default:
			s = unknown;
			(void) sprintf(s, "0x%.4lx ", statb.st_mode & S_IFMT);
			break;
		}
		(void) printf("%4d: %s mode:0%.3lo",
			fd,
			s,
			statb.st_mode & ~S_IFMT);
		if (major(statb.st_dev) != -1 && minor(statb.st_dev) != -1)
			(void) printf(" dev:%lu,%lu",
				major(statb.st_dev),
				minor(statb.st_dev));
		else
			(void) printf(" dev:0x%.8lX", statb.st_dev);
		(void) printf(" ino:%lu uid:%ld gid:%ld",
			statb.st_ino,
			statb.st_uid,
			statb.st_gid);
		if (rdev == (dev_t)(-1))
			(void) printf(" size:%ld\n", statb.st_size);
		else if (major(rdev) != -1 && minor(rdev) != -1)
			(void) printf(" rdev:%lu,%lu\n",
				major(rdev), minor(rdev));
		else
			(void) printf(" rdev:0x%.8lX\n", rdev);

		dofcntl(Pr, fd,
			(statb.st_mode & (S_IFMT|S_ENFMT|S_IXGRP))
			== (S_IFREG|S_ENFMT));
	}
}

/* examine open file with fcntl() */
static void
dofcntl(process_t *Pr, int fd, int manditory)
{
	struct flock flock;
	register int fileflags;
	register int fdflags;

	fileflags = prfcntl(Pr, fd, F_GETFL, 0);
	fdflags = prfcntl(Pr, fd, F_GETFD, 0);

	if (fileflags != -1 || fdflags != -1) {
		(void) printf("      ");
		if (fileflags != -1)
			show_fileflags(fileflags);
		if (fdflags != -1 && (fdflags & FD_CLOEXEC))
			(void) printf(" close-on-exec");
		(void) fputc('\n', stdout);
	}

	flock.l_type = F_WRLCK;
	flock.l_whence = 0;
	flock.l_start = 0;
	flock.l_len = 0;
	flock.l_sysid = 0;
	flock.l_pid = 0;
	if (prfcntl(Pr, fd, F_GETLK, &flock) != -1) {
		if (flock.l_type != F_UNLCK && (flock.l_sysid || flock.l_pid)) {
			unsigned long sysid = flock.l_sysid;

			(void) printf("      %s %s lock set by",
				manditory? "manditory" : "advisory",
				flock.l_type == F_RDLCK? "read" : "write");
			if (sysid & 0xff000000)
				(void) printf(" system %lu.%lu.%lu.%lu",
					(sysid>>24)&0xff, (sysid>>16)&0xff,
					(sysid>>8)&0xff, (sysid)&0xff);
			else if (sysid)
				(void) printf(" system 0x%lX", sysid);
			if (flock.l_pid)
				(void) printf(" process %ld", flock.l_pid);
			(void) fputc('\n', stdout);
		}
	}
}

#ifdef O_PRIV
#define	ALL_O_FLAGS	O_ACCMODE | O_NDELAY | O_NONBLOCK | O_APPEND | \
			O_PRIV | O_SYNC | O_DSYNC | O_RSYNC | \
			O_CREAT | O_TRUNC | O_EXCL | O_NOCTTY
#else
#define	ALL_O_FLAGS	O_ACCMODE | O_NDELAY | O_NONBLOCK | O_APPEND | \
			O_SYNC | O_DSYNC | O_RSYNC | \
			O_CREAT | O_TRUNC | O_EXCL | O_NOCTTY
#endif
static void
show_fileflags(int flags)
{
	char buffer[128];
	register char *str = buffer;

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		(void) strcpy(str, "O_RDONLY");
		break;
	case O_WRONLY:
		(void) strcpy(str, "O_WRONLY");
		break;
	case O_RDWR:
		(void) strcpy(str, "O_RDWR");
		break;
	default:
		(void) sprintf(str, "0x%x", flags & O_ACCMODE);
		break;
	}

	if (flags & O_NDELAY)
		(void) strcat(str, "|O_NDELAY");
	if (flags & O_NONBLOCK)
		(void) strcat(str, "|O_NONBLOCK");
	if (flags & O_APPEND)
		(void) strcat(str, "|O_APPEND");
#ifdef O_PRIV
	if (flags & O_PRIV)
		(void) strcat(str, "|O_PRIV");
#endif
	if (flags & O_SYNC)
		(void) strcat(str, "|O_SYNC");
	if (flags & O_DSYNC)
		(void) strcat(str, "|O_DSYNC");
	if (flags & O_RSYNC)
		(void) strcat(str, "|O_RSYNC");
	if (flags & O_CREAT)
		(void) strcat(str, "|O_CREAT");
	if (flags & O_TRUNC)
		(void) strcat(str, "|O_TRUNC");
	if (flags & O_EXCL)
		(void) strcat(str, "|O_EXCL");
	if (flags & O_NOCTTY)
		(void) strcat(str, "|O_NOCTTY");
	if (flags & ~(ALL_O_FLAGS))
		(void) sprintf(str + strlen(str), "|0x%x",
			flags & ~(ALL_O_FLAGS));

	(void) printf("%s", str);
}
