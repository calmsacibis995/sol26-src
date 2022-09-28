/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pflags.c	1.4	96/06/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>

static	int	look(char *);
static	void	lwplook(lwpstatus_t *, int, int);
static	int	perr(const char *);
static	char	*prflags(int);
static	char	*prwhy(int);
static	char	*prwhat(int, int);

extern	char	*signame(int);
extern	char	*fltname(int);
extern	char	*sysname(int);

static	char	procname[64];

#define	LWPFLAGS	\
	(PR_STOPPED|PR_ISTOP|PR_DSTOP|PR_ASLEEP|PR_PCINVAL|PR_STEP|PR_ASLWP)

#define	PROCFLAGS	\
	(PR_ISSYS|PR_VFORKP \
	|PR_FORK|PR_RLC|PR_KLC|PR_ASYNC|PR_BPTADJ|PR_MSACCT|PR_PTRACE)

#define	ALLFLAGS	(LWPFLAGS|PROCFLAGS)

main(int argc, char **argv)
{
	int rc = 0;

	if (argc <= 1) {
		char *cmd = strrchr(argv[0], '/');

		if (cmd++ == NULL)
			cmd = argv[0];
		(void) fprintf(stderr, "usage:  %s pid ...\n", cmd);
		(void) fprintf(stderr, "  (report process status flags)\n");
		return (2);
	}

	while (--argc > 0)
		rc += look(*++argv);

	return (rc);
}

