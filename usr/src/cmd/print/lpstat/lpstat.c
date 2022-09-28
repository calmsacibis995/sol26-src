/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)lpstat.c	1.14	96/10/15 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif

#include <print/ns.h>
#include <print/network.h>
#include <print/misc.h>
#include <print/list.h>
#include <print/job.h>

#include "parse.h"
#include "sysv-functions.h"
#include "bsd-functions.h"

extern char *optarg;
extern int optind, opterr, optopt;
extern char *getenv(const char *);
extern char * getname(void);

int exit_code = 0;


static ns_bsd_addr_t **_printer_bindings = NULL;
static ns_bsd_addr_t **_printer_bindings_unique = NULL;


static int sysvVerbose = 0;
static int sysvDesc = 0;
static int sysvLocal = 0;

static int bsdFormat = SHOW_QUEUE_SHORT_REQUEST;
static int bsdInterval = 0;
static char **bsdv;
static int bsdc;

typedef enum _work_type work_type_t;
typedef struct _work_entry work_entry_t;

enum _work_type { ACCEPTING, DEFAULT, ENTRIES, STATUS, RUNNING, RANK, SUMMARY,
		FULL, USER, DEVICE, FORMS, CLASSES, CHAR_SETS, BSD_QUEUE };
struct _work_entry {
	ns_bsd_addr_t *binding;
	char *printer;
	char *user;
	int  id;
	work_type_t type;
};

static work_entry_t **workList = NULL;


static void
work_list_append(char *printer, char *user, int id, work_type_t type)
{
	work_entry_t *tmp = (work_entry_t *)malloc(sizeof (work_entry_t));

	memset(tmp, NULL, sizeof (*tmp));
	if (user != NULL)
		tmp->user = strdup(user);
	tmp->id = id;
	tmp->type = type;
	if (printer != NULL) {
		tmp->printer = strdup(printer);
		if ((tmp->binding = ns_bsd_addr_get_name(printer)) != NULL)
			workList = (work_entry_t **)list_append(
						(void **)workList,
						(void *)tmp);
		else if (strcmp(printer, NS_NAME_DEFAULT) == 0) {
			fprintf(stderr, gettext("No default destination\n"));
			exit_code = 1;
		} else {
			fprintf(stderr, gettext("%s: unknown printer\n"),
				printer);
			exit_code = 1;
		}
	} else
		workList = (work_entry_t **)list_append((void **)workList,
					(void *)tmp);
}


static void
work_list_append_list(char *printers, char *users, int id, work_type_t type)
{
	char *p;

	if (printers == NULL && users == NULL && id == -1)
		work_list_append(NULL, NULL, -1, type);

	if (printers != NULL)
		for (p = strtok(printers, "\t ,"); p != NULL;
		    p = strtok(NULL, "\t ,"))
			work_list_append(p, NULL, -1, type);

	if (users != NULL)
		for (p = strtok(users, "\t ,"); p != NULL;
		    p = strtok(NULL, "\t ,"))
			work_list_append(NULL, p, -1, type);

	if (id != -1)
		work_list_append(NULL, NULL, id, type);
}


int
work_execute(work_entry_t *work, int verb)
{
	char *printer = NULL;
	print_queue_t *qp;
	int rank = 0;
	int rc = 0;

	switch (work->type) {
	case RUNNING:
		sysv_running();
		return (0);
	case DEFAULT:
		sysv_default();
		return (0);
	case FORMS:
		return (sysv_local_status("-f", work->user, verb,
			gettext("form listing not supported on client\n")));
	case CLASSES:
		return (sysv_local_status("-c", work->user, 0,
			gettext("class listing not supported on client\n")));
	case CHAR_SETS:
		return (sysv_local_status("-S", work->user, verb,
			gettext("char set listing not supported on client\n")));
	}

	if ((work->binding->printer == NULL) &&
	    (work->binding->server == NULL))
		return (0);

	if ((printer = strchr(work->binding->printer, '|')) != NULL)
		*printer = NULL;
	printer = work->binding->printer;

	switch (work->type) {
	case STATUS:
	case RANK:
	case USER:
	case ENTRIES:
		if ((qp = sysv_get_queue(work->binding, sysvLocal)) != NULL) {
			qp->jobs = job_list_append(qp->jobs, printer,
						SPOOL_DIR);
			switch (work->type) {
			case STATUS:
				if (work->binding->pname)
					rc += sysv_queue_state(qp,
						work->binding->pname, verb,
						sysvDesc);
				else
					rc += sysv_queue_state(qp, printer,
						verb, sysvDesc);
				break;
			case ENTRIES:
			case USER:
				rank = -1;
				/*FALLTHRU*/
			case RANK:
				(void) list_iterate((void **)qp->jobs,
					(VFUNC_T)vsysv_queue_entry, printer,
					work->user, work->id, verb, &rank);
				break;
			}
			if ((qp->state == RAW) && (qp->status != NULL)) {
				printf("%s", qp->status);
				qp->status = NULL;
			}
		}
		break;
	default:
		switch (work->type) {
		case DEVICE:
			rc += sysv_system(work->binding);
			break;
		case ACCEPTING:
			rc += sysv_accept(work->binding);
			break;
		case BSD_QUEUE:
			do {
				if (bsdInterval != 0)
					clear_screen();
				bsd_queue(work->binding, bsdFormat, bsdc,
					bsdv);
				if (bsdInterval != 0)
					sleep(bsdInterval);
			} while (bsdInterval != 0);
			break;
		}
	}

	return (rc);
}


