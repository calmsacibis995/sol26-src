/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)passwd.c	1.29	96/08/22 SMI"	/* SVr4.0 1.4.3.9 */

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */
/*
 * passwd is a program whose sole purpose is to manage
 * the password file, map, or table. It allows system administrator
 * to add, change and display password attributes.
 * Non privileged user can change password or display
 * password attributes which corresponds to their login name.
 */

#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <rpcsvc/nis.h>
#include <syslog.h>

/*
 * flags indicate password attributes to be modified
 */

#define	LFLAG 0x001		/* lock user's password  */
#define	DFLAG 0x002		/* delete user's  password */
#define	MFLAG 0x004		/* set max field -- # of days passwd is valid */
#define	NFLAG 0x008		/* set min field -- # of days between */
				/* password changes */
#define	SFLAG 0x010		/* display password attributes */
#define	FFLAG 0x020		/* expire  user's password */
#define	AFLAG 0x040		/* display password attributes for all users */
#define	SAFLAG (SFLAG|AFLAG)	/* display password attributes for all users */
#define	WFLAG 0x100		/* warn user to change passwd */
#define	OFLAG 0x200		/* domain name */
#define	EFLAG 0x400		/* change shell */
#define	GFLAG 0x800		/* change gecos information */
#define	HFLAG 0x1000		/* change home directory */
#define	NONAGEFLAG	(EFLAG | GFLAG | HFLAG)
#define	AGEFLAG	(LFLAG | FFLAG | MFLAG | NFLAG | WFLAG)


/*
 * exit code
 */

#define	SUCCESS	0	/* succeeded */
#define	NOPERM	1	/* No permission */
#define	BADOPT	2	/* Invalid combination of option */
#define	FMERR	3	/* File/table manipulation error */
#define	FATAL	4	/* Old file/table can not be recovered */
#define	FBUSY	5	/* Lock file/table busy */
#define	BADSYN	6	/* Incorrect syntax */
#define	BADAGE	7	/* Aging is disabled  */

/*
 * define error messages
 */

#define	MSG_NP	"Permission denied"
#define	MSG_BS	"Invalid combination of options"
#define	MSG_FE	"Unexpected failure. Password file/table unchanged."
#define	MSG_FF	"Unexpected failure. Password file/table missing."
#define	MSG_FB	"Password file/table busy. Try again later."
#define	MSG_NV  "Invalid argument to option"
#define	MSG_AD	"Password aging is disabled"

/*
 * return code from ckarg() routine
 */
#define	FAIL 		-1

/*
 *  defind password file name
 */
#define	PASSWD 			"/etc/passwd"

#ifdef DEBUG
#define	dprintf1	printf
#else
#define	dprintf1(w, x)
#endif

extern int	optind;

static int		retval = SUCCESS;
static uid_t		uid;
static char		*prognamep;
static long		maxdate;	/* password aging information */
static int		passwd_conv();
static struct pam_conv	pam_conv = {passwd_conv, NULL};
static pam_handle_t	*pamh;		/* Authentication handle */
static int		repository = PAM_REP_DEFAULT;
static nis_name		nisdomain = NULL;

/*
 * Function Declarations
 */
extern	void		audit_passwd_init_id();
extern	void		audit_passwd_sorf();
extern	void		audit_passwd_attributes_sorf();
extern	nis_name	nis_local_directory();

static	char		*pw_attr_match();
static	void		display_attr();
static	int		get_namelist();
static	void		pw_setup_setattr();
static	char		**get_authtokattr();
static	void		passwd_exit();
static	void		rusage();
static	int		ckuid();
static	int		ckarg();

char *debug_flag = "debug";

/*
 * main():
 *	The main routine will call ckarg() to parse the command line
 *	arguments and call the appropriate functions to perform the
 *	tasks specified by the arguments. It allows system
 * 	administrator to add, change and display password attributes.
 * 	Non privileged user can change password or display
 * 	password attributes which corresponds to their login name.
 */

