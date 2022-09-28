/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ipc.c	1.20	96/10/19 SMI"	/* SVr4.0 1.2	*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/termio.h>

#include <signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include "pcontrol.h"
#include "ramdata.h"
#include "proto.h"

/*
 * Routines related to interprocess communication
 * among the truss processes which are controlling
 * multiple traced processes.
 *
 * This module is carefully coded to contain only read-only data.
 * All read/write data is defined in ramdata.c (see also ramdata.h).
 */

/*
 * Function prototypes for static routines in this module.
 */
static	void	Ecritical(int);
static	void	Xcritical(int);
static	void	UnFlush(void);

void
Flush()		/* ensure everyone keeps out of each other's way */
{		/* while writing lines of trace output		 */

	/* except for regions bounded by Eserialize()/Xserialize(), */
	/* this is the only place anywhere in the program */
	/* where a write() to the trace output file takes place */
	/* so here is where we detect errors writing to the output */

	register FILE *fp = stdout;

	if (fp->_ptr == fp->_base)
		return;

	errno = 0;

	Ecritical(0);
	if (interrupt)
		UnFlush();
	else
		(void) fflush(fp);
	if (slowmode)
		(void) ioctl(fileno(fp), TCSBRK, 1);
	Xcritical(0);

	if (ferror(fp) && errno)	/* error on write(), probably EPIPE */
		interrupt = TRUE;		/* post an interrupt */
}

static void
UnFlush()	/* avoid writing what is in the stdout buffer */
{
	register FILE *fp = stdout;

	fp->_cnt -= (fp->_ptr - fp->_base);	/* this is filthy */
	fp->_ptr = fp->_base;
}

/*
 * Eserialize() and Xserialize() are used to bracket
 * a region which may produce large amounts of output,
 * such as showargs()/dumpargs().
 */

void
Eserialize()
{
	/* serialize output */
	Ecritical(0);
}

void
Xserialize()
{
	(void) fflush(stdout);
	if (slowmode)
		(void) ioctl(fileno(stdout), TCSBRK, 1);
	Xcritical(0);
}

static void		/* enter critical region --- */
Ecritical(int num)	/* wait on mutex, lock out other processes */
{
	int rv = _lwp_mutex_lock(&Cp->mutex[num]);

	if (rv != 0) {
		char mnum[2];
		mnum[0] = '0' + num;
		mnum[1] = '\0';
		errno = rv;
		perror(command);
		errmsg("cannot grab mutex #", mnum);
	}
}

static void		/* exit critical region --- */
Xcritical(int num)	/* release other processes waiting on mutex */
{
	int rv = _lwp_mutex_unlock(&Cp->mutex[num]);

	if (rv != 0) {
		char mnum[2];
		mnum[0] = '0' + num;
		mnum[1] = '\0';
		errno = rv;
		perror(command);
		errmsg("cannot release mutex #", mnum);
	}
}

void
procadd(pid_t spid)	/* add process to list of those being traced */
{
	register int i;
	register int j = -1;

	if (Cp == NULL)
		return;

	Ecritical(1);
	for (i = 0; i < sizeof (Cp->tpid) / sizeof (Cp->tpid[0]); i++) {
		if (Cp->tpid[i] == 0) {
			if (j == -1)	/* remember first vacant slot */
				j = i;
			if (Cp->spid[i] == 0)	/* this slot is better */
				break;
		}
	}
	if (i < sizeof (Cp->tpid) / sizeof (Cp->tpid[0]))
		j = i;
	if (j >= 0) {
		Cp->tpid[j] = getpid();
		Cp->spid[j] = spid;
	}
	Xcritical(1);
}

void
procdel()	/* delete process from list of those being traced */
{
	register int i;
	register pid_t tpid;

	if (Cp == NULL)
		return;

	tpid = getpid();

	Ecritical(1);
	for (i = 0; i < sizeof (Cp->tpid) / sizeof (Cp->tpid[0]); i++) {
		if (Cp->tpid[i] == tpid) {
			Cp->tpid[i] = 0;
			break;
		}
	}
	Xcritical(1);
}

static int	/* close() system call -- executed by subject process */
prclose(process_t *Pr, int fd)
{
	struct sysret rval;		/* return value from close() */
	struct argdes argd[1];		/* arg descriptor for close() */
	register struct argdes *adp = &argd[0];	/* first argument */

	if (Pr == (process_t *)NULL)	/* no subject process */
		return (close(fd));

	adp->value = fd;
	adp->object = NULL;
	adp->type = AT_BYVAL;
	adp->inout = AI_INPUT;
	adp->len = 0;

	rval = Psyscall(Pr, SYS_close, 1, &argd[0]);

	if (rval.errno < 0) {
		(void) fprintf(stderr,
			"%s\t*** error from Psyscall(SYS_close): %d\n",
			pname, rval.errno);
		rval.errno = EINVAL;
	}

	if (rval.errno == 0)
		return (0);
(void) fprintf(stderr, "prclose(%d) failed, errno = %d\n", fd, rval.errno);
	errno = rval.errno;
	return (-1);
}

