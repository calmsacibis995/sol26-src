/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)syslogd.c 1.40	96/06/26 SMI"


/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages.
 * DEFSPRI -- the default priority for kernel messages.
 * NINLOGS -- the maximum number of inputs we can receive messages from.
 *
 */

#define	NINLOGS		10		/* max number of inputs */
#define	MAXLINE		1024		/* maximum line length */
#define	DEFUPRI		(LOG_USER|LOG_INFO)
#define	DEFSPRI		(LOG_KERN|LOG_CRIT)
#define	MARKCOUNT	3		/* ratio of minor to major marks */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <netconfig.h>
#include <netdir.h>
#include <pwd.h>
#include <tiuser.h>
#include <utmp.h>
#include <unistd.h>
#include <limits.h>
#include <wchar.h>
#include <wctype.h>
#include <widec.h>
#include <locale.h>

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/stropts.h>
#include <sys/syslog.h>
#include <sys/strlog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/poll.h>
#include <sys/wait.h>

static char	*LogName = "/dev/log";
static char	*ConfFile = "/etc/syslog.conf";
static char	*PidFile = "/etc/syslog.pid";
static char	ctty[] = "/dev/console";

#define	dprintf		if (Debug) (void) printf

#define	UNAMESZ		8	/* length of a login name */
#define	UDEVSZ		12	/* length of a login device name */
#define	MAXUNAMES	20	/* maximum number of user names */

#define	NOPRI		0x10	/* the "no priority" priority */
#define	LOG_MARK	(LOG_NFACILITIES << 3)	/* mark "facility" */

/*
 * Flags to logmsg().
 */

#define	IGN_CONS	0x001	/* don't print on console */
#define	SYNC_FILE	0x002	/* do fsync on file after printing */
#define	NOCOPY		0x004	/* don't suppress duplicate messages */
#define	ADDDATE		0x008	/* add a date to the message */
#define	MARK		0x010	/* this message is a mark */

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	union {
		char	f_uname[MAXUNAMES][SYS_NMLN];
		struct {
			char	f_hname[SYS_NMLN];
			struct netbuf	f_addr;
		} f_forw;		/* forwarding address */
		char	f_fname[MAXPATHLEN];
	} f_un;
};

typedef	struct	host_list {
	int	hl_cnt;
	char	**hl_hosts;
} HOST_LIST;


/*
 * write lock on whole pid file
 */
static flock_t flk = {F_WRLCK, 0, 0, 0};

/* values for f_type */
#define	F_UNUSED	0		/* unused entry */
#define	F_FILE		1		/* regular file */
#define	F_TTY		2		/* terminal */
#define	F_CONSOLE	3		/* console terminal */
#define	F_FORW		4		/* remote machine */
#define	F_USERS		5		/* list of users */
#define	F_WALL		6		/* everyone logged on */

static char	*TypeNames[7] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL"
};

static struct filed	fallback[2];
static struct filed	*Files;

static int	nlogs;
static int	Debug;			/* debug flag */
static HOST_LIST	hlist;		/* host logging current message */
static HOST_LIST	LocalHostName;	/* our hostname */
static HOST_LIST	PrevHost;	/* previous host */
static char	curaddr[SYS_NMLN];	/* numeric address of last sender */
static char	PrevLine[MAXLINE + 1];	/* copy to supress repeats */
static int	PrevFlags;
static int	PrevPri;
static int	PrevCount = 0;		/* number of times seen */
static int	FlushTimer;		/* timer for flushing messages */
static int	Initialized = 0;	/* set when initialized */
static int	MarkInterval = 20;	/* interval between marks in minutes */
static int	Marking = 0;		/* non-zero if marking some file */
static int	MarkTimer;		/* timer for marks */
static int	Ninputs = 0;		/* number of inputs */
static int	curalarm = 0;

static struct pollfd Pfd[NINLOGS];
static struct netconfig Ncf[NINLOGS];
static struct netbuf *Myaddrs[NINLOGS];
static struct t_unitdata *Udp[NINLOGS];
static struct t_uderr *Errp[NINLOGS];

static void usage(), untty(), printsys(), printline(), getnets(), init();
static void logmsg(), wallmsg(), doalarm(), flushmsg(), logerror();
static void die(), cfline(), add(), setalarm();
static HOST_LIST *cvthname();
static int decode(), logforward(), amiloghost(), same();
static void filter_string(char *, char *);

extern	int errno;
extern	int t_errno, t_nerr;
extern	char *t_errlist[];
extern	char *optarg;
extern	char *ctime();
extern	time_t time();

#define	bzero(ADDR, SIZE)	(void) memset((ADDR), 0, (SIZE))
#define	bcopy(FROM, TO, SIZE)	(void) memcpy((TO), (FROM), (SIZE))

