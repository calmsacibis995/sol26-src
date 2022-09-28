/* Copyright (c) 1993 Sun Microsystems Inc */

#include <stdio.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <errno.h>
#include <sys/systeminfo.h>
#include <netconfig.h>
#include <netdir.h>
#include <rpcsvc/yp_prot.h>
#include "resolv_common.h"

#define	YPDNSVERS	2L
#define	RESOLV_EXEC_PATH	"/usr/sbin/rpc.nisd_resolv"
#define	RESOLV_EXEC_ERR		"can't exec /usr/sbin/rpc.nisd_resolv: %s\n"

extern bool_t xdr_ypfwdreq_key();
extern int verbose;
extern int resolv_pid;

static struct netconfig *udp_nc = NULL;

void
setup_resolv(fwding, child, client, tp_type, prognum)
int	*fwding;
int	*child;
CLIENT	**client;
char	*tp_type;
long	prognum;	/* use transient if this not set */
{
	enum clnt_stat stat;
	struct timeval tv;
	char prog_str[15], fd_str[5];
	SVCXPRT *xprt = NULL;
	char *tp;
	char name[257];
	struct netconfig *nc;
	void *h;

	if (!*fwding)
		return;

	/* try the specified netid (default ticots), then any loopback */
	tp = (tp_type && *tp_type) ? tp_type : "ticots";
	if (!getconf(tp, &h, &nc)) { /* dont forget endnetconfig() */
		syslog(LOG_ERR, "can't get resolv_clnt netconf %s.\n", tp);
		*fwding = FALSE;
		return;
	}
	tp = nc->nc_netid;

	/*
	 * Startup the resolv server: use transient prognum if prognum
	 * isn't set. Using transient means we create mapping then
	 * pass child the fd to use for service.
	 */
	if (!getprognum(&prognum, &xprt, fd_str, prog_str, YPDNSVERS, tp)) {
		syslog(LOG_ERR, "can't create resolv xprt for transient.\n");
		*fwding = FALSE;
		endnetconfig(h);
		return;
	}
	switch (*child = vfork()) {
	case -1: /* error  */
		syslog(LOG_ERR, "can't startup resolv daemon\n");
		endnetconfig(h);
		*fwding = FALSE;
		return;
	case 0:  /* child  */
		/*
		 * if using transient we must maintain fd across
		 * exec cause unset/set on prognum isn't automic.
		 *
		 * if using transient we'll just do svc_tli_create
		 * in child on our bound fd.
		 */
		execlp(RESOLV_EXEC_PATH, "rpc.nisd_resolv",
				"-F",		/* forground  */
				"-C", fd_str,	/* dont close */
				"-p", prog_str,	/* prognum    */
				"-t", tp,	/* tp type    */
				NULL);
		syslog(LOG_ERR, RESOLV_EXEC_ERR, strerror(errno));
		exit(1);
	default: /* parent */
		/* close fd, free xprt, but leave mapping */
		if (xprt) svc_destroy(xprt);

		/* let it crank up before we create client */
		sleep(4);
	}
	/* keep udp_nc for resolv_req() t2uaddr (yp_match() is udp) */
	if (!udp_nc && ((udp_nc = getnetconfigent("udp")) == NULL)) {
		syslog(LOG_ERR, "can't get udp nconf\n");
		(void) kill (*child, SIGINT);
		endnetconfig(h);
		*fwding = FALSE;
		return;
	}
	if (sysinfo(SI_HOSTNAME, name, sizeof (name)-1) == -1) {
		syslog(LOG_ERR, "can't get local hostname.\n");
		(void) kill (*child, SIGINT);
		endnetconfig(h);
		freenetconfigent(udp_nc); udp_nc = NULL;
		*fwding = FALSE;
		return;
	}
	if ((*client = clnt_tp_create(name, prognum, YPDNSVERS, nc)) == NULL) {
		syslog(LOG_ERR, "can't create resolv_clnt\n");
		(void) kill (*child, SIGINT);
		endnetconfig(h);
		freenetconfigent(udp_nc); udp_nc = NULL;
		*fwding = FALSE;
		return;
	}
	endnetconfig(h);

	/* ping for comfort */
	tv.tv_sec = 10; tv.tv_usec = 0;
	if ((stat = clnt_call(*client, 0, xdr_void, 0,
				xdr_void, 0, tv)) != RPC_SUCCESS) {
		syslog(LOG_ERR, "can't talk with resolv server\n");
		clnt_destroy (*client);
		(void) kill (*child, SIGINT);
		freenetconfigent(udp_nc); udp_nc = NULL;
		*fwding = FALSE;
		return;
	}

	if (verbose)
		syslog(LOG_INFO, "finished setup for dns fwding.\n");
}