static int
look(char *arg)
{
	int rval = 0;
	int ifd, sfd;
	char *pidp;
	pstatus_t pstatus;
	psinfo_t psinfo;
	sigset_t sigmask;
	fltset_t fltmask;
	sysset_t entrymask;
	sysset_t exitmask;
	uint32_t sigtrace, sigtrace2, fltbits;
	uint32_t sigpend, sigpend2;
	uint32_t *bits;
	char psinfoname[100];
	char pstatusname[100];
	char lwpdirname[100];
	char *lp;
	DIR *dirp;
	struct dirent *dentp;

	if (strchr(arg, '/') != NULL)
		(void) strncpy(procname, arg, sizeof (procname));
	else {
		(void) strcpy(procname, "/proc/");
		(void) strncat(procname, arg, sizeof (procname)-6);
	}
	pidp = strrchr(procname, '/')+1;
	while (*pidp == '0' && *(pidp+1) != '\0')
		pidp++;

	(void) strcpy(psinfoname, procname);
	(void) strcat(psinfoname, "/psinfo");
	(void) strcpy(pstatusname, procname);
	(void) strcat(pstatusname, "/status");
	(void) strcpy(lwpdirname, procname);
	(void) strcat(lwpdirname, "/lwp");
	if ((ifd = open(psinfoname, O_RDONLY)) < 0)
		return (perr(NULL));
	else if ((sfd = open(pstatusname, O_RDONLY)) < 0) {
		(void) close(ifd);
		return (perr(NULL));
	} else if (read(ifd, (char *)&psinfo, sizeof (psinfo))
	    != sizeof (psinfo)) {
		rval = perr("psinfo");
	} else if (read(sfd, (char *)&pstatus, sizeof (pstatus))
	    != sizeof (pstatus)) {
		rval = perr("pstatus");
	}

	sigmask = pstatus.pr_sigtrace;
	fltmask = pstatus.pr_flttrace;
	entrymask = pstatus.pr_sysentry;
	exitmask = pstatus.pr_sysexit;

	(void) close(ifd);
	(void) close(sfd);
	if (rval) {
		if (errno == ENOENT) {
			(void) printf("%s:\t<defunct>\n", pidp);
			return (0);
		}
		return (rval);
	}

	(void) printf("%s:\t%.72s\n", pidp,
		psinfo.pr_psargs);
	if (pstatus.pr_nlwp > 1 && (pstatus.pr_flags & ~LWPFLAGS))
		(void) printf("\tflags = %s\n",
			prflags(pstatus.pr_flags & ~LWPFLAGS));

	sigtrace = *((uint32_t *)&sigmask);
	sigtrace2 = *((uint32_t *)&sigmask + 1);
	fltbits = *((uint32_t *)&fltmask);
	if (sigtrace || sigtrace2 || fltbits) {
		if (sigtrace || sigtrace2)
			(void) printf("  sigtrace = 0x%.8x,0x%.8x",
				sigtrace, sigtrace2);
		if (fltbits)
			(void) printf("  flttrace = 0x%.8x", fltbits);
		(void) printf("\n");
	}

	bits = ((uint32_t *)&entrymask);
	if (bits[0] | bits[1] | bits[2] | bits[3] |
	    bits[4] | bits[5] | bits[6] | bits[7])
		(void) printf("  entrymask = "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n              "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
			bits[0], bits[1], bits[2], bits[3],
			bits[4], bits[5], bits[6], bits[7]);

	bits = ((uint32_t *)&exitmask);
	if (bits[0] | bits[1] | bits[2] | bits[3] |
	    bits[4] | bits[5] | bits[6] | bits[7])
		(void) printf("  exitmask  = "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n              "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
			bits[0], bits[1], bits[2], bits[3],
			bits[4], bits[5], bits[6], bits[7]);

	sigpend  = *((uint32_t *)&pstatus.pr_sigpend);
	sigpend2 = *((uint32_t *)&pstatus.pr_sigpend + 1);
	if (sigpend || sigpend2)
		(void) printf("  sigpend = 0x%.8x,0x%.8x\n",
			sigpend, sigpend2);

	if ((dirp = opendir(lwpdirname)) == NULL)
		return (0);
	lp = lwpdirname + strlen(lwpdirname);
	*lp++ = '/';

	/* for each lwp */
	while (dentp = readdir(dirp)) {
		int lwpfd;
		lwpstatus_t lwpstatus;

		if (dentp->d_name[0] == '.')
			continue;
		(void) strcpy(lp, dentp->d_name);
		(void) strcat(lp, "/lwpstatus");
		if ((lwpfd = open(lwpdirname, O_RDONLY)) < 0) {
			(void) perr("lwpdirname");
		} else if (read(lwpfd, (char *)&lwpstatus, sizeof (lwpstatus))
		    < 0) {
			(void) close(lwpfd);
			(void) perr("read lwpstatus");
		} else {
			int flags = lwpstatus.pr_flags;

			(void) close(lwpfd);
			if (pstatus.pr_nlwp > 1)
				flags &= ~PROCFLAGS;
			lwplook(&lwpstatus, flags, pstatus.pr_flags & PR_ISSYS);
		}
	}
	(void) closedir(dirp);

	return (0);
}

