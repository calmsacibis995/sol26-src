/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)lp.c	1.19	96/10/28 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/systeminfo.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <termios.h>
#include <libintl.h>
#include <pwd.h>

#include <print/ns.h>
#include <print/network.h>
#include <print/misc.h>
#include <print/list.h>
#include <print/job.h>

/*
 *	 lpr/lp
 *	This program will submit print jobs to a spooler using the BSD
 *	printing protcol as defined in RFC1179, plus some extension for
 *	support of additional lp functionality.
 */

extern char *optarg;
extern int optind, opterr, optopt;
extern char *getenv(const char *);

#define	SEND_RETRY	-1
#define	SEND_ABORT	-2


static int	priority = -1,
		copies = 1,
		width = -1,
		indent = -1,
		linked = 0,
		mail = 0,
		delete = 0,
		suppress = 1,
		banner = 1,
		connection_failed = 0;
static char	*printer = NULL,
		*form = NULL,
		*charset = NULL,
		*title = NULL,
		*class = NULL,
		*jobName = NULL,
		*notification = NULL,
		*handling = NULL,
		*pages = NULL,
		**mode = NULL,
		**s5options = NULL,
		*s5type = NULL,
		*fontR = NULL,
		*fontI = NULL,
		*fontB = NULL,
		*fontS = NULL,
		type = CF_PRINT_ASCII;

static struct s5_types {
	char *name;
	char type;
} output_types[] = {			/* known LP "-T" types */
/*
 *	Switched to ASCII, because some BSD systems don't like the 'o' file
 *	type.
 */
	{ "postscript", CF_PRINT_ASCII },
	{ "ps", CF_PRINT_ASCII },
	{ "simple", CF_PRINT_ASCII },
	{ "ascii", CF_PRINT_ASCII },
	{ "raw", CF_PRINT_RAW },
	{ "dvi", CF_PRINT_DVI },
	{ "tex", CF_PRINT_DVI },
	{ "raster", CF_PRINT_RAS },
	{ "ditroff", CF_PRINT_DROFF },
	{ "otroff", CF_PRINT_TROFF },
	{ "troff", CF_PRINT_DROFF },
	{ "cif", CF_PRINT_CIF },
	{ "plot", CF_PRINT_PLOT },
	{ "fortran", CF_PRINT_FORT },
	{ "pr", CF_PRINT_PR },
	NULL
};


/*ARGSUSED*/
static void sigbus_handler(int i)
{
	fprintf(stdout, "No space in /var/spool/print to store job");
	exit(-1);
}


#define	OLD_LP "/usr/lib/lp/local/lp"	/* for job modification */
#ifdef OLD_LP
/*
 * if the old version of lp exists use it to submit print jobs that
 * require a symlink on the server "lp" or "lpr -s"
 */
static void
submit_local_lp_linked(char *program, int ac, char *av[])
{
	uid_t ruid = getuid();
	struct passwd *pw;

	ruid = getuid();
	if ((pw = getpwuid(ruid)) != NULL)
		initgroups(pw->pw_name, pw->pw_uid);
	setuid(ruid);

	if (strcmp(program, "lp") == 0) {
		while (--ac > 0)
			if (strcmp(av[ac], "-d") == 0) {
				av[ac + 1] = printer;
				break;
			}
		execv(OLD_LP, av);
	} else { /* convert lpr options */
		int argc = 0;
		char *argv[64];

		argv[argc++] = OLD_LP;
		argv[argc++] = "-d";
		argv[argc++] = printer;
		argv[argc++] = "-s";
		if (copies > 1) {
			char buf[12];
			sprintf(buf, "%d", copies);
			argv[argc++] = "-n";
			argv[argc++] = strdup(buf);
		}
		if (banner == 0) {
			argv[argc++] = "-o";
			argv[argc++] = "nobanner";
			}
		if (width > 0) {
			char buf[16];
			sprintf(buf, "prwidth=%d", width);
			argv[argc++] = "-o";
				argv[argc++] = strdup(buf);
		}
		if (indent > 0) {
			char buf[16];
			sprintf(buf, "indent=%d", indent);
			argv[argc++] = "-o";
			argv[argc++] = strdup(buf);
		}
		if (mail != 0)
			argv[argc++] = "-m";
		if (jobName != NULL) {
			argv[argc++] = "-t";
			argv[argc++] = jobName;
		}

		if (type != CF_PRINT_ASCII) {
			struct s5_types *tmp;

			for (tmp = output_types; tmp->name != NULL; tmp++)
				if (tmp->type == type) {
					argv[argc++] = "-T";
					argv[argc++] = tmp->name;
					break;
				}
		}

		while (optind < ac)
			argv[argc++] = av[optind++];

		argv[argc++] = NULL;
		execv(OLD_LP, argv);
	}
}
#endif


