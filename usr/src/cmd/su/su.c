/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)su.c	1.29	96/07/11 SMI"	/* Svr4.0 1.9.5.16 */

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */
/*
 *	su [-] [name [arg ...]] change userid, `-' changes environment.
 *	If SULOG is defined, all attempts to su to another user are
 *	logged there.
 *	If CONSOLE is defined, all successful attempts to su to uid 0
 *	are also logged there.
 *
 *	If su cannot create, open, or write entries into SULOG,
 *	(or on the CONSOLE, if defined), the entry will not
 *	be logged -- thus losing a record of the su's attempted
 *	during this period.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include <crypt.h>
#include <pwd.h>
#include <shadow.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <locale.h>
#include <syslog.h>
#include <sys/utsname.h>

#ifdef DYNAMIC_SU
#include <security/pam_appl.h>
#endif

#define	PATH	"/usr/bin:"		/* path for users other than root */
#define	SUPATH	"/usr/sbin:/usr/bin"	/* path for root */
#define	SUPRMT	"PS1=# "		/* primary prompt for root */
#define	ELIM 128
#define	ROOT 0

/*
 * Intervals to sleep after failed su
 */
#ifndef SLEEPTIME
#define	SLEEPTIME	4
#endif

#define	DEFAULT_LOGIN "/etc/default/login"
#define	DEFFILE "/etc/default/su"		/* default file M000 */
char	*Sulog, *Console;
char	*Path, *Supath;			/* M004 */
extern char *defread();
extern int defopen();

static void envalt(void);
static void log(char *where, char *towho, int how);
static void to(int sig);
static void expired(char *usernam);

#ifdef DYNAMIC_SU
static int su_conv(int, struct pam_message **, struct pam_response **,
    void *);
static struct pam_conv pam_conv = {su_conv, NULL};
static pam_handle_t	*pamh;		/* Authentication handle */
static char	*getme();		/* get host name */
#endif	DYNAMIC_SU

struct	passwd pwd;
char	pwdbuf[1024];			/* buffer for getpwnam_r() */
char	shell[] = "/usr/bin/sh";	/* default shell */
char	su[16] = "su";		/* arg0 for exec of shprog */
char	homedir[MAXPATHLEN] = "HOME=";
char	logname[20] = "LOGNAME=";
char	*suprmt = SUPRMT;
char	termtyp[40] = "TERM=";			/* M002 */
char	*term;
char	shelltyp[40] = "SHELL=";		/* M002 */
char	*hz, *tz;
char	tznam[15] = "TZ=";
char	hzname[10] = "HZ=";
char	path[MAXPATHLEN] = "PATH=";		/* M004 */
char	supath[MAXPATHLEN] = "PATH=";		/* M004 */
char	*envinit[ELIM];
extern	char **environ;
char *ttyn;
char *username;					/* the invoker */
static	int	dosyslog = 0;			/* use syslog? */

int
main(int argc, char **argv)
{
#ifndef DYNAMIC_SU
	struct spwd sp;
	char  spbuf[1024];		/* buffer for getspnam_r() */
	char *password;
#endif
	char *nptr;
	char	*pshell;
	int eflag = 0;
	int envidx = 0;
	uid_t uid;
	gid_t gid;
	char *dir, *shprog, *name;
	int sleeptime = SLEEPTIME;
	char *ptr;
#ifdef DYNAMIC_SU
	int flags = 0;
	int retcode;
#endif	DYNAMIC_SU

#ifndef DYNAMIC_SU
	(void) execv("/usr/bin/su", argv);
#endif

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it wasn't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (argc > 1 && *argv[1] == '-') {
		eflag++;	/* set eflag if `-' is specified */
		argv++;
		argc--;
	}

	/*
	 * determine specified userid, get their password file entry,
	 * and set variables to values in password file entry fields
	 */
	nptr = (argc > 1) ? argv[1] : "root";
	if (defopen(DEFFILE) == 0) {
		char *ptr;

		if (Sulog = defread("SULOG="))
			Sulog = strdup(Sulog);
		if (Console = defread("CONSOLE="))
			Console = strdup(Console);
		if (Path = defread("PATH="))
			Path = strdup(Path);
		if (Supath = defread("SUPATH="))
			Supath = strdup(Supath);
		if ((ptr = defread("SYSLOG=")) != NULL)
			dosyslog = strcmp(ptr, "YES") == 0;

		defopen(NULL);
	}
	(void) strcat(path, (Path) ? Path : PATH);
	(void) strcat(supath, (Supath) ? Supath : SUPATH);
	if ((ttyn = ttyname(0)) == NULL)
		if ((ttyn = ttyname(1)) == NULL)
			if ((ttyn = ttyname(2)) == NULL)
				ttyn = "/dev/???";
	if ((username = cuserid(NULL)) == NULL)
		username = "(null)";

	/*
	 * if Sulog defined, create SULOG, if it does not exist, with
	 * mode read/write user. Change owner and group to root
	 */
	if (Sulog != NULL) {
		(void) close(open(Sulog, O_WRONLY | O_APPEND | O_CREAT,
		    (S_IRUSR|S_IWUSR)));
		(void) chown(Sulog, (uid_t)ROOT, (gid_t)ROOT);
	}

