/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)sysv-functions.c	1.9	96/08/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif

#include <print/ns.h>
#include <print/list.h>
#include <print/misc.h>
#include <print/job.h>
#include <print/network.h>

#include "parse.h"
#include "sysv-functions.h"

extern char *getenv(const char *);

static print_queue_t **_queue_list = NULL;

#define	ALL "all"
int
compair_user(char *full, char *user, char *host)
{
	char	*u1,
		*h1;

	if (full == NULL)
		return (0);

	if ((strcmp(full, ALL) == 0) || strcmp(full, user) == 0)
		return (0);
	if ((u1 = strchr(full, '!')) == NULL)
		return (-1);
	h1 = strcdup(full, '!');
	u1++;
	if (((strcmp(u1, ALL) == 0) || (strcmp(u1, user) == 0)) &&
	    (strcmp(h1, host) == 0)) {
		free(h1);
		return (0);
	}
	if (((strcmp(h1, ALL) == 0) || (strcmp(h1, host) == 0)) &&
	    (strcmp(u1, user) == 0)) {
		free(h1);
		return (0);
	}
	free(h1);
	return (-1);
}


int
compair_queue_binding(print_queue_t *queue, ns_bsd_addr_t *binding)
{
	if ((queue == NULL) || (binding == NULL))
		return (-1);
	if ((strcmp(queue->binding->printer, binding->printer) == 0) &&
	    (strcmp(queue->binding->server, binding->server) == 0))
				return (0);
	return (-1);
}



char *
get_queue_buffer(ns_bsd_addr_t *binding)
{
	char	*q = NULL,
		*p = NULL,
		*server,
		*printer,
		*retVal = NULL;
	int	nd,
		count = 0,
		q_size = 0,
		p_left = 0;

	server = binding->server;
	printer = binding->printer;

	if ((nd = net_open(server, 15)) < 0) {
		char err[128];

		sprintf(err, gettext("server %s not responding\n\n"), server);
		return (strdup(err));
	}

	net_printf(nd, "%c%s \n", SHOW_QUEUE_LONG_REQUEST, printer);

	do {
		p += count;
		if ((p_left -= count) < 10) {
			char *t;

#ifdef SUNOS_4
			if (q == NULL) {
				p = q = malloc(BUFSIZ);
				memset(p, NULL, BUFSIZ);
				q_size = BUFSIZ;
			} else {
				q_size += BUFSIZ;
				t = malloc(q_size);
				memset(t, NULL, q_size);
				strcpy(t, q);
				free(q);
				p = t + (p - q);
				q = t;
			}
#else
			if ((t = (char *)realloc(q, q_size += BUFSIZ)) != q) {
				p = t + (p - q);
				q = t;
			}
#endif
			p_left += BUFSIZ;
			memset(p, NULL, p_left);
		}
	} while ((count = net_read(nd, p, p_left)) > 0);

	net_close(nd);

	return (q);
}



print_queue_t *
sysv_get_queue(ns_bsd_addr_t *binding, int local)
{
	print_queue_t *qp = NULL;
	char	*buf;

	if (_queue_list != NULL)	/* did we already get it ? */
		if ((qp = (print_queue_t *)list_locate((void **)_queue_list,
			(COMP_T)compair_queue_binding, binding)) != NULL)
			return (qp);

	if (local == 0)
		buf = (char *)get_queue_buffer(binding);
	else
		buf = strdup("no entries\n");

	if (buf != NULL) {
		qp = parse_bsd_queue(binding, buf, strlen(buf));
		_queue_list = (print_queue_t **)list_append((void **)
						_queue_list, (void *)qp);
	}
	return (qp);
}






	/*
	 *	SYSV (lpstat) specific routines
	 */

int
vJobfile_size(jobfile_t *file)
{
	if (file != NULL)
		return (file->jf_size);
	return (0);
}


void
vsysv_queue_entry(job_t *job, va_list ap)
{
	char	id[128],
		user[128],
		*printer = va_arg(ap, char *),
		*in_user = va_arg(ap, char *);
	int	in_id = va_arg(ap, int),
		verbose = va_arg(ap, int),
		*rank = va_arg(ap, int *),
		size = 0;

	if ((in_id != -1) && (in_id != job->job_id))
		return;
	if (compair_user(in_user, job->job_user, job->job_host) != 0)
		return;

	sprintf(id, "%.16s-%-5d", printer, job->job_id);
	sprintf(user, "%s!%s", job->job_host, job->job_user);
	size = list_iterate((void **)job->job_df_list, (VFUNC_T)vJobfile_size);
	if (*rank >= 0)
		printf("%d ", (*rank)++);
	printf("%-*s %-*s %*ld %s%s", (*rank >= 0 ? 20 : 22), id, 15, user, 7,
		size, (*rank >= 0 ? "" : "  "), short_date());
	if (verbose == 0) {
		if (*rank == 1)
			printf(" on %s", printer);
			printf("\n");
		} else
			printf("\n\t%s %s\n", ((*rank > 1) ? "on" : "assigned"),
				printer);
}


#define	OLD_LPSTAT "/usr/lib/lp/local/lpstat"		/* for -c -f -S */
#ifdef OLD_LPSTAT
local_printer(char *name)
{
	struct stat st;
	char buf[128];

	sprintf(buf, "/etc/lp/printers/%s", name);
	if ((stat(buf, &st) < 0) || !(S_ISDIR(st.st_mode)))
		return (-1);
	else
		return (0);
}