/*
 * cheat and look in the LP interface to determine if a local printer is
 * rejecting.  If so, don't queue the job.  If the printer is remote or
 * accepting, queue it.  This approximates behaviour of previous releases
 * The check is being done this way for performance.
 */
static int
rejecting(char *printer)
{
	int rc = 0;
	FILE *fp;

	if ((fp = fopen("/usr/spool/lp/system/pstatus", "r+")) != NULL) {
		char buf[BUFSIZ];

		while (fgets(buf, sizeof (buf), fp) != NULL) {
			buf[strlen(buf)-1] = NULL;
			if (strcmp(buf, printer) == 0) {
				char *ptr;

				fgets(buf, sizeof (buf), fp);
				buf[strlen(buf)-1] = NULL;
				if ((ptr = strrchr(buf, ' ')) &&
				    (strcmp(++ptr, "rejecting") == 0)) {
					rc = 1;
					break;
				}
			}
		}
	}
	fclose(fp);
	return (rc);
}


static int _notified = 0;
static void
error_notify(char *user, int id, char *msg, ...)
{
	if (_notified++ == 0) {
		char cmd[BUFSIZ];
		FILE *fp;
		va_list ap;

		va_start(ap, msg);
		sprintf(cmd, "/bin/write %s >/dev/null 2>&1", user);
		fp = popen(cmd, "w+");
		fprintf(fp, gettext("\n\tError transfering print job %d\n"),
			id);
		vfprintf(fp, msg, ap);
		pclose(fp);
		va_end(ap);
	}
}



/*
 *  bsd_options() parses the command line using the BSD lpr semantics and sets
 *	several global variables for use in building the print request.
 */
static void
bsd_options(int ac, char *av[])
{
	int c;

	while ((c = getopt(ac, av, "P:#:C:J:T:w:i:hplrstdgvcfmn1234")) != EOF)
		switch (c) {
		case 'P':
			printer = optarg;
			break;
		case '#':
			copies = atoi(optarg);
			break;
		case 'C':
			class = optarg;
			break;
		case 'J':
			jobName = optarg;
			break;
		case 'T':
			title = optarg;
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'm':
			mail++;
			break;
		case 'i':	/* this may or may not have an arguement */
			if (isdigit(optarg[0]) == 0) {
				indent = 8;
				optind--;
			} else
				indent = atoi(optarg);
			break;
		case 'h':
			banner = 0;
			break;
		case 'r':
			delete = 1;
			break;
		case 's':
			linked = 1;
			break;
		case 'l' :
			type = CF_PRINT_RAW;
			break;
		case 'd' :
			type = CF_PRINT_DVI;
			break;
		case 't' :
			type = CF_PRINT_TROFF;
			break;
		case 'g' :
			type = CF_PRINT_PLOT;
			break;
		case 'v' :
			type = CF_PRINT_RAS;
			break;
		case 'c' :
			type = CF_PRINT_CIF;
			break;
		case 'f' :
			type = CF_PRINT_FORT;
			break;
		case 'n' :
			type = CF_PRINT_DROFF;
			break;
		case 'o' :
			type = CF_PRINT_PS;
			break;
		case 'p' :
			type = CF_PRINT_PR;
			break;
		case '1' :
			fontR = optarg;
			break;
		case '2' :
			fontI = optarg;
			break;
		case '3' :
			fontB = optarg;
			break;
		case '4' :
			fontB = optarg;
			break;
		default:
			fprintf(stderr,
				"Usage: %s [-Pprinter] [-#num] [-Cclass] "
				"[-Jjob] [-Ttitle] [-i [indent]] [-1234 font] "
				"[-wcols] [-m] [-h] [-s] [-pltndgvcf] "
				"files ...\n",
				av[0]);
			exit(1);
		}
}

