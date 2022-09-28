/*
 * Copyright (c) 1992,1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident "@(#)actions.c	1.29	96/07/13 SMI"	/* SVr4.0 1.3	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>

#include <signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/isa_defs.h>
#include "pcontrol.h"
#include "ramdata.h"
#include "systable.h"
#include "print.h"
#include "proto.h"

/*
 * Actions to take when process stops.
 */

/* This module is carefully coded to contain only read-only data */
/* All read/write data is defined in ramdata.c (see also ramdata.h) */

/*
 * Function prototypes for static routines in this module.
 */
static	int	stopsig(process_t *);
static	void	showpaths(const struct systable *);
static	void	showargs(process_t *, int);
static	void	dumpargs(process_t *, char *, const char *);

/*
 * requested() gets called for these reasons:
 *	flag == JOBSIG:		report nothing; change state to JOBSTOP
 *	flag == JOBSTOP:	report "Continued ..."
 *	default:		report sleeping system call
 *
 * It returns a new flag:  JOBSTOP or SLEEPING or 0.
 */
int
requested(process_t *Pr, int flag)
{
	int sig = Pr->why.pr_lwp.pr_cursig;
	int sys;
	int newflag = 0;

	switch (flag) {
	case JOBSIG:
		return (JOBSTOP);

	case JOBSTOP:
		if (sig > 0 && sig <= PRMAXSIG && prismember(&signals, sig)) {
			length = 0;
			(void) printf("%s    Continued with signal #%d, %s",
				pname, sig, signame(sig));
			if (Pr->why.pr_lwp.pr_action.sa_handler == SIG_DFL)
				(void) printf(" [default]");
			else if (Pr->why.pr_lwp.pr_action.sa_handler == SIG_IGN)
				(void) printf(" [ignored]");
			else
				(void) printf(" [caught]");
			(void) fputc('\n', stdout);
			Flush();
		}
		break;

	default:
		if (!(Pr->why.pr_lwp.pr_flags & PR_ASLEEP) ||
		    (sys = Pgetsysnum(Pr)) <= 0 || sys > PRMAXSYS)
			break;

		newflag = SLEEPING;

		/* Make sure we catch sysexit even if we're not tracing it. */
		if (!prismember(&trace, sys))
			(void) Psysexit(Pr, sys, TRUE);
		else if (!cflag) {
			length = 0;
			Pr->why.pr_lwp.pr_what = sys;	/* cheating a little */
			Errno = 0;
			Rval1 = Rval2 = 0;
			(void) sysentry(Pr);
			if (pname[0])
				(void) fputs(pname, stdout);
			length += printf("%s", sys_string);
			sys_leng = 0;
			*sys_string = '\0';
			length >>= 3;
			if (length >= 4)
				(void) fputc(' ', stdout);
			for (; length < 4; length++)
				(void) fputc('\t', stdout);
			(void) fputs("(sleeping...)\n", stdout);
			length = 0;
			Flush();
		}
		break;
	}

	if ((flag = jobcontrol(Pr)) != 0)
		newflag = flag;

	return (newflag);
}

int
jobcontrol(process_t *Pr)
{
	int sig = stopsig(Pr);

	if (sig == 0)
		return (0);

	if (!cflag &&				/* not just counting */
	    prismember(&signals, sig)) {	/* tracing this signal */
		int sys;

		length = 0;
		(void) printf("%s    Stopped by signal #%d, %s",
			pname, sig, signame(sig));
		if ((Pr->why.pr_lwp.pr_flags & PR_ASLEEP) &&
		    (sys = Pgetsysnum(Pr)) > 0 && sys <= PRMAXSYS)
			(void) printf(", in %s()",
				sysname(sys, Pgetsubcode(Pr)));
		(void) fputc('\n', stdout);
		Flush();
	}

	return (JOBSTOP);
}

/*
 * Return the signal the process stopped on iff process is already stopped on
 * PR_JOBCONTROL or is stopped on PR_SIGNALLED or PR_REQUESTED with a current
 * signal that will cause a JOBCONTROL stop when the process is set running.
 */