#ifdef DYNAMIC_SU
	if (pam_start("su", nptr, &pam_conv, &pamh) != PAM_SUCCESS)
		exit(1);
	if (pam_set_item(pamh, PAM_TTY, ttyn) != PAM_SUCCESS)
		exit(1);
	if (pam_set_item(pamh, PAM_RHOST, getme()) != PAM_SUCCESS)
		exit(1);
#endif	DYNAMIC_SU

	/*
	 * Save away the following for writing user-level audit records:
	 *	username desired
	 *	current tty
	 *	whether or not username desired is expired
	 *	auditinfo structure of current process
	 *	uid's and gid's of current process
	 */
	audit_su_init_info(nptr, ttyn);
	openlog("su", LOG_CONS, LOG_AUTH);

#ifdef DYNAMIC_SU

	/*
	 * Ignore SIGQUIT and SIGINT
	 */
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);

	/* call pam_authenticate() to authenticate the user through PAM */
	if (getpwnam_r(nptr, &pwd, pwdbuf, sizeof (pwdbuf)) == NULL)
		retcode = PAM_USER_UNKNOWN;
	else if (flags = (getuid() != (uid_t)0)) {
		retcode = pam_authenticate(pamh, 0);
	} else /* root user does not need to authenticate */
		retcode = PAM_SUCCESS;

	if (retcode == PAM_SUCCESS) {
		if (flags)
			expired(nptr);
		audit_su_reset_ai();
		audit_su_success();
		if (dosyslog)
			syslog(getuid() == 0 ? LOG_INFO : LOG_NOTICE,
			    "'su %s' succeeded for %s on %s",
			    pwd.pw_name, username, ttyn);
		closelog();
		(void) signal(SIGQUIT, SIG_DFL);
		(void) signal(SIGINT, SIG_DFL);
	} else {
		/*
		 * Use the same value of sleeptime that login(1) uses.
		 * This is obtained by reading the file /etc/default/login
		 * using the def*() functions
		 */
		if (defopen(DEFAULT_LOGIN) == 0) {
			if ((ptr = defread("SLEEPTIME=")) != NULL)
				sleeptime = atoi(ptr);
			(void) defopen((char *)NULL);

			if (sleeptime < 0 || sleeptime > 5)
				sleeptime = SLEEPTIME;
		}
		sleep(sleeptime);

		switch (retcode) {
		case PAM_USER_UNKNOWN:
			(void) fprintf(stderr, gettext("su: Unknown id: %s\n"),
									nptr);
			audit_su_bad_username();
			closelog();
			break;

		case PAM_AUTH_ERR:
			if (Sulog != NULL)
				log(Sulog, nptr, 0);	/* log entry */
			(void) fprintf(stderr, gettext("su: Sorry\n"));
			audit_su_bad_authentication();
			if (dosyslog)
				syslog(LOG_CRIT, "'su %s' failed for %s on %s",
				    pwd.pw_name, username, ttyn);
			closelog();
			break;

		case PAM_CONV_ERR:
		default:
			audit_su_unknown_failure();
			if (dosyslog)
				syslog(LOG_CRIT, "'su %s' failed for %s on %s",
				    pwd.pw_name, username, ttyn);
			closelog();
			break;
		}

		(void) signal(SIGQUIT, SIG_DFL);
		(void) signal(SIGINT, SIG_DFL);
		exit(1);
	}