/*
 *  sysv_options() parses the command line using the BSD lpr semantics and sets
 *	several global variables for use in building the print request.
 */
static void
sysv_options(int ac, char *av[])
{
	int c;

#ifdef OLD_LP
	if ((ac > 2) && (strcmp(av[1], "-i") == 0)) {
		if (access(OLD_LP, F_OK) == 0) {
			execv(OLD_LP, av);
			perror("exec local modify");
		} else
			printf(gettext(
				"job modification not supported on clients\n"));
		exit(-1);
	}
#endif

	linked = 1;
	suppress = 0;
	while ((c = getopt(ac, av, "H:P:S:T:d:f:i:o:q:t:y:cmwn:prs")) != EOF)
		switch (c) {
		case 'q':
			priority = atoi(optarg);
			break;
		case 'H':
			handling = optarg;
			break;
		case 'f':
			form = optarg;
			break;
		case 'd':
			printer = optarg;
			break;
		case 'T':
			{
			struct s5_types *tmp;
			int flag = 0;

			for (tmp = output_types;
			    ((flag == 0) && (tmp->name != NULL)); tmp++)
				if (strcasecmp(tmp->name, optarg) == 0) {
					type = tmp->type;
					flag++;
				}
			if (flag == 0)
				s5type = optarg;
			break;
			}
		case 'S':
			charset = optarg;
			break;
		case 'o':
			{
			char *p;

			for (p = strtok(optarg, "\t ,"); p != NULL;
					p = strtok(NULL, "\t ,"))
				if (strcmp(p, "nobanner") != 0)
					s5options = (char **)list_append(
							(void**)s5options,
							(void *)p);
				else
					banner = 0;
			}
			break;
		case 'y':
			{
			char *p;

			for (p = strtok(optarg, "\t ,"); p != NULL;
					p = strtok(NULL, "\t ,"))
				if (strcmp(p, "catv_filter") == 0)
					type = CF_PRINT_RAW;
				else
					mode = (char **)list_append(
							    (void **)mode,
							    (void *)p);
			}
			break;
		case 'P':
			pages = optarg;
			break;
		case 'i':
			printf(gettext(
			"job modification (-i) only supported on server\n"));
			break;
		case 'c':
			linked = 0;
			break;
		case 'm':
			mail++;
			break;
		case 'w':
			mail++;
			break;
		case 'p':
			notification = optarg;
			break;
		case 'n':
			copies = atoi(optarg);
			break;
		case 's':
			suppress = 1;
			break;
		case 't':
			jobName = optarg;
			break;
		case 'r':
			/* not supported - raw */
			break;
		default:
			fprintf(stderr,
				"Usage: %s [-d dest] [-cmwsr] [-n num] "
				"[-t title] [-p notification] [-P page-list] "
				"[-i job-id] [y modes] [-o options] "
				"[-S char-set] [-T input-type] [H handling] "
				"[-q priority] files ...\n",
				av[0]);
			exit(1);
		}
}


/*
 *  stdin_to_file() reads standard input into a file and returns the file name
 *	to the caller
 */
static char *
stdin_to_file()
{
	int	fd,
		rc;
	char	*name,
		buf[BUFSIZ];

	sprintf(buf, "/tmp/stdin-%d", getpid());
	if ((fd = open(buf, O_CREAT|O_TRUNC|O_WRONLY, 0660)) < 0)
		return (NULL);
	name = strdup(buf);
	syslog(LOG_DEBUG, "stdin_to_file: %s", name);
	while ((rc = read(0, buf, sizeof (buf))) > 0)
		write(fd, buf, rc);
	close(fd);
	return (name);
}


static int
sendfile(jobfile_t *file, int nd, int type)
{
	syslog(LOG_DEBUG, "sendfile(%s, %d, %d)",
		((file != NULL) ? file->jf_spl_path : "NULL"), nd, type);
	if (file && file->jf_spl_path)
		return (net_send_file(nd, file->jf_spl_path, file->jf_data,
				file->jf_size, type));
	return (-1);
}



/*
 *  vsendfile() sends a file to a remote print server using the descriptor,
 *	file, and type passed in.  This functions is for use with
 *	list_iterate().
 */