void
main(argc, argv)
	int argc;
	char *argv[];
{

	int	flag;
	char	**namelist;
	int	num_user;
	int	i;
	int	pam_retval;
	char	*usrname;
	char	**getattrp;
	char	**getattr;
	char	*setattr[PAM_MAX_NUM_ATTR];

	/*
	 * Determine command invoked (nispasswd, yppasswd, or passwd)
	 */

	if (prognamep = strrchr(argv[0], '/'))
		++prognamep;
	else
		prognamep = argv[0];

	if (strcmp(prognamep, "nispasswd") == 0) {
		repository = PAM_REP_NISPLUS | PAM_OPWCMD;
		nisdomain = nis_local_directory();
	} else if (strcmp(prognamep, "yppasswd") == 0)
		repository = PAM_REP_NIS | PAM_OPWCMD;
	else
		repository = PAM_REP_DEFAULT;	/* can't determine yet */

	/* initialization for variables, set locale and textdomain  */
	i = 0;
	flag = 0;

	uid = getuid();		/* get the user id */
	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*
	 * ckarg() parses the arguments. In case of an error,
	 * it sets the retval and returns FAIL (-1).
	 */

	flag = ckarg(argc, argv, setattr);
	dprintf1("flag is %0x\n", flag);
	if (flag == FAIL)
		passwd_exit(retval);

	/* Save away uid, gid, euid, egid, pid, and auditinfo. */
	audit_passwd_init_id();

	argc -= optind;

	if (argc < 1) {
		if ((usrname = getlogin()) == NULL) {
			struct passwd *pass = getpwuid(uid);
			if (pass != NULL)
				usrname = pass->pw_name;
			else {
				rusage();
				exit(NOPERM);
			}
		} else if (flag == 0) {
			/*
			 * If flag is zero, change passwd.
			 * Otherwise, it will display or
			 * modify password aging attributes
			 */
			(void) fprintf(stderr, "%s:  %s %s\n",
				prognamep, "Changing password for", usrname);
		}
	} else
		usrname = argv[optind];

	if (pam_start("passwd", usrname, &pam_conv, &pamh) != PAM_SUCCESS)
		passwd_exit(NOPERM);

	/* switch on flag */
	switch (flag) {

	case SAFLAG:	/* display password attributes for all users */

		/*
		 * For nis+, backend will go thru the passwd table and display
		 * all passwd attributes for all users. It can't just look at
		 * local passwd file.
		 * It should not reach here for nis because SAFLAG is not
		 * supported for nis.
		 */
		if (IS_NISPLUS(repository)) {
			/* pass in NULL as a username */
			getattr = get_authtokattr(NULL);
			passwd_exit(SUCCESS);
		} else if (IS_NIS(repository)) {
			/* an assertion that nis should not occur */
			rusage();
			passwd_exit(BADSYN);
		} else {
			/* files only */
			retval = get_namelist(&namelist, &num_user);
			if (retval != SUCCESS)
				(void) passwd_exit(retval);

			if (num_user == 0) {
				(void) fprintf(stderr, "%s: %s\n",
					prognamep, gettext(MSG_FF));
				passwd_exit(FATAL);
			}
			i = 0;
			while (namelist[i] != NULL) {
				getattr = get_authtokattr(namelist[i]);
				(void) display_attr(namelist[i], getattr);
				free(namelist[i]);
				getattrp = getattr;
				while (*getattr != NULL) {
					free(*getattr);
					getattr++;
				}
				free(getattrp);
				i++;
			}
			(void) free(namelist);
			passwd_exit(SUCCESS);
		}
		break;		/* NOT REACHED */

	case SFLAG:		/* display password attributes by user */
		getattr = get_authtokattr(usrname);
		if (getattr)
			(void) display_attr(usrname, getattr);
		getattrp = getattr;
		while (*getattr != NULL) {
			free(*getattr);
			getattr++;
		}
		free(getattrp);
		passwd_exit(SUCCESS);
		break;		/* NOT REACHED */


	case 0:			/* changing user password */
		dprintf1("call pam_chauthtok() repository=%d \n", repository);
		/*
		 * bypass PAM if -r or -D specified at the command line
		 */
		pam_retval = ((repository == PAM_REP_DEFAULT) ?
				pam_chauthtok(pamh, 0) :
				__update_authtok(pamh, 0, repository,
					nisdomain, 0, NULL));

		switch (pam_retval) {
		case PAM_SUCCESS:
			retval = SUCCESS;
			break;
		case PAM_AUTHTOK_DISABLE_AGING:
			retval = BADAGE;
			break;
		case PAM_AUTHTOK_LOCK_BUSY:
			retval = FBUSY;
			break;
		case PAM_AUTHTOK_ERR:
		case PAM_AUTHTOK_RECOVERY_ERR:
		default:
			retval = NOPERM;
			break;
		}

		audit_passwd_sorf(retval);
		(void) passwd_exit(retval);
		break;		/* NOT REACHED */


	default:		/* changing user password attributes */
		pam_retval = __set_authtoken_attr
				(pamh,
				(const char **) &setattr[0],
				repository,
				nisdomain,
				0,
				NULL);
		for (i = 0; setattr[i] != NULL; i++)
			free(setattr[i]);

		switch (pam_retval) {
		case PAM_SUCCESS:
			retval = SUCCESS;
			break;
		case PAM_AUTHTOK_DISABLE_AGING:
			retval = BADAGE;
			break;
		case PAM_AUTHTOK_LOCK_BUSY:
			retval = FBUSY;
			break;
		case PAM_AUTHTOK_ERR:
		case PAM_AUTHTOK_RECOVERY_ERR:
		default:
			retval = NOPERM;
			break;
		}

		audit_passwd_attributes_sorf(retval);
		(void) passwd_exit(retval);
		break;		/* NOT REACHED */
	}
}

