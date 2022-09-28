/*
 * Copyright (c) 1995,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)rpcsec_gss.c 1.15     96/10/18 SMI"

#include  <rpc/rpc.h>

static	void	rpc_gss_nextverf();
static	bool_t	rpc_gss_marshall();
static	bool_t	rpc_gss_validate();
static	bool_t	rpc_gss_refresh();
static	void	rpc_gss_destroy();
bool_t		rpc_gss_wrap();
bool_t		rpc_gss_unwrap();

struct auth_ops rpc_gss_ops = {
	rpc_gss_nextverf,
	rpc_gss_marshall,
	rpc_gss_validate,
	rpc_gss_refresh,
	rpc_gss_destroy,
	rpc_gss_wrap,
	rpc_gss_unwrap,
};


/*
 *  Get the client gss security service handle.
 *  If it is in the cache table, get it, otherwise, create
 *  a new one by calling rpc_gss_seccreate().
 *
 */
/* ARGSUSED */
AUTH *
rpc_gss_secget(
	CLIENT  *clnt,
	char	*principal,	/* The server service principal service@host. */
	rpc_gss_OID	mechanism,
	rpc_gss_service_t service_type,
	u_int	qop,
	rpc_gss_options_req_t *options_req,
	rpc_gss_options_ret_t *options_ret,
	void *cache_key,
	cred_t *cr)
{
	return (NULL);
}



/*
 *  rpc_gss_secfree will destroy a rpcsec_gss context only if its
 *  reference count drops to zero.
 *
 */
/* ARGSUSED */
void
rpc_gss_secfree(AUTH *auth)
{
	(void) rpc_gss_destroy(auth);
}


/*
 *  Create a gss security service context.
 */
/* ARGSUSED */
AUTH *
rpc_gss_seccreate(
	CLIENT			*clnt,		/* client handle */
	char			*principal,	/* target service name */
	rpc_gss_OID		mechanism,	/* security mechanism */
	rpc_gss_service_t	service_type,	/* integrity, privacy, ... */
	u_int			qop,		/* requested QOP */
	rpc_gss_options_req_t	*options_req,	/* requested options */
	rpc_gss_options_ret_t	*options_ret,	/* returned options */
	cred_t			*cr)		/* client's unix cred */
{
	return (NULL);
}

/*
 * Set service defaults.
 */
/* ARGSUSED */
bool_t
rpc_gss_set_defaults(auth, service, qop)
	AUTH			*auth;
	rpc_gss_service_t	service;
	u_int			qop;
{
	return (FALSE);
}

/*
 * Function: rpc_gss_nextverf.  Not used.
 */
/* ARGSUSED */
static void
rpc_gss_nextverf()
{
}

/*
 * Function: rpc_gss_marshall - no op routine.
 *		rpc_gss_wrap() is doing the marshalling.
 */
/* ARGSUSED */
static bool_t
rpc_gss_marshall(auth, xdrs)
	AUTH		*auth;
	XDR		*xdrs;
{
	return (TRUE);
}

/*
 * Validate RPC response verifier from server.  The response verifier
 * is the checksum of the request sequence number.
 */
/* ARGSUSED */
static bool_t
rpc_gss_validate(auth, verf)
	AUTH			*auth;
	struct opaque_auth	*verf;
{
	return (FALSE);
}

/*
 * Refresh client context.  This is necessary sometimes because the
 * server will ocassionally destroy contexts based on LRU method, or
 * because of expired credentials.
 */
/* ARGSUSED */
static bool_t
rpc_gss_refresh(auth, msg, cr)
	AUTH		*auth;
	struct rpc_msg	*msg;
	cred_t		*cr;
{
	return (FALSE);
}

/*
 * Destroy a context.
 */
/* ARGSUSED */
static void
rpc_gss_destroy(auth)
	AUTH		*auth;
{
}


/*
 * Wrap client side data.  The encoded header is passed in through
 * buf and buflen.  The header is up to but not including the
 * credential field.
 */
/* ARGSUSED */
bool_t
rpc_gss_wrap(auth, buf, buflen, out_xdrs, xdr_func, xdr_ptr)
	AUTH			*auth;
	char			*buf;		/* encoded header */
	u_int			buflen;		/* encoded header length */
	XDR			*out_xdrs;
	xdrproc_t		xdr_func;
	caddr_t			xdr_ptr;
{
	return (FALSE);

}

/*
 * Unwrap received data.
 */
/* ARGSUSED */
bool_t
rpc_gss_unwrap(auth, in_xdrs, xdr_func, xdr_ptr)
	AUTH			*auth;
	XDR			*in_xdrs;
	bool_t			(*xdr_func)();
	caddr_t			xdr_ptr;
{
	return (FALSE);
}


/*
 *  return errno.
 */
/* ARGSUSED */
int
rpc_gss_revauth(uid_t uid, rpc_gss_OID mech)
{
	return (0);
}

/* ARGSUSED */
void
rpc_gss_secpurge(void *cache_key)
{
}

/*
 * Set server callback.
 */
/* ARGSUSED */
bool_t
rpc_gss_set_callback(cb)
	rpc_gss_callback_t	*cb;
{
	return (FALSE);
}

/*
 * Get caller credentials.
 */
/* ARGSUSED */
bool_t
rpc_gss_getcred(req, rcred, ucred, cookie)
	struct svc_req		*req;
	rpc_gss_rawcred_t	**rcred;
	rpc_gss_ucred_t		**ucred;
	void			**cookie;
{
	return (FALSE);
}

/*
 * Server side authentication for RPCSEC_GSS.
 */
/* ARGSUSED */
enum auth_stat
__svcrpcsec_gss(rqst, msg, no_dispatch)
	struct svc_req		*rqst;
	struct rpc_msg		*msg;
	bool_t			*no_dispatch;
{
	return (AUTH_FAILED);
}

/*
 * Set the server principal name.
 */
/* ARGSUSED */
bool_t
rpc_gss_set_svc_name(
	char		*principal, /* service principal name, service@host */
	rpc_gss_OID	mechanism,
	u_int		req_time,
	u_int		program,
	u_int		version)
{
	return (FALSE);
}


/*
 * Encrypt the serialized arguments from xdr_func applied to xdr_ptr
 * and write the result to xdrs.
 */
/* ARGSUSED */
bool_t
svc_rpc_gss_wrap(auth, out_xdrs, xdr_func, xdr_ptr)
	SVCAUTH			*auth;
	XDR			*out_xdrs;
	bool_t			(*xdr_func)();
	caddr_t			xdr_ptr;
{
	return (FALSE);
}

/*
 * Decrypt the serialized arguments and XDR decode them.
 */
/* ARGSUSED */
bool_t
svc_rpc_gss_unwrap(auth, in_xdrs, xdr_func, xdr_ptr)
	SVCAUTH			*auth;
	XDR			*in_xdrs;
	bool_t			(*xdr_func)();
	caddr_t			xdr_ptr;
{
	return (FALSE);
}


/* ARGSUSED */
void
rpc_gss_cleanup(SVCXPRT *xprt)
{
}


/* ARGSUSED */
bool_t
rpc_gss_get_versions(u_int *vers_hi, u_int *vers_lo)
{
	return (FALSE);
}


/* ARGSUSED */
int
rpc_gss_max_data_length(AUTH *rpcgss_handle, int max_tp_unit_len)
{
	return (0);
}


/* ARGSUSED */
int
rpc_gss_svc_max_data_length(struct svc_req *req, int max_tp_unit_len)
{
	return (0);
}