static int
vsendfile(jobfile_t *file, va_list ap)
{
	int	nd = va_arg(ap, int),
		type = va_arg(ap, int);

	return (sendfile(file, nd, type));
}


/*
 *  send_job() sends a job to a remote print server.
 */
static int
send_job(job_t *job)
{
	int	lockfd,
		lock_size,
		nd;
	char	buf[BUFSIZ];

	syslog(LOG_DEBUG, "send_job(%s, %s, %d): called", job->job_printer,
		job->job_server, job->job_id);
	if ((lockfd = get_lock(job->job_cf->jf_src_path, 0)) < 0) {
		close(lockfd);
		return (SEND_RETRY);
	}

	/* is job complete ? */

	lock_size = file_size(job->job_cf->jf_src_path);
	sprintf(buf, "%d\n", getpid());		/* add pid to lock file */
	lseek(lockfd, 0, SEEK_END);
	write(lockfd, buf, strlen(buf));

	syslog(LOG_DEBUG, "send_job(%s, %s, %d): have lock", job->job_printer,
		job->job_server, job->job_id);
	connection_failed = 0;
	if ((nd = net_open(job->job_server, 5)) < 0) {
		connection_failed = 1;
		if (nd != NETWORK_ERROR_UNKNOWN)
			job_destroy(job);
		else
			ftruncate(lockfd, lock_size);
		error_notify(job->job_user, job->job_id,
			gettext("\t\t check queue for (%s@%s)\n"),
			job->job_printer, job->job_server);
		close(lockfd);
		return ((nd == NETWORK_ERROR_UNKNOWN) ? SEND_RETRY :
			SEND_ABORT);
	}

	if (net_send_message(nd, "%c%s\n", XFER_REQUEST, job->job_printer)
	    != 0) {
		net_close(nd);
		syslog(LOG_WARNING,
			"send_job failed job %d (%s@%s) check status\n",
			job->job_id, job->job_printer, job->job_server);
		error_notify(job->job_user, job->job_id,
			gettext("\t\t check queue for (%s@%s)\n"),
			job->job_printer, job->job_server);
		ftruncate(lockfd, lock_size);
		close(lockfd);
		return (SEND_RETRY);
	}

	syslog(LOG_DEBUG, "send_job(%s, %s, %d): send data", job->job_printer,
		job->job_server, job->job_id);
	if (list_iterate((void **)job->job_df_list, (VFUNC_T)vsendfile, nd,
	    XFER_DATA) < 0) {
		if (errno == ENOENT) {
			net_close(nd);
			error_notify(job->job_user, job->job_id, gettext(
				"\t\tdata removed before transfer, job "
				"canceled.\n\t\tTry \"lp -c\" or \"lpr\"\n"));
			job_destroy(job);
			close(lockfd);
			return (SEND_ABORT);
		} else {
			net_close(nd);
			ftruncate(lockfd, lock_size);
			error_notify(job->job_user, job->job_id,
				gettext("\t\t check queue for (%s@%s)\n"),
				job->job_printer, job->job_server);
			close(lockfd);
			return (SEND_RETRY);
		}
	}

	if (sendfile(job->job_cf, nd, XFER_CONTROL) < 0) {
		net_send_message(nd, "%c\n", XFER_CLEANUP);
		net_close(nd);
		ftruncate(lockfd, lock_size);
		error_notify(job->job_user, job->job_id,
			gettext("\t\t check queue for (%s@%s)\n"),
			job->job_printer, job->job_server);
		close(lockfd);
		return (SEND_RETRY);
	}

	syslog(LOG_DEBUG, "send_job(%s, %s, %d): complete", job->job_printer,
		job->job_server, job->job_id);
	net_close(nd);
	job_destroy(job);
	close(lockfd);
	return (0);
}


/*
 *  xfer_daemon() attempts to start up a daemon for transfering jobs to a remote
 *	print server.  The daemon runs if it can get the master lock, and it
 *	runs until there are no jobs waiting for transfer.
 */