int
main(argc, argv)
	int argc;
	char **argv;
{
	register int i;
	int funix;
	int	fd;
	struct utsname *up;
	struct strioctl str;
	char line[MAXLINE + 1];
	char *uap;
	char *pstr;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"  /* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((i = getopt(argc, argv, "df:p:m:")) != EOF) {
		switch (i) {
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;

		case 'd':		/* debug */
			Debug++;
			break;

		case 'p':		/* path */
			LogName = optarg;
			break;

		case 'm':		/* mark interval */
			for (pstr = optarg; *pstr; pstr++) {
				if (! (isdigit(*pstr))) {
					(void) fprintf(stderr,
					    "Illegal interval\n");
					usage();
				}
			}
			MarkInterval = atoi(optarg);
			if (MarkInterval < 1 || MarkInterval > INT_MAX) {
				(void) fprintf(stderr,
				    "Interval must be between 1 and %d\n",
				    INT_MAX);
				usage();
			}
			break;

		default:
			usage();
		}
	}
	if (optind < argc)
		usage();

	if (!Debug) {
		if (fork())
			exit(0);
		for (i = 0; i < 10; i++)
			(void) close(i);
		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		untty();
	}
	up = (struct utsname *)malloc(sizeof (struct utsname));
	(void) uname(up);
	LocalHostName.hl_cnt = 1;
	LocalHostName.hl_hosts = (char **) malloc(sizeof (char *));
	LocalHostName.hl_hosts[0] = strdup(up->nodename);
	(void) free(up);
	PrevHost.hl_cnt = 1;
	PrevHost.hl_hosts = (char **) malloc(sizeof (char *));
	PrevHost.hl_hosts[0] = (char *) malloc(SYS_NMLN);
	(void) signal(SIGTERM, die);
	(void) signal(SIGINT, Debug ? die : SIG_IGN);
	(void) signal(SIGQUIT, Debug ? die : SIG_IGN);
	(void) signal(SIGCHLD, SIG_IGN);
	(void) signal(SIGALRM, doalarm);
	/* in case one of our log files is a named pipe */
	(void) signal(SIGPIPE, SIG_IGN);

	if ((funix = open(LogName, O_RDONLY)) < 0) {
		(void) sprintf(line, "cannot open %s", LogName);
		logerror(line);
		dprintf("cannot create %s (%d)\n", LogName, errno);
		die(0);
	}
	str.ic_cmd = I_CONSLOG;
	str.ic_timout = 0;
	str.ic_len = 0;
	str.ic_dp = NULL;
	if (ioctl(funix, I_STR, &str) < 0) {
		logerror("cannot register to log console messages");
		dprintf("cannot register to log console messages (%d)\n",
		    errno);
		die(0);
	}
	Pfd[Ninputs].fd = funix;
	Pfd[Ninputs].events = POLLIN;
	Ninputs++;
	getnets();
	/* tuck my process id away */
	if ((fd = open(PidFile, O_RDWR | O_CREAT, 0644)) >= 0) {
		/*
		 * syslogd holds the write lock to indicate
		 * that it is active.  If it dies, the lock
		 * will go away.  This enables syslog to determine
		 * if the syslogd is really running
		 *
		 * try and aquire the write lock
		 */
		while (fcntl(fd, F_SETLK, &flk) == -1) {
			struct flock ftst;
			/*
			 * oops, didn't get it.  Check to make sure
			 * that some other syslogd doesn't own it
			 */
			ftst.l_type = F_WRLCK;
			ftst.l_whence = 0;
			ftst.l_start = 0;
			ftst.l_len = 0;
			(void) fcntl(fd, F_GETLK, &ftst);
			if (ftst.l_type == F_WRLCK) {
				(void) sprintf(line, "syslogd pid %ld already \
running. Cannot start another syslogd %ld", ftst.l_pid, getpid());
				logerror(line);
				exit(1);
			}
		}

		/* fix for 1088721 - file should be 0644, not 0666 */
		(void) fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		(void) sprintf(line, "%ld\n", getpid());
		if (write(fd, line, strlen(line) + 1) < 0) {
			(void) sprintf(line, "Cannot write pid file (%s)",
			    PidFile);
			logerror(line);
			dprintf("cannot write pid file %s (%d)\n",
			    PidFile, errno);
		}
		/*
		 * Must reaquire the lock so that pid info gets
		 * written to the PidFile
		 */
		if (fcntl(fd, F_SETLKW, &flk) == -1) {
			(void) sprintf(line, "cannot apply write lock to \
pid file %s", PidFile);
			logerror(line);
			dprintf("cannot lock pid file %s (%d)\n",
			    PidFile, errno);
		}
	} else {
		(void) sprintf(line, "cannot create pid file %s", PidFile);
		logerror(line);
		dprintf("cannot create pid file %s (%d)\n", PidFile, errno);
	}

	dprintf("off & running....\n");

	init();

	/*
	 * Use sigset() instead of signal() to make sure  signal is not
	 * temporarily marked as not caught (the old SYSV behaviour that
	 * signal() gives).
	 */

	(void) sigset(SIGHUP, init);

	for (;;) {
		int nfds;
		struct strbuf ctl;
		struct strbuf dat;
		int flags = 0;
		struct log_ctl hdr;
		struct t_unitdata *udp;
		struct t_uderr *errp;
		char buf[MAXLINE+1];
		char *lastline;

		errno = 0;
		t_errno = 0;
		nfds = poll(Pfd, Ninputs, -1);
		dprintf("got a message (%d, %#x)\n", nfds, Ninputs);
		if (nfds == 0)
			continue;

		if (nfds < 0) {
			if (errno != EINTR)
				logerror("poll");
			continue;
		}
		if (Pfd[0].revents & POLLIN) {
			dat.maxlen = MAXLINE;
			dat.buf = buf;
			ctl.maxlen = sizeof (struct log_ctl);
			ctl.buf = (caddr_t)&hdr;

			while ((i = getmsg(Pfd[0].fd, &ctl, &dat, &flags))
					== MOREDATA) {

				lastline = &dat.buf[dat.len];
				*lastline = '\0';

				while (*lastline != '\n' && lastline != buf)
					lastline--;
				if (lastline != buf)
					*lastline++ = '\0';

				printsys(&hdr, buf);

				if (lastline != buf) {
					(void) strcpy(buf, lastline);
					dat.maxlen = MAXLINE - strlen(buf);
					dat.buf = &buf[strlen(buf)];
				} else {
					dat.maxlen = MAXLINE;
					dat.buf = buf;
				}
			}

			if (i == 0 && dat.len > 0) {
				dat.buf[dat.len] = '\0';
				printsys(&hdr, buf);
				nfds--;
			} else if (i < 0 && errno != EINTR) {
				logerror("klog");
				(void) close(Pfd[0].fd);
				Pfd[0].fd = -1;
				nfds--;
			}
		} else if (Pfd[0].revents & (POLLNVAL|POLLHUP|POLLERR)) {
				logerror("klog");
				(void) close(Pfd[0].fd);
				Pfd[0].fd = -1;
		}
		i = 1;
		while (nfds > 0 && i < NINLOGS) {
			if (Pfd[i].revents & POLLIN) {
				udp = Udp[i];
				udp->udata.buf = buf;
				udp->udata.maxlen = MAXLINE;
				udp->udata.len = 0;
				flags = 0;
				if (t_rcvudata(Pfd[i].fd, udp, &flags) < 0) {
					errp = Errp[i];
					if (t_errno == TLOOK) {
						if (t_rcvuderr(Pfd[i].fd,
						    errp) < 0) {
							logerror("t_rcvuderr");
							t_close(Pfd[i].fd);
							Pfd[i].fd = -1;
						}
					} else {
						logerror("t_rcvudata");
						t_close(Pfd[i].fd);
						Pfd[i].fd = -1;
					}
					nfds--;
					continue;
				}
				nfds--;
				if (udp->udata.len > 0) {
					extern HOST_LIST *cvthname();
					struct netconfig *ncp;

					/* Force EOL in buffer */
					buf[udp->udata.len] = '\0';
					ncp = (struct netconfig *)&Ncf[i];
					if ((uap = taddr2uaddr(ncp,
					    &udp->addr)) != (char *)NULL) {
						dprintf("received message "
						    "from %s\n",uap);
						strcpy(curaddr, uap);
					} else
						strcpy(curaddr,"<unknown>");
					printline(cvthname(&udp->addr, ncp),
						&udp->udata);
					free(uap);
				}
			} else if (Pfd[i].revents &
				(POLLNVAL|POLLHUP|POLLERR)) {
				logerror("POLLNVAL|POLLHUP|POLLERR");
				(void) t_close(Pfd[i].fd);
				Pfd[i].fd = -1;
				nfds--;
			}
			i++;
		}
	}
	/*NOTREACHED*/
}

