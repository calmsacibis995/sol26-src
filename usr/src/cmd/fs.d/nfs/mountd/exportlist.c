/*
 *	exportlist.c
 *
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)exportlist.c 1.4     96/04/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <rpcsvc/mount.h>
#include <sys/pathconf.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <thread.h>
#include "../lib/sharetab.h"

extern struct sh_list *share_list;
extern rwlock_t sharetab_lock;
extern void check_sharetab(void);
extern char *exmalloc(int);

extern int errno;

static void freeexports(struct exportnode *);
static struct groupnode **newgroup(char *, struct groupnode **);
static struct exportnode **newexport(char *, struct groupnode *,
						struct exportnode **);

static char *optlist[] = {
#define	OPT_RO		0
	SHOPT_RO,
#define	OPT_RW		1
	SHOPT_RW,
	NULL
};

/*
 * Send current export list to a client
 */
void
export(rqstp)
	struct svc_req *rqstp;
{
	SVCXPRT *transp;
	struct exportnode *exportlist;
	struct exportnode **tail;
	struct groupnode *groups;
	struct groupnode **grtail;
	struct share *sh;
	struct sh_list *shp;
	char *gr, *p, *opts, *val, *lasts;
	extern void log_cant_reply(SVCXPRT *);

	transp = rqstp->rq_xprt;
	if (!svc_getargs(transp, xdr_void, NULL)) {
		svcerr_decode(transp);
		return;
	}

	check_sharetab();

	exportlist = NULL;
	tail = &exportlist;

	(void) rw_rdlock(&sharetab_lock);

	for (shp = share_list; shp; shp = shp->shl_next) {

		groups = NULL;
		grtail = &groups;

		sh = shp->shl_sh;
		opts = strdup(sh->sh_opts);
		p = opts;

		/*
		 * Just concatenate all the hostnames/groups
		 * from the "ro" and "rw" lists for each flavor.
		 * This list is rather meaningless now, but
		 * that's what the protocol demands.
		 */
		while (*p) {
			switch (getsubopt(&p, optlist, &val)) {
			case OPT_RO:
			case OPT_RW:
				if (val == NULL)
					continue;

				while ((gr = (char *)strtok_r(val, ":", &lasts))
						!= NULL) {
					val = NULL;
					grtail = newgroup(gr, grtail);
				}
				break;
			}
		}

		free(opts);
		tail = newexport(sh->sh_path, groups, tail);
	}

	(void) rw_unlock(&sharetab_lock);

	errno = 0;
	if (!svc_sendreply(transp, xdr_exports, (char *) &exportlist))
		log_cant_reply(transp);

	freeexports(exportlist);
}


static void
freeexports(ex)
	struct exportnode *ex;
{
	struct groupnode *groups, *tmpgroups;
	struct exportnode *tmpex;

	while (ex) {
		groups = ex->ex_groups;
		while (groups) {
			tmpgroups = groups->gr_next;
			free(groups->gr_name);
			free((char *)groups);
			groups = tmpgroups;
		}
		tmpex = ex->ex_next;
		free(ex->ex_dir);
		free((char *)ex);
		ex = tmpex;
	}
}


static struct groupnode **
newgroup(grname, tail)
	char *grname;
	struct groupnode **tail;
{
	struct groupnode *new;
	char *newname;

	/* LINTED pointer alignment */
	new = (struct groupnode *) exmalloc(sizeof (*new));
	newname = (char *) exmalloc(strlen(grname) + 1);
	(void) strcpy(newname, grname);

	new->gr_name = newname;
	new->gr_next = NULL;
	*tail = new;
	return (&new->gr_next);
}


static struct exportnode **
newexport(grname, grplist, tail)
	char *grname;
	struct groupnode *grplist;
	struct exportnode **tail;
{
	struct exportnode *new;
	char *newname;

	/* LINTED pointer alignment */
	new = (struct exportnode *) exmalloc(sizeof (*new));
	newname = (char *) exmalloc(strlen(grname) + 1);
	(void) strcpy(newname, grname);

	new->ex_dir = newname;
	new->ex_groups = grplist;
	new->ex_next = NULL;
	*tail = new;
	return (&new->ex_next);
}
