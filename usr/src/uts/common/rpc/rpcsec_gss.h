/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * rpcsec_gss.h, RPCSEC_GSS security service interface.
 */

#ifndef	_RPCSEC_GSS_H
#define	_RPCSEC_GSS_H

#pragma ident	"@(#)rpcsec_gss.h	1.17	96/10/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <rpc/auth.h>
#include <rpc/clnt.h>

/*
 * Interface definitions.
 */
#define	MAX_NAME_LEN			 64
#define	MAX_GSS_MECH			128
#define	MAX_GSS_NAME			128

typedef enum {
	rpc_gss_svc_default = 0,
	rpc_gss_svc_none = 1,
	rpc_gss_svc_integrity = 2,
	rpc_gss_svc_privacy = 3
} rpc_gss_service_t;

/*
 * GSS-API based security mechanism type specified as
 * object identifiers (OIDs).
 * This type is derived from gss_OID_desc/gss_OID.
 */
typedef struct {
	u_int	length;
	void	*elements;
} rpc_gss_OID_desc, *rpc_gss_OID;

/*
 * Interface data.
 */
typedef struct rpc_gss_principal {
	int	len;
	char	name[1];
} *rpc_gss_principal_t;

#ifndef _GSSAPI_H_
typedef void * gss_ctx_id_t;
typedef void * gss_cred_id_t;
typedef void * gss_channel_bindings_t;
#endif

typedef struct {
	int			req_flags;
	int			time_req;
	gss_cred_id_t		my_cred;
	gss_channel_bindings_t	input_channel_bindings;
} rpc_gss_options_req_t;

typedef struct {
	int			major_status;
	int			minor_status;
	u_int			rpcsec_version;
	int			ret_flags;
	int			time_ret;
	gss_ctx_id_t		gss_context;
#ifdef _KERNEL
	rpc_gss_OID		actual_mechanism;
#else
	char			actual_mechanism[MAX_GSS_MECH];
#endif
} rpc_gss_options_ret_t;

/*
 * raw credentials
 */
typedef struct {
	u_int			version;
#ifdef _KERNEL
	rpc_gss_OID		mechanism;
	u_int			qop;
#else
	char			*mechanism;
	char			*qop;
#endif
	rpc_gss_principal_t	client_principal;
	char	*svc_principal;	/* service@server, e.g. nfs@caribe */
	rpc_gss_service_t	service;
} rpc_gss_rawcred_t;

/*
 * unix credentials
 */
typedef struct {
	uid_t			uid;
	gid_t			gid;
	short			gidlen;
	gid_t			*gidlist;
} rpc_gss_ucred_t;

/*
 * for callback routine
 */
typedef struct {
	u_int			program;
	u_int			version;
	bool_t			(*callback)();
} rpc_gss_callback_t;

/*
 * lock used for the callback routine
 */
typedef struct {
	bool_t			locked;
	rpc_gss_rawcred_t	*raw_cred;
} rpc_gss_lock_t;


/*
 * This is for user RPC applications.
 * Structure used to fetch the error code when one of
 * the rpc_gss_* routines fails.
 */
typedef struct {
	int	rpc_gss_error;
	int	system_error;
} rpc_gss_error_t;

#define	RPC_GSS_ER_SUCCESS	0	/* no error */
#define	RPC_GSS_ER_SYSTEMERROR	1	/* system error */


/*
 * This is for Kernel RPC applications.
 * RPCSEC_GSS flavor specific data in sec_data opaque field.
 */
typedef struct gss_clnt_data {
	rpc_gss_OID_desc	mechanism;
	rpc_gss_service_t	service;
	char		uname[MAX_NAME_LEN];	/* server's user name */
	char		inst[MAX_NAME_LEN];	/* server's instance name */
	char		realm[MAX_NAME_LEN];	/* server's realm */
	u_int		qop;
} gss_clntdata_t;


struct svc_req;
/*
 *  KERNEL rpc_gss_* interfaces.
 */
