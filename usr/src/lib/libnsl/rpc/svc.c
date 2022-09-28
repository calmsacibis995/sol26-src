/*
 * Copyright (c) 1986-1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)svc.c	1.36	96/06/20 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)svc.c 1.58 89/03/16 Copyr 1988 Sun Micro";
#endif

/*
 * svc.c, Server-side remote procedure call interface.
 *
 * There are two sets of procedures here.  The xprt routines are
 * for handling transport handles.  The svc routines handle the
 * list of service routines.
 *
 */


#include <assert.h>
#include "rpc_mt.h"
#include <errno.h>
#include <sys/types.h>
#include <stropts.h>
#include <sys/conf.h>
#include <rpc/trace.h>
#include <rpc/rpc.h>
#ifdef PORTMAP
#include <rpc/pmap_clnt.h>
#endif
#include <sys/poll.h>
#include <netconfig.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

extern int ffs();
extern bool_t __svc_get_door_cred();
extern bool_t __rpc_get_local_cred();

SVCXPRT **svc_xports;

XDR **svc_xdrs;		/* common XDR receive area */

#define	NULL_SVC ((struct svc_callout *)0)
#define	RQCRED_SIZE	400		/* this size is excessive */

/*
 * The services list
 * Each entry represents a set of procedures (an rpc program).
 * The dispatch routine takes request structs and runs the
 * appropriate procedure.
 */
static struct svc_callout {
	struct svc_callout *sc_next;
	u_long		    sc_prog;
	u_long		    sc_vers;
	char		   *sc_netid;
	void		    (*sc_dispatch)();
} *svc_head;
extern rwlock_t	svc_lock;

static struct svc_callout *svc_find();
void _svc_prog_dispatch();
void svc_getreq_common();
char *strdup();

static const char svc_getreqset_errstr[] =
	"svc_getreqset: No transport handle for fd %d";

extern mutex_t	svc_door_mutex;
extern cond_t	svc_door_waitcv;
extern int	svc_ndoorfds;
extern SVCXPRT_LIST *_svc_xprtlist;
extern mutex_t xprtlist_lock;
extern void __svc_rm_from_xlist();

/*
 * Provides default service-side functions for authentication flavors
 * that do not use all the fields in struct svc_auth_ops.
 */

/*ARGSUSED*/
static int
authany_wrap(auth, xdrs, xfunc, xwhere)
	AUTH		*auth;
	XDR		*xdrs;
	xdrproc_t	xfunc;
	caddr_t		xwhere;
{
	return (*xfunc)(xdrs, xwhere);
}

struct svc_auth_ops svc_auth_any_ops = {
	authany_wrap,
	authany_wrap,
};

/*
 * Return pointer to server authentication structure.
 */
SVCAUTH *
__svc_get_svcauth(xprt)
	SVCXPRT	*xprt;
{
/* LINTED pointer alignment */
	return (&SVC_XP_AUTH(xprt));
}

/*
 * A callback routine to cleanup after a procedure is executed.
 */
void (*__proc_cleanup_cb)() = NULL;

void *
__svc_set_proc_cleanup_cb(cb)
	void	*cb;
{
	void	*tmp = (void *)__proc_cleanup_cb;

	__proc_cleanup_cb = (void (*)())cb;
	return (tmp);
}

/* ***************  SVCXPRT related stuff **************** */

/*
 * Activate a transport handle.
 */
void
xprt_register(xprt)
	const SVCXPRT *xprt;
{
	register int fd = xprt->xp_fd;
	extern rwlock_t svc_fd_lock;
#ifdef CALLBACK
	extern void (*_svc_getreqset_proc)();
#endif

/* VARIABLES PROTECTED BY svc_fd_lock: svc_xports, svc_fdset */

	trace1(TR_xprt_register, 0);
	rw_wrlock(&svc_fd_lock);
	if (svc_xports == NULL) {
		svc_xports = (SVCXPRT **)
			mem_alloc((FD_SETSIZE + 1) * sizeof (SVCXPRT *));
			memset(svc_xports, 0,
					(FD_SETSIZE + 1) * sizeof (SVCXPRT *));
#ifdef CALLBACK
		/*
		 * XXX: This code does not keep track of the server state.
		 *
		 * This provides for callback support.	When a client
		 * recv's a call from another client on the server fd's,
		 * it calls _svc_getreqset_proc() which would return
		 * after serving all the server requests.  Also look under
		 * clnt_dg.c and clnt_vc.c  (clnt_call part of it)
		 */
		_svc_getreqset_proc = svc_getreq_poll;
#endif
	}
	if (fd < FD_SETSIZE +1)
		svc_xports[fd] = (SVCXPRT *)xprt;
	if (fd < __rpc_dtbsize()) {
		FD_SET(fd, &svc_fdset);
		svc_nfds++;
		svc_nfds_set++;
		if (fd >= svc_max_fd)
			svc_max_fd = fd + 1;
		if (svc_polling) {
			char dummy;

			/*
			 * This happens only in one of the MT modes.
			 * Wake up poller.
			 */
			write(svc_pipe[1], &dummy, sizeof (dummy));
		}
		/*
		 * If already dispatching door based services, start
		 * dispatching TLI based services now.
		 */
		mutex_lock(&svc_door_mutex);
		if (svc_ndoorfds > 0)
			cond_signal(&svc_door_waitcv);
		mutex_unlock(&svc_door_mutex);
	}

	if (svc_xdrs == NULL) {
		svc_xdrs = (XDR **) mem_alloc((FD_SETSIZE + 1) *
							sizeof (XDR *));
		memset(svc_xdrs, 0, (FD_SETSIZE + 1) * sizeof (XDR *));
	}
	rw_unlock(&svc_fd_lock);

	trace1(TR_xprt_register, 1);
}

/*
 * De-activate a transport handle.
 */
void
xprt_unregister(xprt)
	const SVCXPRT *xprt;
{
	register int fd = xprt->xp_fd;
	extern rwlock_t svc_fd_lock;

	trace1(TR_xprt_unregister, 0);
	rw_wrlock(&svc_fd_lock);
	if ((fd < __rpc_dtbsize()) && (svc_xports[fd] == xprt)) {
		svc_xports[fd] = (SVCXPRT *)NULL;
		if (FD_ISSET(fd, &svc_fdset)) {
			FD_CLR(fd, &svc_fdset);
			svc_nfds_set--;
		}
		if (fd == (svc_max_fd - 1))
			svc_max_fd--;
		svc_nfds--;
	}
	rw_unlock(&svc_fd_lock);
	__svc_rm_from_xlist(&_svc_xprtlist, xprt, &xprtlist_lock);
	trace1(TR_xprt_unregister, 1);
}


/* ********************** CALLOUT list related stuff ************* */

