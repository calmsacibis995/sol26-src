/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)sulogin.c	1.25	96/06/18 SMI"	/* SVr4.0 1.9 */

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */


/*
 *	@(#) sulogin.c 1.2 88/05/05 sulogin:sulogin.c
 */
/*
 * sulogin - special login program exec'd from init to let user
 * come up single user, or go multi straight away.
 *
 *	Explain the scoop to the user, and prompt for
 *	root password or ^D. Good root password gets you
 *	single user, ^D exits sulogin, and init will
 *	go multi-user.
 *
 *	If /etc/passwd is missing, or there's no entry for root,
 *	go single user, no questions asked.
 *
 * Anthony Short, 11/29/82
 */

/*
 *	MODIFICATION HISTORY
 *
 *	M000	01 May 83	andyp	3.0 upgrade
 *	- index ==> strchr.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <termio.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <signal.h>
#include <utmpx.h>
#include <unistd.h>
#include <string.h>
#include <deflt.h>

/*
 * Intervals to sleep after failed login
 */
#ifndef SLEEPTIME
#define	SLEEPTIME	4	/* sleeptime before login incorrect msg */
#endif

/*
 *	the name of the file containing the login defaults we deliberately
 *	use the same file as login(1)
 */
#define	DEFAULT_LOGIN	"/etc/default/login"
#define	DEFAULT_SULOGIN	"/etc/default/sulogin"

#define	FAIL    (-1)
#define	USE_PAM (1)
#define	ROOT    "root"

#define	FALSE	0
#define	TRUE	1

#ifdef	M_V7
#define	strchr	index
#define	strrchr	rindex
#endif

char	minus[]	= "-";
char	shell[]	= "/sbin/sh";
char	su[]	= "/sbin/su";

char	*crypt();
static	char	*getpass();
struct utmpx *getutxent(), *pututxline();
char	*strchr(), *strrchr();
static struct utmpx *u;
char	*ttyn = NULL;
#define	SCPYN(a, b)	strncpy(a, b, sizeof (a))
extern char *findttyname();

/*	the following should be in <deflt.h>	*/
extern	int	defcntl();

static	void	noop(int signal);


main()
{
	struct stat info;			/* buffer for stat() call */
	register struct spwd *shpw;
	register char *pass;			/* password from user	  */
	register char *namep;
	int err;
	int	sleeptime = SLEEPTIME;
	int	passreq = TRUE;
	register int  flags;
	register char *ptr;

	signal(SIGINT, noop);
	signal(SIGQUIT, noop);

	/*
	 * Use the same value of sleeptime that login(1) uses. This is obtained
	 * by reading the file /etc/default/login using the def*() functions
	 */
	if (defopen(DEFAULT_LOGIN) == 0) {
		/*
		 * ignore case
		 */
		flags = defcntl(DC_GETFLAGS, 0);
		TURNOFF(flags, DC_CASE);
		defcntl(DC_SETFLAGS, flags);

		if ((ptr = defread("SLEEPTIME=")) != NULL)
			sleeptime = atoi(ptr);

		if (sleeptime < 0 || sleeptime > 5)
			sleeptime = SLEEPTIME;

		(void) defopen((char *)NULL);
	}

	(void) defopen((char *)NULL);

	/*
	 * Use the same value of sleeptime that login(1) uses. This is obtained
	 * by reading the file /etc/default/sulogin using the def*() functions
	 */
	if (defopen(DEFAULT_SULOGIN) == 0) {
		if ((ptr = defread("PASSREQ=")) != (char *)NULL)
			if (strcmp("NO", ptr) == 0)
				passreq = FALSE;

		(void) defopen((char *)NULL);
	}

	(void) defopen((char *)NULL);

	if (passreq == FALSE)
		single(shell);

	setspent();
	if ((shpw = getspnam("root")) == (struct spwd *) 0) {
		printf("\n*** NO ENTRY FOR root IN PASSWORD FILE! ***\n\n");
		single(shell);
	}
	endspent();

	/*
	 * Drop into main-loop
	 */

	while (1) {
		printf("\nType Ctrl-d to proceed with normal startup,\n");
		printf("(or give root password for system maintenance): ");

		if ((pass = getpass()) == (char *)0) {
			exit(0);	/* ^D, so straight to multi-user */
		}

		if (*shpw->sp_pwdp == '\0')
			namep = pass;
		else
			namep = crypt(pass, shpw->sp_pwdp);

		if (strcmp(namep, shpw->sp_pwdp)) {
			(void) sleep(sleeptime);
			printf("Login incorrect\n");
		} else {
			single(su);
		}

	}
}