#else	/* STATIC pam */
	if ((getpwnam_r(nptr, &pwd, pwdbuf, sizeof (pwdbuf)) == NULL) ||
	    (getspnam_r(nptr, &sp, spbuf, sizeof (spbuf)) == NULL)) {
		fprintf(stderr, gettext("su: Unknown id: %s\n"), nptr);
		audit_su_bad_username();
		closelog();
		exit(1);
	}

	/*
	 * Prompt for password if invoking user is not root or
	 * if specified(new) user requires a password
	 */
	if (sp.sp_pwdp[0] == '\0' || getuid() == (uid_t)0)
		goto ok;
	password = getpass("Password:");

	if ((strcmp(sp.sp_pwdp, crypt(password, sp.sp_pwdp)) != 0)) {
		/* clear password file entry */
		memset((void *)spbuf, 0, sizeof (spbuf));
		if (Sulog != NULL)
			log(Sulog, nptr, 0);    /* log entry */
		(void) fprintf(stderr, gettext("su: Sorry\n"));
		audit_su_bad_authentication();
		if (dosyslog)
			syslog(LOG_CRIT, "'su %s' failed for %s on %s",
			    pwd.pw_name, username, ttyn);
		closelog();
		exit(2);
	}
	/* clear password file entry */
	memset((void *)spbuf, 0, sizeof (spbuf));
ok:
	audit_su_reset_ai();
	audit_su_success();
	if (dosyslog)
		syslog(getuid() == 0 ? LOG_INFO : LOG_NOTICE,
		    "'su %s' succeeded for %s on %s",
		    pwd.pw_name, username, ttyn);
#endif

	uid = pwd.pw_uid;
	gid = pwd.pw_gid;
	dir = strdup(pwd.pw_dir);
	shprog = strdup(pwd.pw_shell);
	name = strdup(pwd.pw_name);

	if (Sulog != NULL)
		log(Sulog, nptr, 1);	/* log entry */

	/* set user and group ids to specified user */

	/* set the real (and effective) GID */
	if (setgid(gid) == -1) {
		(void) printf(gettext("su: Invalid GID\n"));
		exit(2);
	}
	/* Initialize the supplementary group access list. */
	if (!nptr)
		exit(2);
	if (initgroups(nptr, gid) == -1) {
		exit(2);
	}
#ifdef DYNAMIC_SU
	(void) pam_setcred(pamh, PAM_ESTABLISH_CRED);
#endif
	/* set the real (and effective) UID */
	if (setuid(uid) == -1) {
		(void) printf(gettext("su: Invalid UID\n"));
		exit(2);
	}
#ifdef DYNAMIC_SU
	if (pamh)
		(void) pam_end(pamh, PAM_SUCCESS);
