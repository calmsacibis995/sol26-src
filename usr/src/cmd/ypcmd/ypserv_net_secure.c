#ident	"@(#)ypserv_net_secure.c	1.5	96/04/25 SMI"

/*
 * Copyright (c) 1995 by Sun Microsystems Inc.
 * All rights reserved.
 */
							    

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <syslog.h>

#define	ACCFILE "/var/yp/securenets"
#define	MAXLINE 128

struct seclist {
	u_long mask;
	u_long net;
	struct seclist *next;
};

static struct seclist *slist;
static int nofile = 0;

void
get_secure_nets(char *daemon_name)
{
	FILE *fp;
	char strung[MAXLINE], nmask[MAXLINE], net[MAXLINE];
	unsigned long maskin, netin;
	struct seclist *tmp1, *tmp2;
	int items = 0, line = 0;
	if (fp = fopen(ACCFILE, "r")) {
		tmp1 = (struct seclist *) malloc(sizeof (struct seclist));
		slist = tmp2 = tmp1;
		while (fgets(strung, MAXLINE, fp)) {
			line++;
			if (strung[strlen(strung) - 1] != '\n') {
				syslog(LOG_ERR|LOG_DAEMON,
					"%s: %s line %d: too long\n",
					daemon_name, ACCFILE, line);
				exit(1);
			}
			if (strung[0] != '#') {
				items++;
				if (sscanf(strung,
					"%16s%16s", nmask, net) < 2) {

					syslog(LOG_ERR|LOG_DAEMON,
					"%s: %s line %d: missing fields\n",
						daemon_name, ACCFILE, line);
					exit(1);
				}
				maskin = inet_addr(nmask);
				if ((int) maskin == -1 &&
				strcmp(nmask, "255.255.255.255") != 0 &&
				strcmp(nmask, "host") != 0) {

					syslog(LOG_ERR|LOG_DAEMON,
					"%s: %s line %d: error in netmask\n",
						daemon_name, ACCFILE, line);
					exit(1);
				}
				netin = inet_addr(net);
				if ((int) netin == -1) {
					syslog(LOG_ERR|LOG_DAEMON,
					"%s: %s line %d: error in address\n",
						daemon_name, ACCFILE, line);
					exit(1);
				}

				if ((maskin & netin) != netin) {
					syslog(LOG_ERR|LOG_DAEMON,
			"%s: %s line %d: netmask does not match network\n",
						daemon_name, ACCFILE, line);
					exit(1);
				}
				tmp1->mask = maskin;
				tmp1->net = netin;
				tmp1->next = (struct seclist *)
					malloc(sizeof (struct seclist));
				tmp2 = tmp1;
				tmp1 = tmp1->next;
			}
		}
		tmp2->next = NULL;
		/* if nothing to process, set nofile flag and free up memory */
		if (items == 0) {
			free(slist);
			nofile = 1;
		}
	} else {
		syslog(LOG_WARNING|LOG_DAEMON, "%s: no %s file\n",
			daemon_name, ACCFILE);
		nofile = 1;
	}
}

int
check_secure_net(struct sockaddr_in *caller, char *ypname)
{
	struct seclist *tmp;
	tmp = slist;
	if (nofile)
		return (1);
	while (tmp != NULL) {
		if ((caller->sin_addr.s_addr & tmp->mask) == tmp->net) {
			return (1);
		}
		tmp = tmp->next;
	}
	syslog(LOG_ERR|LOG_DAEMON, "%s: access denied for %s\n",
		ypname, inet_ntoa(caller->sin_addr));

	return (0);
}