/*
 * Add a service program to the callout list.
 * The dispatch routine will be called when a rpc request for this
 * program number comes in.
 */
bool_t
svc_reg(xprt, prog, vers, dispatch, nconf)
	const SVCXPRT *xprt;
	u_long prog;
	u_long vers;
	void (*dispatch)();
	const struct netconfig *nconf;
{
	bool_t dummy;
	struct svc_callout *prev;
	register struct svc_callout *s, **s2;
	struct netconfig *tnconf;
	register char *netid = NULL;
	int flag = 0;

/* VARIABLES PROTECTED BY svc_lock: s, prev, svc_head */

	trace3(TR_svc_reg, 0, prog, vers);
	if (xprt->xp_netid) {
		netid = strdup(xprt->xp_netid);
		flag = 1;
	} else if (nconf && nconf->nc_netid) {
		netid = strdup(nconf->nc_netid);
		flag = 1;
	} else if ((tnconf = __rpcfd_to_nconf(xprt->xp_fd, xprt->xp_type))
			!= NULL) {
		netid = strdup(tnconf->nc_netid);
		flag = 1;
		freenetconfigent(tnconf);
	} /* must have been created with svc_raw_create */
	if ((netid == NULL) && (flag == 1)) {
		trace3(TR_svc_reg, 1, prog, vers);
		return (FALSE);
	}

	rw_wrlock(&svc_lock);
	if ((s = svc_find(prog, vers, &prev, netid)) != NULL_SVC) {
		if (netid)
			free(netid);
		if (s->sc_dispatch == dispatch)
			goto rpcb_it; /* he is registering another xptr */
		trace3(TR_svc_reg, 1, prog, vers);
		rw_unlock(&svc_lock);
		return (FALSE);
	}
	s = (struct svc_callout *)mem_alloc(sizeof (struct svc_callout));
	if (s == (struct svc_callout *)NULL) {
		if (netid)
			free(netid);
		trace3(TR_svc_reg, 1, prog, vers);
		rw_unlock(&svc_lock);
		return (FALSE);
	}

	s->sc_prog = prog;
	s->sc_vers = vers;
	s->sc_dispatch = dispatch;
	s->sc_netid = netid;
	s->sc_next = NULL;

	/*
	 * The ordering of transports is such that the most frequently used
	 * one appears first.  So add the new entry to the end of the list.
	 */
	for (s2 = &svc_head; *s2 != NULL; s2 = &(*s2)->sc_next)
		;
	*s2 = s;

	if ((xprt->xp_netid == NULL) && (flag == 1) && netid)
		((SVCXPRT *)xprt)->xp_netid = strdup(netid);

rpcb_it:
	rw_unlock(&svc_lock);
	/* now register the information with the local binder service */
	if (nconf) {
		dummy = rpcb_set(prog, vers, nconf, &xprt->xp_ltaddr);
		trace3(TR_svc_reg, 1, prog, vers);
		return (dummy);
	}
	trace3(TR_svc_reg, 1, prog, vers);
	return (TRUE);
}

/*
 * Remove a service program from the callout list.
 */
void
svc_unreg(prog, vers)
	u_long prog;
	u_long vers;
{
	struct svc_callout *prev;
	register struct svc_callout *s;

	trace3(TR_svc_unreg, 0, prog, vers);
	/* unregister the information anyway */
	(void) rpcb_unset(prog, vers, NULL);
	rw_wrlock(&svc_lock);
	while ((s = svc_find(prog, vers, &prev, NULL)) != NULL_SVC) {
		if (prev == NULL_SVC) {
			svc_head = s->sc_next;
		} else {
			prev->sc_next = s->sc_next;
		}
		s->sc_next = NULL_SVC;
		if (s->sc_netid)
			mem_free((char *)s->sc_netid,
					(u_int)sizeof (s->sc_netid) + 1);
		mem_free((char *)s, (u_int) sizeof (struct svc_callout));
	}
	rw_unlock(&svc_lock);
	trace3(TR_svc_unreg, 1, prog, vers);
}

#ifdef PORTMAP
/*
 * Add a service program to the callout list.
 * The dispatch routine will be called when a rpc request for this
 * program number comes in.
 * For version 2 portmappers.
 */
#ifdef KERNEL
/*ARGSUSED*/
#endif
bool_t
svc_register(xprt, prog, vers, dispatch, protocol)
	SVCXPRT *xprt;
	u_long prog;
	u_long vers;
	void (*dispatch)();
	int protocol;
{
	bool_t dummy;
	struct svc_callout *prev;
	register struct svc_callout *s;
	register struct netconfig *nconf;
	register char *netid = NULL;
	int flag = 0;

	trace4(TR_svc_register, 0, prog, vers, protocol);
	if (xprt->xp_netid) {
		netid = strdup(xprt->xp_netid);
		flag = 1;
	} else if ((ioctl(xprt->xp_fd, I_FIND, "timod") > 0) && ((nconf =
	__rpcfd_to_nconf(xprt->xp_fd, xprt->xp_type)) != NULL)) {
		/* fill in missing netid field in SVCXPRT */
		netid = strdup(nconf->nc_netid);
		flag = 1;
		freenetconfigent(nconf);
	} /* must be svc_raw_create */

	if ((netid == NULL) && (flag == 1)) {
		trace4(TR_svc_register, 1, prog, vers, protocol);
		return (FALSE);
	}

	rw_wrlock(&svc_lock);
	if ((s = svc_find(prog, vers, &prev, netid)) != NULL_SVC) {
		if (netid)
			free(netid);
		if (s->sc_dispatch == dispatch)
			goto pmap_it;  /* he is registering another xptr */
		rw_unlock(&svc_lock);
		trace4(TR_svc_register, 1, prog, vers, protocol);
		return (FALSE);
	}
	s = (struct svc_callout *)mem_alloc(sizeof (struct svc_callout));
#ifndef KERNEL
	if (s == (struct svc_callout *)0) {
		if (netid)
			free(netid);
		trace4(TR_svc_register, 1, prog, vers, protocol);
		rw_unlock(&svc_lock);
		return (FALSE);
	}
#endif
	s->sc_prog = prog;
	s->sc_vers = vers;
	s->sc_dispatch = dispatch;
	s->sc_netid = netid;
	s->sc_next = svc_head;
	svc_head = s;

	if ((xprt->xp_netid == NULL) && (flag == 1) && netid)
		xprt->xp_netid = strdup(netid);

pmap_it:
	rw_unlock(&svc_lock);
#ifndef KERNEL
	/* now register the information with the local binder service */
	if (protocol) {
		dummy = pmap_set(prog, vers, protocol, xprt->xp_port);
		trace4(TR_svc_register, 1, prog, vers, protocol);
		return (dummy);
	}
#endif
	trace4(TR_svc_register, 1, prog, vers, protocol);
	return (TRUE);
}