#endif

	/*
	 * If new user's shell field is neither NULL nor equal to /usr/bin/sh,
	 * set:
	 *
	 *	pshell = their shell
	 *	su = [-]last component of shell's pathname
	 *
	 * Otherwise, set the shell to /usr/bin/sh and set argv[0] to '[-]su'.
	 */
	if (shprog[0] != '\0' && strcmp(shell, shprog) != 0) {
		char *p;

		pshell = shprog;
		(void) strcpy(su, eflag ? "-" : "");

		if ((p = strrchr(pshell, '/')) != NULL)
			(void) strcat(su, p + 1);
		else
			(void) strcat(su, pshell);
	} else {
		pshell = shell;
		(void) strcpy(su, eflag ? "-su" : "su");
	}

	/*
	 * set environment variables for new user;
	 * arg0 for exec of shprog must now contain `-'
	 * so that environment of new user is given
	 */
	if (eflag) {
		(void) strcat(homedir, dir);
		(void) strcat(logname, name);		/* M003 */
		if (hz = getenv("HZ"))
			(void) strcat(hzname, hz);
		if (tz = getenv("TZ"))
			(void) strcat(tznam, tz);
		(void) strcat(shelltyp, pshell);
		(void) chdir(dir);
		envinit[envidx = 0] = homedir;
		envinit[++envidx] = ((uid == (uid_t)0) ? supath : path);
		envinit[++envidx] = logname;
		envinit[++envidx] = hzname;
		envinit[++envidx] = tznam;
		if ((term = getenv("TERM")) != NULL) {
			(void) strcat(termtyp, term);
			envinit[++envidx] = termtyp;
		}
		envinit[++envidx] = shelltyp;
		envinit[++envidx] = NULL;
		environ = envinit;
	} else {
		char **pp = environ, **qq, *p;

		while ((p = *pp) != NULL) {
			if (*p == 'L' && p[1] == 'D' && p[2] == '_') {
				for (qq = pp; (*qq = qq[1]) != NULL; qq++)
					;
				/* pp is not advanced */
			} else {
				pp++;
			}
		}
	}

	/*
	 * if new user is root:
	 *	if CONSOLE defined, log entry there;
	 *	if eflag not set, change environment to that of root.
	 */
	if (uid == (uid_t)0) {
		if (Console != NULL)
			if (strcmp(ttyn, Console) != 0) {
				(void) signal(SIGALRM, to);
				(void) alarm(30);
				log(Console, nptr, 1);
				(void) alarm(0);
			}
		if (!eflag)
			envalt();
	}

	/*
	 * if additional arguments, exec shell program with array
	 * of pointers to arguments:
	 *	-> if shell = default, then su = [-]su
	 *	-> if shell != default, then su = [-]last component of
	 *						shell's pathname
	 *
	 * if no additional arguments, exec shell with arg0 of su
	 * where:
	 *	-> if shell = default, then su = [-]su
	 *	-> if shell != default, then su = [-]last component of
	 *						shell's pathname
	 */
	if (argc > 2) {
		argv[1] = su;
		(void) execv(pshell, &argv[1]);
	} else
		(void) execl(pshell, su, 0);

	(void) fprintf(stderr, gettext("su: No shell\n"));
	exit(3);
}

/*
 * Environment altering routine -
 *	This routine is called when a user is su'ing to root
 *	without specifying the - flag.
 *	The user's PATH and PS1 variables are reset
 *	to the correct value for root.
 *	All of the user's other environment variables retain
 *	their current values after the su (if they are exported).
 */
static void
envalt(void)
{
	/*
	 * If user has PATH variable in their environment, change its value
	 *		to /bin:/etc:/usr/bin ;
	 * if user does not have PATH variable, add it to the user's
	 *		environment;
	 * if either of the above fail, an error message is printed.
	 */
	if (putenv(supath) != 0) {
		(void) printf(gettext(
		    "su: unable to obtain memory to expand environment"));
		exit(4);
	}

	/*
	 * If user has PROMPT variable in their environment, change its value
	 *		to # ;
	 * if user does not have PROMPT variable, add it to the user's
	 *		environment;
	 * if either of the above fail, an error message is printed.
	 */
	if (putenv(suprmt) != 0) {
		(void) printf(gettext(
		    "su: unable to obtain memory to expand environment"));
		exit(4);
	}
}

/*
 * Logging routine -
 *	where = SULOG or CONSOLE
 *	towho = specified user ( user being su'ed to )
 *	how = 0 if su attempt failed; 1 if su attempt succeeded
 */
static void
log(char *where, char *towho, int how)
{
	FILE *logf;
	long now;
	struct tm *tmp;

	/*
	 * open SULOG or CONSOLE - if open fails, return
	 */
	if ((logf = fopen(where, "a")) == NULL)
		return;

	now = time(0);
	tmp = localtime(&now);

	/*
	 * write entry into SULOG or onto CONSOLE - if write fails, return
	 */
	(void) fprintf(logf, "SU %.2d/%.2d %.2d:%.2d %c %s %s-%s\n",
	    tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min,
	    how ? '+' : '-', ttyn + sizeof ("/dev/") - 1, username, towho);

	(void) fclose(logf);	/* close SULOG or CONSOLE */
}