int
getprognum(prognum, xprt, fd_str, prog_str, vers, tp_type)
long *prognum;
SVCXPRT **xprt;
char *fd_str;
char *prog_str;
long vers;
char *tp_type;
{
	static u_long start = 0x40000000;
	int fd;
	struct netconfig *nc;
	struct netbuf *nb;

	/* If prognum specified, use it instead of transient hassel. */
	if (*prognum) {
		*xprt = NULL;
		sprintf(fd_str, "-1"); /* have child close all fds */
		sprintf(prog_str, "%u", *prognum);
		return (TRUE);
	}

	/*
	 * Transient hassel:
	 *	- parent must create mapping since someone else could
	 *	  steal the transient prognum before child created it
	 * 	- pass the child the fd to use for service
	 * 	- close the fd (after exec), free xprt, leave mapping intact
	 */
	/* tp_type is legit: users choice or a loopback netid */
	if ((nc = getnetconfigent(tp_type)) == NULL)
		return (FALSE);
	if ((*xprt = svc_tli_create(RPC_ANYFD, nc, NULL, 0, 0)) == NULL) {
		freenetconfigent(nc);
		return (FALSE);
	}
	nb = &(*xprt)->xp_ltaddr;
	fd = (*xprt)->xp_fd;
	while (!rpcb_set(start, vers, nc, nb))
		start++;
	freenetconfigent(nc);

	*prognum = start;
	sprintf(fd_str, "%u", fd);
	sprintf(prog_str, "%u", *prognum);

	return (TRUE);
}

int
getconf(netid, handle, nconf)
char *netid;
void **handle;
struct netconfig **nconf;
{
	struct netconfig *nc, *save = NULL;

	if ((*handle = setnetconfig()) == NULL)
		return (FALSE);

	while (nc = getnetconfig((void*)*handle)) {
		if (strcmp(nc->nc_netid, netid) != 0) {
			*nconf = nc;
			return (TRUE);
		} else if (!save && strcmp(nc->nc_protofmly, "loopback") != 0)
			save = nc;
	}

	if (save) {
		*nconf = save;
		return (TRUE);
	} else {
		endnetconfig(*handle);
		return (FALSE);
	}
}

int
resolv_req(fwding, client, pid, tp, xprt, req, map)
int *fwding;
CLIENT **client;
int *pid;
char *tp;
SVCXPRT *xprt;
struct ypreq_key *req;
char *map;
{
	enum clnt_stat stat;
	struct timeval tv;
	struct ypfwdreq_key fwd_req;
	int byname, byaddr;
	struct netbuf *nb;
	char *uaddr;
	char *cp;
	int i;

	if (!*fwding)
		return (FALSE);

	byname = strcmp(map, "hosts.byname") == 0;
	byaddr = strcmp(map, "hosts.byaddr") == 0;
	if ((!byname && !byaddr) || req->keydat.dsize == 0 ||
				req->keydat.dptr[0] == '\0' ||
				!isascii(req->keydat.dptr[0]) ||
				!isgraph(req->keydat.dptr[0])) {
		/* default status is YP_NOKEY */
		return (FALSE);
	}

	fwd_req.map = map;
	fwd_req.keydat = req->keydat;
	fwd_req.xid = svc_getxid(xprt);
	nb = svc_getrpccaller(xprt);
	if ((uaddr = taddr2uaddr(udp_nc, nb)) == NULL) {
			syslog(LOG_ERR,
			"can't get caller uaddr: req not resolved\n");
			return (FALSE);
	}
	fwd_req.port = 0;
	for (i = 0; i < 2; i++) {
		if (!(cp = strrchr(uaddr, '.')))
			return (FALSE);
		*cp = '\0';
		fwd_req.port = fwd_req.port | (atoi(++cp)<<(8*i));
	}
	if ((fwd_req.ip = inet_addr(uaddr)) == -1) {
			syslog(LOG_ERR,
			"can't get caller ipaddr; req not resolved\n");
			return (FALSE);
	}
	fwd_req.ip = ntohl(fwd_req.ip);
	free(uaddr);

	/* Restart resolver if it died. (possible overkill) */
	if (kill(*pid, 0)) {
		syslog(LOG_INFO,
		"Restarting resolv server: old one (pid %d) died.\n", *pid);
		clnt_destroy (*client);
		setup_resolv(fwding, pid, client, tp, 0 /* transient p# */);
		if (!*fwding) {
			syslog(LOG_ERR,
			"can't restart resolver: ending resolv service.\n");
			return (FALSE);
		}
	}

	/* may need to up timeout */
	tv.tv_sec = 10; tv.tv_usec = 0;
	stat = clnt_call(*client, YPDNSPROC, xdr_ypfwdreq_key,
					(char *) &fwd_req, xdr_void, 0, tv);
	if (stat == RPC_SUCCESS) /* expected */
		return (TRUE);

	else { /* Over kill error recovery */
		/* make one attempt to restart service before turning off */
		syslog(LOG_INFO,
			"Restarting resolv server: old one not responding.\n");

		if (!kill(*pid, 0))
			kill (*pid, SIGINT); /* cleanup old one */

		clnt_destroy (*client);
		setup_resolv(fwding, pid, client, tp, 0 /* transient p# */);
		if (!*fwding) {
			syslog(LOG_ERR,
			"can't restart resolver: ending resolv service.\n");
			return (FALSE);
		}

		stat = clnt_call(*client, YPDNSPROC, xdr_ypfwdreq_key,
					(char *) &fwd_req, xdr_void, 0, tv);
		if (stat == RPC_SUCCESS) /* expected */
			return (TRUE);
		else {
			/* no more restarts */
			clnt_destroy (*client);
			*fwding = FALSE; /* turn off fwd'ing */
			syslog(LOG_ERR,
		"restarted resolver not responding: ending resolv service.\n");
			return (FALSE);
		}
	}
}