static int
stopsig(process_t *Pr)
{
	int sig = 0;

	if (Pr->state == PS_STOP) {
		switch (Pr->why.pr_lwp.pr_why) {
		case PR_JOBCONTROL:
			sig = Pr->why.pr_lwp.pr_what;
			if (sig < 0 || sig > PRMAXSIG)
				sig = 0;
			break;
		case PR_SIGNALLED:
		case PR_REQUESTED:
			if (Pr->why.pr_lwp.pr_action.sa_handler == SIG_DFL) {
				switch (Pr->why.pr_lwp.pr_cursig) {
				case SIGSTOP:
				case SIGTSTP:
				case SIGTTIN:
				case SIGTTOU:
					sig = Pr->why.pr_lwp.pr_cursig;
					break;
				}
			}
			break;
		}
	}

	return (sig);
}

int
signalled(process_t *Pr)
{
	int sig = Pr->why.pr_lwp.pr_what;
	int flag = 0;

	if (sig <= 0 || sig > PRMAXSIG)	/* check bounds */
		return (0);

	if (cflag)			/* just counting */
		Cp->sigcount[sig]++;

	if ((flag = jobcontrol(Pr)) == 0 &&
	    !cflag &&
	    prismember(&signals, sig)) {
		int sys;

		length = 0;
		(void) printf("%s    Received signal #%d, %s",
			pname, sig, signame(sig));
		if ((Pr->why.pr_lwp.pr_flags & PR_ASLEEP) &&
		    (sys = Pgetsysnum(Pr)) > 0 && sys <= PRMAXSYS)
			(void) printf(", in %s()",
				sysname(sys, Pgetsubcode(Pr)));
		if (Pr->why.pr_lwp.pr_action.sa_handler == SIG_DFL)
			(void) printf(" [default]");
		else if (Pr->why.pr_lwp.pr_action.sa_handler == SIG_IGN)
			(void) printf(" [ignored]");
		else
			(void) printf(" [caught]");
		(void) fputc('\n', stdout);
		if (Pr->why.pr_lwp.pr_info.si_code != 0 ||
		    Pr->why.pr_lwp.pr_info.si_pid != 0)
			print_siginfo(&Pr->why.pr_lwp.pr_info);
		Flush();
	}

	if (flag == JOBSTOP)
		flag = JOBSIG;
	return (flag);
}

void
faulted(process_t *Pr)
{
	int flt = Pr->why.pr_lwp.pr_what;

	if ((unsigned)flt > PRMAXFAULT)	/* check bounds */
		flt = 0;
	Cp->fltcount[flt]++;

	if (cflag)		/* just counting */
		return;
	length = 0;
	(void) printf("%s    Incurred fault #%d, %s  %%pc = 0x%.8X",
		pname, flt, fltname(flt), Pr->why.pr_lwp.pr_reg[R_PC]);
	if (flt == FLTPAGE)
		(void) printf("  addr = 0x%.8lX",
			(long)Pr->why.pr_lwp.pr_info.si_addr);
	(void) fputc('\n', stdout);
	if (Pr->why.pr_lwp.pr_info.si_signo != 0)
		print_siginfo(&Pr->why.pr_lwp.pr_info);
	Flush();
}

/* set up sys_nargs and sys_args[] (syscall args) */
static void
setupsysargs(process_t *Pr)
{
	int nargs;
	int what = Pr->why.pr_lwp.pr_what;
	int i;

	/* protect ourself from operating system error */
	if (what <= 0 || what > PRMAXSYS)
		what = 0;

#if sparc
	/* determine whether syscall is indirect */
	(void) Pgetareg(Pr, R_G1);
	sys_indirect = (Pr->REG[R_G1] == SYS_syscall)? 1 : 0;
#elif __ppc
	(void) Pgetareg(Pr, R_R0);
	sys_indirect = (Pr->REG[R_R0] == SYS_syscall)? 1 : 0;
#else
	sys_indirect = 0;
#endif

	(void) memset(sys_args, 0, sizeof (sys_args));
	if (what != Pr->why.pr_lwp.pr_syscall) {	/* assertion */
		(void) printf("%s\t*** Inconsistent syscall: %d vs %d ***\n",
			pname, what, Pr->why.pr_lwp.pr_syscall);
	}
	nargs = Pr->why.pr_lwp.pr_nsysarg;
	for (i = 0;
	    i < nargs && i < sizeof (sys_args) / sizeof (sys_args[0]); i++)
		sys_args[i] = Pr->why.pr_lwp.pr_sysarg[i];
	sys_nargs = nargs;
}