static void
usage()
{
	(void) fprintf(stderr,
	    "usage: syslogd [-d] [-mmarkinterval] [-ppath] [-fconffile]\n");
	exit(1);
}

static void
untty()
{
	if (!Debug)
		(void) setsid();
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
static void
printline(hlp, nbp)
	HOST_LIST *hlp;
	struct netbuf *nbp;
{
	register char *p, *q;
	register int i;
	register int c;
	int pri;
	char line[MAXLINE + 1];

	/* test for special codes */
	pri = DEFUPRI;
	p = nbp->buf;

	if (*p == '<' && isdigit(*(p+1))) {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
		if (pri <= 0 || pri >= (LOG_NFACILITIES << 3))
			pri = DEFUPRI;
	}

	/* don't allow users to log kernel messages */
	if ((pri & LOG_PRIMASK) == LOG_KERN)
		pri |= LOG_USER;

	q = line;
	i = 0;
	while ((c = *p++) != '\0' && c != '\n' && i < MAXLINE) {
			*q++ = c;
			i++;
	}
	*q = '\0';
	logmsg(hlp, pri, line, 0);

	/* free up dynamically allocated hostname storage */
	dprintf("Freeing storage for %d hostnames\n", hlp->hl_cnt);
	for (i = 0; i < hlp->hl_cnt; i++)
		free(hlp->hl_hosts[i]);
	free(hlp->hl_hosts);
}

static void
printsys(lp, msg)
	struct log_ctl *lp;
	char *msg;
{
	register char *p, *q;
	register int c;
	register int i;
	int flags;
	time_t now;
	char line[MAXLINE + 1];

	(void) time(&now);
	flags = SYNC_FILE;	/* fsync file after write */

	/*
	 * If "mid" and "sid" are 0 then this message came to us
	 * by way of writekmsg() in the kernel and has already
	 * been printed on the console.
	 *
	 * NOTE:  This is only a convention and does not gaurantee
	 * that other callers have avoided using 0 for these values.
	 */
	if (lp->mid == 0 && lp->sid == 0)
		flags |= IGN_CONS;
	for (p = msg; *p != '\0'; ) {

		/* extract facility */
		if ((lp->pri & LOG_FACMASK) == LOG_KERN)
			(void) sprintf(line, "%.15s unix: ", ctime(&now) + 4);
		else
			(void) sprintf(line, "");
		q = line + strlen(line);
		i = 0;
		while (*p != '\0' && (c = *p++) != '\n' && i < MAXLINE) {
			*q++ = c;
			i++;
		}
		*q = '\0';
		if (i != 0)
			logmsg(&LocalHostName, lp->pri, line, flags);
	}
}

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
static void
logmsg(hlp, pri, msg, flags)
	HOST_LIST *hlp;
	int pri;
	char *msg;
	int flags;
{
	register struct filed *f;
	register int l;
	register char *cp;
	int fac, prilev;
	time_t now;
	sigset_t osigs, sigs;
	char *text;
	struct t_unitdata ud;
	char line[MAXLINE*2];	/* watch for overflow */
	char line2[MAXLINE*2];
	char filtered[MAXLINE*2];
	char *eomp, *eomp2;
	char *from;
	int	i;
	int	forwardingloop = 0;
	char *errmsg =
	    " %s to %s forwarding loop detected, message not forwarded\n";

	if (hlp == (HOST_LIST *)NULL)
		from = curaddr;
	else
		from = hlp->hl_hosts[0];

	dprintf("logmsg: pri %o, flags %x, from %s, msg %s\n", pri, flags,
	    from, msg);

	(void) sigemptyset(&osigs);
	(void) sigemptyset(&sigs);
	(void) sigprocmask(SIG_BLOCK, NULL, &osigs);
	sigs = osigs;
	(void) sigaddset(&sigs, SIGALRM);
	(void) sigaddset(&sigs, SIGHUP);
	(void) sigprocmask(SIG_SETMASK, &sigs, NULL);

	/*
	 * Check to see if msg looks non-standard.
	 */
	if ((int) strlen(msg) < 16 || msg[3] != ' ' || msg[6] != ' ' ||
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
		flags |= ADDDATE;

	if (!(flags & NOCOPY)) {
		if (flags & (ADDDATE|MARK))
			flushmsg();
		else if (!(strcmp(msg + 16, PrevLine + 16))) {
			/* we found a match, update the time */
			(void) strncpy(PrevLine, msg, 15);
			if (PrevCount == 0) {
				FlushTimer = MarkInterval * 60 / MARKCOUNT;
				setalarm(FlushTimer);
			}
			PrevCount++;
			(void) sigprocmask(SIG_SETMASK, &osigs, NULL);
			return;
		} else {
			/* new line, save it */
			flushmsg();
			(void) strcpy(PrevLine, msg);
			(void) strcpy(PrevHost.hl_hosts[0], from);
			PrevFlags = flags;
			PrevPri = pri;
		}
	}

	(void) time(&now);
	cp = line;
	if (flags & ADDDATE)
		(void) strncpy(cp, ctime(&now) + 4, 15);
	else
		(void) strncpy(cp, msg, 15);
	line[15] = '\0';
	(void) strcat(cp, " ");
	(void) strcat(cp, from);
	(void) strcat(cp, " ");
	text = cp + strlen(cp);
	if (flags & ADDDATE)
		(void) strcat(cp, msg);
	else
		(void) strcat(cp, msg+16);

	/* extract facility and priority level */
	fac = (pri & LOG_FACMASK) >> 3;
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	prilev = pri & LOG_PRIMASK;

	/* filter the message in preparation for writing it */
	/* save the original for possible forwarding */
	filter_string(cp, filtered);
	eomp = filtered + strlen(filtered);

	/* log the message to the particular outputs */
	if (!Initialized) {
		int cfd = open(ctty, O_WRONLY|O_NOCTTY);

		if (cfd >= 0) {
			untty();
			/* write filtered message */
			(void) strcat(filtered, "\r\n");
			(void) write(cfd, filtered, strlen(filtered));
			(void) close(cfd);
		}
		(void) sigprocmask(SIG_SETMASK, &osigs, NULL);
		return;
	}

	for (f = Files; f < &Files[nlogs]; f++) {

		/* skip messages that are incorrect priority */
		if (f->f_pmask[fac] < (unsigned)prilev ||
		    f->f_pmask[fac] == NOPRI)
			continue;

		/* don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) <
			(MarkInterval * 60 / 2))
			continue;

		dprintf("Logging to %s", TypeNames[f->f_type]);
		f->f_time = now;
		errno = 0;
		t_errno = 0;
		switch (f->f_type) {
		case F_UNUSED:
			dprintf("\n");
			break;

		case F_FORW:
			/*
			 * can not forward message if we do
			 * not have a host to forward to
			 */
			if (hlp == (HOST_LIST *)NULL)
				break;

			/*
			 * a forwarding loop is created on machines with
			 * multiple interfaces because the network address
			 * of the sender is different to the reciever even
			 * though it is the same machine. Instead, if the
			 * hostname the source and target are the same the
			 * message if thrown away
			 */
			for (i = 0; i < hlp->hl_cnt; i++) {
				if (strcmp(hlp->hl_hosts[i],
				    f->f_un.f_forw.f_hname) == 0) {
					dprintf(errmsg, f->f_un.f_forw.f_hname,
					    hlp->hl_hosts[i]);
					forwardingloop = 1;
					break;
				}
			}

			if (forwardingloop == 1)
				break;

			dprintf(" %s\n", f->f_un.f_forw.f_hname);
			(void) sprintf(line2, "<%d>%.15s %s", pri, cp, text);
			l = strlen(line2);
			if (l > MAXLINE)
				l = MAXLINE;
			ud.opt.buf = NULL;
			ud.opt.len = 0;
			ud.udata.buf = line2;
			ud.udata.len = l;
			ud.addr.maxlen = f->f_un.f_forw.f_addr.maxlen;
			ud.addr.buf = f->f_un.f_forw.f_addr.buf;
			ud.addr.len = f->f_un.f_forw.f_addr.len;
			if (t_sndudata(f->f_file, &ud) < 0) {
				logerror("t_sndudata");
				(void) t_close(f->f_file);
				f->f_type = F_UNUSED;
			}
			break;

		case F_CONSOLE:
			if (flags & IGN_CONS) {
				dprintf(" (ignored)\n");
				break;
			}
			/*FALLTHROUGH*/
		case F_TTY:
		case F_FILE:
			dprintf(" %s\n", f->f_un.f_fname);
			if (f->f_type != F_FILE) {
				(void) strcpy(eomp, "\r\n");
			} else {
				if ((eomp2 = strchr(filtered, '\r')) != NULL)
					(void) strcpy(eomp2, "\n");
				else
					(void) strcpy(eomp, "\n");
			}
			/* write filtered message */
			if (write(f->f_file, filtered, strlen(filtered)) < 0) {
				int e = errno;
				(void) close(f->f_file);
				/*
				 * Check for EBADF on TTY's due to vhangup() XXX
				 */
				if (e == EBADF && f->f_type != F_FILE) {
					f->f_file = open(f->f_un.f_fname,
					    O_WRONLY|O_APPEND|O_NOCTTY);
					if (f->f_file < 0) {
						f->f_type = F_UNUSED;
						logerror(f->f_un.f_fname);
					}
					untty();
				} else {
					f->f_type = F_UNUSED;
					errno = e;
					logerror(f->f_un.f_fname);
				}
			} else if (flags & SYNC_FILE)
				(void) fsync(f->f_file);
			break;

		case F_USERS:
		case F_WALL:
			dprintf("\n");
			(void) strcpy(eomp, "\r\n");
			/* write filtered message */
			wallmsg(f, from, filtered);
			break;
		}
	}
	(void) sigprocmask(SIG_SETMASK, &osigs, NULL);
}


/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
static void
wallmsg(f, from, msg)
	register struct filed *f;
	char *from;
	register char *msg;
{
	register int i;
	register char *cp;
	int ttyf, len;
	static int reenter = 0;
	struct utmp *utp;
	time_t now;
	char dev[100];
	char line[MAXLINE*2];
	struct stat statbuf;

	dprintf("wallmsg called\n");

	if (reenter++)
		return;

	if (access(UTMP_FILE, R_OK) != 0 || stat(UTMP_FILE, &statbuf) != 0) {
		logerror(UTMP_FILE);
		reenter = 0;
		return;
	} else if (statbuf.st_uid != 0 || (statbuf.st_mode & 07777) != 0644) {
		(void) sprintf(line, "%s %s%s%s%s", UTMP_FILE,
	"not owned by root or\n",
	"not mode 644.  This file must be owned by root and not writable by\n",
	"anyone other than root.  This alert is being dropped because of\n",
	"this problem.");
		logerror(line);
		reenter = 0;
		return;
	}

	if (f->f_type == F_WALL) {
		(void) time(&now);
		(void) sprintf(line,
		    "\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		    from, ctime(&now));
		(void) strcat(line, msg+16);
		cp = line;
		len = strlen(line);
	} else {
		cp = msg;
		len = strlen(msg);
	}

	/* scan the user login file */
	setutent();
	while ((utp = getutent()) != NULL) {
		/* is this slot used? */
		if (utp->ut_name[0] == '\0' || utp->ut_line[0] == '\0' ||
		    utp->ut_type == DEAD_PROCESS)
			continue;

		/* should we send the message to this user? */
		if (f->f_type == F_USERS) {
			for (i = 0; i < MAXUNAMES; i++) {
				if (!f->f_un.f_uname[i][0]) {
					i = MAXUNAMES;
					break;
				}
				if (strncmp(f->f_un.f_uname[i], utp->ut_name,
				    UNAMESZ) == 0)
					break;
			}
			if (i >= MAXUNAMES)
				continue;
		}

		/* compute the device name */
		(void) strcpy(dev, "/dev/");
		(void) strncat(dev, utp->ut_line, UDEVSZ);
		dprintf("write to '%s'\n", dev);

		/*
		 * Might as well fork instead of using nonblocking I/O
		 * and doing notty().
		 */
		if (fork() == 0) {
			char errorbuf[100];

			(void) signal(SIGALRM, SIG_DFL);
			(void) alarm(30);
			/* open the terminal */
			ttyf = open(dev, O_WRONLY|O_NOCTTY);
			if (ttyf >= 0) {
			    struct stat statb;
			    struct passwd *pwent;

			    if (fstat(ttyf, &statb) != 0) {
					dprintf("Can't stat '%s'\n", dev);
					(void) sprintf(errorbuf,
					    "Can't stat '%s'", dev);
					errno = 0;
					logerror(errorbuf);
			    } else if (!(statb.st_mode & S_IWRITE)) {
					dprintf("Can't write to '%s'\n", dev);
			    } else if (! isatty(ttyf)) {
					dprintf("'%s' not a tty\n", dev);
					(void) sprintf(errorbuf,
					    "'%s' not a tty", dev);
					errno = 0;
					logerror(errorbuf);
			    } else if ((pwent = getpwuid(statb.st_uid)) ==
						NULL) {
					/*CSTYLED*/
					dprintf("Can't determine owner of '%s'\n", dev);
					(void) sprintf(errorbuf,
					    "Can't determine owner of '%s'",
					    dev);
					errno = 0;
					logerror(errorbuf);
			    } else if (strncmp(pwent->pw_name, utp->ut_name,
					UNAMESZ) != 0) {
					dprintf("Bad terminal owner '%s'\n",
					    dev);
					(void) sprintf(errorbuf,
					    "%s %s owns '%s' %s %.*s",
					    "Bad terminal owner;",
					    pwent->pw_name, dev,
					    "but utmp says", UNAMESZ,
					    utp->ut_name);
					errno = 0;
					logerror(errorbuf);
			    } else if (write(ttyf, cp, len) != len) {
					dprintf("Write failed to '%s'\n", dev);
					(void) sprintf(errorbuf,
					    "Write failed to '%s'", dev);
					errno = 0;
					logerror(errorbuf);
			    }
			} else {
			    dprintf("Can't open '%s'\n", dev);
			}
			exit(0);
		}
	}
	/* close the user login file */
	endutent();
	reenter = 0;
}


/*
 * Return a printable representation of a host address. If unable to
 * look up hostname, format the numeric address for display instead.
 */
HOST_LIST *
cvthname(nbp,ncp)
	register struct netbuf *nbp;
	register struct netconfig *ncp;
{
	register int i;
	register HOST_LIST *h = &hlist;
	struct nd_hostservlist *hsp;
	struct nd_hostserv	*hspp;

	if (ncp->nc_semantics == NC_TPI_CLTS) {
		if (netdir_getbyaddr(ncp, &hsp, nbp) == 0) {
			if (hsp->h_cnt > 0) {
				hspp = hsp->h_hostservs;
				h->hl_cnt = hsp->h_cnt;
				h->hl_hosts = (char **) malloc(sizeof
				    (char *) * (h->hl_cnt));
				dprintf("Found %d hostnames\n", h->hl_cnt);
				for (i = 0; i < h->hl_cnt; i++) {
					h->hl_hosts[i] = (char *)
					    malloc(sizeof (char) *
						strlen(hspp->h_host));
					strcpy(h->hl_hosts[i], hspp->h_host);
					hspp++;
				}
			} else {
				netdir_free((void *)hsp, ND_HOSTSERVLIST);
				return ((HOST_LIST *) NULL);	
			}
			netdir_free((void *)hsp, ND_HOSTSERVLIST);
		} else { /* unknown address */
			h->hl_cnt = 1;
			h->hl_hosts = (char **) malloc(sizeof (char *));
			h->hl_hosts[0] = (char *) malloc(sizeof (char) *
			    (strlen(curaddr) + 3));
			sprintf(h->hl_hosts[0], "[%s]", curaddr);
			dprintf("Hostname lookup failed - using address"
			    " %s instead\n", h->hl_hosts[0]);
		}
	}
	return (h);
}


/*
 * If the alarm is more than "secs" seconds in the future, set it to "secs"
 * seconds.  Adjust any timers by subtracting the time elapsed since the last
 * "alarm" call.
 */
static void
setalarm(secs)
	int secs;
{
	register int alarmval;
	register int elapsed;

	alarmval = alarm((unsigned)0);
	elapsed = curalarm - alarmval;
	dprintf("setalarm: curalarm %d alarmval %d\n", curalarm, alarmval);
	if (PrevCount > 0)
		FlushTimer -= elapsed;
	if (Marking)
		MarkTimer -= elapsed;
	if (secs < alarmval || alarmval == 0)
		curalarm = secs;
	else
		curalarm = alarmval;
	(void) alarm((unsigned)curalarm);
	dprintf("Next alarm in %d seconds\n", curalarm);
}

/*
 * SIGALRM catcher: adjust the timers, call the appropriate timeout routines,
 * and set up the next alarm.
 */
static void
doalarm()
{
	dprintf("doalarm: FlushTimer %d MarkTimer %d curalarm %d\n",
	    FlushTimer, MarkTimer, curalarm);
	if (PrevCount > 0) {
		FlushTimer -= curalarm;
		if (FlushTimer <= 0)
			flushmsg();
	}
	if (Marking) {
		MarkTimer -= curalarm;
		if (MarkTimer <= 0) {
			logmsg(&LocalHostName, LOG_INFO, "-- MARK --",
			    ADDDATE|MARK);
			MarkTimer = MarkInterval * 60;
		}
	}
	curalarm = 0;
	if (FlushTimer > 0)
		curalarm = FlushTimer;
	if (Marking && MarkTimer > 0 &&
		(MarkTimer < curalarm || curalarm == 0))
		curalarm = MarkTimer;

	/*
	 * Reset signal handler since the signal's disposition has
	 * been set to SIG_DFL (System V signals) before this routine
	 * is called.
	 */
	(void) signal(SIGALRM, (void (*)())doalarm);
	(void) alarm((unsigned)curalarm);
	dprintf("Next alarm in %d seconds\n", curalarm);
}

static void
flushmsg()
{
	FlushTimer = 0;
	if (PrevCount == 0)
		return;
	if (PrevCount > 1)
		(void) sprintf(PrevLine+16, "last message repeated %d times",
		    PrevCount);
	PrevCount = 0;
	logmsg(&PrevHost, PrevPri, PrevLine, PrevFlags|NOCOPY);
	PrevLine[0] = '\0';
	/* Next statement affects strcmp of previous line in logmsg routine */
	PrevLine[15] = '\0';	/* Make sure match fails next time */
}

/*
 * Print syslogd errors some place.
 */
static void
logerror(type)
	char *type;
{
	char buf[MAXLINE+1];

	if (t_errno == 0 || t_errno == TSYSERR) {
		char *errstr;

		if (errno == 0)
			(void) sprintf(buf, "syslogd: %.*s", MAXLINE, type);
		else if ((errstr = strerror(errno)) == (char *) NULL)
			(void) sprintf(buf, "syslogd: %s: error %d",
				type, errno);
		else
			(void) sprintf(buf, "syslogd: %s: %s", type, errstr);
	} else {
		if ((unsigned)t_errno > t_nerr)
			(void) sprintf(buf, "syslogd: %s: t_error %d",
				type, t_errno);
		else
			(void) sprintf(buf, "syslogd: %s: %s",
				type, t_errlist[t_errno]);
	}
	errno = 0;
	t_errno = 0;
	dprintf("%s\n", buf);
	logmsg(&LocalHostName, LOG_SYSLOG|LOG_ERR, buf, ADDDATE);
}

static void
die(sig)
{
	char buf[100];

	if (sig) {
		dprintf("syslogd: going down on signal %d\n", sig);
		flushmsg();
		(void) sprintf(buf, "going down on signal %d", sig);
		errno = 0;
		logerror(buf);
	}
	exit(0);
}

static int
countlines()
{
	register FILE *cf;
	char cline[BUFSIZ];

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
nofile:
		dprintf("cannot open %s\n", ConfFile);
		return (2);
	}
	(void) fclose(cf);

	/*
	 * Run the configuration file through m4 to handle any ifdefs.
	 */
	(void) sprintf(cline, "echo '%s' | /usr/ccs/bin/m4 - %s",
	    amiloghost() ? "define(LOGHOST, 1)" : "", ConfFile);
	if ((cf = popen(cline, "r")) == NULL) {
		(void) sprintf(cline, "echo '%s' | /usr/bin/m4 - %s",
		    amiloghost() ? "define(LOGHOST, 1)" : "", ConfFile);
		if ((cf = popen(cline, "r")) == NULL) {
			goto nofile;
		}
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	while (fgets(cline, sizeof (cline), cf) != NULL) {
		/* check for end-of-section */
		if (cline[0] == '\n' || cline[0] == '#')
			continue;

		nlogs++;
		dprintf("nlogs %d\n", nlogs);
	}

	return (nlogs);
}


/*
 *  INIT -- Initialize syslogd from configuration table
 */
static void
init()
{
	register int i;
	register FILE *cf;
	register struct filed *f;
	register char *p;
	sigset_t osigs, sigs;
	char cline[BUFSIZ];
	int	iamloghost = 0;

	dprintf("init\n");

	/* flush any pending output */
	flushmsg();

	nlogs = 0;

	if (Files != NULL)
		free(Files);

	nlogs = countlines();

	Files = (struct filed *)malloc(sizeof (struct filed) * nlogs);

	dprintf("nlogs %d\n", nlogs);

	if (!Files) {
		Files = (struct filed *)&fallback;
		cfline("*.ERR\t/dev/syscon", 0, &Files[0]);
		cfline("*.PANIC\t*", 0, &Files[1]);
		return;
	}


	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	for (f = Files; f < &Files[nlogs]; f++) {
		switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_FORW:
		case F_CONSOLE:
			(void) close(f->f_file);
			f->f_type = F_UNUSED;
			break;
		}
	}

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
nofile:
		dprintf("cannot open %s\n", ConfFile);
		cfline("*.ERR\t/dev/syscon", 0, &Files[0]);
		cfline("*.PANIC\t*", 0, &Files[1]);
		return;
	}

	if ((iamloghost = amiloghost()) == 1)
		dprintf("I am loghost\n");

	/*
	 * Run the configuration file through m4 to handle any ifdefs.
	 */
	(void) fclose(cf);
	(void) sprintf(cline, "echo '%s' | /usr/ccs/bin/m4 - %s",
	    iamloghost ? "define(LOGHOST, 1)" : "", ConfFile);
	if ((cf = popen(cline, "r")) == NULL) {
		(void) sprintf(cline, "echo '%s' | /usr/bin/m4 - %s",
		    iamloghost ? "define(LOGHOST, 1)" : "", ConfFile);
		if ((cf = popen(cline, "r")) == NULL) {
			goto nofile;
		}
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = Files;
	i = 0;
	while ((fgets(cline, sizeof (cline),
	    cf) != NULL) && (f < &Files[nlogs])) {
		i++;
		/* check for end-of-section */
		if (cline[0] == '\n' || cline[0] == '#')
			continue;

		/* strip off newline character */
		p = strchr(cline, '\n');
		if (p)
			*p = '\0';
		cfline(cline, i, f++);
	}

	/* close the configuration file */
	(void) pclose(cf);

	Initialized = 1;

	if (Debug) {
		for (f = Files; f < &Files[nlogs]; f++) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == NOPRI)
					(void) printf("X ");
				else
					(void) printf("%d ", f->f_pmask[i]);
			(void) printf("%s: ", TypeNames[f->f_type]);
			switch (f->f_type) {
			case F_FILE:
			case F_TTY:
			case F_CONSOLE:
				(void) printf("%s", f->f_un.f_fname);
				break;

			case F_FORW:
				(void) printf("%s", f->f_un.f_forw.f_hname);
				break;

			case F_USERS:
				for (i = 0; i < MAXUNAMES &&
				    *f->f_un.f_uname[i]; i++) {
					(void) printf("%s, ",
					    f->f_un.f_uname[i]);
					}
				break;
			}
			(void) printf("\n");
		}
	}

	/*
	 * See if marks are to be written to any files.  If so, set up a
	 * timeout for marks.
	 */
	Marking = 0;
	for (f = Files; f < &Files[nlogs]; f++) {
		if (f->f_type != F_UNUSED &&
		    f->f_pmask[LOG_NFACILITIES] != NOPRI)
			Marking = 1;
	}
	if (Marking) {
		(void) sigemptyset(&osigs);
		(void) sigemptyset(&sigs);
		(void) sigaddset(&sigs, SIGALRM);
		(void) sigprocmask(SIG_SETMASK, &sigs, &osigs);
		setalarm(MarkInterval * 60);
		(void) sigprocmask(SIG_SETMASK, &osigs, NULL);
	}
	logmsg(&LocalHostName, LOG_SYSLOG|LOG_INFO,
		"syslogd: restart", ADDDATE);
	dprintf("syslogd: restarted\n");
}

