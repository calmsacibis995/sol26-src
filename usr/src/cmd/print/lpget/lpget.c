/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)lpget.c	1.6	96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
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
	(void) fprintf(stderr,
		gettext("Usage: %s [-k key] [list|(printer) ...]\n"),
		name);
	exit(1);
}

static int
display_kvp(char *key, char *value)
{
	int rc = -1;

	if (value != NULL) {
		rc = 0;
		(void) printf("\n\t%s=%s", key, value);
	} else
		(void) printf(gettext("\n\t%s - undefined"), key);

	return (rc);
}


static int
display_value(ns_printer_t *printer, char *name, char **keys)
{
	int rc = -1;

	if (printer != NULL) {
		rc = 0;
		(void) printf("%s:", name);
		if (keys != NULL) {
			while (*keys != NULL) {
				char *string = ns_get_value_string(*keys,
							printer);
				rc += display_kvp(*keys, string);
				keys++;
			}
		} else {
			ns_kvp_t **list = printer->attributes;

			for (list = printer->attributes;
			    (list != NULL && *list != NULL); list++) {
				char *string;
				if (((*list)->key[0] == '\t') ||
				    ((*list)->key[0] == ' '))
					continue;

				string = ns_get_value_string((*list)->key,
							    printer);
				rc += display_kvp((*list)->key, string);
			}
		}
		(void) printf("\n");
	} else
		(void) printf(gettext("%s: Not Found\n"), name);

	return (rc);
}


/*
 *  main() calls the appropriate routine to parse the command line arguments
 *	and then calls the local remove routine, followed by the remote remove
 *	routine to remove jobs.
 */
int
main(int ac, char *av[])
{
	char *program;
	int c;
	char **keys = NULL;
	char *ns = NULL;
	int exit_code = 0;

	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);
	while ((c = getopt(ac, av, "k:t:n:")) != EOF)
		switch (c) {
		case 'k':
		case 't':
			keys = (char **)list_append((void **)keys,
						    (void *)optarg);
			break;
		case 'n':
			ns = optarg;
			break;
		default:
			Usage(program);
		}

	if (optind >= ac)
		Usage(program);

	if ((ns != NULL) && (strcasecmp(ns, "system") == 0))
		ns = NS_SVC_ETC;

	while (optind < ac) {
		char *name = av[optind++];

		if (strcmp(name, "list") == 0) {
			ns_printer_t **printers = ns_printer_get_list(ns);

			while (printers != NULL && *printers != NULL) {
				exit_code += display_value(*printers,
						(*printers)->name, keys);
				printers++;
			}
		} else
			exit_code = display_value(ns_printer_get_name(name, ns),
						name, keys);


	}
	exit(exit_code);
}