#define	ISREAD(code) \
	((code) == SYS_read || (code) == SYS_pread || \
	(code) == SYS_pread64 || (code) == SYS_readv || \
	(code) == SYS_recv || (code) == SYS_recvfrom)
#define	ISWRITE(code) \
	((code) == SYS_write || (code) == SYS_pwrite || \
	(code) == SYS_pwrite64 || (code) == SYS_writev || \
	(code) == SYS_send || (code) == SYS_sendto)

/* return TRUE iff syscall is being traced */
int
sysentry(process_t *Pr)
{
	int arg;
	int nargs;
	int i;
	int x;
	int len;
	char *s;
	const struct systable *stp;
	int what = Pr->why.pr_lwp.pr_what;
	int subcode;
	int istraced;
	int raw;

	/* protect ourself from operating system error */
	if (what <= 0 || what > PRMAXSYS)
		what = 0;

	/* set up the system call arguments (sys_nargs & sys_args[]) */
	setupsysargs(Pr);
	nargs = sys_nargs;

	/* get systable entry for this syscall */
	subcode = Pgetsubcode(Pr);
	stp = subsys(what, subcode);

	if (nargs > stp->nargs)
		nargs = stp->nargs;
	sys_nargs = nargs;

	/* fetch and remember first argument if it's a string */
	sys_valid = FALSE;
	if (nargs > 0 &&
	    stp->arg[0] == STG &&
	    (s = fetchstring((long)sys_args[0], 400)) != NULL) {
		sys_valid = TRUE;
		len = strlen(s);
		while (len >= sys_psize) {	/* reallocate if necessary */
			free(sys_path);
			sys_path = malloc(sys_psize *= 2);
			if (sys_path == NULL)
				abend("cannot allocate pathname buffer", 0);
		}
		(void) strcpy(sys_path, s);	/* remember pathname */
	}

	istraced = prismember(&trace, what);
	raw = prismember(&rawout, what);

	/* force tracing of read/write buffer dump syscalls */
	if (!istraced && nargs > 2) {
		int fdp1 = sys_args[0] + 1;

		if (ISREAD(what)) {
			if (prismember(&readfd, fdp1))
				istraced = TRUE;
		} else if (ISWRITE(what)) {
			if (prismember(&writefd, fdp1))
				istraced = TRUE;
		}
	}

	sys_leng = 0;
	if (cflag || !istraced)		/* just counting */
		*sys_string = 0;
	else {
		int argprinted = FALSE;

		sys_leng = sprintf(sys_string, "%s(",
			sysname(what, raw? -1 : subcode));
		for (i = 0; i < nargs; i++) {
			arg = sys_args[i];
			x = stp->arg[i];

			if (x == STG && !raw &&
			    i == 0 && sys_valid) {	/* already fetched */
				outstring("\"");
				outstring(sys_path);
				outstring("\"");
				argprinted = TRUE;
			} else if (x != HID || raw) {
				if (argprinted)
					outstring(", ");
				if (x == LLO)
					(*Print[x])(raw, arg, sys_args[++i]);
				else
					(*Print[x])(raw, arg);
				argprinted = TRUE;
			}
		}
		outstring(")");
	}

	return (istraced);
}
#undef	ISREAD
#undef	ISWRITE

