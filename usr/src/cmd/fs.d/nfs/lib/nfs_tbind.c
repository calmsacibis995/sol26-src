/*
 *	nfs_tbind.c
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nfs_tbind.c	1.2	96/07/14 SMI"

#include <tiuser.h>
#include <fcntl.h>
#include <netconfig.h>
#include <stropts.h>
#include <errno.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netdir.h>
#include <unistd.h>
#include <string.h>
#include "nfs_tbind.h"

/*
 * this file contains transport routines common to nfsd and lockd
 */

static int nofile_increase(int);
static int reuseaddr(int);

/*
 * Number of elements to add to the poll array on each allocation
 */
#define	NOFILE_INC_SIZE	64

/*
 * Called to create and prepare a transport descriptor for in-kernel
 * RPC service.
 * Returns -1 on failure and a valid descriptor on success.
 */
int
nfslib_transport_open(struct netconfig *nconf)
{
	int fd;
	struct strioctl	strioc;

	if ((nconf == (struct netconfig *) NULL) ||
	    (nconf->nc_device == (char *) NULL)) {
		syslog(LOG_ERR, "no netconfig device");
		return (-1);
	}

	/*
	 * Open the transport device.
	 */
	fd = t_open(nconf->nc_device, O_RDWR, (struct t_info *)NULL);
	if (fd == -1) {
		if (t_errno == TSYSERR && errno == EMFILE &&
		    (nofile_increase(0) == 0)) {
			/* Try again with a higher NOFILE limit. */
			fd = t_open(nconf->nc_device, O_RDWR,
					(struct t_info *)NULL);
		}
		if (fd == -1) {
			syslog(LOG_ERR, "t_open %s failed:  t_errno %d, %m",
			    nconf->nc_device, t_errno);
			return (-1);
		}
	}

	/*
	 * Pop timod because the RPC module must be as close as possible
	 * to the transport.
	 */
	if (ioctl(fd, I_POP, "timod") < 0) {
		syslog(LOG_ERR, "I_POP of timod failed: %m");
		(void) t_close(fd);
		return (-1);
	}

	if (nconf->nc_semantics == NC_TPI_CLTS) {
		/*
		 * Push rpcmod to filter data traffic to KRPC.
		 */
		if (ioctl(fd, I_PUSH, "rpcmod") < 0) {
			syslog(LOG_ERR, "I_PUSH of rpcmod failed: %m");
			(void) t_close(fd);
			return (-1);
		}
	} else {
		if (ioctl(fd, I_PUSH, "rpcmod") < 0) {
			syslog(LOG_ERR, "I_PUSH of CONS rpcmod failed: %m");
			(void) t_close(fd);
			return (-1);
		}

		strioc.ic_cmd = RPC_SERVER;
		strioc.ic_dp = (char *)0;
		strioc.ic_len = 0;
		strioc.ic_timout = -1;
		/* Tell CONS rpcmod to act like a server stream. */
		if (ioctl(fd, I_STR, &strioc) < 0) {
			syslog(LOG_ERR, "CONS rpcmod set-up ioctl failed: %m");
			(void) t_close(fd);
			return (-1);
		}
	}

	/*
	 * Re-push timod so that we will still be doing TLI
	 * operations on the descriptor.
	 */
	if (ioctl(fd, I_PUSH, "timod") < 0) {
		syslog(LOG_ERR, "I_PUSH of timod failed: %m");
		(void) t_close(fd);
		return (-1);
	}

	return (fd);
}

static int
nofile_increase(int limit)
{
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
		syslog(LOG_ERR, "getrlimit of NOFILE failed: %m");
		return (-1);
	}

	if (limit > 0)
		rl.rlim_cur = limit;
	else
		rl.rlim_cur += NOFILE_INC_SIZE;

	if (rl.rlim_cur > rl.rlim_max &&
	    rl.rlim_max != RLIM_INFINITY)
		rl.rlim_max = rl.rlim_cur;

	if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
		syslog(LOG_ERR, "setrlimit of NOFILE to %d failed: %m",
			rl.rlim_cur);
		return (-1);
	}

	return (0);
}

int
nfslib_bindit(struct netconfig *nconf, struct netbuf **addr,
	struct nd_hostserv *hs, int listen_backlog)
{
	int fd;
	struct t_bind * ntb;
	struct t_bind tb;
	struct nd_addrlist *addrlist;