#ifdef _KERNEL
AUTH	*rpc_gss_secget(CLIENT *, char *, rpc_gss_OID,
			rpc_gss_service_t, u_int, rpc_gss_options_req_t *,
			rpc_gss_options_ret_t *, void *, cred_t *);

void	rpc_gss_secfree(AUTH *);

AUTH	*rpc_gss_seccreate(CLIENT *, char *, rpc_gss_OID,
			rpc_gss_service_t, u_int, rpc_gss_options_req_t *,
			rpc_gss_options_ret_t *, cred_t *);

int rpc_gss_revauth(uid_t, rpc_gss_OID);
void rpc_gss_secpurge(void *);
bool_t rpc_gss_set_svc_name(char *, rpc_gss_OID, u_int, u_int, u_int);
enum auth_stat __svcrpcsec_gss(struct svc_req *,
			struct rpc_msg *, bool_t *);
bool_t rpc_gss_set_defaults(AUTH *, rpc_gss_service_t, u_int);


#else
/*
 *  USER rpc_gss_* public interfaces
 */
AUTH *
rpc_gss_seccreate(
	CLIENT			*clnt,		/* associated client handle */
	char			*principal,	/* server service principal */
	char			*mechanism,	/* security mechanism */
	rpc_gss_service_t	service_type,	/* security service */
	char			*qop,		/* requested QOP */
	rpc_gss_options_req_t	*options_req,	/* requested options */
	rpc_gss_options_ret_t   *options_ret    /* returned options */
);

bool_t
rpc_gss_get_principal_name(
	rpc_gss_principal_t	*principal,
	char			*mechanism,
	char			*user_name,
	char			*node,
	char			*secdomain
);

char **rpc_gss_get_mechanisms();

char **rpc_gss_get_mech_info(
	char			*mechanism,
	rpc_gss_service_t	*service
);

bool_t
rpc_gss_is_installed(
	char	*mechanism
);

bool_t
rpc_gss_mech_to_oid(
	char		*mech,
	rpc_gss_OID	*oid
);

bool_t
rpc_gss_qop_to_num(
	char	*qop,
	char	*mech,
	u_int	*num
);

bool_t
rpc_gss_set_svc_name(
	char			*principal,
	char			*mechanism,
	u_int			req_time,
	u_int			program,
	u_int			version
);

bool_t
rpc_gss_set_defaults(
	AUTH			*auth,
	rpc_gss_service_t	service,
	char			*qop
);

void
rpc_gss_get_error(
	rpc_gss_error_t		*error
);

/*
 * User level private interfaces
 */
enum auth_stat __svcrpcsec_gss();
bool_t	__rpc_gss_wrap();
bool_t	__rpc_gss_unwrap();

#endif

/*
 *  USER and KERNEL rpc_gss_* interfaces.
 */
bool_t
rpc_gss_set_callback(
	rpc_gss_callback_t	*cb
);

bool_t
rpc_gss_getcred(
	struct svc_req		*req,
	rpc_gss_rawcred_t	**rcred,
	rpc_gss_ucred_t		**ucred,
	void			**cookie
);

int
rpc_gss_max_data_length(
	AUTH			*rpcgss_handle,
	int			max_tp_unit_len
);

int
rpc_gss_svc_max_data_length(
	struct	svc_req		*req,
	int			max_tp_unit_len
);

bool_t
rpc_gss_get_versions(
	u_int	*vers_hi,
	u_int	*vers_lo
);


/*
 * Protocol data.
 *
 * The reason to put these definition in this header file
 * is for 2.6 snoop to handle the RPCSEC_GSS protocol
 * interpretation.
 */
#define	RPCSEC_GSS_NULL			0
#define	RPCSEC_GSS_INIT			1
#define	RPCSEC_GSS_CONTINUE_INIT	2
#define	RPCSEC_GSS_DESTROY		3

#define	RPCSEC_GSS_VERSION		1

#ifdef	__cplusplus
}
#endif

#endif	/* !_RPCSEC_GSS_H */