void
sysexit(process_t *Pr)
{
	int r0;
	int r1;
	int ccr;
	int what = Pr->why.pr_lwp.pr_what;
	const struct systable *stp;
	int arg0;
	int istraced;
	int raw;

	/* protect ourself from operating system error */
	if (what <= 0 || what > PRMAXSYS)
		what = 0;

	/*
	 * If we aren't supposed to be tracing this one, then
	 * delete it from the traced signal set.  We got here
	 * because the process was sleeping in an untraced syscall.
	 */
	if (!prismember(&traceeven, what)) {
		(void) Psysexit(Pr, what, FALSE);
		return;
	}

	/* get systable entry for this syscall */
	stp = subsys(what, -1);

	/* pick up registers & set Errno before anything else */
	(void) Pgetareg(Pr, R_RVAL0); r0 = Pr->REG[R_RVAL0];
	(void) Pgetareg(Pr, R_RVAL1); r1 = Pr->REG[R_RVAL1];
	(void) Pgetareg(Pr, R_CCREG); ccr = Pr->REG[R_CCREG];

	Errno = (ccr & ERRBIT)? r0 : 0;
	Rval1 = r0;
	Rval2 = r1;

	switch (what) {
	case SYS_exit:		/* these are traced on entry */
	case SYS_lwp_exit:
	case SYS_evtrapret:
	case SYS_context:
		istraced = prismember(&trace, what);
		break;
	case SYS_exec:		/* these are traced on entry */
	case SYS_execve:
		istraced = prismember(&trace, what);
		if (exec_string && *exec_string) {
			if (!cflag && istraced)	/* print exec() string now */
				(void) fputs(exec_string, stdout);
			exec_string[0] = '\0';
		}
		break;
	default:
		if (slowmode) {		/* everything traced on entry */
			istraced = prismember(&trace, what);
			break;
		}
		istraced = sysentry(Pr);
		length = 0;
		if (!cflag && istraced) {
			if (pname[0])
				(void) fputs(pname, stdout);
			length += printf("%s", sys_string);
		}
		sys_leng = 0;
		*sys_string = '\0';
		break;
	}

	if (istraced) {
		Cp->syscount[what]++;
		accumulate(&Cp->systime[what], &Pr->why.pr_stime, &syslast);
	}
	syslast = Pr->why.pr_stime;
	usrlast = Pr->why.pr_utime;

	arg0 = sys_args[0];

	if (!cflag && istraced) {
		if ((what == SYS_fork ||
		    what == SYS_vfork ||
		    what == SYS_fork1) &&
		    Errno == 0 && r1 != 0) {
			length &= ~07;
			length += 14 + printf("\t\t(returning as child ...)");
		}
		if (Errno != 0 || (what != SYS_exec && what != SYS_execve)) {
			/* prepare to print the return code */
			length >>= 3;
			if (length >= 6)
				(void) fputc(' ', stdout);
			for (; length < 6; length++)
				(void) fputc('\t', stdout);
		}
	}
	length = 0;

	/* interpret syscalls with sub-codes */
	if (sys_nargs > 0 && what != SYS_open)
		stp = subsys(what, arg0);

	raw = prismember(&rawout, what);

	if (Errno != 0) {		/* error in syscall */
		if (istraced) {
			Cp->syserror[what]++;
			if (!cflag) {
				const char *ename = errname(r0);

				(void) printf("Err#%d", r0);
				if (ename != NULL) {
					(void) fputc(' ', stdout);
					(void) fputs(ename, stdout);
				}
				(void) fputc('\n', stdout);
			}
		}
	} else {
		/* show arguments on successful exec */
		if (what == SYS_exec || what == SYS_execve) {
			if (!cflag && istraced)
				showargs(Pr, raw);
		} else if (!cflag && istraced) {
			const char *fmt = NULL;
			int rv1 = r0;
			int rv2 = r1;

			switch (what) {
			case SYS_llseek:
#ifdef _LONG_LONG_LTOH	/* first long of a longlong is the low order */
				if (rv2 != 0) {
					int temp = rv1;
					fmt = "= 0x%X%.8X";
					rv1 = rv2;
					rv2 = temp;
					break;
				}
#else	/* the other way around */
				if (rv1 != 0) {
					fmt = "= 0x%X%.8X";
					break;
				}
				rv1 = rv2;	/* ugly */
#endif
				/* FALLTHROUGH */
			case SYS_lseek:
			case SYS_ulimit:
				if (rv1 & 0xff000000)
					fmt = "= 0x%.8X";
				break;
			case SYS_signal:
				if (raw)
					/* EMPTY */;
				else if (rv1 == (int)SIG_DFL)
					fmt = "= SIG_DFL";
				else if (rv1 == (int)SIG_IGN)
					fmt = "= SIG_IGN";
				else if (rv1 == (int)SIG_HOLD)
					fmt = "= SIG_HOLD";
				break;
			case SYS_sigtimedwait:
				if (raw)
					/* EMPTY */;
				else if ((fmt = rawsigname(rv1)) != NULL) {
					rv1 = (int)fmt;	/* filthy */
					fmt = "= %s";
				}
				break;
			}

			if (fmt == NULL) {
				switch (stp->rval[0]) {
				case HEX:
					fmt = "= 0x%.8X";
					break;
				case HHX:
					fmt = "= 0x%.4X";
					break;
				case OCT:
					fmt = "= %#o";
					break;
				default:
					fmt = "= %d";
					break;
				}
			}

			(void) printf(fmt, rv1, rv2);

			switch (stp->rval[1]) {
			case NOV:
				fmt = NULL;
				break;
			case HEX:
				fmt = " [0x%.8X]";
				break;
			case HHX:
				fmt = " [0x%.4X]";
				break;
			case OCT:
				fmt = " [%#o]";
				break;
			default:
				fmt = " [%d]";
				break;
			}

			if (fmt != NULL)
				(void) printf(fmt, rv2);
			(void) fputc('\n', stdout);
		}

		if (what == SYS_fork ||
		    what == SYS_vfork ||
		    what == SYS_fork1) {
			if (r1 == 0)		/* child was created */
				child = r0;
			else if (istraced)	/* this is the child */
				Cp->syscount[what]--;
		}
	}

#define	ISREAD(code) \
	((code) == SYS_read || (code) == SYS_pread || (code) == SYS_pread64 || \
	(code) == SYS_recv || (code) == SYS_recvfrom)
#define	ISWRITE(code) \
	((code) == SYS_write || (code) == SYS_pwrite || \
	(code) == SYS_pwrite64 || (code) == SYS_send || (code) == SYS_sendto)

	if (!cflag && istraced) {
		int fdp1 = arg0+1;	/* read()/write() filedescriptor + 1 */

		if (raw) {
			if (what != SYS_exec && what != SYS_execve)
				showpaths(stp);
			if (ISREAD(what) || ISWRITE(what)) {
				if (iob_buf[0] != '\0')
					(void) printf("%s     0x%.8X: %s\n",
						pname, sys_args[1], iob_buf);
			}
		}

		/*
		 * Show buffer contents for read()/pread() or write()/pwrite().
		 * IOBSIZE bytes have already been shown;
		 * don't show them again unless there's more.
		 */
		if ((ISREAD(what) && Errno == 0 &&
		    prismember(&readfd, fdp1)) ||
		    (ISWRITE(what) && prismember(&writefd, fdp1))) {
			int nb = ISWRITE(what) ? sys_args[2] : r0;

			if (nb > IOBSIZE) {
				/* enter region of lengthy output */
				if (nb > BUFSIZ/4)
					Eserialize();

				showbuffer(Pr, (long)sys_args[1], nb);

				/* exit region of lengthy output */
				if (nb > BUFSIZ/4)
					Xserialize();
			}
		}
#undef	ISREAD
#undef	ISWRITE
		/*
		 * Do verbose interpretation if requested.
		 * If buffer contents for read or write have been requested and
		 * this is a readv() or writev(), force verbose interpretation.
		 */
/* XXX add SYS_sendmsg and SYS_recvmsg */
		if (prismember(&verbose, what) ||
		    (what == SYS_readv && Errno == 0 &&
		    prismember(&readfd, fdp1)) ||
		    (what == SYS_writev && prismember(&writefd, fdp1)))
			expound(Pr, r0, raw);
	}
}