int
sysv_local_status(char *option, char *arg, int verbose, char *invalid)
{
	pid_t stat;

	if (access(OLD_LPSTAT, F_OK) == 0) {
		char buf[BUFSIZ];

		/*
		 * Need the fflush to preserve output order when
		 * output re-directed to a file. Close of old lpstat
		 * flushes buffers causing old lpstat output to preceed
		 * all other output to the file.
		 */
		fflush(stdout);
		fflush(stderr);
		sprintf(buf, "%s %s %s%s", OLD_LPSTAT, option,
			(arg ? arg : ""), (verbose ? " -l" : ""));
		stat = system(buf);
		if (WIFEXITED(stat)) {
			return (WEXITSTATUS(stat));
		} else {
			if (stat == -1)
				return (errno);
			else
				return (ENOMSG);
		}
	} else
		printf("%s", invalid);

	return (0);
}
#endif


int
sysv_queue_state(print_queue_t *qp, char *printer, int verbose, int description)
{
#ifdef OLD_LPSTAT
	pid_t stat;
	int estatus;

	if ((local_printer(printer) == 0) && (access(OLD_LPSTAT, F_OK) == 0)) {
		char buf[BUFSIZ];

		/* see sysv_local_status for reason for fflush */
		fflush(stdout);
		fflush(stderr);
		sprintf(buf, "%s -p %s%s%s", OLD_LPSTAT, printer,
			(verbose ? " -l" : ""), (description ? " -D" : ""));
		stat = system(buf);
		if (WIFEXITED(stat)) {
			return (WEXITSTATUS(stat));
		} else {
			if (stat == -1)
				return (errno);
			else
				return (ENOMSG);
		}


	}
#endif

	printf("printer %s ", printer);
	switch (qp->state) {
	case IDLE:
		printf(gettext("is idle. "));
		break;
	case PRINTING:
		printf(gettext("now printing %s-%d. "), printer,
			qp->jobs[0]->job_id);
		break;
	case FAULTED:
		printf(gettext("faulted printing %s-%d. "), printer,
			(qp->jobs != NULL ? qp->jobs[0]->job_id : 0));
		break;
	case RAW:
		printf(gettext("unknown state. "));
		break;
	default:
		printf(gettext("disabled. "));
	}
	printf(gettext("enabled since %s. available.\n"), long_date());
	if (qp->state == FAULTED)
		printf("\t%s\n", qp->status);
	if (description != 0) {
		ns_printer_t *pobj;
		char *desc;

		if (((pobj = ns_printer_get_name(qp->binding->printer, NULL))
		    != NULL) &&
		    ((desc = ns_get_value(NS_KEY_DESCRIPTION, pobj)) != NULL))
			printf(gettext("\tDescription: %s\n"), desc);
		else
			printf(gettext("\tDescription: %s@%s\n"),
				qp->binding->printer, qp->binding->server);
	}
	if (verbose != 0)
		printf(gettext("\tRemote Name: %s\n\tRemote Server: %s\n"),
			qp->binding->printer, qp->binding->server);

	return (0);
}


int
sysv_accept(ns_bsd_addr_t *binding)
{
#ifdef OLD_LPSTAT
	pid_t stat;

	if ((local_printer(binding->printer) == 0) &&
		(access(OLD_LPSTAT, F_OK) == 0)) {
		char buf[BUFSIZ];

		/* see sysv_local_status for reason for fflush */
		fflush(stdout);
		fflush(stderr);
		sprintf(buf, "%s -a %s", OLD_LPSTAT, binding->printer);
		stat = system(buf);

		if (WIFEXITED(stat)) {
			return (WEXITSTATUS(stat));
		} else {
			if (stat == -1)
				return (errno);
			else
				return (ENOMSG);
		}
	}
#endif


	if (binding->pname != NULL)
		printf(gettext("%s accepting requests since %s\n"),
			binding->pname, long_date());
	else
		printf(gettext("%s accepting requests since %s\n"),
			binding->printer, long_date());

	return (0);
}


int
sysv_system(ns_bsd_addr_t *binding)
{
	char *host;
	char *printer;
	pid_t stat;

	if (binding->pname)
		printer = binding->pname;
	else
		printer = binding->printer;
	host = binding->server;
#ifdef OLD_LPSTAT

	if ((local_printer(printer) == 0) && (access(OLD_LPSTAT, F_OK) == 0)) {
		char buf[BUFSIZ];

		/* see sysv_local_status for reason for fflush */
		fflush(stdout);
		fflush(stderr);
		sprintf(buf, "%s -v %s", OLD_LPSTAT, printer);
		stat = system(buf);

		if (WIFEXITED(stat)) {
			return (WEXITSTATUS(stat));
		} else {
			if (stat == -1)
				return (errno);
			else
				return (ENOMSG);
		}
	} else
#endif
	if (printer && host) {
		if (strcmp(printer, binding->printer) == 0)
			printf(gettext("system for %s: %s\n"), printer, host);
		else
			printf(gettext("system for %s: %s (as printer %s)\n"),
				printer, host, binding->printer);
	}

	return (0);
}


void
sysv_running()
{
	int lock;
	struct stat st;

	lock = stat("/usr/spool/lp/SCHEDLOCK", &st);
	if (lock < 0)
		printf(gettext("scheduler is not running\n"));
	else
		printf(gettext("scheduler is running\n"));
}


void
sysv_default()
{
	char *printer;

	if ((printer = getenv((const char *)"LPDEST")) == NULL)
		printer = getenv((const char *)"PRINTER");
	if (printer == NULL) {
		ns_bsd_addr_t *addr;
		if ((addr = ns_bsd_addr_get_name(NS_NAME_DEFAULT)) != NULL) {
			static char buf[64];

			sprintf(buf, "%s", addr->printer);
			printer = buf;
		}
	}
	if (printer != NULL)
		printf(gettext("system default destination: %s\n"), printer);
	else
		printf(gettext("no system default destination\n"));
}
