/*
 * Copyright (c) 1995,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sec_clnt.c	1.24	96/10/21 SMI"	/* SVr4.0 1.14	*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/tiuser.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/session.h>
#include <sys/dnlc.h>
#include <sys/bitmap.h>

#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_des.h>	/* for authdes_create() */
#include <rpc/clnt.h>
#include <rpc/rpcsec_gss.h>

#include <kerberos/krb.h>

#define	MAXCLIENTS	16

static u_int authdes_win = 5*60;  /* 5 minutes -- should be mount option */

static struct desauthent {
	struct	sec_data *da_data;
	uid_t da_uid;
	short da_inuse;
	AUTH *da_auth;
} desauthtab[MAXCLIENTS];
static int nextdesvictim;
static kmutex_t desauthtab_lock;	/* Lock to protect DES auth cache */

static u_int authkerb_win = 5*60;  /* 5 minutes -- should be mount option */

static struct kerbauthent {
	struct  sec_data *ka_data;
	uid_t ka_uid;
	short ka_inuse;
	AUTH *ka_auth;
} kerbauthtab[MAXCLIENTS];
static int nextkerbvictim;
static kmutex_t kerbauthtab_lock;	/* Lock to protect KERB auth cache */

struct unixauthent {
	struct unixauthent *ua_next;
	AUTH *ua_auth;
};
/* list of avail UNIX auth handles */
static struct unixauthent *unixauthtab = NULL;
/* list of all UNIX auth handles */
static struct unixauthent *unixauthlist;
static kmutex_t unixauthtab_lock; 	/* Lock to protect UNIX auth cache */

/* RPC stuff */
kmutex_t authdes_ops_lock;   /* auth_ops initialization in authdes_ops() */
kmutex_t authkerb_ops_lock;  /* auth_ops initialization in authkerb_ops() */
kmutex_t authdes_lock;    /* protects the authdes cache (svcauthdes.c) */
kmutex_t authkerb_lock;   /* protects the authkerb cache (svcauthkerb.c) */
/* protects the svcauthdes_stats structure (svcauthdes.c) */
kmutex_t svcauthdesstats_lock;
/* protects the svcauthkerb_stats structure (svcauthkerb.c) */
kmutex_t svcauthkerbstats_lock;

static void  kerb_create_failure(int);
static void  purge_authtab(struct sec_data *);

/*
 *  Load RPCSEC_GSS specific data from user space to kernel space.
 */
long
gss_clnt_loadinfo(caddr_t usrdata, caddr_t *kdata)
{
	struct gss_clnt_data *data;
	caddr_t	elements;

	/* map opaque data to gss specific strucutre */
	data = (struct gss_clnt_data *) kmem_alloc(sizeof (*data), KM_SLEEP);

	if (!copyin((caddr_t) usrdata, (caddr_t) data, sizeof (*data))) {
	    if (data->mechanism.length > 0) {
		elements = kmem_alloc(data->mechanism.length, KM_SLEEP);
		if (!(copyin((caddr_t) data->mechanism.elements, elements,
				data->mechanism.length))) {
			data->mechanism.elements = elements;
			*kdata = (caddr_t) data;
			return (0);
		} else
			kmem_free(elements, data->mechanism.length);
	    }
	}

	*kdata = NULL;
	kmem_free((caddr_t) data, sizeof (*data));
	return (EFAULT);
}


/*
 *  Load AUTH_DES or AUTH_KERB specific data from user space
 *  to kernel space.
 */