/*
 * Remove a service program from the callout list.
 * For version 2 portmappers.
 */
void
svc_unregister(prog, vers)
	u_long prog;
	u_long vers;
{
	struct svc_callout *prev;
	register struct svc_callout *s;

	trace3(TR_svc_unregister, 0, prog, vers);
	rw_wrlock(&svc_lock);
	while ((s = svc_find(prog, vers, &prev, NULL)) != NULL_SVC) {
		if (prev == NULL_SVC) {
			svc_head = s->sc_next;
		} else {
			prev->sc_next = s->sc_next;
		}
		s->sc_next = NULL_SVC;
		if (s->sc_netid)
			mem_free((char *)s->sc_netid,
					(u_int)sizeof (s->sc_netid) + 1);
		mem_free((char *) s, (u_int) sizeof (struct svc_callout));
#ifndef KERNEL
		/* unregister the information with the local binder service */
		(void) pmap_unset(prog, vers);
#endif
	}
	rw_unlock(&svc_lock);
	trace3(TR_svc_unregister, 1, prog, vers);
}

#endif /* PORTMAP */
/*
 * Search the callout list for a program number, return the callout
 * struct.
 * Also check for transport as well.  Many routines such as svc_unreg
 * dont give any corresponding transport, so dont check for transport if
 * netid == NULL
 */
static struct svc_callout *
svc_find(prog, vers, prev, netid)
	u_long prog;
	u_long vers;
	struct svc_callout **prev;
	char *netid;
{
	register struct svc_callout *s, *p;

	trace3(TR_svc_find, 0, prog, vers);

/* WRITE LOCK HELD ON ENTRY: svc_lock */

/*	assert(RW_WRITE_HELD(&svc_lock)); */
	p = NULL_SVC;
	for (s = svc_head; s != NULL_SVC; s = s->sc_next) {
		if (((s->sc_prog == prog) && (s->sc_vers == vers)) &&
			((netid == NULL) || (s->sc_netid == NULL) ||
			(strcmp(netid, s->sc_netid) == 0)))
				break;
		p = s;
	}
	*prev = p;
	trace3(TR_svc_find, 1, prog, vers);
	return (s);
}


/* ******************* REPLY GENERATION ROUTINES  ************ */

/*
 * Send a reply to an rpc request
 */
bool_t
svc_sendreply(xprt, xdr_results, xdr_location)
	const SVCXPRT *xprt;
	xdrproc_t xdr_results;
	caddr_t xdr_location;
{
	bool_t dummy;
	struct rpc_msg rply;

	trace1(TR_svc_sendreply, 0);
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = SUCCESS;
	rply.acpted_rply.ar_results.where = xdr_location;
	rply.acpted_rply.ar_results.proc = xdr_results;
	dummy = SVC_REPLY((SVCXPRT *)xprt, &rply);
	trace1(TR_svc_sendreply, 1);
	return (dummy);
}

/*
 * No procedure error reply
 */
void
svcerr_noproc(xprt)
	const SVCXPRT *xprt;
{
	struct rpc_msg rply;

	trace1(TR_svcerr_noproc, 0);
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROC_UNAVAIL;
	SVC_REPLY((SVCXPRT *)xprt, &rply);
	trace1(TR_svcerr_noproc, 1);
}

/*
 * Can't decode args error reply
 */
void
svcerr_decode(xprt)
	const SVCXPRT *xprt;
{
	struct rpc_msg rply;

	trace1(TR_svcerr_decode, 0);
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = GARBAGE_ARGS;
	SVC_REPLY((SVCXPRT *)xprt, &rply);
	trace1(TR_svcerr_decode, 1);
}

/*
 * Some system error
 */
void
svcerr_systemerr(xprt)
	const SVCXPRT *xprt;
{
	struct rpc_msg rply;

	trace1(TR_svcerr_systemerr, 0);
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = SYSTEM_ERR;
	SVC_REPLY((SVCXPRT *)xprt, &rply);
	trace1(TR_svcerr_systemerr, 1);
}

/*
 * Tell RPC package to not complain about version errors to the client.	 This
 * is useful when revving broadcast protocols that sit on a fixed address.
 * There is really one (or should be only one) example of this kind of
 * protocol: the portmapper (or rpc binder).
 */
void
__svc_versquiet_on(xprt)
	register SVCXPRT *xprt;
{
	trace1(TR___svc_versquiet_on, 0);
/* LINTED pointer alignment */
	svc_flags(xprt) |= SVC_VERSQUIET;
	trace1(TR___svc_versquiet_on, 1);
}

void
__svc_versquiet_off(xprt)
	register SVCXPRT *xprt;
{
	trace1(TR___svc_versquiet_off, 0);
/* LINTED pointer alignment */
	svc_flags(xprt) &= ~SVC_VERSQUIET;
	trace1(TR___svc_versquiet_off, 1);
}

void
svc_versquiet(xprt)
	register SVCXPRT *xprt;
{
	trace1(TR_svc_versquiet, 0);
	__svc_versquiet_on(xprt);
	trace1(TR_svc_versquiet, 1);
}

int
__svc_versquiet_get(xprt)
	register SVCXPRT *xprt;
{
	trace1(TR___svc_versquiet_get, 0);
	trace2(TR___svc_versquiet_get, 1, tmp);
/* LINTED pointer alignment */
	return (svc_flags(xprt) & SVC_VERSQUIET);
}

/*
 * Authentication error reply
 */
void
svcerr_auth(xprt, why)
	const SVCXPRT *xprt;
	enum auth_stat why;
{
	struct rpc_msg rply;

	trace1(TR_svcerr_auth, 0);
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_DENIED;
	rply.rjcted_rply.rj_stat = AUTH_ERROR;
	rply.rjcted_rply.rj_why = why;
	SVC_REPLY((SVCXPRT *)xprt, &rply);
	trace1(TR_svcerr_auth, 1);
}

/*
 * Auth too weak error reply
 */
void
svcerr_weakauth(xprt)
	const SVCXPRT *xprt;
{
	trace1(TR_svcerr_weakauth, 0);
	svcerr_auth(xprt, AUTH_TOOWEAK);
	trace1(TR_svcerr_weakauth, 1);
}

/*
 * Program unavailable error reply
 */
void
svcerr_noprog(xprt)
	const SVCXPRT *xprt;
{
	struct rpc_msg rply;

	trace1(TR_svcerr_noprog, 0);
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROG_UNAVAIL;
	SVC_REPLY((SVCXPRT *)xprt, &rply);
	trace1(TR_svcerr_noprog, 1);
}

/*
 * Program version mismatch error reply
 */