	if ((fd = nfslib_transport_open(nconf)) == -1) {
		syslog(LOG_ERR, "cannot establish transport service over %s",
			nconf->nc_device);
		return (-1);
	}

	addrlist = (struct nd_addrlist *)NULL;
	if (netdir_getbyname(nconf, hs, &addrlist) != 0) {
		syslog(LOG_ERR,
		"Cannot get address for transport %s host %s service %s",
			nconf->nc_netid, hs->h_host, hs->h_serv);
		(void) t_close(fd);
		return (-1);
	}

	if (strcmp(nconf->nc_proto, "tcp") == 0) {
		/*
		 * If we're running over TCP, then set the
		 * SO_REUSEADDR option so that we can bind
		 * to our preferred address even if previously
		 * left connections exist in FIN_WAIT states.
		 * This is somewhat bogus, but otherwise you have
		 * to wait 2 minutes to restart after killing it.
		 */
		if (reuseaddr(fd) == -1) {
			syslog(LOG_WARNING,
			"couldn't set SO_REUSEADDR option on transport");
		}
	}

	if (nconf->nc_semantics == NC_TPI_CLTS)
		tb.qlen = 0;
	else
		tb.qlen = listen_backlog;

	/* LINTED pointer alignment */
	ntb = (struct t_bind *) t_alloc(fd, T_BIND, T_ALL);
	if (ntb == (struct t_bind *) NULL) {
		syslog(LOG_ERR, "t_alloc failed:  t_errno %d, %m", t_errno);
		(void) t_close(fd);
		netdir_free((void *)addrlist, ND_ADDRLIST);
		return (-1);
	}

	/*
	 * XXX - what about the space tb->addr.buf points to? This should
	 * be either a memcpy() to/from the buf fields, or t_alloc(fd,T_BIND,)
	 * should't be called with T_ALL.
	 */
	tb.addr = *(addrlist->n_addrs);		/* structure copy */

	if (t_bind(fd, &tb, ntb) == -1) {
		syslog(LOG_ERR, "t_bind failed:  t_errno %d, %m", t_errno);
		(void) t_free((char *) ntb, T_BIND);
		netdir_free((void *)addrlist, ND_ADDRLIST);
		(void) t_close(fd);
		return (-1);
	}

	/* make sure we bound to the right address */
	if (tb.addr.len != ntb->addr.len ||
	    memcmp(tb.addr.buf, ntb->addr.buf, tb.addr.len) != 0) {
		syslog(LOG_ERR, "t_bind to wrong address");
		(void) t_free((char *) ntb, T_BIND);
		netdir_free((void *)addrlist, ND_ADDRLIST);
		(void) t_close(fd);
		return (-1);
	}

	*addr = &ntb->addr;
	netdir_free((void *)addrlist, ND_ADDRLIST);

	return (fd);
}

static int
reuseaddr(int fd)
{
	struct t_optmgmt req, resp;
	struct opthdr *opt;
	char reqbuf[128];
	int *ip;

	/* LINTED pointer alignment */
	opt = (struct opthdr *)reqbuf;
	opt->level = SOL_SOCKET;
	opt->name = SO_REUSEADDR;
	opt->len = sizeof (int);

	/* LINTED pointer alignment */
	ip = (int *)&reqbuf[sizeof (struct opthdr)];
	*ip = 1;

	req.flags = T_NEGOTIATE;
	req.opt.len = sizeof (struct opthdr) + opt->len;
	req.opt.buf = (char *)opt;

	resp.flags = 0;
	resp.opt.buf = reqbuf;
	resp.opt.maxlen = sizeof (reqbuf);

	if (t_optmgmt(fd, &req, &resp) < 0 || resp.flags != T_SUCCESS) {
		t_error("t_optmgmt");
		return (-1);
	}
	return (0);
}

void
nfslib_log_tli_error(char *tli_name, int fd, struct netconfig *nconf)
{
	int error;

	/*
	 * Save the error code across syslog(), just in case syslog()
	 * gets its own error and, therefore, overwrites errno.
	 */
	error = errno;
	if (t_errno == TSYSERR) {
		syslog(LOG_ERR, "%s(file descriptor %d/transport %s) %m",
			tli_name, fd, nconf->nc_proto);
	} else {
		syslog(LOG_ERR,
			"%s(file descriptor %d/transport %s) TLI error %d",
			tli_name, fd, nconf->nc_proto, t_errno);
	}
	errno = error;
}