long
dh_k4_clnt_loadinfo(caddr_t usrdata, caddr_t *kdata)
{
	int nlen, error = 0;
	char *userbufptr;
	dh_k4_clntdata_t *data;
	char	netname[MAXNETNAMELEN+1];
	struct	netbuf *syncaddr;
	struct knetconfig *knconf;

	/* map opaque data to krb4/des specific strucutre */
	data = (dh_k4_clntdata_t *) kmem_alloc(sizeof (*data), KM_SLEEP);

	if (copyin((caddr_t) usrdata, (caddr_t) data, sizeof (*data)))
		error = EFAULT;
	else {
	    syncaddr = &data->syncaddr;
	    if (syncaddr == NULL)
		error = EINVAL;
	    else {
		userbufptr = syncaddr->buf;
		syncaddr->buf = (char *) kmem_alloc(syncaddr->len, KM_SLEEP);
		syncaddr->maxlen = syncaddr->len;
		if (copyin(userbufptr, syncaddr->buf, syncaddr->len)) {
			kmem_free((caddr_t) syncaddr->buf, syncaddr->len);
			syncaddr->buf = (char *) NULL;
			error = EFAULT;
		} else {
			copyinstr(data->netname, netname, sizeof (netname),
					(u_int *) &nlen);
			if (nlen > 0) {
			    data->netname = (char *) kmem_alloc((u_int)nlen,
								KM_SLEEP);
			    bcopy(netname, data->netname, (u_int)nlen);
			    data->netnamelen = nlen;
			}
		}
	    }
	}

	if (!error) {
	/*
	 * Allocate space for a knetconfig structure and
	 * its strings and copy in from user-land.
	 */
	    knconf = (struct knetconfig *) kmem_alloc(sizeof (*knconf),
			KM_SLEEP);
	    if (copyin((caddr_t)data->knconf, (caddr_t)knconf,
			sizeof (*knconf))) {
		kmem_free((caddr_t)knconf, sizeof (*knconf));
		kmem_free((caddr_t)syncaddr->buf, syncaddr->len);
		kmem_free((caddr_t)data->netname, nlen);
		error = EFAULT;
	    } else {
		size_t nmoved_tmp;
		char *p, *pf;

		pf = (char *) kmem_alloc(KNC_STRSIZE, KM_SLEEP);
		p = (char *) kmem_alloc(KNC_STRSIZE, KM_SLEEP);
		error = copyinstr((caddr_t)knconf->knc_protofmly, pf,
			KNC_STRSIZE, &nmoved_tmp);
		if (!error) {
			error = copyinstr((caddr_t)knconf->knc_proto,
				p, KNC_STRSIZE, &nmoved_tmp);
			if (!error) {
			    knconf->knc_protofmly = pf;
			    knconf->knc_proto = p;
			} else {
			    kmem_free((caddr_t)pf, KNC_STRSIZE);
			    kmem_free((caddr_t)p, KNC_STRSIZE);
			    kmem_free((caddr_t)knconf, sizeof (*knconf));
			    kmem_free((caddr_t)syncaddr->buf, syncaddr->len);
			    kmem_free((caddr_t)data->netname, nlen);
			}
		} else {
			kmem_free((caddr_t)pf, KNC_STRSIZE);
			kmem_free((caddr_t)p, KNC_STRSIZE);
			kmem_free((caddr_t)knconf, sizeof (*knconf));
			kmem_free((caddr_t)syncaddr->buf, syncaddr->len);
			kmem_free((caddr_t)data->netname, nlen);
		}
	    }
	}

	if (!error) {
		data->knconf = knconf;
		*kdata = (caddr_t) data;
		return (0);
	} else {
		*kdata = NULL;
		kmem_free((caddr_t) data, sizeof (*data));
		return (error);
	}
}

/*
 *  Free up AUTH_DES or AUTH_KERB specific data.
 */
void
dh_k4_clnt_freeinfo(caddr_t cdata)
{
	dh_k4_clntdata_t *data;

	data = (dh_k4_clntdata_t *)cdata;
	if (data->netnamelen > 0) {
		kmem_free((caddr_t) data->netname, (u_int)data->netnamelen);
	}
	if (data->syncaddr.buf != NULL) {
		kmem_free((caddr_t) data->syncaddr.buf, data->syncaddr.len);
	}
	if (data->knconf != NULL) {
		kmem_free((caddr_t)data->knconf->knc_protofmly, KNC_STRSIZE);
		kmem_free((caddr_t)data->knconf->knc_proto, KNC_STRSIZE);
		kmem_free((caddr_t)data->knconf, sizeof (*data->knconf));
	}

	kmem_free((caddr_t) data, sizeof (*data));
}