/*
 * Crack a configuration file line
 */

struct code {
	char	*c_name;
	int	c_val;
};

static struct code	PriNames[] = {
	"panic",	LOG_EMERG,
	"emerg",	LOG_EMERG,
	"alert",	LOG_ALERT,
	"crit",		LOG_CRIT,
	"err",		LOG_ERR,
	"error",	LOG_ERR,
	"warn",		LOG_WARNING,
	"warning",	LOG_WARNING,
	"notice",	LOG_NOTICE,
	"info",		LOG_INFO,
	"debug",	LOG_DEBUG,
	"none",		NOPRI,
	NULL,		-1
};

static struct code	FacNames[] = {
	"kern",		LOG_KERN,
	"user",		LOG_USER,
	"mail",		LOG_MAIL,
	"daemon",	LOG_DAEMON,
	"auth",		LOG_AUTH,
	"security",	LOG_AUTH,
	"mark",		LOG_MARK,
	"syslog",	LOG_SYSLOG,
	"lpr",		LOG_LPR,
	"news",		LOG_NEWS,
	"uucp",		LOG_UUCP,
	"cron",		LOG_CRON,
	"local0",	LOG_LOCAL0,
	"local1",	LOG_LOCAL1,
	"local2",	LOG_LOCAL2,
	"local3",	LOG_LOCAL3,
	"local4",	LOG_LOCAL4,
	"local5",	LOG_LOCAL5,
	"local6",	LOG_LOCAL6,
	"local7",	LOG_LOCAL7,
	NULL,		-1
};