/*
 * ckarg():
 *	This function parses and verifies the
 * 	arguments.  It takes three parameters:
 * 	argc => # of arguments
 * 	argv => pointer to an argument
 * 	setattr => pointer to a character array
 * 	In case of an error it prints the appropriate error
 * 	message, sets the retval and returns FAIL(-1).
 *	In case of success it will return an integer flag to indicate
 *	whether the caller wants to change password, or to display/set
 *	password attributes.  When setting password attributes is
 *	requested, the charater array pointed by "setattr" will be
 *	filled with ATTRIBUTE=VALUE pairs after the call.  Those
 *	pairs are set according to the input arguments' value
 */

static int
ckarg(argc, argv, setattr)
	int argc;
	char **argv;
	char *setattr[];
{
	extern char	*optarg;
	char		*char_p;
	register int	opt;
	register int	k;
	register int	flag;
	register int	entry_n, entry_x;
	char		*tmp;

	flag = 0;
	k = 0;
	entry_n = entry_x = -1;
	while ((opt = getopt(argc, argv, "r:aldefghsx:n:w:D:")) != EOF) {
		switch (opt) {

		case 'r': /* Repository Specified */
			/* repository: this option should be specified first */
			if (IS_OPWCMD(repository)) {
				(void) fprintf(stderr, gettext(
		"can't invoke nispasswd or yppasswd with -r option\n"));
				rusage();
				retval = BADSYN;
				return (FAIL);
			}
			if (repository != PAM_REP_DEFAULT) {
				(void) fprintf(stderr, gettext(
			"Repository is already defined or specified.\n"));
				rusage();
				retval = BADSYN;
				return (FAIL);
			}
			if (strcmp(optarg, "nisplus") == 0) {
				repository = PAM_REP_NISPLUS;
				nisdomain = nis_local_directory();
				dprintf1("domain is %s\n", nisdomain);
			} else if (strcmp(optarg, "nis") == 0)
				repository = PAM_REP_NIS;
			else if (strcmp(optarg, "files") == 0)
				repository = PAM_REP_FILES;
			else {
				(void) fprintf(stderr,
				    gettext("invalid repository: %s\n"),
				    optarg);
				rusage();
				retval = BADOPT;
				return (FAIL);
			}
			break;

		case 'd': /* Delete Auth Token */
			/* if no repository the default for -d is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			/* if invoked from nispasswd, it means "display" */
			if (IS_OPWCMD(repository) && IS_NISPLUS(repository)) {
				/* map to new flag -s */
				flag |= SFLAG;
				break;
			}

			/*
			 * Delete the password - only privileged processes
			 * can execute this for FILES
			 */
			if (IS_FILES(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
				    "-d only applies to files repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			if (ckuid() != SUCCESS) {
				retval = NOPERM;
				return (FAIL);
			}
			if (flag & (LFLAG|SAFLAG|DFLAG)) {
				rusage();
				retval = BADOPT;
				return (FAIL);
			}
			flag |= DFLAG;
			pw_setup_setattr(setattr, k++, "AUTHTOK_DEL=", "1");
			break;

		case 'l': /* lock the password */

			/* if no repository the default for -l is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_FILES(repository) == FALSE &&
			    IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
			"-l only applies to files or nisplus repository\n"));
				rusage();	/* exit */
				retval = BADOPT;
				return (FAIL);
			}

			/*
			 * Only privileged processes can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) &&
				((retval = ckuid()) != SUCCESS))
				return (FAIL);
			if (flag & (DFLAG|SAFLAG|LFLAG|NONAGEFLAG)) {
				rusage();	/* exit */
				retval = BADOPT;
				return (FAIL);
			}
			flag |= LFLAG;
			pw_setup_setattr(setattr, k++, "AUTHTOK_LK=", "1");
			break;

		case 'x': /* set the max date */

			/* if no repository the default for -x is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_FILES(repository) == FALSE &&
			    IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
			"-x only applies to files or nisplus repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) && (ckuid() != SUCCESS)) {
				retval = NOPERM;
				return (FAIL);
			}
			if (flag & (SAFLAG|MFLAG|NONAGEFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= MFLAG;
			if ((int)strlen(optarg)  <= 0 ||
			    (maxdate = strtol(optarg, &char_p, 10)) < -1 ||
			    *char_p != '\0') {
				(void) fprintf(stderr, "%s: %s -x\n",
					prognamep, gettext(MSG_NV));
				retval = BADSYN;
				return (FAIL);
			}
			entry_x = k;
			pw_setup_setattr(setattr, k++,
					"AUTHTOK_MAXAGE=", optarg);
			break;

		case 'n': /* set the min date */

			/* if no repository the default for -n is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_FILES(repository) == FALSE &&
			    IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
			"-n only applies to files or nisplus repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) &&
				((retval = ckuid()) != SUCCESS))
				return (FAIL);
			if (flag & (SAFLAG|NFLAG|NONAGEFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= NFLAG;
			if ((int)strlen(optarg)  <= 0 ||
			    (strtol(optarg, &char_p, 10)) < 0 ||
			    *char_p != '\0') {
				(void) fprintf(stderr, "%s: %s -n\n",
					prognamep, gettext(MSG_NV));
				retval = BADSYN;
				return (FAIL);
			}
			entry_n = k;
			pw_setup_setattr(setattr, k++,
					"AUTHTOK_MINAGE=", optarg);
			break;

		case 'w': /* set the warning field */

			/* if no repository the default for -w is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_FILES(repository) == FALSE &&
			    IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
			"-w only applies to files or nisplus repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) && (ckuid() != SUCCESS)) {
				retval = NOPERM;
				return (FAIL);
			}
			if (flag & (SAFLAG|WFLAG|NONAGEFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= WFLAG;
			if ((int)strlen(optarg)  <= 0 ||
			    (strtol(optarg, &char_p, 10)) < 0 ||
			    *char_p != '\0') {
				(void) fprintf(stderr, "%s: %s -w\n",
					prognamep, gettext(MSG_NV));
				retval = BADSYN;
				return (FAIL);
			}
			pw_setup_setattr(setattr, k++,
					"AUTHTOK_WARNDATE=", optarg);
			break;

		case 's': /* display password attributes */

			/* if no repository the default for -s is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			/* if invoked from nispasswd, change shell */
			if (IS_OPWCMD(repository) && IS_NISPLUS(repository)) {
				if (flag & (EFLAG|SAFLAG|AGEFLAG)) {
					(void) fprintf(stderr, "%s\n",
					    gettext(MSG_BS));
					retval = BADOPT;
					return (FAIL);
				}
				flag |= EFLAG;
				/* set attr */
				/* handle prompting in backend */
				pw_setup_setattr(setattr, k++,
				    "AUTHTOK_SHELL=", "1");
				break;
			}

			/* display password attributes */
			if (IS_FILES(repository) == FALSE &&
			    IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
			"-s only applies to files or nisplus repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) &&
				((retval = ckuid()) != SUCCESS))
				return (FAIL);
			if (flag && (flag != AFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= SFLAG;
			break;

		case 'a': /* display password attributes */

			/* if no repository the default for -a is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			/* if invoked from nispasswd, it means "display all" */
			if (IS_OPWCMD(repository) && IS_NISPLUS(repository)) {
				if (flag) {
					(void) fprintf(stderr, "%s\n",
					    gettext(MSG_BS));
					retval = BADOPT;
					return (FAIL);
				}
				flag |= SAFLAG;
				break;
			}

			if (IS_FILES(repository) == FALSE &&
			    IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
			"-a only applies to files or nisplus repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) &&
				((retval = ckuid()) != SUCCESS))
				return (FAIL);
			if (flag && (flag != SFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= AFLAG;
			break;

		case 'f': /* expire password attributes	*/

			/* if no repository the default for -f is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_FILES(repository) == FALSE &&
			    IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
			"-f only applies to files or nisplus repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) &&
				((retval = ckuid()) != SUCCESS))
				return (FAIL);
			if (flag & (SAFLAG|FFLAG|NONAGEFLAG)) {
				(void) fprintf(stderr, "%s\n", MSG_BS);
				retval = BADOPT;
				return (FAIL);
			}
			flag |= FFLAG;
			pw_setup_setattr(setattr, k++, "AUTHTOK_EXP=", "1");
			break;

		case 'D': /* domain name specified */
			if (IS_NISPLUS(repository) == FALSE) {
				(void) fprintf(stderr, gettext(
				    "-D only applies to nisplus repository\n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			if (flag & AFLAG) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			/* It is cleaner not to set this flag */
			/* flag |= OFLAG; */

			/* get domain from optarg */
			nisdomain = optarg;
			break;

		case 'e': /* change login shell */

			/* if no repository the default for -e is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_OPWCMD(repository) && IS_NIS(repository)) {
				(void) fprintf(stderr, gettext(
				    "-e doesn't apply to yppasswd \n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) && (ckuid() != SUCCESS)) {
				retval = NOPERM;
				return (FAIL);
			}
			if (flag & (EFLAG|SAFLAG|AGEFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= EFLAG;
			/* set attr */
			/* handle prompting in backend */
			pw_setup_setattr(setattr, k++, "AUTHTOK_SHELL=", "1");
			break;

		case 'g': /* change gecos information */

			/* if no repository the default for -g is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_OPWCMD(repository) && IS_NIS(repository)) {
				(void) fprintf(stderr, gettext(
				    "-g doesn't apply to yppasswd \n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) && (ckuid() != SUCCESS)) {
				retval = NOPERM;
				return (FAIL);
			}
			if (flag & (GFLAG|SAFLAG|AGEFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= GFLAG;
			pw_setup_setattr(setattr, k++, "AUTHTOK_GECOS=", "1");
			break;

		case 'h': /* change home dir */

			/* if no repository the default for -h is files */
			if (repository == PAM_REP_DEFAULT)
				repository = PAM_REP_FILES;

			if (IS_OPWCMD(repository) && IS_NIS(repository)) {
				(void) fprintf(stderr, gettext(
				    "-h doesn't apply to yppasswd \n"));
				rusage();	/* exit */
				retval = BADSYN;
				return (FAIL);
			}

			/*
			 * Only privileged process can execute this
			 * for FILES
			 */
			if (IS_FILES(repository) && (ckuid() != SUCCESS)) {
				retval = NOPERM;
				return (FAIL);
			}
			if (flag & (HFLAG|SAFLAG|AGEFLAG)) {
				(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
				retval = BADOPT;
				return (FAIL);
			}
			flag |= HFLAG;
			pw_setup_setattr(setattr, k++, "AUTHTOK_HOMEDIR=", "1");
			break;

		case '?':
			rusage();
			retval = BADSYN;
			return (FAIL);
		}
	}

	/* terminate the ATTRIBUTE=VALUE pairs array with NULL */
	(void) pw_setup_setattr(setattr, k++, NULL, NULL);

	argc -= optind;
	if (argc > 1) {
		rusage();
		retval = BADSYN;
		return (FAIL);
	}

	/*
	 * If option -n appears before -x, exchange them in the table because
	 * field max must be set before setting field min.
	 */
	if ((entry_n >= 0 && entry_x >= 0) && (entry_n < entry_x)) {
		tmp = setattr[entry_n];
		setattr[entry_n] = setattr[entry_x];
		setattr[entry_x] = tmp;
	}



	/* If no options are specified or only the show option */
	/* is specified, return because no option error checking */
	/* is needed */
	if (!flag || (flag == SFLAG))
		return (flag);

	/* AFLAG must be used with SFLAG */
	if (flag == AFLAG) {
		rusage();
		retval = BADSYN;
		return (FAIL);
	}

	if (flag != SAFLAG && argc < 1) {
		/*
		 * user name is not specified (argc<1), it can't be
		 * aging info update.
		 */
		if (!(flag & NONAGEFLAG)) {
			rusage();
			retval = BADSYN;
			return (FAIL);
		}
	}

	/* user name(s) may not be specified when SAFLAG is used. */
	if (flag == SAFLAG && argc >= 1) {
		rusage();
		retval = BADSYN;
		return (FAIL);
	}

	/*
	 * If aging is being turned off (maxdate == -1), mindate may not
	 * be specified.
	 */
	if ((maxdate == -1) && (flag & NFLAG)) {
		(void) fprintf(stderr, "%s: %s -x\n",
				prognamep, gettext(MSG_NV));
		retval = BADSYN;
		return (FAIL);
	}

	return (flag);
}

/*
 *
 * ckuid():
 *	This function returns SUCCESS if the caller is root, else
 *	it returns NOPERM.
 *
 */

static int
ckuid()
{
	if (uid != 0) {
		return (retval = NOPERM);
	}
	return (SUCCESS);
}

/*
 * get_authtokattr():
 *	This function sets user name in PAM buffer pointed by the
 *	authentication handle "pamh" first, then calls
 *	__get_authtoken_attr() to get the values of the authentication
 *	token attributes associated with the user specified by
 *	"username".  Upon success, it will return a pointer which
 *	points to a character array which stores the user's
 *	authentication token ATTRIBUTE=VALUE pairs.  Else, it will
 *	call passwd_exit() to exit properly.
 *
 */

char **
get_authtokattr(username)
	char *username;
{
	char			**get_attr;
	int 			pam_retval;

	/* nis+: if username is NULL, it gets all users */
	if (pam_set_item(pamh, PAM_USER, username) != PAM_SUCCESS)
		passwd_exit(NOPERM);

	pam_retval = __get_authtoken_attr(pamh, &get_attr, repository,
	    nisdomain, 0, NULL);
	switch (pam_retval) {
	case PAM_SUCCESS:
		retval = SUCCESS;
		return (get_attr);
		break;
	case PAM_USER_UNKNOWN:
		retval = NOPERM;
		(void) fprintf(stderr,
			"%s: %s\n", gettext("User unknown"), username);
	default:
		retval = NOPERM;
		passwd_exit(retval);
	}
}


/*
 *
 * display_attr():
 *	This function prints out the password attributes of a user
 *	onto standand output.
 *
 */

void
display_attr(usrname, getattr)
	char *usrname;
	char **getattr;
{
	char 		*value;
	long		lstchg;
	struct tm	*tmp;

	(void) fprintf(stdout, "%s  ", usrname);

	if ((value = pw_attr_match("AUTHTOK_STATUS", getattr)) != NULL)
		(void) fprintf(stdout, "%s  ", value);

	if ((value = pw_attr_match("AUTHTOK_LASTCHANGE", getattr)) != NULL) {
		lstchg = atoi(value);
		if (lstchg == 0)
			(void) strcpy(value, "00/00/00  ");
		else {
			tmp = gmtime(&lstchg);
			(void) sprintf(value, "%.2d/%.2d/%.2d  ",
			(tmp->tm_mon + 1), tmp->tm_mday, (tmp->tm_year % 100));
		}

		(void) fprintf(stdout, "%s  ", value);
	}

	if ((value = pw_attr_match("AUTHTOK_MINAGE", getattr)) != NULL)
		(void) fprintf(stdout, "%s  ", value);

	if ((value = pw_attr_match("AUTHTOK_MAXAGE", getattr)) != NULL)
		(void) fprintf(stdout, "%s  ", value);

	if ((value = pw_attr_match("AUTHTOK_WARNDATE", getattr)) != NULL)
		(void) fprintf(stdout, "%s  ", value);

	(void) fprintf(stdout, "\n");
}

/*
 *
 * get_namelist():
 *	This function gets a list of user names on the system from
 *	the /etc/passwd file.
 *
 */

int
get_namelist(namelist_p, num_user)
	char ***namelist_p;
	int *num_user;
{
	FILE		*pwfp;
	struct passwd	*pwd;
	int		max_user;
	int		nuser;
	char	**nl;

	nuser = 0;
	errno = 0;
	pwd = NULL;

	if ((pwfp = fopen(PASSWD, "r")) == NULL)
		return (NOPERM);

	/*
	 * find out the actual number of entries in the PASSWD file
	 */
	max_user = 1;			/* need one slot for terminator NULL */
	while ((pwd = fgetpwent(pwfp)) != NULL)
		max_user++;

	/*
	 *	reset the file stream pointer
	 */
	rewind(pwfp);

	nl = (char **)calloc(max_user, (sizeof (char *)));
	if (nl == NULL) {
		(void) fclose(pwfp);
		return (FMERR);
	}

	while ((pwd = fgetpwent(pwfp)) != NULL) {
		if ((nl[nuser] = strdup(pwd->pw_name)) == NULL) {
			(void) fclose(pwfp);
			return (FMERR);
		}
		nuser++;
	}

	nl[nuser] = NULL;
	*num_user = nuser;
	*namelist_p = nl;
	(void) fclose(pwfp);
	return (SUCCESS);
}

/*
 *
 * passwd_exit():
 *	This function will call exit() with appropriate exit code
 *	according to the input "retcode" value.
 *	It also calls pam_end() to clean-up buffers before exit.
 *
 */

void
passwd_exit(retcode)
	int	retcode;
{

	if (pamh)
		pam_end(pamh, PAM_SUCCESS);

	switch (retcode) {
	case SUCCESS:
			break;
	case NOPERM:
			(void) fprintf(stderr, "%s\n", gettext(MSG_NP));
			break;
	case BADOPT:
			(void) fprintf(stderr, "%s\n", gettext(MSG_BS));
			break;
	case FMERR:
			(void) fprintf(stderr, "%s\n", gettext(MSG_FE));
			break;
	case FATAL:
			(void) fprintf(stderr, "%s\n", gettext(MSG_FF));
			break;
	case FBUSY:
			(void) fprintf(stderr, "%s\n", gettext(MSG_FB));
			break;
	case BADSYN:
			(void) fprintf(stderr, "%s\n", gettext(MSG_NV));
			break;
	case BADAGE:
			(void) fprintf(stderr, "%s\n", gettext(MSG_AD));
			break;
	default:
			(void) fprintf(stderr, "%s\n", gettext(MSG_NP));
			retcode = NOPERM;
			break;
	}
	exit(retcode);
}

/*
 *
 * passwd_conv():
 *	This is the conv (conversation) function called from
 *	a PAM authentication module to print error messages
 *	or garner information from the user.
 *
 */

static int
passwd_conv(num_msg, msg, response, appdata_ptr)
	int num_msg;
	struct pam_message **msg;
	struct pam_response **response;
	void *appdata_ptr;
{
	struct pam_message	*m;
	struct pam_response	*r;
	char 			*temp;
	int			k, i;

	if (num_msg <= 0)
		return (PAM_CONV_ERR);

	*response = (struct pam_response *)calloc(num_msg,
						sizeof (struct pam_response));
	if (*response == NULL)
		return (PAM_BUF_ERR);

	k = num_msg;
	m = *msg;
	r = *response;
	while (k--) {

		switch (m->msg_style) {

		case PAM_PROMPT_ECHO_OFF:
			temp = getpass(m->msg);
			if (temp != NULL) {
				if ((r->resp = strdup(temp)) == NULL) {
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
			r->resp = (char *)calloc(PAM_MAX_RESP_SIZE,
							sizeof (char));
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
			if (fgets(r->resp, PAM_MAX_RESP_SIZE-1, stdin)) {
				int len = strlen(r->resp);
				if (r->resp[len-1] == '\n')
					r->resp[len-1] = '\0';
			}
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
 * 		Utilities Functions
 */


/*
 *	s1 is either name, or name=value
 *	s2 is an array of name=value pairs
 *	if name in s1 match the name in a pair stored in s2,
 *	then return value of the matched pair, else NULL
 */

static char *
pw_attr_match(s1, s2)
	register char *s1;
	register char **s2;
{
	char *s3;
	char *s1_save;

	s1_save = s1;
	while (*s2 != NULL) {
		s3 = *s2;
		while (*s1 == *s3++)
			if (*s1++ == '=')
				return (s3);
		if (*s1 == '\0' && *(s3-1) == '=')
			return (s3);
		s2++;
		s1 = s1_save;

	}
	return (NULL);
}

void
pw_setup_setattr(setattr, k, attr, value)
	char *setattr[];
	int k;
	char attr[];
	char value[];
{

	if (attr != NULL) {
		setattr[k] = (char *)malloc(strlen(attr) + strlen(value) + 1);
		if (setattr[k] == NULL)
			return;
		(void) strcpy(setattr[k], attr);
		(void) strcat(setattr[k], value);
	} else
		setattr[k] = NULL;
}

void
rusage()
{
	if (IS_OPWCMD(repository)) {
		(void) fprintf(stderr, gettext(
"yppasswd and nispasswd have been replaced by the new passwd command.\n"));
		(void) fprintf(stderr,
		    gettext("The usage is displayed below.\n"));
		(void) fprintf(stderr, gettext(
	"To continue using yppasswd/nispasswd, please refer to man pages.\n"));
	}
	(void) fprintf(stderr, gettext("usage:\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd [-r files | -r nis | -r nisplus] [name]\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd [-r files] [-egh] [name]\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd [-r files] -sa\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd [-r files] -s [name]\n"));
	(void) fprintf(stderr, gettext(
	"\tpasswd [-r files] [-d|-l] [-f] [-n min] [-w warn] [-x max] name\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd -r nis [-egh] [name]\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd -r nisplus [-egh] [-D domainname] [name]\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd -r nisplus -sa\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd -r nisplus [-D domainname] -s [name]\n"));
	(void) fprintf(stderr, gettext(
	    "\tpasswd -r nisplus [-l] [-f] [-n min] [-w warn]"));
	(void) fprintf(stderr, gettext(" [-x max] [-D domainname] name\n"));
}