/*
 *  Load application auth related data from user land to kernel.
 *  Map opaque data field to dh_k4_clntdata_t for AUTH_DES and AUTH_KERB.
 *
 */
long
sec_clnt_loadinfo(struct sec_data *in, struct sec_data **out)
{
	struct	sec_data	*secdata;
	int	error = 0;

	secdata = (struct sec_data *) kmem_alloc(sizeof (*secdata),
			KM_SLEEP);
	if (copyin((caddr_t) in, (caddr_t) secdata, sizeof (*secdata))
			== -1) {
		error = EFAULT;
	} else {
	/*
	 * Copy in opaque data field per flavor.
	 */
	    if (!error) {
		switch (secdata->rpcflavor) {

		    case AUTH_NONE:
		    case AUTH_UNIX:
			break;

		    case AUTH_DES:
		    case AUTH_KERB:
			error = dh_k4_clnt_loadinfo(secdata->data,
					&secdata->data);
			break;

		    case RPCSEC_GSS:
			error = gss_clnt_loadinfo(secdata->data,
					&secdata->data);
			break;

		    default:
			error = EINVAL;
			break;
		}
	    }
	}
	if (!error) {
		*out = secdata;
	} else {
		kmem_free((caddr_t) secdata, sizeof (*secdata));
		*out = (struct sec_data *)NULL;
	}

	return (error);
}

/*
 *  Null the sec_data index in the cache table, and
 *  free the memory allocated by sec_clnt_loadinfo.
 */
void
sec_clnt_freeinfo(struct sec_data *secdata)
{
	switch (secdata->rpcflavor) {
	    case AUTH_DES:
	    case AUTH_KERB:
		purge_authtab(secdata);
		if (secdata->data)
			dh_k4_clnt_freeinfo(secdata->data);
		break;

	    case RPCSEC_GSS:
		rpc_gss_secpurge((void *) secdata);
		if (secdata->data) {
		    gss_clntdata_t *gss_data;

		    gss_data = (gss_clntdata_t *) secdata->data;
		    if (gss_data->mechanism.elements) {
			kmem_free((caddr_t) gss_data->mechanism.elements,
				gss_data->mechanism.length);
		    }
		    kmem_free((caddr_t) secdata->data,
			sizeof (gss_clntdata_t));
		}
		break;

	    case AUTH_NONE:
	    case AUTH_UNIX:
	    default:
		break;
	}
	kmem_free((caddr_t) secdata, sizeof (*secdata));
}

/*
 *  Get an AUTH handle for a RPC client based on the given sec_data.
 *  If an AUTH handle exists for the same sec_data, use that AUTH handle,
 *  otherwise create a new one.
 */