/*
 * single() - exec shell for single user mode
 */
single(cmd)
char *cmd;
{
	/*
	 * update the utmpx file.
	 */
	ttyn = findttyname(0);
	if (ttyn == NULL)
		ttyn = "/dev/???";
	while ((u = getutxent()) != NULL) {
		if (strcmp(u->ut_line, (ttyn + sizeof ("/dev/") -1)) == 0) {
			time(&u->ut_tv.tv_sec);
			u->ut_type = INIT_PROCESS;
			if (strcmp(u->ut_user, "root")) {
				u->ut_pid = getpid();
				SCPYN(u->ut_user, "root");
			}
			break;
		}
	}
	if (u != NULL)
		pututxline(u);
	endutxent();		/* Close utmp file */
	printf("Entering System Maintenance Mode\n\n");
	execl(cmd, cmd, minus, (char *)0);
	exit(0);
}



/*
 * getpass() - hacked from the stdio library
 * version so we can distinguish newline and EOF.
 * Also don't need this routine to give a prompt.
 *
 * RETURN:	(char *)address of password string
 *			(could be null string)
 *
 *	   or	(char *)0 if user typed EOF
 */
static char *
getpass()
{
	struct termio ttyb;
	int flags;
	register char *p;
	register c;
	FILE *fi;
	static char pbuf[9];
	void (*signal())();
	void (*sig)();
	char *rval;		/* function return value */

	if ((fi = fopen("/dev/tty", "r")) == NULL)
		fi = stdin;
	else
		setbuf(fi, (char *)NULL);
	sig = signal(SIGINT, SIG_IGN);
	(void) ioctl(fileno(fi), TCGETA, &ttyb);
	flags = ttyb.c_lflag;
	ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	(void) ioctl(fileno(fi), TCSETAF, &ttyb);
	p = pbuf;
	rval = pbuf;
	while ((c = getc(fi)) != '\n') {
		if (c == EOF) {
			if (p == pbuf)		/* ^D, No password */
				rval = (char *)0;
			break;
		}
		if (p < &pbuf[8])
			*p++ = c;
	}
	*p = '\0';			/* terminate password string */
	fprintf(stderr, "\n");		/* echo a newline */
	ttyb.c_lflag = flags;
	(void) ioctl(fileno(fi), TCSETAW, &ttyb);
	signal(SIGINT, sig);
	if (fi != stdin)
		fclose(fi);
	return (rval);
}

char *
findttyname(fd)
int	fd;
{
	extern char *ttyname();
	char *ttyn;

	ttyn = ttyname(fd);

/* do not use syscon or contty if console is present, assuming they are links */
	if (((strcmp(ttyn, "/dev/syscon") == 0) ||
	    (strcmp(ttyn, "/dev/contty") == 0)) &&
	    (access("/dev/console", F_OK)))
		ttyn = "/dev/console";

	return (ttyn);
}


/* ARGSUSED */
static	void
noop(int sig)
{

	/*
	 * This signal handler does nothing except return
	 * We use it as the signal disposition in this program
	 * instead of SIG_IGN so that we do not have to restore
	 * the disposition back to SIG_DFL. Instead we allow exec(2)
	 * to set the dispostion to SIG_DFL to avoid a race condition
	 *
	 */
}