static void
lwplook(lwpstatus_t *psp, int flags, int issys)
{
	uint32_t sighold, sighold2;
	uint32_t sigpend, sigpend2;
	int cursig;

	(void) printf("  /%ld:\t", psp->pr_lwpid);
	(void) printf("flags = %s", prflags(flags));
	if (flags & PR_ASLEEP) {
		if ((flags & ~PR_ASLEEP) != 0)
			(void) printf("|");
		(void) printf("PR_ASLEEP");
		if (psp->pr_syscall && !issys) {
			u_int i;

			(void) printf(" [ %s(", sysname(psp->pr_syscall));
			for (i = 0; i < psp->pr_nsysarg; i++) {
				if (i != 0)
					(void) printf(",");
				(void) printf("0x%lx", psp->pr_sysarg[i]);
			}
			(void) printf(") ]");
		}
	}
	(void) printf("\n");

	if (flags & PR_STOPPED) {
		(void) printf("  why = %s", prwhy(psp->pr_why));
		if (psp->pr_why != PR_REQUESTED &&
		    psp->pr_why != PR_SUSPENDED)
			(void) printf("  what = %s",
				prwhat(psp->pr_why, psp->pr_what));
		(void) printf("\n");
	}

	sighold  = *((uint32_t *)&psp->pr_lwphold);
	sighold2 = *((uint32_t *)&psp->pr_lwphold + 1);
	sigpend  = *((uint32_t *)&psp->pr_lwppend);
	sigpend2 = *((uint32_t *)&psp->pr_lwppend + 1);
	cursig   = psp->pr_cursig;

	if (sighold || sighold2 || sigpend || sigpend2 || cursig) {
		if (sighold || sighold2)
			(void) printf("  sigmask = 0x%.8x,0x%.8x",
				sighold, sighold2);
		if (sigpend || sigpend2)
			(void) printf("  lwppend = 0x%.8x,0x%.8x",
				sigpend, sigpend2);
		if (cursig)
			(void) printf("  cursig = %s", signame(cursig));
		(void) printf("\n");
	}
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

static char *
prflags(int arg)
{
	static char code_buf[100];
	register char *str = code_buf;

	if (arg == 0)
		return ("0");

	if (arg & ~ALLFLAGS)
		(void) sprintf(str, "0x%x", arg & ~ALLFLAGS);
	else
		*str = '\0';

	if (arg & PR_STOPPED)
		(void) strcat(str, "|PR_STOPPED");
	if (arg & PR_ISTOP)
		(void) strcat(str, "|PR_ISTOP");
	if (arg & PR_DSTOP)
		(void) strcat(str, "|PR_DSTOP");
#if 0		/* displayed elsewhere */
	if (arg & PR_ASLEEP)
		(void) strcat(str, "|PR_ASLEEP");
#endif
	if (arg & PR_PCINVAL)
		(void) strcat(str, "|PR_PCINVAL");
	if (arg & PR_STEP)
		(void) strcat(str, "|PR_STEP");
	if (arg & PR_ASLWP)
		(void) strcat(str, "|PR_ASLWP");
	if (arg & PR_ISSYS)
		(void) strcat(str, "|PR_ISSYS");
	if (arg & PR_VFORKP)
		(void) strcat(str, "|PR_VFORKP");
	if (arg & PR_FORK)
		(void) strcat(str, "|PR_FORK");
	if (arg & PR_RLC)
		(void) strcat(str, "|PR_RLC");
	if (arg & PR_KLC)
		(void) strcat(str, "|PR_KLC");
	if (arg & PR_ASYNC)
		(void) strcat(str, "|PR_ASYNC");
	if (arg & PR_BPTADJ)
		(void) strcat(str, "|PR_BPTADJ");
	if (arg & PR_MSACCT)
		(void) strcat(str, "|PR_MSACCT");
	if (arg & PR_PTRACE)
		(void) strcat(str, "|PR_PTRACE");

	if (*str == '|')
		str++;

	return (str);
}

static char *
prwhy(int why)
{
	static char buf[20];
	register char *str;

	switch (why) {
	case PR_REQUESTED:
		str = "PR_REQUESTED";
		break;
	case PR_SIGNALLED:
		str = "PR_SIGNALLED";
		break;
	case PR_SYSENTRY:
		str = "PR_SYSENTRY";
		break;
	case PR_SYSEXIT:
		str = "PR_SYSEXIT";
		break;
	case PR_JOBCONTROL:
		str = "PR_JOBCONTROL";
		break;
	case PR_FAULTED:
		str = "PR_FAULTED";
		break;
	case PR_SUSPENDED:
		str = "PR_SUSPENDED";
		break;
	default:
		str = buf;
		(void) sprintf(str, "%d", why);
		break;
	}

	return (str);
}

static char *
prwhat(int why, int what)
{
	static char buf[20];
	register char *str;

	switch (why) {
	case PR_SIGNALLED:
	case PR_JOBCONTROL:
		str = signame(what);
		break;
	case PR_SYSENTRY:
	case PR_SYSEXIT:
		str = sysname(what);
		break;
	case PR_FAULTED:
		str = fltname(what);
		break;
	default:
		(void) sprintf(str = buf, "%d", what);
		break;
	}

	return (str);
}
