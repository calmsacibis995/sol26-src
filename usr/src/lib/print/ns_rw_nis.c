/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident  "@(#)ns_rw_nis.c 1.13     96/08/07 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <string.h>
#include <stdarg.h>
#include <pwd.h>

#include <print/ns.h>
#include <print/list.h>

#include "ns_rw_nis.h"
#include "ns_rw_file.h"

static char	*domain = NULL;

struct cb_args {
	ns_printer_t *(*conv)(char *, char *);
	char *svc;
	ns_printer_t **list;
};
typedef struct cb_args cb_args_t;

/*ARGSUSED*/
static int
cb(int stat, char *key, int keylen, char *val, int vallen, char *data)
{
	if (stat == YP_TRUE) {
		/*LINTED this is from example code */
		cb_args_t *arg = (cb_args_t *)data;
		char *value;
		ns_printer_t *printer;

		val[vallen] = 0;
		key[keylen] = 0;
		value = strdup(val);
		printer = (arg->conv)(value, arg->svc);

		if (list_locate((void **)arg->list,
				(int (*)(void *, void*))ns_printer_match_name,
				printer->name) == NULL)

			arg->list =  (ns_printer_t **)list_append(
						(void **)arg->list, printer);
	}
	return (0);
}


ns_printer_t *
_nis_get_name(const char *map, const char *name,
		ns_printer_t *(*conv)(char *, char *), char *svc)
{
	ns_printer_t *printer = NULL;
	int	len;
	char	*entry = NULL;

	if (domain == NULL)
		(void) yp_get_default_domain(&domain);

	if (yp_match(domain, (char *)map, (char *)name, strlen(name), &entry,
			&len) == 0) {
		entry[len] = NULL;
		printer = (conv)(strdup(entry), svc);
	}

	return (printer);
}


ns_printer_t **
_nis_get_list(const char *map, ns_printer_t *(*conv)(char *, char *),
		char *svc)
{
	cb_args_t arg;
	struct ypall_callback ypcb;

	if (domain == NULL)
		(void) yp_get_default_domain(&domain);

	arg.conv = conv;
	arg.svc = svc;
	arg.list = NULL;
	ypcb.foreach = cb;
	ypcb.data = (char *) &arg;
	(void) yp_all(domain, (char *)map, &ypcb);
	/* Should qsort list here (maybe soon) */

	return (arg.list);
}


/*
 * Run the remote command.  We aren't interested in any io, Only the
 * return code.
 */
static int
remote_command(char *command, char *host)
{
	struct passwd *pw;

	if ((pw = getpwuid(getuid())) != NULL) {
		int fd;

		if ((fd = rcmd(&host, htons(514), pw->pw_name, "root",
				command, NULL)) < 0)
			return (-1);
		close(fd);
		return (0);
	} else
		return (-1);
}


/*
 * This isn't all that pretty, but you can update NIS if the machine this
 * runs on is in the /.rhosts or /etc/hosts.equiv on the NIS master.
 *   copy it local, update it, copy it remote
 */
#define	TMP_PRINTERS_FILE	"/tmp/printers.NIS"
#define	NIS_MAKEFILE		"/var/yp/Makefile"
#define	MAKE_EXCERPT		"/usr/lib/print/Makefile.yp"
/*ARGSUSED*/
int
_nis_put_printer(const char *map, const ns_printer_t *printer,
		ns_printer_t *(*iconv)(char *, char *), char *svc,
		char *(*oconv)(ns_printer_t *))
{
	char *tmp = NULL;
	char lfile[BUFSIZ];
	char rfile[BUFSIZ];
	char *host = NULL;
	char cmd[BUFSIZ];

	if (domain == NULL)
		(void) yp_get_default_domain(&domain);

	if ((yp_master(domain, (char *)map, &host) != 0) &&
	    (yp_master(domain, "passwd.byname", &host) != 0))
		return (-1);

	sprintf(lfile, "/tmp/%s", map);
	sprintf(rfile, "root@%s:/etc/%s", host, map);

	if (((tmp = strrchr(rfile, '.')) != NULL) &&
	    (strcmp(tmp, ".byname") == 0))
		*tmp = NULL;	/* strip the .byname */

	/* copy it local */
	sprintf(cmd, "rcp %s %s >/dev/null 2>&1", rfile, lfile);
	system(cmd);	/* could fail because it doesn't exist */


	/* update it */
	if (_file_put_printer(lfile, printer, iconv, NS_SVC_ETC, oconv)
	    != 0)
		return (-1);

	/* copy it back */
	sprintf(cmd, "rcp %s %s >/dev/null 2>&1", lfile, rfile);
	if (system(cmd) != 0)
		return (-1);

	/* copy the Makefile excerpt */
	sprintf(cmd, "rcp %s root@%s:%s.print >/dev/null 2>&1",
		MAKE_EXCERPT, host, NIS_MAKEFILE);
	if (system(cmd) != 0)
		return (-1);

	/* run the make */
	sprintf(cmd, "/bin/sh -c 'PATH=/usr/ccs/bin:/bin:/usr/bin:$PATH "
		"make -f %s -f %s.print printers.conf >/dev/null 2>&1'",
		NIS_MAKEFILE, NIS_MAKEFILE);

	return (remote_command(cmd, host));
}