static void
showpaths(const struct systable *stp)
{
	int i;

	for (i = 0; i < sys_nargs; i++) {
		if ((stp->arg[i] == STG) ||
		    (stp->arg[i] == RST && !Errno) ||
		    (stp->arg[i] == RLK && !Errno && Rval1 > 0)) {
			long addr = (long)sys_args[i];
			int maxleng =
			    (stp->arg[i] == RLK)? Rval1 : 400;
			char *s;

			if (i == 0 && sys_valid)	/* already fetched */
				s = sys_path;
			else
				s = fetchstring(addr,
				    maxleng > 400 ? 400 : maxleng);

			if (s != (char *)NULL)
				(void) printf("%s     0x%.8lX: \"%s\"\n",
					pname, addr, s);
		}
	}
}

/* display arguments to successful exec() */
static void
showargs(process_t *Pr, int raw)
{
	int nargs;
	char *ap;

	length = 0;

#if i386
	(void) Pgetareg(Pr, R_SP);
	ap = (char *)Pr->REG[R_SP];
	if (Pread(Pr, (long)ap, (char *)&nargs, sizeof (nargs))
	    != sizeof (nargs)) {
		(void) printf("\n%s\t*** Bad argument list? ***\n", pname);
		return;
	}
	ap += sizeof (int);
#endif /* i386 */

#if sparc
	(void) Pgetareg(Pr, R_SP);
	ap = (char *)(Pr->REG[R_SP]) + 16 * sizeof (int);
	if (Pread(Pr, (long)ap, (char *)&nargs, sizeof (nargs))
	    != sizeof (nargs)) {
		(void) printf("\n%s\t*** Bad argument list? ***\n", pname);
		return;
	}
	ap += sizeof (int);
#endif /* !i386 */

#if __ppc
	(void) Pgetareg(Pr, R_R3);
	(void) Pgetareg(Pr, R_R4);
	nargs = Pr->REG[R_R3];
	ap = (char *)Pr->REG[R_R4];
#endif

	(void) printf("  argc = %d\n", nargs);
	if (raw)
		showpaths(&systable[SYS_exec]);

	show_cred(Pr, FALSE);

	if (aflag || eflag) {		/* dump args or environment */

		/* enter region of (potentially) lengthy output */
		Eserialize();

		if (aflag)		/* dump the argument list */
			dumpargs(Pr, ap, "argv:");
#if __ppc
		(void) Pgetareg(Pr, R_R5);
		ap = (char *)Pr->REG[R_R5];
#else
		ap += (nargs+1) * sizeof (char *);
#endif
		if (eflag)		/* dump the environment */
			dumpargs(Pr, ap, "envp:");

		/* exit region of lengthy output */
		Xserialize();
	}
}

