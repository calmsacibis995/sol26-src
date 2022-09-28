/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)lpset.c	1.5	96/08/28 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif

#include <print/ns.h>
#include <print/misc.h>
#include <print/list.h>

extern char *optarg;
extern int optind, opterr, optopt;
extern char *getenv(const char *);


static void
Usage(char *name)
{
	fprintf(stderr,
		gettext("Usage: %s [-n (system|xfn) ] [-x] [-a key=value] "
			"[-d key] (printer)\n"),
		name);
	exit(1);
}


/*
 *  main() calls the appropriate routine to parse the command line arguments
 *	and then calls the local remove routine, followed by the remote remove
 *	routine to remove jobs.
 */
int
main(int ac, char *av[])
{
	int result = 0;
	int delete_printer = 0;
	int c;
	char	*program = NULL,
		*printer = NULL,
		*ins = NULL,
		*ons = NS_SVC_ETC;
	char	**changes = NULL;
	ns_printer_t *printer_obj = NULL;


	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);

	if (ac < 2)
		Usage(program);

	while ((c = getopt(ac, av, "a:d:n:r:x")) != EOF)
		switch (c) {
		case 'a':
		case 'd':
			changes = (char **)list_append((void**)changes,
						(void *)strdup(optarg));
			break;
		case 'n':
			ons = optarg;
			break;
		case 'r':
			ins = optarg;
			break;
		case 'x':
			delete_printer++;
			break;
		default:
			Usage(program);
		}

	if (optind != ac-1)
		Usage(program);

	printer = av[optind];

	/* check / set the name service for writing */
	if (strcasecmp(NS_SVC_USER, ons) == 0) {
		setuid(getuid());
	} else if ((strcasecmp(NS_SVC_ETC, ons) == 0) ||
			(strcasecmp("system", ons) == 0)) {
		if (getuid() != 0) {
			int len;
			gid_t list[NGROUPS_MAX];


			len = getgroups(sizeof (list), list);
			if (len == -1) {
				fprintf(stderr, gettext(
					"Call to getgroups failed with "
					"errno %d\n"), errno);
				exit(1);
			}

			for (len; len >= 0; len--)
				if (list[len] == 14)
					break;

			if (len == -1) {
				fprintf(stderr, gettext(
				    "Permission denied: not in group 14\n"));
				exit(1);
			}
		}
		ons = NS_SVC_ETC;
	} else if ((strcasecmp(NS_SVC_NISPLUS, ons) == 0) ||
			(strcasecmp(NS_SVC_XFN, ons) == 0) ||
			(strcasecmp("fns", ons) == 0)) {
		ons = NS_SVC_XFN;
	} else {
		fprintf(stderr,
			gettext("%s is not a supported name service.\n"),
			ons);
		exit(1);
	}

	if ((ins != NULL) && (strcasecmp("system", ins) == 0))
		ins = NS_SVC_ETC;

	/* get the printer object */
	if ((printer_obj = ns_printer_get_name(printer, ins)) == NULL) {
		printer_obj = (ns_printer_t *)malloc(sizeof (*printer_obj));
		memset(printer_obj, '\0', sizeof (*printer_obj));
		printer_obj->name = strdup(printer);
	}
	printer_obj->source = ons;

	/* make the changes to it */
	while (changes != NULL && *changes != NULL) {
		char **list;

		if ((list = (char **)strsplit(*(changes++), "=")) != NULL)
			ns_set_value_from_string(list[0], list[1], printer_obj);

	}
	if (delete_printer != 0)
		printer_obj->attributes = NULL;

	/* write it back */
	if (ns_printer_put(printer_obj) != 0) {
		fprintf(stderr, gettext("write operation failed\n"));
		result = 1;
	}

	exit(result);
}
