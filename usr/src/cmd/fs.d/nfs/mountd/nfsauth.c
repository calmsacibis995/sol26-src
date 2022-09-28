/*
 *	nfsauth.c
 *
 *	Copyright (c) 1988-1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nfsauth.c 1.5     96/04/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
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
#include <syslog.h>
#include <locale.h>
#include <unistd.h>
#include <thread.h>
#include <netdir.h>
#include <rpcsvc/nfsauth_prot.h>
#include "../lib/sharetab.h"

void nfsauth_prog(struct svc_req *, SVCXPRT *);
void nfsauth_access_svc(auth_req *, auth_res *, struct svc_req *);

extern struct share *findentry(char *);
extern int check_client(struct share *, struct netbuf *,
				struct nd_hostservlist *, int);

void
nfsauth_prog(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		auth_req nfsauth_access_arg;
	} argument;
	auth_res  result;

	bool_t (*xdr_argument)();
	bool_t (*xdr_result)();
	void   (*local)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case NFSAUTH_ACCESS:
		xdr_argument = xdr_auth_req;
		xdr_result = xdr_auth_res;
		local = nfsauth_access_svc;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}

	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		return;
	}

	(*local)(&argument, &result, rqstp);

	if (!svc_sendreply(transp, xdr_result, (caddr_t)&result)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
	}
}

/*ARGSUSED*/

void
nfsauth_access_svc(argp, result, rqstp)
	auth_req *argp;
	auth_res *result;
	struct svc_req *rqstp;
{
	struct netconfig *nconf;
	struct nd_hostservlist *clnames = NULL;
	struct netbuf nbuf;
	struct share *sh;

	result->auth_perm = NFSAUTH_DENIED;

	/*
	 * Convert the client's address to a hostname
	 */
	nconf = getnetconfigent(argp->req_netid);
	if (nconf == NULL) {
		syslog(LOG_ERR, "No netconfig entry for %s", argp->req_netid);
		return;
	}

	nbuf.len = argp->req_client.n_len;
	nbuf.buf = argp->req_client.n_bytes;

	if (netdir_getbyaddr(nconf, &clnames, &nbuf)) {
		syslog(LOG_ERR, "No address for %s",
			taddr2uaddr(nconf, &nbuf));
		goto done;
	}

	/*
	 * Now find the export
	 */
	sh = findentry(argp->req_path);
	if (sh == NULL) {
		syslog(LOG_ERR, "%s not exported", argp->req_path);
		goto done;
	}

	result->auth_perm = check_client(sh, &nbuf, clnames, argp->req_flavor);
	sharefree(sh);
done:
	freenetconfigent(nconf);
	if (clnames)
		netdir_free(clnames, ND_HOSTSERVLIST);
}
