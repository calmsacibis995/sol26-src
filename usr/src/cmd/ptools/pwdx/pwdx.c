/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pwdx.c	1.6	96/06/18 SMI"

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
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stack.h>
#include <elf.h>

#include "../libproc/pcontrol.h"
#include "../libproc/ramdata.h"

#define	LIBCWD	"/usr/proc/lib/libcwd.so.1"

static void intr(int);
static void errmsg(const char *, const char *);
static void abend(const char *, const char *);

static	int	cwd_self(void);
static	int	grabit(process_t *, pid_t);
static	char	*fetchstring(long);
static	pid_t	getproc(char *, char **);
static	int	issignalled(process_t *);

main(int argc, char **argv)
{
	int retc = 0;
	int sys;
	int sig;
	int flt;
	int opt;
	int fd;
	caddr_t ptr1;
	caddr_t ptr2;
	char *(*cwd)();
	long ill = -12;
	int errflg = FALSE;
	Elf32_Ehdr Ehdr;
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
		(void) fprintf(stderr, "usage:\t%s pid ...\n", command);
		(void) fprintf(stderr, "  (show process working directory)\n");
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
#if i386
		int sp;
#endif

		PR = NULL;	/* for abend() */

		(void) fflush(stdout);	/* line-at-a-time */

		/* get the specified pid and its /proc directory */
		pid = getproc(*argv++, &pdir);

		gret = 0;
		if (pid < 0 || (gret = grabit(Pr, pid)) != 0) {
			switch (gret) {
			case 1:		/* system process */
				(void) printf("%ld:\t/\t[system process]\n",
					pid);
				break;
			case 2:		/* attempt to grab self */
				if (cwd_self() != 0)
					retc++;
				break;
			default:
				retc++;
				break;
			}
			continue;
		}

		PR = Pr;	/* for abend() */

		if (scantext(Pr) != 0) {
			(void) fprintf(stderr, "%s: cannot find text in %ld\n",
				command, pid);
			retc++;
			goto out;
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
				    "%s: Cannot determine syscall number\n",
				    command);
				retc++;
				goto out;
			}

			/* remember the registers before aborting the syscall */
			(void) Pgetregs(Pr);
			for (reg = 0; reg < NGREG; reg++)
				savedreg[reg] = Pr->REG[reg];

			/* move the lwp to SYSEXIT */
			(void) Psetrun(Pr, 0, PRSABORT, 0);
			(void) Pwait(Pr, 0);

			if (Pr->state != PS_STOP ||
			    Pr->why.pr_lwp.pr_why != PR_SYSEXIT) {
				(void) fprintf(stderr,
				"%s: expected to be stopped on PR_SYSEXIT\n",
				    command);
				retc++;
				goto out;
			}
		}

		/* remember old hold mask */
		holdmask = Pr->why.pr_lwp.pr_lwphold;
		/* hold all signals */
		prfillset(&Pr->why.pr_lwp.pr_lwphold);
		Pr->sethold = TRUE;

		(void) Pgetregs(Pr);

		/* can't happen? */
		if ((sig = Pr->why.pr_lwp.pr_cursig) != 0) {
			long ctl[3];

			(void) fprintf(stderr,
				"%s: unexpected cursig: %d\n", command, sig);
			ctl[0] = PCKILL;
			ctl[1] = sig;
			ctl[2] = PCCSIG;
			(void) write(Pr->lwpctlfd, (char *)ctl,
				3*sizeof (long));
			Pr->why.pr_lwp.pr_cursig = 0;
		}

		if (!reissue) {
			/* remember the registers */
			for (reg = 0; reg < NGREG; reg++)
				savedreg[reg] = Pr->REG[reg];
		}

#if i386
		sp = Pr->REG[R_SP];
		Pr->REG[R_SP] -= 2*sizeof (int);
		(void) Pputareg(Pr, R_SP);