static void
xfer_daemon()
{
	job_t **list = NULL;
	int i;



	closelog();
	for (i = 0; i < 5; i++)
		close(i);

	_notified = 1;
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_WRONLY);
	dup(1);
	if ((i = open("/dev/tty", O_RDWR)) > 0) {
		ioctl(i, TIOCNOTTY, 0);
	close(i);
	}

	openlog("printd", LOG_PID, LOG_LPR);
	if (fork() != 0)
		exit(0);

	if ((i = get_lock(MASTER_LOCK, 1)) < 0)
		exit(0);

	chdir(SPOOL_DIR);
	while ((list = job_list_append(NULL, NULL, SPOOL_DIR)) != NULL) {
		job_t **tmp;

		syslog(LOG_DEBUG, "got the queue...");
		for (tmp = list; *tmp != NULL; tmp++)
			if (send_job(*tmp) != 0) {
				char *s = strdup((*tmp)->job_server);
				char *p = strdup((*tmp)->job_printer);

				job_free(*tmp);

				for (tmp++; ((*tmp != NULL) &&
					(strcmp(s, (*tmp)->job_server) == 0));
					tmp++)
					if ((connection_failed == 0) &&
					    (strcmp(p,
						    (*tmp)->job_printer) == 0))
						job_free(*tmp);
					else
						break;
				tmp--;
				free(s);
				free(p);
			}
		free(list);

		/* look for more work to do before we sleep */
		if ((list = job_list_append(NULL, NULL, SPOOL_DIR)) != NULL) {
			list_iterate((void **)list, (VFUNC_T)job_free);
			free(list);
			sleep(60);
		}
	}
	syslog(LOG_DEBUG, "daemon exiting...");
}

static void
append_string(char *s, va_list ap)
{
	char *buf = va_arg(ap, char *);

	if (strlen(buf) != 0)
		strcat(buf, " ");
	strcat(buf, s);
}


static char *
build_string(char **list)
{
	int size = 0;
	char *buf = NULL;

	if (list != NULL) {
		size = list_iterate((void **)list, (VFUNC_T)strlen);
		size += 16;
		buf = malloc(size);
		memset(buf, NULL, size);
		list_iterate((void **)list, (VFUNC_T)append_string, buf);
	}
	return (buf);
}


#define	ADD_PRIMATIVE(job, primative, value) \
	if ((job != NULL) && (value != NULL)) job_primative(job, primative, \
							value);
#define	ADD_SVR4_PRIMATIVE(job, primative, value) \
	if ((job != NULL) && (value != NULL)) job_svr4_primative(job, \
							primative, value);

#define	ADD_INT_PRIMATIVE(job, primative, value, ok) \
	if ((job != NULL) && (value != ok)) { \
				sprintf(buf, "%d", value); \
				job_primative(job, primative, buf);\
				}
#define	ADD_SVR4_INT_PRIMATIVE(job, primative, value, ok) \
	if ((job != NULL) && (value != ok)) { \
					sprintf(buf, "%d", value); \
					job_svr4_primative(job, primative, \
									buf); \
					}

#define	OPTION_ERROR(option, value) \
	if (value != NULL) \
		fprintf(stderr, gettext("\tignoring: %s %s\n"), option, value);

#define	OPTION_ERROR_INT(option, value) \
	if (value != -1) \
		fprintf(stderr, gettext("\tignoring: %s %d\n"), option, value);



/*
 * Main program.  if called with "lpr" use the BSD syntax, if called
 * with "lp", use the SYSV syntax.  If called by any other name,
 * become a transfer daemon.  In the lpr/lp case, build a job and
 * attempt to send it to the print server.  If the server doesn't
 * respond, become a daemon if none is currently running and attempt
 * to xfer all waiting jobs.
 */