/*
 * check for open of /proc/nnnnn file
 * return TRUE iff process opened its own
 * else inform controlling truss process
 * 'what' is either SYS_open or SYS_open64
 */
int
checkproc(process_t *Pr, int what, char *path, int err)
{
	int pid;
	int i;
	const char *dirname;
	char *next;
	char *sp1;
	char *sp2;
	int rc = FALSE;		/* assume not self-open */

	/*
	 * A bit heuristic ...
	 * Test for the cases:
	 *	1234
	 *	1234/as
	 *	1234/ctl
	 *	1234/lwp/24/lwpctl
	 *	.../1234
	 *	.../1234/as
	 *	.../1234/ctl
	 *	.../1234/lwp/24/lwpctl
	 * Insert a '\0', if necessary, so the path becomes ".../1234".
	 */
	if ((sp1 = strrchr(path, '/')) == NULL)		/* last component */
		/* EMPTY */;
	else if (isdigit(*(sp1+1))) {
		sp1 += strlen(sp1);
		while (--sp1 > path && isdigit(*sp1))
			;
		if (*sp1 != '/')
			return (FALSE);
	} else if (strcmp(sp1+1, "as") == 0 ||
	    strcmp(sp1+1, "ctl") == 0) {
		*sp1 = '\0';
	} else if (strcmp(sp1+1, "lwpctl") == 0) {
		/*
		 * .../1234/lwp/24/lwpctl
		 *                ^-- sp1
		 */
		while (--sp1 > path && isdigit(*sp1))
			;
		if (*sp1 != '/' ||
		    (sp1 -= 4) <= path ||
		    strncmp(sp1, "/lwp", 4) != 0)
			return (FALSE);
		*sp1 = '\0';
	} else {
		return (FALSE);
	}

	if ((sp2 = strrchr(path, '/')) == NULL)
		dirname = path;
	else
		dirname = sp2 + 1;
	if ((pid = strtol(dirname, &next, 10)) < 0 ||
	    *next != '\0') {	/* dirname not a number */
		if (sp1 != NULL)
			*sp1 = '/';
		return (FALSE);
	}
	if (sp2 == NULL)
		dirname = ".";
	else {
		*sp2 = '\0';
		dirname = path;
	}

	if (!isprocdir(Pr, dirname) ||	/* file not in a /proc directory */
	    pid == getpid() ||		/* process opened truss's /proc file */
	    pid == 0) {			/* process opened process 0 */
		if (sp1 != NULL)
			*sp1 = '/';
		if (sp2 != NULL)
			*sp2 = '/';
		return (FALSE);
	}
	if (sp1 != NULL)
		*sp1 = '/';
	if (sp2 != NULL)
		*sp2 = '/';

	/* process did open a /proc file --- */

	if (pid == Pr->pid)	/* process opened its own /proc file */
		rc = TRUE;
	else {			/* send signal to controlling truss process */
		for (i = 0; i < sizeof (Cp->tpid)/sizeof (Cp->tpid[0]); i++) {
			if (Cp->spid[i] == pid) {
				pid = Cp->tpid[i];
				break;
			}
		}
		if (i >= sizeof (Cp->tpid) / sizeof (Cp->tpid[0]))
			return (FALSE);	/* don't attempt retry of open() */

		{	/* wait for controlling process to terminate */
			while (pid && Cp->tpid[i] == pid) {
				if (kill(pid, SIGUSR1) == -1)
					break;
				msleep(1000);
			}
			Ecritical(1);
			if (Cp->tpid[i] == 0)
				Cp->spid[i] = 0;
			Xcritical(1);
		}
	}

	if (err == 0 && prclose(Pr, Rval1) == 0)
		err = 1;
	if (err) {	/* prepare to reissue the open() system call */
		UnFlush();	/* don't print the failed open() */
		if (rc && !cflag && prismember(&trace, what)) {
			/* last gasp */
			(void) sysentry(Pr);
			(void) printf("%s%s\n", pname, sys_string);
			sys_leng = 0;
			*sys_string = '\0';
		}
#if sparc
		if (sys_indirect) {
			Pr->REG[R_G1] = SYS_syscall;
			Pr->REG[R_O0] = what;
			for (i = 0; i < 5; i++)
				Pr->REG[R_O1+i] = sys_args[i];
		} else {
			Pr->REG[R_G1] = what;
			for (i = 0; i < 6; i++)
				Pr->REG[R_O0+i] = sys_args[i];
		}
		Pr->REG[R_nPC] = Pr->REG[R_PC];
#elif __ppc
		if (sys_indirect) {
			Pr->REG[R_R0] = SYS_syscall;
			Pr->REG[R_R3] = what;
			for (i = 0; i < 7; i++)
				Pr->REG[R_R4+i] = sys_args[i];
		} else {
			Pr->REG[R_R0] = what;
			for (i = 0; i < 8; i++)
				Pr->REG[R_R3+i] = sys_args[i];
		}
#else
		(void) Psetsysnum(Pr, what);
#endif
		Pr->REG[R_PC] -= sizeof (syscall_t);
		(void) Pputregs(Pr);
	}

	return (rc);
}