static int
vbinding_execute(ns_bsd_addr_t *binding, va_list ap)
{
	work_entry_t *work = va_arg(ap, work_entry_t *);
	int verbose = va_arg(ap, int);

	work->binding = binding;
	return (work_execute(work, verbose));
}





static int
vwork_list_execute(work_entry_t *work, va_list ap)
{
	int verbose = va_arg(ap, int);
	work_entry_t tmp;
	int rc = 0;

	if ((work->binding == NULL) || ((work->binding->printer == NULL) &&
	    (work->binding->server == NULL))) {
		switch (work->type) {
		case RUNNING:
		case DEFAULT:
		case CLASSES:
		case FORMS:
		case CHAR_SETS:
			rc += work_execute(work, verbose);
			break;
		case STATUS:
		case RANK:
		case ENTRIES:
		case ACCEPTING:
		case DEVICE:
		case SUMMARY:
		case USER:
		case FULL:

			if (_printer_bindings == NULL) {
				if (sysvLocal == 0) {
					_printer_bindings =
						ns_bsd_addr_get_all(NOTUNIQUE);
					_printer_bindings_unique =
						ns_bsd_addr_get_all(UNIQUE);
				} else {
					_printer_bindings =
						ns_bsd_addr_get_list(NOTUNIQUE);
					_printer_bindings_unique =
						ns_bsd_addr_get_list(UNIQUE);
				}
			}
			if ((_printer_bindings != NULL) &&
				(_printer_bindings_unique != NULL)) {


				memcpy(&tmp, work, sizeof (tmp));

				if (work->type == FULL ||
				    work->type == SUMMARY) {
					tmp.type = RUNNING;
					rc += work_execute(&tmp, verbose);

					tmp.type = DEFAULT;
					rc += work_execute(&tmp, verbose);

					tmp.type = CLASSES;
					rc += work_execute(&tmp, verbose);

					tmp.type = DEVICE;
					rc += list_iterate(
						(void **)_printer_bindings,
						(VFUNC_T)vbinding_execute,
						&tmp, verbose);

					tmp.type = CHAR_SETS;
					rc += work_execute(&tmp, verbose);


				if (work->type == FULL) {
					tmp.type = ACCEPTING;
					rc += list_iterate(
					    (void **)_printer_bindings_unique,
					    (VFUNC_T)vbinding_execute,
					    &tmp, verbose);
					tmp.type = STATUS;
					rc += list_iterate(
						(void **)_printer_bindings,
						(VFUNC_T)vbinding_execute,
						&tmp, verbose);
					tmp.type = ENTRIES;
					rc += list_iterate(
					    (void **)_printer_bindings_unique,
					    (VFUNC_T)vbinding_execute,
					    &tmp, verbose);
				}

				/*
				 * Put this here to preserve same ordering
				 * as found in previous releases.
				 */
				if (work->type == FULL ||
				    work->type == SUMMARY) {

					tmp.type = FORMS;
					rc += work_execute(&tmp, verbose);
				}

			} else if (work->type == USER ||
					work->type == ACCEPTING ||
					work->type == ENTRIES ||
					work->type == RANK) {

					tmp.type = work->type;
					rc += list_iterate(
					    (void **)_printer_bindings_unique,
					    (VFUNC_T)vbinding_execute,
					    &tmp, verbose);
			} else
				rc += list_iterate(
					(void **)_printer_bindings,
					(VFUNC_T)vbinding_execute, &tmp,
					verbose);
			}
			return (rc);
		default:
			return (rc);
		}
	} else {
		rc += work_execute(work, verbose);
	}
	return (rc);
}