main(int ac, char *av[])
{
	ns_bsd_addr_t *binding = NULL;
	int	numFiles = 0,
		queueStdin = 0,
		exit_code = 0;
	char	*program,
		*user,
		hostname[128],
		buf[BUFSIZ];
	job_t *job;


	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);

	if (check_client_spool(NULL) < 0) {
		fprintf(stderr, "couldn't validate local spool area (%s)\n",
			SPOOL_DIR);
		exit(-1);
	}
	if (strcmp(program, "lpr") == 0) {
		if ((printer = getenv((const char *)"PRINTER")) == NULL)
			printer = getenv((const char *)"LPDEST");
		bsd_options(ac, av);
	} else if (strcmp(program, "lp") == 0) {
		if ((printer = getenv((const char *)"LPDEST")) == NULL)
			printer = getenv((const char *)"PRINTER");
		sysv_options(ac, av);
	} else {
		xfer_daemon();
		exit(0);
	}

	if (printer == NULL) {
		ns_printer_t *pobj = ns_printer_get_name(NS_NAME_DEFAULT, NULL);

		if (pobj != NULL) {
			printer = ns_get_value_string(NS_KEY_USE, pobj);
			ns_printer_destroy(pobj);
		}

		if (printer == NULL)
			printer = NS_NAME_DEFAULT;
	}

	if (printer == NULL) {
		fprintf(stderr, gettext("No default destination\n"));
		exit(1);
	}

	if ((binding = ns_bsd_addr_get_name(printer)) == NULL) {
		fprintf(stderr, gettext("%s: unknown printer\n"), printer);
		exit(1);
	}

	if (rejecting(binding->printer) != 0) {
		fprintf(stderr, gettext("%s: queue is disabled\n"), printer);
		exit(1);
	}

	sysinfo(SI_HOSTNAME, hostname, sizeof (hostname));
#ifdef OLD_LP
	if ((linked != 0) && (strcasecmp(binding->server, hostname) == 0) &&
	    (access(OLD_LP, F_OK) == 0)) {
		printer = binding->printer;
		submit_local_lp_linked(program, ac, av);
	}