long
sec_clnt_geth(CLIENT *client, struct sec_data *secdata,
		cred_t *cr, AUTH **ap)
{
	int i;
	register struct unixauthent *ua;
	register struct desauthent *da;
	int authflavor;
	cred_t *savecred;
	int stat;			/* return (errno) status */
	register char *p;
	register struct kerbauthent *ka;
	char kname[ANAME_SZ + INST_SZ + 1];
	char gss_svc_name[MAX_GSS_NAME];
	int kstat;
	dh_k4_clntdata_t	*desdata, *kdata;
	AUTH *auth;
	gss_clntdata_t *gssdata;


	if ((client == NULL) || (secdata == NULL) || (ap == NULL))
		return (EINVAL);
	*ap = (AUTH *)NULL;

	authflavor = secdata->rpcflavor;
	for (;;) {
		switch (authflavor) {
		case AUTH_NONE:
			/*
			 * XXX: should do real AUTH_NONE, instead of AUTH_UNIX
			 */
		case AUTH_UNIX:
			mutex_enter(&unixauthtab_lock);
			if ((ua = unixauthtab) != NULL)
				unixauthtab = ua->ua_next;
			mutex_exit(&unixauthtab_lock);

			if (ua == NULL) {
				/* overflow of unix auths */
				*ap = authkern_create();
				return ((*ap != NULL) ? 0 : EINTR);
			}

			if (ua->ua_auth == NULL)
				ua->ua_auth = authkern_create();
			*ap = ua->ua_auth;
			return ((*ap != NULL) ? 0 : EINTR);

		case AUTH_DES:
			mutex_enter(&desauthtab_lock);
			for (da = desauthtab;
			    da < &desauthtab[MAXCLIENTS];
			    da++) {
				if (da->da_data == secdata &&
				    da->da_uid == cr->cr_uid &&
				    !da->da_inuse &&
				    da->da_auth != NULL) {
					da->da_inuse = 1;
					mutex_exit(&desauthtab_lock);
					*ap = da->da_auth;
					return (0);
				}
			}
			mutex_exit(&desauthtab_lock);

			/*
			 *  A better way would be to have a cred paramater to
			 *  authdes_create.
			 */
			savecred = curthread->t_cred;
			curthread->t_cred = cr;
			desdata = (dh_k4_clntdata_t *)secdata->data;
			stat = authdes_create(desdata->netname, authdes_win,
				&desdata->syncaddr, desdata->knconf,
				(des_block *)NULL,
				(secdata->flags & AUTH_F_RPCTIMESYNC) ? 1 : 0,
				&auth);
			curthread->t_cred = savecred;
			*ap = auth;

			if (stat != 0) {
				cmn_err(CE_WARN,
				"sec_clnt_geth: authdes_create failed\n");

				/*
				 *  If AUTH_F_TRYNONE is on, try again
				 *  with AUTH_NONE.  See bug 1180236.
				 */
				if (secdata->flags & AUTH_F_TRYNONE) {
					authflavor = AUTH_NONE;
					continue;
				} else
					return ((long)stat);
			}

			i = MAXCLIENTS;
			mutex_enter(&desauthtab_lock);
			do {
				da = &desauthtab[nextdesvictim++];
				nextdesvictim %= MAXCLIENTS;
			} while (da->da_inuse && --i > 0);

			if (da->da_inuse) {
				mutex_exit(&desauthtab_lock);
				/* overflow of des auths */
				return ((long)stat);
			}
			da->da_inuse = 1;
			mutex_exit(&desauthtab_lock);

			if (da->da_auth != NULL)
				auth_destroy(da->da_auth);

			da->da_auth = auth;
			da->da_uid = cr->cr_uid;
			da->da_data = secdata;
			return ((long)stat);

		case AUTH_KERB:
			mutex_enter(&kerbauthtab_lock);
			for (ka = kerbauthtab;
			    ka < &kerbauthtab[MAXCLIENTS];
			    ka++) {
				if (ka->ka_data == secdata &&
				    ka->ka_uid == cr->cr_uid &&
				    !ka->ka_inuse &&
				    ka->ka_auth != NULL) {
					ka->ka_inuse = 1;
					mutex_exit(&kerbauthtab_lock);
					*ap = ka->ka_auth;
					return (0);
				}
			}
			mutex_exit(&kerbauthtab_lock);

			kdata = (dh_k4_clntdata_t *) secdata->data;
			/* separate principal name and instance */
			(void) strncpy(kname, kdata->netname,
					ANAME_SZ + INST_SZ);
			kname[ANAME_SZ + INST_SZ] = '\0';
			for (p = kname; *p && *p != '.'; p++)
				;
			if (*p)
				*p++ = '\0';

			/*
			 *  A better way would be to have a cred paramater to
			 *  authkerb_create.
			 */
			savecred = curthread->t_cred;
			curthread->t_cred = cr;
			stat = authkerb_create(kname, p, NULL, authkerb_win,
				&kdata->syncaddr, &kstat, kdata->knconf,
				(secdata->flags & AUTH_F_RPCTIMESYNC) ? 1 : 0,
				&auth);
			curthread->t_cred = savecred;
			*ap = auth;

			if (stat != 0) {
				kerb_create_failure(kstat);
				/*
				 *  If AUTH_F_TRYNONE is on, try again
				 *  with AUTH_NONE.  See bug 1180236.
				 */
				if (secdata->flags & AUTH_F_TRYNONE) {
					authflavor = AUTH_NONE;
					continue;
				} else
					return ((long)stat);
			}

			i = MAXCLIENTS;
			mutex_enter(&kerbauthtab_lock);
			do {
				ka = &kerbauthtab[nextkerbvictim++];
				nextkerbvictim %= MAXCLIENTS;
			} while (ka->ka_inuse && --i > 0);

			if (ka->ka_inuse) {
				mutex_exit(&kerbauthtab_lock);
				/* overflow of kerb auths */
				return ((long)stat);
			}
			ka->ka_inuse = 1;
			mutex_exit(&kerbauthtab_lock);

			if (ka->ka_auth != NULL)
				auth_destroy(ka->ka_auth);

			ka->ka_auth = auth;
			ka->ka_uid = cr->cr_uid;
			ka->ka_data = secdata;
			return ((long)stat);

		case RPCSEC_GSS:
			/*
			 *  For RPCSEC_GSS, cache is done in rpc_gss_secget().
			 *  For every rpc_gss_secget(),  it should have
			 *  a corresponding rpc_gss_secfree() call.
			 */
			gssdata = (gss_clntdata_t *)secdata->data;
			sprintf(gss_svc_name, "%s@%s", gssdata->uname,
					gssdata->inst);

			*ap = rpc_gss_secget(client, gss_svc_name,
					&gssdata->mechanism,
					gssdata->service,
					gssdata->qop,
					NULL, NULL,
					(caddr_t) secdata, cr);

			if (*ap == NULL) {
				cmn_err(CE_WARN,
				"sec_clnt_geth: rpc_gss_secget failed\n");

				/*
				 *  If AUTH_F_TRYNONE is on, try again
				 *  with AUTH_NONE.  See bug 1180236.
				 */
				if (secdata->flags & AUTH_F_TRYNONE) {
					authflavor = AUTH_NONE;
					continue;
				} else
					return (EINVAL);
			} else
				return (0);

		default:
			/*
			 * auth create must have failed, try AUTH_NONE
			 * (this relies on AUTH_NONE never failing)
			 */
			cmn_err(CE_WARN,
		"sec_clnt_geth: unknown authflavor %d\n", authflavor);
			authflavor = AUTH_NONE;
		}
	}
}