static void
sysv_options(int ac, char *av[])
{
	char	**argv,
		*program,
		*p;
	int	c;
	int	argc = 0;
	char * user;

	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	chdir(SPOOL_DIR);

	argv = (char **)calloc((ac + 1), sizeof (char *));
	for (argc = 0; argc < ac; argc++)
		argv[argc] = av[argc];
	argv[argc++] = "--";

	while ((c = getopt(argc, argv, "R:a:c:f:o:p:u:v:DLSdlrst")) != EOF) {
		switch (c) {	/* these may or may not have an option */
		case 'R':
		case 'a':
		case 'c':
		case 'f':
		case 'o':
		case 'p':
		case 'u':
		case 'v':
			if (optarg[0] == '-') {
				/* this check stop a possible infinite loop */
				if ((optind > 1) && (argv[optind-1][1] != c))
					optind--;
				optarg = NULL;
			} else if (strcmp(optarg, "all") == 0)
				optarg = NULL;
		}

		switch (c) {
		case 'D':
			sysvDesc++;
			break;
		case 'L':
			sysvLocal++;
			/* undocumented option that keeps it long and local */
			break;
		case 'R':
			work_list_append_list(optarg, NULL, -1, RANK);
			break;
		case 'S':
			work_list_append_list(NULL, optarg, -1, CHAR_SETS);
			break;
		case 'a':
			if (optarg == NULL)
			work_list_append_list(optarg, NULL, -1, ACCEPTING);
			else {
				while (optarg[0] != '-') {
					work_list_append_list(optarg, NULL,
								-1, ACCEPTING);
					optarg = argv[optind];
					optind++;
					}
				optind--;
			}
			break;
		case 'c':
			work_list_append_list(NULL, optarg, -1, CLASSES);
			break;
		case 'd':
			work_list_append(NULL, NULL, -1, DEFAULT);
			break;
		case 'f':
			work_list_append_list(NULL, optarg, -1, FORMS);
			break;
		case 'l':
			sysvVerbose++;
			break;
		case 'o':
			if (optarg == NULL)
				work_list_append_list(NULL, NULL, -1, ENTRIES);
			else if ((p = strrchr(optarg, '-')) == NULL)
				work_list_append(optarg, NULL, -1, ENTRIES);
			else if (ns_bsd_addr_get_name(optarg) != NULL) {
				work_list_append(optarg, NULL, -1, ENTRIES);
			} else {
				*p++ = NULL;
				work_list_append(optarg, NULL, atoi(p),
					ENTRIES);
			}
			break;
		case 'p':
			if (optarg == NULL)
			work_list_append_list(optarg, NULL, -1, STATUS);
			else {
				while (optarg[0] != '-') {
					work_list_append_list(optarg, NULL,
								-1, STATUS);
					optarg = argv[optind];
					optind++;
					}
				optind--;
			}
			break;
		case 'r':
			work_list_append(NULL, NULL, -1, RUNNING);
			break;
		case 's':
			work_list_append(NULL, NULL, -1, SUMMARY);
			break;
		case 't':
			work_list_append(NULL, NULL, -1, FULL);
			break;
		case 'u':
			work_list_append_list(NULL, optarg, -1, USER);
			break;
		case 'v':
			if (optarg == NULL)
			work_list_append_list(optarg, NULL, -1, DEVICE);
			else {
				while (optarg[0] != '-') {
					work_list_append_list(optarg, NULL,
								-1, DEVICE);
					optarg = argv[optind];
					optind++;
					}
				optind--;
			}
			break;
		default:
			fprintf(stderr,
				"Usage: %s [-drtslDL] [-R [list]] [-S [list]] "
				"[-a [list]] [-c [list]] [-f [list]] "
				"[-o [list]] [-p [printer]] [-u [list]] "
				"[-v [list]] [list]\n",
				av[0]);
			exit(1);
		}
	}
	for (argc = optind; argc < ac; argc++) {	/* add requests */
		char *p;

		if ((p = strrchr(av[argc], '-')) == NULL)
			work_list_append(av[argc], NULL, -1, ENTRIES);
		else if (ns_bsd_addr_get_name(av[argc]) != NULL) {
			work_list_append(av[argc], NULL, -1, ENTRIES);
		} else {
			*p++ = NULL;
			work_list_append(av[argc], NULL, atoi(p), ENTRIES);
		}
	}
	if ((workList == NULL) && (ac == 1)) {
		user = get_user_name();
		work_list_append(NULL, user, -1, USER);
	}
}


static void
bsd_options(int ac, char **av)
{
	char	*printer;
	int	c;

	if ((bsdv = (char **)calloc(ac, sizeof (char *))) == NULL)
		return;

	if ((printer = getenv((const char *)"PRINTER")) == NULL)
		printer = getenv((const char *)"LPDEST");
	if (printer == NULL)
		printer = NS_NAME_DEFAULT;

	while ((c = getopt(ac, av, "lP:+:")) != EOF)
		switch (c) {
		case 'l':
			bsdFormat = SHOW_QUEUE_LONG_REQUEST;
			break;
		case 'P':
			printer = optarg;
			break;
		default:
			fprintf(stderr,
		"Usage: %s [-P printer] [-l] [+ interval] [list]\n",
				av[0]);
			exit(1);
		}

	work_list_append(printer, NULL, -1, BSD_QUEUE);

	while (optind < ac)
		if (av[optind][0] == '+')
			bsdInterval = atoi(&av[optind++][1]);
		else
			bsdv[bsdc++] = av[optind++];
}


int
main(int ac, char *av[])
{
	char *program;
	int rc = 0;

	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);

	if (check_client_spool(NULL) < 0) {
		fprintf(stderr, "couldn't validate local spool area (%s)\n",
			SPOOL_DIR);
		exit(1);
	}
	chdir(SPOOL_DIR);

	if (strcmp(program, "lpq") == 0)
		bsd_options(ac, av);
	else {
		sysv_options(ac, av);
	}

	rc = list_iterate((void **)workList, (VFUNC_T)vwork_list_execute,
				sysvVerbose, sysvDesc);

	if (exit_code == 0)
		exit_code = rc;

	exit(exit_code);
}