void
svcerr_progvers(xprt, low_vers, high_vers)
	const SVCXPRT *xprt;
	u_long low_vers;
	u_long high_vers;
{
	struct rpc_msg rply;

	trace3(TR_svcerr_progvers, 0, low_vers, high_vers);
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROG_MISMATCH;
	rply.acpted_rply.ar_vers.low = low_vers;
	rply.acpted_rply.ar_vers.high = high_vers;
	SVC_REPLY((SVCXPRT *)xprt, &rply);
	trace3(TR_svcerr_progvers, 1, low_vers, high_vers);
}

/* ******************* SERVER INPUT STUFF ******************* */

/*
 * Get server side input from some transport.
 *
 * Statement of authentication parameters management:
 * This function owns and manages all authentication parameters, specifically
 * the "raw" parameters (msg.rm_call.cb_cred and msg.rm_call.cb_verf) and
 * the "cooked" credentials (rqst->rq_clntcred).
 * However, this function does not know the structure of the cooked
 * credentials, so it make the following assumptions:
 *   a) the structure is contiguous (no pointers), and
 *   b) the cred structure size does not exceed RQCRED_SIZE bytes.
 * In all events, all three parameters are freed upon exit from this routine.
 * The storage is trivially management on the call stack in user land, but
 * is mallocated in kernel land.
 */

void
svc_getreq(rdfds)
	int rdfds;
{
	fd_set readfds;

	trace2(TR_svc_getreq, 0, rdfds);
	FD_ZERO(&readfds);
	readfds.fds_bits[0] = rdfds;
	svc_getreqset(&readfds);
	trace2(TR_svc_getreq, 1, rdfds);
}

void
svc_getreqset(readfds)
	fd_set *readfds;
{
	register u_long mask;
	register int bit;
	register u_long *maskp;
	register int i;

	trace1(TR_svc_getreqset, 0);
	maskp = (u_long *)readfds->fds_bits;
	for (i = 0; i < svc_max_fd; i += NFDBITS)
		for (mask = *maskp++; bit = ffs(mask);
			mask ^= (1 << (bit - 1)))
			/* fd has input waiting */
			svc_getreq_common(i + bit - 1);
	trace1(TR_svc_getreqset, 1);
}

void
svc_getreq_poll(pfdp, pollretval)
	struct pollfd	*pfdp;
	int	pollretval;
{
	register int i;
	register int fds_found;
	extern rwlock_t svc_fd_lock;

	trace2(TR_svc_getreq_poll, 0, pollretval);
	for (i = fds_found = 0; i < svc_max_fd && fds_found < pollretval; i++) {
		register struct pollfd *p = &pfdp[i];

		if (p->revents) {
			/* fd has input waiting */
			fds_found++;
			/*
			 *	We assume that this function is only called
			 *	via someone select()ing from svc_fdset or
			 *	poll()ing from svc_pollset[].  Thus it's safe
			 *	to handle the POLLNVAL event by simply turning
			 *	the corresponding bit off in svc_fdset.  The
			 *	svc_pollset[] array is derived from svc_fdset
			 *	and so will also be updated eventually.
			 *
			 *	XXX Should we do an xprt_unregister() instead?
			 */
			if (p->revents & POLLNVAL) {
				rw_wrlock(&svc_fd_lock);
				FD_CLR(p->fd, &svc_fdset);	/* XXX */
				svc_nfds--;
				svc_nfds_set--;
				if (p->fd == (svc_max_fd - 1))
					svc_max_fd--;
				rw_unlock(&svc_fd_lock);
			} else
				svc_getreq_common(p->fd);
		}
	}
	trace2(TR_svc_getreq_poll, 1, pollretval);
}

void
svc_getreq_common(fd)
	int fd;
{
	register SVCXPRT *xprt;
	enum xprt_stat stat;
	struct rpc_msg *msg;
	struct svc_req *r;
	char *cred_area;
	extern rwlock_t svc_fd_lock;

	trace2(TR_svc_getreq_common, 0, fd);

	rw_rdlock(&svc_fd_lock);
	if ((xprt = svc_xports[fd]) == NULL) {
		syslog(LOG_ERR, svc_getreqset_errstr, fd);
		rw_unlock(&svc_fd_lock);
		trace2(TR_svc_getreq_common, 1, fd);
		return;
	}
	rw_unlock(&svc_fd_lock);
/* LINTED pointer alignment */
	msg = SVCEXT(xprt)->msg;
/* LINTED pointer alignment */
	r = SVCEXT(xprt)->req;
/* LINTED pointer alignment */
	cred_area = SVCEXT(xprt)->cred_area;
	msg->rm_call.cb_cred.oa_base = cred_area;
	msg->rm_call.cb_verf.oa_base = &(cred_area[MAX_AUTH_BYTES]);
	r->rq_clntcred = &(cred_area[2 * MAX_AUTH_BYTES]);

	/* receive msgs from xprtprt (support batch calls) */
	do {
		bool_t dispatch;

		if (dispatch = SVC_RECV(xprt, msg))
			_svc_prog_dispatch(xprt, msg, r);
		/*
		 * Check if the xprt has been disconnected in a recursive call
		 * in the service dispatch routine. If so, then break
		 */
		rw_rdlock(&svc_fd_lock);
		if (xprt != svc_xports[fd]) {
			rw_unlock(&svc_fd_lock);
			break;
		}
		rw_unlock(&svc_fd_lock);

		/*
		 * Call cleanup procedure if set.
		 */
		if (__proc_cleanup_cb != NULL && dispatch)
			(*__proc_cleanup_cb)(xprt);

		if ((stat = SVC_STAT(xprt)) == XPRT_DIED) {
			SVC_DESTROY(xprt);
			break;
		}
	} while (stat == XPRT_MOREREQS);
	trace2(TR_svc_getreq_common, 1, fd);
}

void
_svc_prog_dispatch(xprt, msg, r)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
	struct svc_req *r;
{
	register struct svc_callout *s;
	enum auth_stat why;
	int prog_found;
	u_long low_vers;
	u_long high_vers;
	void (*disp_fn)();

	trace1(TR_prog_dispatch, 0);
	r->rq_xprt = xprt;
	r->rq_prog = msg->rm_call.cb_prog;
	r->rq_vers = msg->rm_call.cb_vers;
	r->rq_proc = msg->rm_call.cb_proc;
	r->rq_cred = msg->rm_call.cb_cred;
/* LINTED pointer alignment */
	SVC_XP_AUTH(r->rq_xprt).svc_ah_ops = svc_auth_any_ops;
/* LINTED pointer alignment */
	SVC_XP_AUTH(r->rq_xprt).svc_ah_private = NULL;

	/* first authenticate the message */
	/* Check for null flavor and bypass these calls if possible */