static void
dumpargs(process_t *Pr, char *ap, const char *str)
{
	char *string;
	unsigned int leng = 0;
	char *arg;
	char badaddr[32];

	if (interrupt)
		return;

	if (pname[0])
		(void) fputs(pname, stdout);
	(void) fputc(' ', stdout);
	(void) fputs(str, stdout);
	leng += 1 + strlen(str);

	while (!interrupt) {
		if (Pread(Pr, (long)ap, (char *)&arg, sizeof (char *))
		    != sizeof (char *)) {
			(void) printf("\n%s\t*** Bad argument list? ***\n",
				pname);
			return;
		}
		ap += sizeof (char *);

		if (arg == (char *)NULL)
			break;
		string = fetchstring((long)arg, 400);
		if (string == NULL) {
			(void) sprintf(badaddr, "BadAddress:0x%.8lX",
				(long)arg);
			string = badaddr;
		}
		if ((leng += strlen(string)) < 63) {
			(void) fputc(' ', stdout);
			leng++;
		} else {
			(void) fputc('\n', stdout);
			leng = 0;
			if (pname[0])
				(void) fputs(pname, stdout);
			(void) fputs("  ", stdout);
			leng += 2 + strlen(string);
		}
		(void) fputs(string, stdout);
	}
	(void) fputc('\n', stdout);
}

/* display contents of read() or write() buffer */
void
showbuffer(process_t *Pr, long offset, int count)
{
	char buffer[320];
	int nbytes;
	char *buf;
	int n;

	while (count > 0 && !interrupt) {
		nbytes = (count < sizeof (buffer))? count : sizeof (buffer);
		if ((nbytes = Pread(Pr, offset, buffer, nbytes)) <= 0)
			break;
		count -= nbytes;
		offset += nbytes;
		buf = buffer;
		while (nbytes > 0 && !interrupt) {
			char obuf[65];

			n = (nbytes < 32)? nbytes : 32;
			showbytes(buf, n, obuf);

			if (pname[0])
				(void) fputs(pname, stdout);
			(void) fputs("  ", stdout);
			(void) fputs(obuf, stdout);
			(void) fputc('\n', stdout);
			nbytes -= n;
			buf += n;
		}
	}
}

void
showbytes(const char *buf, int n, char *obuf)
{
	int c;

	while (--n >= 0) {
		int c1 = '\\';
		int c2;

		switch (c = (*buf++ & 0xff)) {
		case '\0':
			c2 = '0';
			break;
		case '\b':
			c2 = 'b';
			break;
		case '\t':
			c2 = 't';
			break;
		case '\n':
			c2 = 'n';
			break;
		case '\v':
			c2 = 'v';
			break;
		case '\f':
			c2 = 'f';
			break;
		case '\r':
			c2 = 'r';
			break;
		default:
			if (isprint(c)) {
				c1 = ' ';
				c2 = c;
			} else {
				c1 = c>>4;
				c1 += (c1 < 10)? '0' : 'A'-10;
				c2 = c&0xf;
				c2 += (c2 < 10)? '0' : 'A'-10;
			}
			break;
		}
		*obuf++ = (char)c1;
		*obuf++ = (char)c2;
	}

	*obuf = '\0';
}