/*ARGSUSED*/
static void
to(int sig)
{}

#ifdef DYNAMIC_SU
/* ******************************************************************** */
/*									*/
/* su_conv():								*/
/*	This is the conv (conversation) function called from		*/
/*	a PAM authentication module to print error messages		*/
/*	or garner information from the user.				*/
/*									*/
/* ******************************************************************** */
/*ARGSUSED*/
static int
su_conv(
	int		num_msg,
	struct pam_message **msg,
	struct pam_response **response,
	void		*appdata_ptr)
{
	struct pam_message	*m;
	struct pam_response	*r;
	char			*temp;
	int			k, i;

	if (num_msg <= 0)
		return (PAM_BUF_ERR);

	*response = (struct pam_response *)calloc(num_msg,
	    sizeof (struct pam_response));
	if (*response == NULL)
		return (PAM_CONV_ERR);

	k = num_msg;
	m = *msg;
	r = *response;
	while (k--) {

		switch (m->msg_style) {

		case PAM_PROMPT_ECHO_OFF:
			temp = getpass(m->msg);
			if (temp != NULL) {
				r->resp = strdup(temp);
				if (r->resp == NULL) {
					/* free responses */
					r = *response;
					for (i = 0; i < num_msg; i++, r++) {
						if (r->resp)
						free(r->resp);
					}
					free(*response);
					*response = NULL;
					return (PAM_BUF_ERR);
				}
			}
			m++;
			r++;
			break;

		case PAM_PROMPT_ECHO_ON:
			if (m->msg != NULL) {
				(void) fputs(m->msg, stdout);
			}
			r->resp = (char *)malloc(PAM_MAX_RESP_SIZE);
			if (r->resp == NULL) {
				/* free responses */
				r = *response;
				for (i = 0; i < num_msg; i++, r++) {
					if (r->resp)
					free(r->resp);
				}
				free(*response);
				*response = NULL;
				return (PAM_BUF_ERR);
			}
			(void) fgets(r->resp, PAM_MAX_RESP_SIZE-1, stdin);
			r->resp[PAM_MAX_RESP_SIZE-1] = NULL;
			m++;
			r++;
			break;

		case PAM_ERROR_MSG:
			if (m->msg != NULL) {
				(void) fputs(m->msg, stderr);
				(void) fputs("\n", stderr);
			}
			m++;
			r++;
			break;

		case PAM_TEXT_INFO:
			if (m->msg != NULL) {
				(void) fputs(m->msg, stdout);
				(void) fputs("\n", stdout);
			}
			m++;
			r++;
			break;

		default:
			break;
		}
	}
	return (PAM_SUCCESS);
}

/*
 * expired - calls exec_pass() to prompt user for
 * a passwd if the passwd has expired
 */
static void
expired(char *usernam)
{
	int error = 0;

	if (error = pam_acct_mgmt(pamh, 0)) {
		if (error == PAM_AUTHTOKEN_REQD) {
		    (void) fprintf(stderr, "%s '%s' %s\n",
			gettext("Passwd for user"), usernam,
			gettext("has expired - use passwd(1) to update it"));
		    audit_su_bad_authentication();
		    if (dosyslog)
			syslog(LOG_CRIT, "'su %s' failed for %s on %s",
			    pwd.pw_name, usernam, ttyn);
		    closelog();
		    exit(1);
		} else {
			(void) fprintf(stderr, gettext("su: Sorry\n"));
			audit_su_bad_authentication();
			if (dosyslog)
			    syslog(LOG_CRIT, "'su %s' failed for %s on %s",
				pwd.pw_name, usernam, ttyn);
			closelog();
			exit(3);
		}
	}
}


static char *
getme()
{
	static struct utsname	unstr;

	uname(&unstr);

	return (unstr.nodename);
}
#endif	/* DYNMAIC_SU */
