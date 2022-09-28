/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)lpmove.c	1.3	96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif

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


/*
 *	creates a new job moves/modifies control data and data files.
 */
static int
vjob_reset_destination(job_t *job, va_list ap)
{
	char	*user = va_arg(ap, char *),
		buf[BUFSIZ];
	int	id = va_arg(ap, int),
		lock,
		killed_daemon = 0;
	ns_bsd_addr_t *src = va_arg(ap, ns_bsd_addr_t *),
		*dst = va_arg(ap, ns_bsd_addr_t *);


	if ((id != -1) && (job->job_id != id))
		return (0);
	if (strcmp(job->job_server, src->server) != 0)
		return (0);
	if ((strcmp(user, "root") != 0) &&
	    (strcmp(user, job->job_user) != 0))
		return (0);

	while ((lock = get_lock(job->job_cf->jf_src_path, 0)) < 0) {
		kill_process(job->job_cf->jf_src_path);
		killed_daemon = 1;
	}
	/* just do it */
	sprintf(buf, "%s:%s", dst->printer, dst->server);
	ftruncate(lock, 0);
	write(lock, buf, strlen(buf));
	close(lock);
	printf(gettext("moved: %s-%d to %s-%d\n"), src->printer, job->job_id,
		dst->printer, job->job_id);
	return (killed_daemon);
}


static int
job_move(char *user, int id, char *src, char *dst)
{
	job_t	**list = NULL;
	ns_bsd_addr_t	*src_binding,
			*dst_binding;
	int	rc = 0;

	if ((src_binding = ns_bsd_addr_get_name(src)) == NULL) {
		fprintf(stderr, "%s: unknown printer\n", src);
		return (-1);
	}

	if ((dst_binding = ns_bsd_addr_get_name(dst)) == NULL) {
		fprintf(stderr, "%s: unknown printer\n", dst);
		return (-1);
	}

	if ((list = job_list_append(NULL, src_binding->printer,
				    SPOOL_DIR)) != NULL)
		rc = list_iterate((void **)list,
				(VFUNC_T)vjob_reset_destination, user, id,
				src_binding, dst_binding);
	return (rc);

}

/*
 *	move print jobs from one queue to another.  This gets the lock
 *  file (killing the locking process if necessary), Moves the jobs, and
 *  restarts the transfer daemon.
 */

#define	OLD_LPMOVE	"/usr/lib/lp/local/lpmove"

main(int ac, char *av[])
{
	char	*program = NULL,
		*dst = NULL,
		*user = get_user_name();
	int	argc;
	char	**argv = NULL;
	int	killed_daemon = 0;

	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);

	if (access(OLD_LPMOVE, F_OK) == 0) {	/* copy args for local move */
		argv = (char **)calloc(ac + 1, sizeof (char *));
		for (argc = 0; argc < ac; argc++)
			if (av[argc] != NULL)
				argv[argc] = strdup(av[argc]);
	}

	chdir(SPOOL_DIR);
	dst = av[--ac];
	while (--ac > 0) {
		char	*src = av[ac],
			*s;
		int id = -1;

		if ((s = strrchr(src, '-')) != NULL) {
			*(s++) = NULL;

			id = atoi(s);
			if ((id <= 0) && (errno == EINVAL)) {
				id = -1;
				*(--s) = '-';
			}
		}
		killed_daemon += job_move(user, id, src, dst);
	}
	if (killed_daemon != 0)
		start_daemon(1);

	if (argv != NULL)
		execv(OLD_LPMOVE, argv);

	exit(0);
}