#endif

	if ((job = job_create(strdup(binding->printer), strdup(binding->server),
			SPOOL_DIR)) == NULL) {
		syslog(LOG_ERR,
			"Error creating job: check spooling directory: %s",
			SPOOL_DIR);
		fprintf(stderr, gettext(
			"Error creating job: check spooling directory: %s\n"),
			SPOOL_DIR);
		exit(-1);
	}

	umask(0);
	user = get_user_name();

	ADD_PRIMATIVE(job, CF_HOST, hostname);
	ADD_PRIMATIVE(job, CF_USER, user);


	if (banner != 0) {
		if (jobName != NULL) {
			ADD_PRIMATIVE(job, CF_JOBNAME, jobName);
		} else if ((av[optind] == NULL) ||
				(strcmp(av[optind], "-") == 0)) {
			ADD_PRIMATIVE(job, CF_JOBNAME, "standard input");
		} else {
			ADD_PRIMATIVE(job, CF_JOBNAME, av[optind]);
		}
		ADD_PRIMATIVE(job, CF_CLASS, (class ? class : hostname));
		ADD_PRIMATIVE(job, CF_PRINT_BANNER, user);
	}

	if (mail != 0) {
		sprintf(buf, "%s@%s", user, hostname);
		ADD_PRIMATIVE(job, CF_MAIL, buf);
	}

	ADD_INT_PRIMATIVE(job, CF_INDENT, indent, -1); /* ASCII */
	ADD_INT_PRIMATIVE(job, CF_WIDTH, width, -1);

	if ((type == CF_PRINT_DVI) || (type == CF_PRINT_DROFF) ||
	    (type == CF_PRINT_TROFF)) {
		ADD_PRIMATIVE(job, CF_FONT_TROFF_R, fontR);
		ADD_PRIMATIVE(job, CF_FONT_TROFF_I, fontI);
		ADD_PRIMATIVE(job, CF_FONT_TROFF_B, fontB);
		ADD_PRIMATIVE(job, CF_FONT_TROFF_S, fontS);
	}

	if (binding->extension == NULL)
		binding->extension = "";

	if ((strcasecmp(binding->extension, NS_EXT_SOLARIS) == 0) ||
	    (strcasecmp(binding->extension, NS_EXT_GENERIC) == 0)) {
		/* RFC1179 compliant don't get this */
		syslog(LOG_DEBUG, "main(): add Solaris extensions");
		ADD_PRIMATIVE(job, CF_SYSV_OPTION, build_string(s5options));
		ADD_SVR4_INT_PRIMATIVE(job, CF_SYSV_PRIORITY, priority, -1);
		ADD_SVR4_PRIMATIVE(job, CF_SYSV_FORM, form);
		ADD_SVR4_PRIMATIVE(job, CF_SYSV_CHARSET, charset);
		ADD_SVR4_PRIMATIVE(job, CF_SYSV_NOTIFICATION, notification);
		ADD_SVR4_PRIMATIVE(job, CF_SYSV_HANDLING, handling);
		ADD_SVR4_PRIMATIVE(job, CF_SYSV_PAGES, pages);
		ADD_SVR4_PRIMATIVE(job, CF_SYSV_TYPE, s5type);
		ADD_SVR4_PRIMATIVE(job, CF_SYSV_MODE, build_string(mode));
	} else if (strcasecmp(binding->extension, NS_EXT_HPUX) == 0) {
		syslog(LOG_DEBUG, "main(): add HP-UX extensions");
		if (s5options != NULL) {
			char buf[BUFSIZ];

			sprintf(buf, " O%s", s5options);
			ADD_PRIMATIVE(job, CF_SOURCE_NAME, buf);
		}
	} else {
		if ((s5options != NULL) || (form != NULL) || (pages != NULL) ||
		    (charset != NULL) || (notification != NULL) ||
		    (handling != NULL) || (s5type != NULL) || (mode != NULL) ||
		    (priority != -1))
			fprintf(stderr, gettext(
		"Warning: %s not configured to handle all lp options:\n"),
			printer);
		OPTION_ERROR("-o", build_string(s5options));
		OPTION_ERROR("-f", form);
		OPTION_ERROR("-P", pages);
		OPTION_ERROR("-S", charset);
		OPTION_ERROR("-p", notification);
		OPTION_ERROR("-H", handling);
		OPTION_ERROR("-T", s5type);
		OPTION_ERROR("-y", build_string(mode));
		OPTION_ERROR_INT("-q", priority);
	}

	syslog(LOG_DEBUG, "main(): add files");
	if (ac-optind > 0) {
		while (optind < ac)
			if (strcmp(av[optind++], "-") == 0)
				queueStdin++;
			else if (job_add_data_file(job, av[optind-1], title,
					type, copies, linked, delete) < 0) {
				switch (errno) {
				case EISDIR:
					fprintf(stderr, gettext(
						"%s: not a regular file\n"),
						av[optind-1]);
					break;
				case ESRCH:
					fprintf(stderr, gettext(
						"%s: empty file\n"),
						av[optind-1]);
					break;
				case ENFILE:
					fprintf(stderr, gettext(
					"too many files, ignoring %s\n"),
						av[optind-1]);
					break;
				case EOVERFLOW:
					fprintf(stderr, gettext(
					"%s: largefile (>= 2GB), ignoring\n"),
						av[optind-1]);
					break;
				default:
					perror(av[optind-1]);
				}
				exit_code = -1;
			} else
				numFiles++;
	} else
		queueStdin++;

	if (queueStdin != 0) {
		char *name;

		/* standard input */
		if ((name = stdin_to_file()) != NULL) {
			if (job_add_data_file(job, name,
					gettext("standard input"),
					type, copies, 0, 0) < 0) {
				switch (errno) {
				case ESRCH:
					fprintf(stderr, gettext(
						"standard input empty\n"));
					break;
				case ENFILE:
					fprintf(stderr, gettext(
				"too many files, ignoring standard input\n"));
					break;
				default:
					perror(name);
				}
				exit_code = -1;
			} else
				numFiles++;
			unlink(name);
			free(name);
		}
	}

	if (numFiles == 0)
		exit(-1);

	if (seteuid(0) < 0)
		perror("seteuid(0)");

	signal(SIGBUS, sigbus_handler);
	chdir(SPOOL_DIR);
	job_store(job);

	if (suppress == 0)
		if (numFiles == 1)
			printf(gettext("request id is %s-%d (%d file)\n"),
				printer, job->job_id, numFiles);
		else
			printf(gettext("request id is %s-%d (%d files)\n"),
				printer, job->job_id, numFiles);
	fflush(stdout);

	/*
	 * bgolden 10/2/96
	 * BUG 1264627
	 * when executed from xemacs, a sighup will kill
	 * the child before the job is sent. ignore the signal
	 */
	signal(SIGHUP, SIG_IGN);

	switch (fork()) {	/* for immediate response */
	case -1:
		syslog(LOG_ERR, "fork() failed: %m");
		break;
	case 0:
		break;
	default:
		exit(exit_code);
	}

	if (send_job(job) == SEND_RETRY) {
		syslog(LOG_DEBUG, "main(): transfer failed");
		start_daemon(0);
	}
	else
		syslog(LOG_DEBUG, "main(): transfer succeeded");

	exit(0);
}