#endif

		for (sys = 1; sys <= PRMAXSYS; sys++)	/* no tracing */
			(void) Psysexit(Pr, sys, FALSE);

		/* make the lwp map in the getcwd library code */
		fd = propen(Pr, LIBCWD, O_RDONLY, 0);
		ptr1 = prmmap(Pr, (caddr_t)0, 4096, PROT_READ|PROT_EXEC,
			MAP_PRIVATE, fd, 0);
		(void) prclose(Pr, fd);

		fd = propen(Pr, "/dev/zero", O_RDONLY, 0);
		ptr2 = prmmap(Pr, (caddr_t)0, 8192, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
		(void) prclose(Pr, fd);

		/* get the ELF header for the entry point */
		(void) Pread(Pr, (off_t)ptr1, &Ehdr, sizeof (Ehdr));
		/* should do some checking here */
		cwd = (char *(*)())(ptr1 + (long)Ehdr.e_entry);

		/* arrange to call (*cwd)() */
#if i386
		(void) Pwrite(Pr, (long)(sp-4), (char *)&ptr2, 4);
		(void) Pwrite(Pr, (long)(sp-8), (char *)&ill, 4);
		Pr->REG[R_PC] = (int)cwd;
		Pr->REG[R_SP] -= SA(MINFRAME);
#elif __ppc
		Pr->REG[R_R3] = (int)ptr2;
		Pr->REG[R_LR] = (int)ill;
		Pr->REG[R_PC] = (int)cwd;
		Pr->REG[R_SP] -= SA(MINFRAME);
#elif sparc
		Pr->REG[R_O0] = (int)ptr2;
		Pr->REG[R_O7] = (int)ill;
		Pr->REG[R_PC] = (int)cwd;
		Pr->REG[R_nPC] = (int)cwd + 4;
		Pr->REG[R_SP] -= SA(MINFRAME);
#else
		"unknown architecture"
#endif
		(void) Pputregs(Pr);
		(void) Pfault(Pr, FLTBOUNDS, TRUE);
		(void) Pfault(Pr, FLTILL, TRUE);

		if (Pstart(Pr, 0) == 0 &&
		    Pwait(Pr, 0) == 0 &&
		    Pr->state == PS_STOP &&
		    Pr->why.pr_lwp.pr_why == PR_FAULTED) {
			long ctl[1];
			char *dir;
			char *addr;
			if (Pr->why.pr_lwp.pr_what != FLTBOUNDS)
				(void) printf("%ld:\texpected FLTBOUNDS\n",
					Pr->pid);
			addr = (char *)Pr->REG[R_RVAL0];
			ctl[0] = PCCFAULT;
			(void) write(Pr->lwpctlfd, (char *)ctl, sizeof (long));
			if (addr == (char *)NULL ||
			    (dir = fetchstring((long)addr)) == (char *)NULL)
				(void) printf("%ld:\t???\n", Pr->pid);
			else
				(void) printf("%ld:\t%s\n", Pr->pid, dir);
		}

		(void) Pfault(Pr, FLTILL, FALSE);
		(void) Pfault(Pr, FLTBOUNDS, FALSE);
		(void) prmunmap(Pr, ptr1, 4096);
		(void) prmunmap(Pr, ptr2, 8192);

		/* restore the registers */
		for (reg = 0; reg < NGREG; reg++)
			Pr->REG[reg] = savedreg[reg];
		(void) Pputregs(Pr);

		/* get back to the sleeping syscall */
		if (reissue) {
#if i386
			Pr->REG[R_PC] -= 7;
			(void) Pputareg(Pr, R_PC);
#elif __ppc
			Pr->REG[R_PC] -= 4;
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
cwd_self()
{
	int fd;
	char *ptr1;
	char *ptr2;
	char *(*cwd)();
	int rv = 0;

	fd = open(LIBCWD, O_RDONLY, 0);
	ptr1 = mmap((caddr_t)0, 4096, PROT_READ|PROT_EXEC,
		MAP_PRIVATE, fd, 0);
	(void) close(fd);
	if (ptr1 == NULL) {
		perror(LIBCWD);
		rv = -1;
	}

	fd = open("/dev/zero", O_RDONLY, 0);
	ptr2 = mmap((caddr_t)0, 8192, PROT_READ|PROT_WRITE,
		MAP_PRIVATE, fd, 0);
	(void) close(fd);
	if (ptr2 == NULL) {
		perror("/dev/zero");
		rv = -1;
	}

	/* LINTED improper alignment */
	cwd = (char *(*)())(ptr1 + (long)((Elf32_Ehdr *)ptr1)->e_entry);

	/* arrange to call (*cwd)() */
	if (ptr1 != NULL && ptr2 != NULL) {
		char *dir = (*cwd)(ptr2);
		if (dir == (char *)NULL)
			(void) printf("%ld:\t???\n", getpid());
		else
			(void) printf("%ld:\t%s\n", getpid(), dir);
	}

	(void) munmap(ptr1, 4096);
	(void) munmap(ptr2, 8192);

	return (rv);
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
	char ** pdirp)		/* points to /proc directory on success */
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

	pid = strtol(name, &next, 10);
	if (isdigit(*name) && pid >= 0 && *next == '\0') {
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

static char *
fetchstring(long addr)
{
	register process_t *Pr = &Proc;
	register int nbyte;
	register leng = 0;
	char string[41];
	string[40] = '\0';

	if (str_bsize == 0) {	/* initial allocation of string buffer */
		str_buffer = malloc(str_bsize = 16);
		if (str_buffer == NULL)
			abend("cannot allocate string buffer", 0);
	}
	*str_buffer = '\0';

	for (nbyte = 40; nbyte == 40 && leng < 400; addr += 40) {
		if ((nbyte = Pread(Pr, addr, string, 40)) < 0)
			return (leng? str_buffer : NULL);
		if (nbyte > 0 &&
		    (nbyte = strlen(string)) > 0) {
			while (leng+nbyte >= str_bsize) {
				str_buffer =
				    realloc(str_buffer, str_bsize *= 2);
				if (str_buffer == NULL)
					abend("cannot reallocate string buffer",
					    0);
			}
			(void) strcpy(str_buffer+leng, string);
			leng += nbyte;
		}
	}

	return (str_buffer);
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
			command, pid, Pr->why.pr_lwp.pr_why,
			Pr->why.pr_lwp.pr_what);
		if (Pr->state != PS_STOP) {
			(void) Prelease(Pr);
			goto bad;
		}
	}

	return (0);

bad:
	if (gcode == G_NOPROC || gcode == G_ZOMB || errno == ENOENT)
		(void) fprintf(stderr, "%s: no process %ld\n",
			command, pid);
	else
		(void) fprintf(stderr, "%s: cannot control process %ld\n",
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

static void
errmsg(const char *s, const char *q)
{
	char msg[200];

	msg[0] = '\0';
	if (command) {
		(void) strcpy(msg, command);
		(void) strcat(msg, ": ");
	}
	if (s) (void) strcat(msg, s);
	if (q) (void) strcat(msg, q);
	(void) strcat(msg, "\n");
	(void) write(2, msg, (unsigned)strlen(msg));
}

static void
abend(const char *s, const char *q)
{
	if (s || q)
		errmsg(s, q);
	if (PR)
		(void) Prelease(PR);
	exit(2);
}
