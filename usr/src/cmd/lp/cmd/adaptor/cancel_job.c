/*
 * Copyright (c) 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)cancel_job.c	1.5	96/10/04 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/systeminfo.h>
#include <string.h>
#include <libintl.h>

#include <syslog.h>

/* lpsched include files */
#include "lp.h"
#include "msgs.h"
#include "printers.h"
#include "class.h"

#include "misc.h"

/* print NS include */
#include <print/ns.h>


static char *
cancel_requestor(const char *printer, const char *user, const char *host)
{
	static char buf[128];	/* I doubt that this is ever to small */
	char *tmp;
	ns_printer_t *pobj;

	if (((pobj = ns_printer_get_name(printer, NULL)) != NULL) &&
	    ((tmp = ns_get_value_string("user-equivalence", pobj)) != NULL) &&
	    (strcasecmp(tmp, "true") == 0))
		host = "all";

	if (strcmp(user, "root") == 0) {
		user = "all";		/* root can cancel anyone's request */
		if (strcmp(host, "all") != 0) {
			char thost[BUFSIZ];

			sysinfo(SI_HOSTNAME, thost, sizeof (thost));
			if (strcmp(host, thost) == 0)
				host = "all"; 	/* cancel from anywhere */
		}
	}

	sprintf(buf, "%s!%s", host, user);

	return (buf);
}


/*
 * lpsched_cancel_job() attempts to cancel an lpsched requests that match the
 * passed in criteria.  a message is written for each cancelation or
 * attempted cancelation
 */
int
lpsched_cancel_job(const char *printer, FILE *ofp, const char *requestor,
			const char *host, const char **list)
{
	short status;
	char **job_list = NULL;
	char *cancel_name;

	syslog(LOG_DEBUG, "cancel_job(%s, %d, %s, %s, 0x%x)",
		(printer ? printer : "NULL"), ofp, requestor, host, list);

	if ((printer == NULL) || (requestor == NULL) || (host == NULL) ||
	    (list == NULL))
		return (-1);

	if (!isprinter((char *)printer) && !isclass((char *)printer)) {
		fprintf(ofp, gettext("unknown printer/class"));
		return (-1);
	}

	if (snd_msg(S_INQUIRE_REQUEST, "", printer, "", "", "") < 0) {
		fprintf(ofp, gettext("Failure to communicate with lpsched\n"));
		return (-1);
	}

	do {
		size_t	size;
		time_t	date;
		short	outcome;
		char *dest, *form, *pwheel, *file, *owner, *reqid;
		const char **list_ptr = list;

		if (rcv_msg(R_INQUIRE_REQUEST, &status, &reqid, &owner, &size,
				&date, &outcome, &dest, &form, &pwheel,
				&file) < 0) {
			fprintf(ofp,
			gettext("Failure to communicate with lpsched\n"));
			return (-1);
		}

		switch (status) {
		case MOK:
		case MOKMORE:
			if (strcasecmp(requestor, "-all") == 0) {
				char buf[BUFSIZ];

				sprintf(buf, "%s %s", owner, reqid);
				appendlist(&job_list, buf);
				break;
			}

			while ((list_ptr != NULL) && (*list_ptr != NULL)) {
				char *user = (char *)user_name(owner);
				int rid = id_no(reqid);
				int id = atoi(*list_ptr++);
				char buf[BUFSIZ];

				if ((rid == id) ||
				    (strcmp(user, list_ptr[-1]) == 0)) {
					sprintf(buf, "%s %s", owner, reqid);
					appendlist(&job_list, buf);
				}
			}
			break;
		default:
			break;
		}
	} while (status == MOKMORE);

	if (strcasecmp(requestor, "-all") == 0)
		requestor = "root";

	cancel_name = cancel_requestor(printer, requestor, host);

	while ((job_list != NULL) && (*job_list != NULL)) {
		char *user = strtok(*job_list, " ");
		char *reqid = strtok(NULL, " ");

		syslog(LOG_DEBUG,
			"cancel %s, owned by %s, on %s, requested by %s\n",
			reqid, user, printer, cancel_name);

		if (snd_msg(S_CANCEL, printer, cancel_name, reqid) < 0) {
			fprintf(ofp,
			gettext("Failure to communicate with lpsched\n"));
			return (-1);
		}

		do {
			int status2;
			char *job_name = "unknown";

			if (rcv_msg(R_CANCEL, &status, &status2,
					&job_name) < 0) {
			fprintf(ofp,
			gettext("Failure to communicate with lpsched\n"));
				return (-1);
			}

			switch (status2) {
			case MOK:
			case MOKMORE:
				fprintf(ofp, gettext("%s: cancelled\n"),
					job_name);
				break;
			case MUNKNOWN:
			case MNOPERM:
				fprintf(ofp, gettext("%s: permission denied\n"),
					reqid);
				break;
				break;
			case M2LATE:
				fprintf(ofp, gettext("cannot dequeue %s\n"),
					job_name);
				break;
			default:
				fprintf(ofp,
					gettext("%s: cancel failed (%d)\n"),
					reqid, status2);
				break;
			}
		} while (status == MOKMORE);
		job_list++;
	}

	return (0);
}