	if (msg->rm_call.cb_cred.oa_flavor == AUTH_NULL) {
		r->rq_xprt->xp_verf.oa_flavor = _null_auth.oa_flavor;
		r->rq_xprt->xp_verf.oa_length = 0;
	} else {
		bool_t no_dispatch;

		if ((why = __gss_authenticate(r, msg,
			&no_dispatch)) != AUTH_OK) {
			svcerr_auth(xprt, why);
			trace1(TR_prog_dispatch, 1);
			return;
		}
		if (no_dispatch)
			return;
	}
	/* match message with a registered service */
	prog_found = FALSE;
	low_vers = (u_long) (0 - 1);
	high_vers = 0;
	rw_rdlock(&svc_lock);
	for (s = svc_head; s != NULL_SVC; s = s->sc_next) {
		if (s->sc_prog == r->rq_prog) {
			prog_found = TRUE;
			if (s->sc_vers == r->rq_vers) {
				if ((xprt->xp_netid == NULL) ||
				    (s->sc_netid == NULL) ||
				    (strcmp(xprt->xp_netid,
					    s->sc_netid) == 0)) {
					disp_fn = (*s->sc_dispatch);
					rw_unlock(&svc_lock);
					disp_fn(r, xprt);
					trace1(TR_prog_dispatch, 1);
					return;
				} else {
					prog_found = FALSE;
				}
			}
			if (s->sc_vers < low_vers)
				low_vers = s->sc_vers;
			if (s->sc_vers > high_vers)
				high_vers = s->sc_vers;
		}		/* found correct program */
	}
	rw_unlock(&svc_lock);

	/*
	 * if we got here, the program or version
	 * is not served ...
	 */
	if (prog_found) {
/* LINTED pointer alignment */
		if (!version_keepquiet(xprt))
			svcerr_progvers(xprt, low_vers, high_vers);
	} else {
		svcerr_noprog(xprt);
	}
	trace1(TR_prog_dispatch, 1);
}

/* ******************* SVCXPRT allocation and deallocation ***************** */

/*
 * svc_xprt_alloc() - allocate a service transport handle
 */
SVCXPRT *
svc_xprt_alloc()
{
	SVCXPRT		*xprt = NULL;
	SVCXPRT_EXT	*xt = NULL;
	SVCXPRT_LIST	*xlist = NULL;
	struct rpc_msg	*msg = NULL;
	struct svc_req	*req = NULL;
	char		*cred_area = NULL;

	if ((xprt = (SVCXPRT *) calloc(1, sizeof (SVCXPRT))) == NULL)
		goto err_exit;

	if ((xt = (SVCXPRT_EXT *) calloc(1, sizeof (SVCXPRT_EXT))) == NULL)
		goto err_exit;
	xprt->xp_p3 = (caddr_t)xt; /* SVCEXT(xprt) = xt */

	if ((xlist = (SVCXPRT_LIST *) calloc(1, sizeof (SVCXPRT_LIST))) == NULL)
		goto err_exit;
	xt->my_xlist = xlist;
	xlist->xprt = xprt;

	if ((msg = (struct rpc_msg *) malloc(sizeof (struct rpc_msg))) == NULL)
		goto err_exit;
	xt->msg = msg;

	if ((req = (struct svc_req *) malloc(sizeof (struct svc_req))) == NULL)
		goto err_exit;
	xt->req = req;

	if ((cred_area = (char *) malloc(2*MAX_AUTH_BYTES +
							RQCRED_SIZE)) == NULL)
		goto err_exit;
	xt->cred_area = cred_area;

/* LINTED pointer alignment */
	mutex_init(&svc_send_mutex(xprt), USYNC_THREAD, (void *)0);
	return (xprt);

err_exit:
	svc_xprt_free(xprt);
	return (NULL);
}


/*
 * svc_xprt_free() - free a service handle
 */
void
svc_xprt_free(xprt)
	SVCXPRT	*xprt;
{
/* LINTED pointer alignment */
	SVCXPRT_EXT	*xt = xprt ? SVCEXT(xprt) : NULL;
	SVCXPRT_LIST	*my_xlist = xt ? xt->my_xlist: NULL;
	struct rpc_msg	*msg = xt ? xt->msg : NULL;
	struct svc_req	*req = xt ? xt->req : NULL;
	char		*cred_area = xt ? xt->cred_area : NULL;

	if (xprt)
		free((char *)xprt);
	if (xt)
		free((char *)xt);
	if (my_xlist)
		free((char *)my_xlist);
	if (msg)
		free((char *)msg);
	if (req)
		free((char *)req);
	if (cred_area)
		free((char *)cred_area);
}


/*
 * svc_xprt_destroy() - free parent and child xprt list
 */
void
svc_xprt_destroy(xprt)
	SVCXPRT		*xprt;
{
	SVCXPRT_LIST	*xlist, *xnext = NULL;
	int		type;

/* LINTED pointer alignment */
	if (SVCEXT(xprt)->parent)
/* LINTED pointer alignment */
		xprt = SVCEXT(xprt)->parent;
/* LINTED pointer alignment */
	type = svc_type(xprt);
/* LINTED pointer alignment */
	for (xlist = SVCEXT(xprt)->my_xlist; xlist != NULL; xlist = xnext) {
		xnext = xlist->next;
		xprt = xlist->xprt;
		switch (type) {
		case SVC_DGRAM:
			svc_dg_xprtfree(xprt);
			break;
		case SVC_RENDEZVOUS:
			svc_vc_xprtfree(xprt);
			break;
		case SVC_CONNECTION:
			svc_fd_xprtfree(xprt);
			break;
		case SVC_DOOR:
			svc_door_xprtfree(xprt);
			break;
		}
	}
}


/*
 * svc_copy() - make a copy of parent
 */
SVCXPRT *
svc_copy(xprt)
	SVCXPRT *xprt;
{
/* LINTED pointer alignment */
	switch (svc_type(xprt)) {
	case SVC_DGRAM:
		return (svc_dg_xprtcopy(xprt));
	case SVC_RENDEZVOUS:
		return (svc_vc_xprtcopy(xprt));
	case SVC_CONNECTION:
		return (svc_fd_xprtcopy(xprt));
	}
	return ((SVCXPRT *) NULL);
}


/*
 * _svc_destroy_private() - private SVC_DESTROY interface
 */
void
_svc_destroy_private(xprt)
	SVCXPRT *xprt;
{
/* LINTED pointer alignment */
	switch (svc_type(xprt)) {
	case SVC_DGRAM:
		_svc_dg_destroy_private(xprt);
		break;
	case SVC_RENDEZVOUS:
	case SVC_CONNECTION:
		_svc_vc_destroy_private(xprt);
		break;
	}
}

/*
 * svc_get_local_cred() - fetch local user credentials.  This always
 * works over doors based transports.  For local transports, this
 * does not yield correct results unless the __rpc_negotiate_uid()
 * call has been invoked to enable this feature.
 */
