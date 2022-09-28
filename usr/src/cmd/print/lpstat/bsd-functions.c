/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)bsd-functions.c	1.4	96/07/10 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif
extern char *getenv(const char *);

#include <print/ns.h>
#include <print/network.h>
#include <print/misc.h>
#include <print/list.h>
#include <print/job.h>

#include "bsd-functions.h"


static char *order[] = {
	"", "st", "nd", "rd", "th", "th", "th", "th", "th", "th", "th" };

static char *
show_rank(int i)
{
	static char rank[12];
	if ((i%100)/10 == 1)
		sprintf(rank, "%dth", i);
	else
		sprintf(rank, "%d%s", i, order[i%10]);
	return (rank);
}


int
vadd_file(jobfile_t *file, va_list ap)
{
	char *mesg = va_arg(ap, char *);

	if (file != NULL) {
		strncat(mesg, file->jf_name, 38);
		strncat(mesg, " ", 38);
		return (file->jf_size);
	}
	return (0);
}

int
vprint_file(jobfile_t *file, va_list ap)
{
	if (file != NULL)
		printf("\t%-33.33s%d bytes\n", file->jf_name, file->jf_size);
	return (0);
}

int
vprint_job(job_t *job, va_list ap)
{
	int	fileSize = 0,
		jobSize = 0,
		*rank,
		format,
		curr,
		printIt = 0,
		ac;
	char	fileList[48],
		*cf,
		*tmp,
		*printer,
		**av;

	printer = va_arg(ap, char *);
	format = va_arg(ap, int);
	rank = va_arg(ap, int *);
	ac = va_arg(ap, int);
	av = va_arg(ap, char **);

	if (strcmp(job->job_printer, printer) != 0)
		return (0);

	if (ac > 0) {
		for (curr = 0; curr < ac; curr++)
			if ((av[curr][0] >= '0') && (av[curr][0] <= '9') &&
				(job->job_id == atoi(av[curr])) ||
				(strcmp(job->job_user, av[curr]) == 0)) {
					printIt++;
					break;
			}
	} else
		printIt++;

	if (printIt != 0) {
		if (format == SHOW_QUEUE_SHORT_REQUEST) {
			memset(fileList, 0, sizeof (fileList));
			jobSize = list_iterate((void **)job->job_df_list,
				(VFUNC_T)vadd_file, fileList);
			printf("%-7.7s%-8.8s	  %3.3d  %-38.38s%d bytes\n",
				show_rank((*rank)++), job->job_user,
				job->job_id, fileList, jobSize);
		} else {
			printf("%s: %-7.7s \t\t\t\t [job %.3d%s]\n",
				job->job_user, show_rank((*rank)++),
				job->job_id, job->job_host);
			list_iterate((void **)job->job_df_list,
					(VFUNC_T)vprint_file);
			printf("\n");
		}
	}
}

int
vjob_count(job_t *job, va_list ap)
{
	int	curr,
		ac;
	char	*cf,
		*printer,
		**av;

	printer = va_arg(ap, char *);
	ac = va_arg(ap, int);
	av = va_arg(ap, char **);

	if (strcmp(job->job_printer, printer) != 0)
		return (0);

	if (ac == 0)
		return (1);

	for (curr = 0; curr < ac; curr++)
		if ((av[curr][0] >= '0') && (av[curr][0] <= '9') &&
			(job->job_id == atoi(av[curr])) ||
			(strcmp(job->job_user, av[curr]) == 0))
				return (1);

	return (0);
}


void
clear_screen()	 /* for now use tput rather than link in UCB stuff */
{
	system("/bin/tput clear");
}


int
bsd_queue(ns_bsd_addr_t *binding, int format, int ac, char *av[])
{
	char	buf[BUFSIZ],
		*server = NULL,
		*printer = NULL;
	job_t	**list = NULL;
	int	nd = -1;

	server = binding->server;
	printer = binding->printer;

	if ((nd = net_open(server, 15)) >= 0) {
		char buf[BUFSIZ];

		memset(buf, 0, sizeof (buf));
		while (ac--) { /* potential SEGV if av's are more than BUFSIZ */
			strcat(buf, av[ac]);
			strcat(buf, " ");
		}

		net_printf(nd, "%c%s %s\n", format, printer, buf);
#ifdef SUNOS_4
		do {
		memset(buf, 0, sizeof (buf));
		if ((ac = net_read(nd, buf, sizeof (buf))) > 0)
			printf("%s", buf);
		} while (ac > 0);
#else
		while (memset(buf, 0, sizeof (buf)) &&
			(net_read(nd, buf, sizeof (buf)) > 0))
			printf("%s", buf);
#endif

		net_close(nd);
	}

	if (nd < 0) {
		if (server != NULL)
			fprintf(stderr, gettext(
				"could not talk to print service at %s\n"),
				server);
		else
			fprintf(stderr, gettext(
				"could not locate server for printer: %s\n"),
				printer);
	}

	if (((list = job_list_append(NULL, printer, SPOOL_DIR)) != NULL) &&
	    (list_iterate((void **)list, (VFUNC_T)vjob_count, printer,
			ac, av) != 0)) {
		if ((nd < 0) && (format == SHOW_QUEUE_SHORT_REQUEST))
			printf(gettext(
			"Rank\tOwner	Job\tFiles\t\t\t\t\tTotal Size\n"));
	}

	if (format == SHOW_QUEUE_LONG_REQUEST)
		printf("\n");

	nd = 1;
	list_iterate((void **)list, (VFUNC_T)vprint_job, printer, format,
			&nd, ac, av);

	return (0);
}