/*
 * Print an error message about authkerb_create's failing.
 */
static void
kerb_create_failure(int kstat)
{
	char *kerbmsg;
	extern char *krb_err_txt[KRB_ERRORS_TABLE_SIZE];

	/*
	 * A kstat of 0 isn't an error, so don't assume it has a useful
	 * error message.
	 */
	if (kstat < 1 || kstat > MAX_KRB_ERRORS) {
		kerbmsg = NULL;
	} else {
		kerbmsg = krb_err_txt[kstat];
	}

	if (kerbmsg != NULL) {
		cmn_err(CE_WARN,
		    "sec_clnt_geth: authkerb_create failed: (%s)\n", kerbmsg);
	} else {
		cmn_err(CE_WARN,
	"sec_clnt_geth: authkerb_create failed: (kerberos error %d)\n", kstat);
	}
}

void
sec_clnt_freeh(AUTH *auth)
{
	register struct unixauthent *ua;
	register struct desauthent *da;
	register struct kerbauthent *ka;

	switch (auth->ah_cred.oa_flavor) {
	case AUTH_NONE: /* XXX: do real AUTH_NONE */
	case AUTH_UNIX:
		for (ua = unixauthlist; ua != NULL; ua = ua->ua_next) {
			if (ua->ua_auth == auth) {
				mutex_enter(&unixauthtab_lock);
				ua->ua_next = unixauthtab;
				unixauthtab = ua;
				mutex_exit(&unixauthtab_lock);
				return;
			}
		}
		auth_destroy(auth);	/* was overflow */
		break;

	case AUTH_DES:
		mutex_enter(&desauthtab_lock);
		for (da = desauthtab; da < &desauthtab[MAXCLIENTS]; da++) {
			if (da->da_auth == auth) {
				da->da_inuse = 0;
				mutex_exit(&desauthtab_lock);
				return;
			}
		}
		mutex_exit(&desauthtab_lock);
		auth_destroy(auth);	/* was overflow */
		break;

	case AUTH_KERB:
		mutex_enter(&kerbauthtab_lock);
		for (ka = kerbauthtab; ka < &kerbauthtab[MAXCLIENTS]; ka++) {
			if (ka->ka_auth == auth) {
				ka->ka_inuse = 0;
				mutex_exit(&kerbauthtab_lock);
				return;
			}
		}
		mutex_exit(&kerbauthtab_lock);
		auth_destroy(auth);	/* was overflow */
		break;

	case RPCSEC_GSS:
		(void) rpc_gss_secfree(auth);
		break;

	default:
		cmn_err(CE_WARN, "sec_clnt_freeh: unknown authflavor %d",
			auth->ah_cred.oa_flavor);
		break;
	}
}