bool_t
svc_get_local_cred(xprt, lcred)
	SVCXPRT			*xprt;
	svc_local_cred_t	*lcred;
{
	/* LINTED pointer alignment */
	if (svc_type(xprt) == SVC_DOOR)
		return (__svc_get_door_cred(xprt, lcred));
	return (__rpc_get_local_cred(xprt, lcred));
}


/* ******************* DUPLICATE ENTRY HANDLING ROUTINES ************** */

/*
 * the dup cacheing routines below provide a cache of received
 * transactions. rpc service routines can use this to detect
 * retransmissions and re-send a non-failure response. Uses a
 * lru scheme to find entries to get rid of entries in the cache,
 * though only DUP_DONE entries are placed on the lru list.
 * the routines were written towards development of a generic
 * SVC_DUP() interface, which can be expanded to encompass the
 * svc_dg_enablecache() routines as well. the cache is currently
 * private to the automounter.
 */


/* dupcache header contains xprt specific information */
static struct dupcache
{
	rwlock_t	dc_lock;
	time_t		dc_time;
	int		dc_buckets;
	int		dc_maxsz;
	int		dc_basis;
	struct dupreq 	*dc_mru;
	struct dupreq	**dc_hashtbl;
};

/*
 * private duplicate cache request routines
 */
static int __svc_dupcache_check(struct svc_req *, caddr_t *, u_int *,
		struct dupcache *, u_long, u_long);
static struct dupreq * __svc_dupcache_victim(struct dupcache *, time_t);
static int __svc_dupcache_enter(struct svc_req *, struct dupreq *,
		struct dupcache *, u_long, u_long, time_t);
static int __svc_dupcache_update(struct svc_req *, caddr_t, u_int, int,
		struct dupcache *, u_long, u_long);
#ifdef DUP_DEBUG
static void __svc_dupcache_debug(struct dupcache *);
#endif /* DUP_DEBUG */

/* default parameters for the dupcache */
#define	DUPCACHE_BUCKETS	257
#define	DUPCACHE_TIME		900
#define	DUPCACHE_MAXSZ		INT_MAX

/*
 * __svc_dupcache_init(void *condition, int basis, char *xprt_cache)
 * initialize the duprequest cache and assign it to the xprt_cache
 * Use default values depending on the cache condition and basis.
 * return TRUE on success and FALSE on failure
 */
bool_t
__svc_dupcache_init(void *condition, int basis, char **xprt_cache)
{
	static mutex_t initdc_lock = DEFAULTMUTEX;
	int i;
	struct dupcache *dc;

	mutex_lock(&initdc_lock);
	if (*xprt_cache != NULL) { /* do only once per xprt */
		mutex_unlock(&initdc_lock);
		syslog(LOG_ERR,
		"__svc_dupcache_init: multiply defined dup cache");
		return (FALSE);
	}

	switch (basis) {
	case DUPCACHE_FIXEDTIME:
		dc = (struct dupcache *) mem_alloc(sizeof (struct dupcache));
		if (dc == NULL) {
			syslog(LOG_ERR,
				"__svc_dupcache_init: memory alloc failed");
			return (FALSE);
		}
		rwlock_init(&(dc->dc_lock), USYNC_THREAD, NULL);
		if (condition != NULL)
			dc->dc_time = *((time_t *)condition);
		else
			dc->dc_time = DUPCACHE_TIME;
		dc->dc_buckets = DUPCACHE_BUCKETS;
		dc->dc_maxsz = DUPCACHE_MAXSZ;
		dc->dc_basis = basis;
		dc->dc_mru = NULL;
		dc->dc_hashtbl = (struct dupreq **) mem_alloc(dc->dc_buckets *
						sizeof (struct dupreq *));
		if (dc->dc_hashtbl == NULL) {
			syslog(LOG_ERR,
				"__svc_dupcache_init: memory alloc failed");
			return (FALSE);
		}
		for (i = 0; i < DUPCACHE_BUCKETS; i++)
			dc->dc_hashtbl[i] = NULL;
		*xprt_cache = (char *)dc;
		break;
	default:
		mutex_unlock(&initdc_lock);
		syslog(LOG_ERR,
		"__svc_dupcache_init: undefined dup cache basis");
		return (FALSE);
	}

	mutex_unlock(&initdc_lock);

	return (TRUE);
}

/*
 * __svc_dup(struct svc_req *req, caddr_t *resp_buf, u_int *resp_bufsz,
 *	char *xprt_cache)
 * searches the request cache. Creates an entry and returns DUP_NEW if
 * the request is not found in the cache.  If it is found, then it
 * returns the state of the request (in progress, drop, or done) and
 * also allocates, and passes back results to the user (if any) in
 * resp_buf, and its length in resp_bufsz. DUP_ERROR is returned on error.
 */
int
__svc_dup(struct svc_req *req, caddr_t *resp_buf, u_int *resp_bufsz,
	char *xprt_cache)
{
	u_long drxid, drhash;
	int rc;
	struct dupreq *dr = NULL;
	time_t timenow = time(NULL);

	/* LINTED pointer alignment */
	struct dupcache *dc = (struct dupcache *)xprt_cache;

	if (dc == NULL) {
		syslog(LOG_ERR, "__svc_dup: undefined cache");
		return (DUP_ERROR);
	}

	/* get the xid of the request */
	if (SVC_CONTROL(req->rq_xprt, SVCGET_XID, (void*) &drxid) == FALSE) {
		syslog(LOG_ERR, "__svc_dup: xid error");
		return (DUP_ERROR);
	}
	drhash = drxid % dc->dc_buckets;

	if ((rc = __svc_dupcache_check(req, resp_buf, resp_bufsz, dc, drxid,
			drhash)) != DUP_NEW)
		return (rc);

	if ((dr = __svc_dupcache_victim(dc, timenow)) == NULL)
		return (DUP_ERROR);

	if ((rc = __svc_dupcache_enter(req, dr, dc, drxid, drhash, timenow))
			== DUP_ERROR)
		return (rc);

	return (DUP_NEW);
}



/*
 * __svc_dupcache_check(struct svc_req *req, caddr_t *resp_buf,
 *		u_int *resp_bufsz,truct dupcache *dc, u_long drxid,
 * 		u_long drhash)
 * Checks to see whether an entry already exists in the cache. If it does
 * copy back into the resp_buf, if appropriate. Return the status of
 * the request, or DUP_NEW if the entry is not in the cache
 */
static int
__svc_dupcache_check(struct svc_req *req, caddr_t *resp_buf, u_int *resp_bufsz,
		struct dupcache *dc, u_long drxid, u_long drhash)
{
	struct dupreq *dr = NULL;