static void
cfline(line, lineno, f)
	char *line;
	int lineno;
	register struct filed *f;
{
	register char *p;
	register char *q;
	register int i;
	char *bp;
	int pri;
	char buf[MAXLINE];
	char xbuf[200];
	char ebuf[100];

	dprintf("cfline(%s)\n", line);

	errno = 0;	/* keep sys_errlist stuff out of logerror messages */

	/* clear out file entry */
	bzero((char *) f, sizeof (*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = NOPRI;

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t'; ) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(", ;", *q))
			q++;

		/* decode priority name */
		pri = decode(buf, PriNames);
		if (pri < 0) {
			(void) sprintf(xbuf,
			    "line %d: unknown priority name \"%s\"",
			    lineno, buf);
			logerror(xbuf);
			return;
		}

		/* scan facilities */
		while (*p && !strchr("\t.;", *p)) {
			for (bp = buf; *p && !strchr("\t,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++)
					f->f_pmask[i] = pri;
			else {
				i = decode(buf, FacNames);
				if (i < 0) {
					/*CSTYLED*/
					(void) sprintf(xbuf, "line %d: unknown facility name \"%s\"",lineno, buf);
					logerror(xbuf);
					return;
				}
				f->f_pmask[i >> 3] = pri;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t' || *p == ' ')
		p++;

	switch (*p)
	{
	case '\0':
		(void) sprintf(xbuf, "line %d: no action part", lineno);
		errno = 0;
		logerror(xbuf);
		break;

	case '@':
		(void) strcpy(f->f_un.f_forw.f_hname, ++p);
		if (logforward(f, ebuf) != 0) {
			(void) sprintf(xbuf, "line %d: %s", lineno, ebuf);
			logerror(xbuf);
			break;
		}
		f->f_type = F_FORW;
		break;

	case '/':
		(void) strcpy(f->f_un.f_fname, p);
		if ((f->f_file = open(p, O_WRONLY|O_APPEND|O_NOCTTY)) < 0) {
			logerror(p);
			break;
		}
		if (isatty(f->f_file)) {
			f->f_type = F_TTY;
			untty();
		}
		else
			f->f_type = F_FILE;
		if (strcmp(p, ctty) == 0)
			f->f_type = F_CONSOLE;
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void) strncpy(f->f_un.f_uname[i], p, UNAMESZ);
			if ((q - p) > UNAMESZ)
				f->f_un.f_uname[i][UNAMESZ] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}
}


/*
 *  Decode a symbolic name to a numeric value
 */
static int
decode(name, codetab)
	char *name;
	struct code *codetab;
{
	register struct code *c;
	register char *p;
	char buf[40];

	if (isdigit(*name))
		return (atoi(name));

	(void) strcpy(buf, name);
	for (p = buf; *p; p++)
		if (isupper(*p))
			*p = tolower(*p);
	for (c = codetab; c->c_name; c++)
		if (!(strcmp(buf, c->c_name)))
			return (c->c_val);

	return (-1);
}
static int
ismyaddr(nbp)
	register struct netbuf *nbp;
{
	register int i;
	register int Jinputs;

	if (nbp == NULL)
		return (0);
	Jinputs = ((Ninputs < NINLOGS) ? Ninputs : NINLOGS);

	for (i = 1; i < Jinputs; i++) {
		if (nbp->len == Myaddrs[i]->len &&
			same(nbp->buf, Myaddrs[i]->buf, nbp->len))
				return (1);
	}
	return (0);
}

static void
getnets()
{
	struct nd_hostserv hs;
	struct netconfig *ncp;
	struct nd_addrlist *nap;
	struct netbuf *nbp;
	int i;
	void *handle;
	char *uap;

	hs.h_host = HOST_SELF;
	hs.h_serv = "syslog";

	if ((handle = setnetconfig()) == NULL)
		return;
	while ((ncp = getnetconfig(handle)) != NULL) {
		if (ncp->nc_semantics == NC_TPI_CLTS) {
			if (netdir_getbyname(ncp, &hs, &nap) == 0) {
				if (!nap)
					continue;
				dprintf("getnets() found %d addresses",
								nap->n_cnt);
				nbp = nap->n_addrs;
				if (nap->n_cnt > 0)
					dprintf(", they are: ");
				for (i = 0; i < nap->n_cnt; i++) {
					if ((uap = taddr2uaddr(ncp, nbp)) !=
					    (char *)NULL) {
						dprintf("%s ", uap);
					}
					free(uap);
					nbp++;
				}
				dprintf("\n");

				nbp = nap->n_addrs;
				for (i = 0; i < nap->n_cnt; i++) {
					add(ncp, nbp);
					nbp++;
				}
				netdir_free((void *)nap, ND_ADDRLIST);
			}
		}
	}
	endnetconfig(handle);
}

static void
add(ncp, nbp)
	struct netconfig *ncp;
	struct netbuf *nbp;
{
	int fd;
	struct t_bind bind;
	struct t_bind *bound;

	if (Ninputs >= NINLOGS)
		return;
	fd = t_open(ncp->nc_device, O_RDWR, NULL);
	if (fd < 0)
		return;
	(void) memcpy(&Ncf[Ninputs], ncp, sizeof (struct netconfig));
	bound = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	bind.addr = *nbp;
	bind.qlen = 0;
	if (t_bind(fd, &bind, bound) < 0) {
		t_close(fd);
		t_free((char *)bound, T_BIND);
		return;
	}
	if ((bind.addr.len != bound->addr.len) ||
	    !same(bind.addr.buf, bound->addr.buf, bind.addr.len)) {
		t_close(fd);
		t_free((char *)bound, T_BIND);
		return;
	}
	Udp[Ninputs] = (struct t_unitdata *)t_alloc(fd, T_UNITDATA, T_ADDR);
	if (Udp[Ninputs] == NULL) {
		t_close(fd);
		t_free((char *)bound, T_BIND);
		return;
	}
	Errp[Ninputs] = (struct t_uderr *)t_alloc(fd, T_UDERROR, T_ADDR);
	if (Errp[Ninputs] == NULL) {
		t_close(fd);
		t_free((char *)Udp[Ninputs], T_UNITDATA);
		t_free((char *)bound, T_BIND);
		return;
	}
	Pfd[Ninputs].fd = fd;
	Pfd[Ninputs].events = POLLIN;
	Myaddrs[Ninputs++] = &bound->addr;
}

static int
logforward(f, ebuf)
	register struct filed *f;
	char *ebuf;
{
	struct nd_hostserv hs;
	struct netbuf *nbp;
	struct netconfig *ncp;
	struct nd_addrlist *nap;
	void *handle;
	char *hp;

	hp = f->f_un.f_forw.f_hname;
	hs.h_host = hp;
	hs.h_serv = "syslog";

	if ((handle = setnetconfig()) == NULL) {
		(void) strcpy(ebuf,
		    "unable to rewind the netconfig database");
		errno = 0;
		return (-1);
	}
	nap = (struct nd_addrlist *)NULL;
	while ((ncp = getnetconfig(handle)) != NULL) {
		if (ncp->nc_semantics == NC_TPI_CLTS) {
			if (netdir_getbyname(ncp, &hs, &nap) == 0) {
				if (!nap)
					continue;
				nbp = nap->n_addrs;
				break;
			}
		}
	}
	if (nap == (struct nd_addrlist *)NULL) {
		endnetconfig(handle);
		(void) sprintf(ebuf, "unknown host %s", hp);
		errno = 0;
		return (-1);
	}
	if (ismyaddr(nbp)) {
		netdir_free((void *)nap, ND_ADDRLIST);
		endnetconfig(handle);
		(void) sprintf(ebuf, "host %s is this host - logging loop",
		    hp);
		errno = 0;
		return (-1);
	}
	f->f_un.f_forw.f_addr.buf = malloc(nbp->len);
	if (f->f_un.f_forw.f_addr.buf == NULL) {
		netdir_free((void *)nap, ND_ADDRLIST);
		endnetconfig(handle);
		(void) strcpy(ebuf, "malloc");
		return (-1);
	}
	bcopy(nbp->buf, f->f_un.f_forw.f_addr.buf, nbp->len);
	f->f_un.f_forw.f_addr.len = nbp->len;
	f->f_file = t_open(ncp->nc_device, O_RDWR, NULL);
	if (f->f_file < 0) {
		netdir_free((void *)nap, ND_ADDRLIST);
		endnetconfig(handle);
		(void) strcpy(ebuf, "t_open");
		return (-1);
	}
	netdir_free((void *)nap, ND_ADDRLIST);
	endnetconfig(handle);
	if (t_bind(f->f_file, NULL, NULL) < 0) {
		(void) strcpy(ebuf, "t_bind");
		t_close(f->f_file);
		return (-1);
	}
	return (0);
}

static int
amiloghost()
{
	struct nd_hostserv hs;
	struct netconfig *ncp;
	struct nd_addrlist *nap;
	struct netbuf *nbp;
	int i;
	void *handle;
	char *uap;
	struct t_bind bind;
	struct t_bind *bound;
	int	fd;

	/*
	 * we need to know if we are running on the loghost. This is
	 * checked by binding to the address associated with "loghost"
	 * and "syslogd" service over the connectionless transport
	 */

	hs.h_host = "loghost";
	hs.h_serv = "syslog";

	if ((handle = setnetconfig()) == NULL)
		return (0);
	while ((ncp = getnetconfig(handle)) != NULL) {
		if (ncp->nc_semantics == NC_TPI_CLTS) {
			if (netdir_getbyname(ncp, &hs, &nap) == 0) {
				if (!nap)
					continue;
				nbp = nap->n_addrs;
				for (i = 0; i < nap->n_cnt; i++) {
					if ((uap = taddr2uaddr(ncp, nbp))
					    != (char *)NULL) {
						/*CSTYLED*/
						dprintf("amiloghost() testing %s\n", uap);
					}
					free(uap);

					fd = t_open(ncp->nc_device, O_RDWR,
					    NULL);
					if (fd < 0)
						return (0);
					bound = (struct t_bind *)t_alloc(fd,
					    T_BIND, T_ADDR);
					bind.addr = *nbp;
					bind.qlen = 0;
					if (t_bind(fd, &bind, bound) == 0) {
						t_close(fd);
						t_free((char *)bound, T_BIND);
						netdir_free((void *)nap,
						    ND_ADDRLIST);
						return (1);
					} else {
						t_close(fd);
						t_free((char *)bound, T_BIND);
					}
					nbp++;
				}
				netdir_free((void *)nap, ND_ADDRLIST);
			}
		}
	}
	endnetconfig(handle);
	return (0);
}

static int
same(a, b, n)
	register char *a;
	register char *b;
	register unsigned int n;
{
	if (n == 0)
		return (0);
	while (n-- > 0)
		if (*a++ != *b++)
			return (0);
	return (1);
}

/*
 * Filter out non-printable characters, with the exception of tabs
 * and new lines, return the filtered string
 * Save the original string for possible forwarding
 * If we are running in a multibyte locale, MB_CUR_MAX > 1, do the
 * filtering on wide characters.
 */

static void
filter_string(char *mbstr, char *filtered)
{
	int i, mbyte_sz = 0;
	wchar_t wdchar;
	char c;

	if (MB_CUR_MAX > 1) {
		while (*mbstr != '\0') {
			if ((mbyte_sz = mbtowc(&wdchar, mbstr,
				MB_CUR_MAX)) == -1) {
					mbstr++;
					continue;
			}
			/*
			* copy printable character sequences, tabs and
			* new lines back into second string
			*/
			if (wdchar == (wchar_t)'\t' ||
			    wdchar == (wchar_t)'\n' || iswprint(wdchar)) {
				for (i = 0; i < mbyte_sz; i++)
					*filtered++ = *mbstr++;
			} else {
			/* otherwise, just increment the pointer */
				mbstr += mbyte_sz;
			}
		}
	} else {
		while (*mbstr != '\0') {
			c = *mbstr;
			if (c == '\t' || c == '\n' || isprint(c)) {
				*filtered++ = *mbstr++;
			} else {
				mbstr++;
			}
		}
	}
	*filtered = '\0';
}