/*
 *  Revoke the authentication key in the given AUTH handle by setting
 *  it to NULL.  If newkey is true, then generate a new key instead of
 *  nulling out the old one.  This is necessary for AUTH_DES because
 *  the new key will be used next time the user does a keylogin.  If
 *  the zero'd key is used as actual key, then it cannot be revoked
 *  again!  This is not a problem for AUTH_KERB because the key is
 *  embedded in the ticket, which is cached.
 */
void
revoke_key(register AUTH *auth, int newkey)
{
	if (auth) {
		if (newkey) {
			if (key_gendes(&auth->ah_key) != RPC_SUCCESS) {
				/* failed to get new key, munge the old one */
				auth->ah_key.key.high ^= auth->ah_key.key.low;
				auth->ah_key.key.low  += auth->ah_key.key.high;
			}
		} else {
			/* null out old key */
			auth->ah_key.key.high = 0;
			auth->ah_key.key.low  = 0;
		}
	}
}

/*
 *  Revoke all rpc credentials (of the selected auth type) for the given uid
 *  from the auth cache.  Must be root to do this if the requested uid is not
 *  the effective uid of the requestor.
 *
 *  Called from nfssys() for backward compatibility, and also
 *  called from krpc_sys().
 *
 *  AUTH_DES and AUTH_KERB does not refer to the "mechanism" information.
 *  RPCSEC_GSS requires the "mechanism" input.
 *  The input argument, mechanism, is a user-space address and needs
 *  to be copied into the kernel address space.
 *
 *  Returns error number.
 */