	rw_rdlock(&(dc->dc_lock));
	dr = dc->dc_hashtbl[drhash];
	while (dr != NULL) {
		if (dr->dr_xid == drxid &&
		    dr->dr_proc == req->rq_proc &&
		    dr->dr_prog == req->rq_prog &&
		    dr->dr_vers == req->rq_vers &&
		    dr->dr_addr.len == req->rq_xprt->xp_rtaddr.len &&
		    memcmp((caddr_t)dr->dr_addr.buf,
				(caddr_t)req->rq_xprt->xp_rtaddr.buf,
				dr->dr_addr.len) == 0) { /* entry found */
			if (dr->dr_hash != drhash) {
				/* sanity check */
				rw_unlock((&dc->dc_lock));
				syslog(LOG_ERR,
					"\n__svc_dupdone: hashing error");
				return (DUP_ERROR);
			}

			/*
			 * return results for requests on lru list, if
			 * appropriate requests must be DUP_DROP or DUP_DONE
			 * to have a result. A NULL buffer in the cache
			 * implies no results were sent during dupdone.
			 * A NULL buffer in the call implies not interested
			 * in results.
			 */
			if (((dr->dr_status == DUP_DONE) ||
				(dr->dr_status == DUP_DROP)) &&
				resp_buf != NULL &&
				dr->dr_resp.buf != NULL) {
				*resp_buf = (caddr_t) mem_alloc
					(dr->dr_resp.len);
				if (*resp_buf == NULL) {
					syslog(LOG_ERR,
					"__svc_dupcache_check: malloc failed");
					return (DUP_ERROR);
				}
				memset((caddr_t) *resp_buf, 0,
					dr->dr_resp.len);
				memcpy(*resp_buf, (caddr_t) dr->dr_resp.buf,
					dr->dr_resp.len);
				*resp_bufsz = dr->dr_resp.len;
			} else {
				/* no result */
				if (resp_buf)
					*resp_buf = NULL;
				if (resp_bufsz)
					*resp_bufsz = 0;
			}
			rw_unlock(&(dc->dc_lock));
			return (dr->dr_status);
		}
		dr = dr->dr_chain;
	}
	rw_unlock(&(dc->dc_lock));
	return (DUP_NEW);
}

/*
 * __svc_dupcache_victim(struct dupcache *dc, time_t timenow)
 * Return a victim dupreq entry to the caller, depending on cache policy.
 */
static struct dupreq *
__svc_dupcache_victim(struct dupcache *dc, time_t timenow)
{
	struct dupreq *dr = NULL;

	switch (dc->dc_basis) {
	case DUPCACHE_FIXEDTIME:
		/*
		 * The hash policy is to free up a bit of the hash
		 * table before allocating a new entry as the victim.
		 * Freeing up the hash table each time should split
		 * the cost of keeping the hash table clean among threads.
		 * Note that only DONE or DROPPED entries are on the lru
		 * list but we do a sanity check anyway.
		 */
		rw_wrlock(&(dc->dc_lock));
		while ((dc->dc_mru) && (dr = dc->dc_mru->dr_next) &&
				((timenow - dr->dr_time) > dc->dc_time)) {
			/* clean and then free the entry */
			if (dr->dr_status != DUP_DONE &&
				dr->dr_status != DUP_DROP) {
				rw_unlock(&(dc->dc_lock));
				syslog(LOG_ERR,
				"__svc_dupcache_victim: bad victim");
#ifdef DUP_DEBUG
				__svc_dupcache_debug(dc);
#endif /* DUP_DEBUG */
			}
			/* free buffers */
			if (dr->dr_resp.buf) {
				mem_free(dr->dr_resp.buf, dr->dr_resp.len);
				dr->dr_resp.buf = NULL;
			}
			if (dr->dr_addr.buf) {
				mem_free(dr->dr_addr.buf, dr->dr_addr.len);
				dr->dr_addr.buf = NULL;
			}

			/* unhash the entry */
			if (dr->dr_chain)
				dr->dr_chain->dr_prevchain = dr->dr_prevchain;
			if (dr->dr_prevchain)
				dr->dr_prevchain->dr_chain = dr->dr_chain;
			if (dc->dc_hashtbl[dr->dr_hash] == dr)
				dc->dc_hashtbl[dr->dr_hash] = dr->dr_chain;

			/* modify the lru pointers */
			if (dc->dc_mru == dr)
				dc->dc_mru = NULL;
			else {
				dc->dc_mru->dr_next = dr->dr_next;
				dr->dr_next->dr_prev = dc->dc_mru;
			}
			mem_free(dr, sizeof (struct dupreq));
			dr = NULL;
		}
		rw_unlock(&(dc->dc_lock));

		/*
		 * Allocate and return new clean entry as victim
		 */
		if ((dr = (struct dupreq *)mem_alloc(sizeof (*dr))) == NULL) {
			syslog(LOG_ERR,
				"__svc_dupcache_victim: mem_alloc failed");
			return (NULL);
		}
		memset((caddr_t)dr, 0, sizeof (*dr));
		return (dr);
	default:
		syslog(LOG_ERR,
		"__svc_dupcache_victim: undefined dup cache_basis");
		return (NULL);
	}
}

/*
 * __svc_dupcache_enter(struct svc_req *req, struct dupreq *dr,
 *	struct dupcache *dc, u_long drxid, u_long drhash, time_t timenow)
 * build new duprequest entry and then insert into the cache
 */
static int
__svc_dupcache_enter(struct svc_req *req, struct dupreq *dr,
	struct dupcache *dc, u_long drxid, u_long drhash, time_t timenow)
{
	dr->dr_xid = drxid;
	dr->dr_prog = req->rq_prog;
	dr->dr_vers = req->rq_vers;
	dr->dr_proc = req->rq_proc;
	dr->dr_addr.maxlen = req->rq_xprt->xp_rtaddr.len;
	dr->dr_addr.len = dr->dr_addr.maxlen;
	if ((dr->dr_addr.buf = (caddr_t)mem_alloc(dr->dr_addr.maxlen))
				== NULL) {
		syslog(LOG_ERR, "__svc_dupcache_enter: mem_alloc failed");
		mem_free(dr, sizeof (struct dupreq));
		return (DUP_ERROR);
	}
	memset(dr->dr_addr.buf, 0, dr->dr_addr.len);
	memcpy((caddr_t) dr->dr_addr.buf,
		(caddr_t)req->rq_xprt->xp_rtaddr.buf, dr->dr_addr.len);
	dr->dr_resp.buf = NULL;
	dr->dr_resp.maxlen = 0;
	dr->dr_resp.len = 0;
	dr->dr_status = DUP_INPROGRESS;
	dr->dr_time = timenow;
	dr->dr_hash = drhash;	/* needed for efficient victim cleanup */

	/* place entry at head of hash table */
	rw_wrlock(&(dc->dc_lock));
	dr->dr_chain = dc->dc_hashtbl[drhash];
	dr->dr_prevchain = NULL;
	if (dc->dc_hashtbl[drhash] != NULL)
		dc->dc_hashtbl[drhash]->dr_prevchain = dr;
	dc->dc_hashtbl[drhash] = dr;
	rw_unlock(&(dc->dc_lock));
	return (DUP_NEW);
}

/*
 * __svc_dupdone(struct svc_req *req, caddr_t resp_buf, u_int resp_bufsz,
 *		int status, char *xprt_cache)
 * Marks the request done (DUP_DONE or DUP_DROP) and stores the response.
 * Only DONE and DROP requests can be marked as done. Sets the lru pointers
 * to make the entry the most recently used. Returns DUP_ERROR or status.
 */
int
__svc_dupdone(struct svc_req *req, caddr_t resp_buf, u_int resp_bufsz,
		int status, char *xprt_cache)
{
	u_long drxid, drhash;
	int rc;

	/* LINTED pointer alignment */
	struct dupcache *dc = (struct dupcache *)xprt_cache;

	if (dc == NULL) {
		syslog(LOG_ERR, "__svc_dupdone: undefined cache");
		return (DUP_ERROR);
	}

	if (status != DUP_DONE && status != DUP_DROP) {
		syslog(LOG_ERR, "__svc_dupdone: invalid dupdone status");
		syslog(LOG_ERR, "         must be DUP_DONE or DUP_DROP");
		return (DUP_ERROR);
	}

	/* find the xid of the entry in the cache */
	if (SVC_CONTROL(req->rq_xprt, SVCGET_XID, (void*) &drxid) == FALSE) {
		syslog(LOG_ERR, "__svc_dup: xid error");
		return (DUP_ERROR);
	}
	drhash = drxid % dc->dc_buckets;

	/* update the status of the entry and result buffers, if required */
	if ((rc = __svc_dupcache_update(req, resp_buf, resp_bufsz, status,
			dc, drxid, drhash)) == DUP_ERROR) {
		syslog(LOG_ERR, "__svc_dupdone: cache entry error");
		return (DUP_ERROR);
	}

	return (rc);
}

/*
 * __svc_dupcache_update(struct svc_req *req, caddr_t resp_buf,
 * 	u_int resp_bufsz, int status, struct dupcache *dc, u_long drxid,
 * 	u_long drhash)
 * Check if entry exists in the dupcacache. If it does, update its status
 * and time and also its buffer, if appropriate. Its possible, but unlikely
 * for DONE requests to not exist in the cache. Return DUP_ERROR or status.
 */
static int
__svc_dupcache_update(struct svc_req *req, caddr_t resp_buf, u_int resp_bufsz,
		int status, struct dupcache *dc, u_long drxid, u_long drhash)
{
	struct dupreq *dr = NULL;
	time_t timenow = time(NULL);

	rw_wrlock(&(dc->dc_lock));
	dr = dc->dc_hashtbl[drhash];
	while (dr != NULL) {
		if (dr->dr_xid == drxid &&
		    dr->dr_proc == req->rq_proc &&
		    dr->dr_prog == req->rq_prog &&
		    dr->dr_vers == req->rq_vers &&
		    dr->dr_addr.len == req->rq_xprt->xp_rtaddr.len &&
		    memcmp((caddr_t)dr->dr_addr.buf,
				(caddr_t)req->rq_xprt->xp_rtaddr.buf,
				dr->dr_addr.len) == 0) { /* entry found */
			if (dr->dr_hash != drhash) {
				/* sanity check */
				rw_unlock(&(dc->dc_lock));
				syslog(LOG_ERR,
				"\n__svc_dupdone: hashing error");
				return (DUP_ERROR);
			}

			/* store the results if bufer is not NULL */
			if (resp_buf != NULL) {
				if ((dr->dr_resp.buf = (caddr_t)
					mem_alloc(resp_bufsz)) == NULL) {
					rw_unlock(&(dc->dc_lock));
					syslog(LOG_ERR,
					"__svc_dupdone: mem_alloc failed");
					return (DUP_ERROR);
				}
				memset(dr->dr_resp.buf, 0, resp_bufsz);
				memcpy((caddr_t)dr->dr_resp.buf, resp_buf,
					(u_int)resp_bufsz);
				dr->dr_resp.len = resp_bufsz;
			}

			/* update status and done time */
			dr->dr_status = status;
			dr->dr_time = timenow;

			/* move the entry to the mru position */
			if (dc->dc_mru == NULL) {
				dr->dr_next = dr;
				dr->dr_prev = dr;
			} else {
				dr->dr_next = dc->dc_mru->dr_next;
				dc->dc_mru->dr_next->dr_prev = dr;
				dr->dr_prev = dc->dc_mru;
				dc->dc_mru->dr_next = dr;
			}
			dc->dc_mru = dr;

			rw_unlock(&(dc->dc_lock));
			return (status);
		}
		dr = dr->dr_chain;
	}
	rw_unlock(&(dc->dc_lock));
	syslog(LOG_ERR, "__svc_dupdone: entry not in dup cache");
	return (DUP_ERROR);
}

#ifdef DUP_DEBUG
/*
 * __svc_dupcache_debug(struct dupcache *dc)
 * print out the hash table stuff
 */
static void
__svc_dupcache_debug(struct dupcache *dc)
{
	struct dupreq *dr = NULL;
	int i;
	bool_t bval;

	rw_rdlock(&dc->dc_lock);
	fprintf(stderr, "   HASHTABLE\n");
	for (i = 0; i < dc->dc_buckets; i++) {
		bval = FALSE;
		dr = dc->dc_hashtbl[i];
		while (dr != NULL) {
			if (!bval) {	/* ensures bucket printed only once */
				fprintf(stderr, "    bucket : %d\n", i);
				bval = TRUE;
			}
			fprintf(stderr, "\txid: %u status: %d time: %d",
				dr->dr_xid, dr->dr_status, dr->dr_time);
			fprintf(stderr, " dr: %x chain: %x prevchain: %x\n",
				dr, dr->dr_chain, dr->dr_prevchain);
			dr = dr->dr_chain;
		}
	}

	fprintf(stderr, "   LRU\n");
	if (dc->dc_mru) {
		dr = dc->dc_mru->dr_next;	/* lru */
		while (dr != dc->dc_mru) {
			fprintf(stderr, "\txid: %u status : %d time : %d",
				dr->dr_xid, dr->dr_status, dr->dr_time);
			fprintf(stderr, " dr: %x next: %x prev: %x\n",
				dr, dr->dr_next, dr->dr_prev);
			dr = dr->dr_next;
		}
		fprintf(stderr, "\txid: %u status: %d time: %d",
			dr->dr_xid, dr->dr_status, dr->dr_time);
		fprintf(stderr, " dr: %x next: %x prev: %x\n", dr,
			dr->dr_next, dr->dr_prev);
	}
	rw_unlock(&dc->dc_lock);
}
#endif /* DUP_DEBUG */