int
sec_clnt_revoke(int rpcflavor, uid_t uid, cred_t *cr, void *mechanism)
{
	register struct desauthent *da;
	register struct kerbauthent *ka;
	int error = 0;

	if (uid != cr->cr_uid && !suser(cr))
		return (EPERM);

	switch (rpcflavor) {
	    case AUTH_DES:
		mutex_enter(&desauthtab_lock);
		for (da = desauthtab; da < &desauthtab[MAXCLIENTS]; da++) {
			if (uid == da->da_uid)
				revoke_key(da->da_auth, 1);
		}
		mutex_exit(&desauthtab_lock);
		return (0);

	    case AUTH_KERB:
		mutex_enter(&kerbauthtab_lock);
		for (ka = kerbauthtab; ka < &kerbauthtab[MAXCLIENTS]; ka++) {
			if (uid == ka->ka_uid)
				revoke_key(ka->ka_auth, 0);
		}
		mutex_exit(&kerbauthtab_lock);
		return (0);

	    case RPCSEC_GSS: {
		rpc_gss_OID	mech;
		caddr_t		elements;

		if (!mechanism)
			return (EINVAL);

		/* copyin the gss mechanism type */
		mech = kmem_alloc(sizeof (rpc_gss_OID_desc), KM_SLEEP);
		if (copyin((caddr_t) mechanism, (caddr_t) mech,
				sizeof (rpc_gss_OID_desc))) {
			return (EFAULT);
		}
		elements = kmem_alloc(mech->length, KM_SLEEP);
		if (copyin((caddr_t) mech->elements, (caddr_t) elements,
				mech->length)) {
			return (EFAULT);
		}
		mech->elements = elements;

		error = (rpc_gss_revauth(cr->cr_uid, mech));

		kmem_free(elements, mech->length);
		kmem_free(mech, sizeof (rpc_gss_OID_desc));

		return (error);
	    }

	    default:
		/* not an auth type with cached creds */
		return (EINVAL);
	}
}

/*
 *  Since sec_data is the index for the client auth handles
 *  cache table,  whenever the sec_data is freed, the index needs
 *  to be nulled.
 */
void
purge_authtab(struct sec_data *secdata)
{
	register struct desauthent *da;
	register struct kerbauthent *ka;

	switch (secdata->rpcflavor) {

	case AUTH_DES:
		mutex_enter(&desauthtab_lock);
		for (da = desauthtab;
		    da < &desauthtab[MAXCLIENTS]; da++) {
			if (da->da_data == secdata) {
				da->da_data = NULL;
				da->da_inuse = 0;
			}
		}
		mutex_exit(&desauthtab_lock);
		return;

	case AUTH_KERB:
		mutex_enter(&kerbauthtab_lock);
		for (ka = kerbauthtab;
		    ka < &kerbauthtab[MAXCLIENTS]; ka++) {
			if (ka->ka_data == secdata) {
				ka->ka_data = NULL;
				ka->ka_inuse = 0;
			}
		}
		mutex_exit(&kerbauthtab_lock);
		return;

	case RPCSEC_GSS:
		rpc_gss_secpurge((void *) secdata);
		return;

	default:
		return;
	}
}

void
sec_subrinit()
{
	int i;
	struct unixauthent *ua;

	mutex_init(&unixauthtab_lock, "unixauthtab_lock", MUTEX_DEFAULT, NULL);
	for (i = 0; i < MAXCLIENTS; i++) {
		ua = (struct unixauthent *) kmem_alloc(sizeof (*ua), KM_SLEEP);
		ua->ua_auth = NULL;
		ua->ua_next = unixauthtab;
		unixauthtab = ua;
	}
	unixauthlist = unixauthtab;
	mutex_init(&desauthtab_lock, "desauthtab_lock", MUTEX_DEFAULT, NULL);
	mutex_init(&kerbauthtab_lock, "kerbauthtab_lock", MUTEX_DEFAULT, NULL);

	/* RPC stuff */
	mutex_init(&authdes_ops_lock, "auth_ops", MUTEX_DEFAULT, DEFAULT_WT);
	mutex_init(&authdes_lock, "authdes cache", MUTEX_DEFAULT, DEFAULT_WT);
	mutex_init(&authkerb_lock, "authkerb cache", MUTEX_DEFAULT, DEFAULT_WT);
	mutex_init(&svcauthdesstats_lock, "svcauthdes_stats", MUTEX_DEFAULT,
		DEFAULT_WT);
	mutex_init(&svcauthkerbstats_lock, "svcauthkerb_stats", MUTEX_DEFAULT,
		DEFAULT_WT);
}
