/*
 * Copyright (c) 1995,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * nfs security related library routines.
 *
 * Some of the routines in this file are adopted from
 * lib/libnsl/netselect/netselect.c and are modified to be
 * used for accessing /etc/nfssec.conf.
 */

#ident	"@(#)nfs_sec.c	1.23	96/10/23 SMI"	/* SVr4.0 1.18	*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <syslog.h>
#include <synch.h>
#include <rpc/rpc.h>
#include <nfs/nfs_sec.h>

#define	GETBYNAME	1
#define	GETBYNUM	2

/*
 * mapping for /etc/nfssec.conf
 */
struct sc_data {
	char	*string;
	int	value;
};

static struct sc_data sc_service[] = {
	"default",	rpc_gss_svc_default,
	"-",		rpc_gss_svc_none,
	"none",		rpc_gss_svc_none,
	"integrity",	rpc_gss_svc_integrity,
	"privacy",	rpc_gss_svc_privacy,
	NULL,		SC_FAILURE
};

static char *gettoken(char *, int);
extern	int atoi(const char *str);

extern	bool_t rpc_gss_get_principal_name(rpc_gss_principal_t *, char *,
			char *, char *, char *);

extern	bool_t rpc_gss_mech_to_oid(char *, rpc_gss_OID *);
extern	bool_t rpc_gss_qop_to_num(char *, char *, u_int *);

/*
 *  blank() returns true if the line is a blank line, 0 otherwise
 */
static int
blank(cp)
char *cp;
{
	while (*cp && isspace(*cp)) {
		cp++;
	}
	return (*cp == '\0');
}

/*
 *  comment() returns true if the line is a comment, 0 otherwise.
 */
static int
comment(cp)
char *cp;
{
	while (*cp && isspace(*cp)) {
		cp++;
	}
	return (*cp == '#');
}


/*
 *	getvalue() searches for the given string in the given array,
 *	and returns the integer value associated with the string.
 */
static unsigned long
getvalue(cp, sc_data)
char *cp;
struct sc_data sc_data[];
{
	int i;	/* used to index through the given struct sc_data array */

	for (i = 0; sc_data[i].string; i++) {
		if (strcmp(sc_data[i].string, cp) == 0) {
			break;
		}
	}
	return (sc_data[i].value);
}

/*
 *	shift1left() moves all characters in the string over 1 to
 *	the left.
 */
static void
shift1left(p)
char *p;
{
	for (; *p; p++)
		*p = *(p + 1);
}


/*
 *	gettoken() behaves much like strtok(), except that
 *	it knows about escaped space characters (i.e., space characters
 *	preceeded by a '\' are taken literally).
 *
 *	XXX We should make this MT-hot by making it more like strtok_r().
 */
static char *
gettoken(cp, skip)
char	*cp;
int skip;
{
	static char	*savep;	/* the place where we left off    */
	register char	*p;	/* the beginning of the new token */
	register char	*retp;	/* the token to be returned	  */


	/* Determine if first or subsequent call  */
	p = (cp == NULL)? savep: cp;

	/* Return if no tokens remain.  */
	if (p == 0) {
		return (NULL);
	}

	while (isspace(*p))
		p++;

	if (*p == '\0') {
		return (NULL);
	}

	/*
	 *	Save the location of the token and then skip past it
	 */

	retp = p;
	while (*p) {
		if (isspace(*p))
			if (skip == TRUE) {
				shift1left(p);
				continue;
			} else
				break;
		/*
		 *	Only process the escape of the space separator;
		 *	since the token may contain other separators,
		 *	let the other routines handle the escape of
		 *	specific characters in the token.
		 */

		if (*p == '\\' && *(p + 1) != '\n' && isspace(*(p + 1))) {
			shift1left(p);
		}
		p++;
	}
	if (*p == '\0') {
		savep = 0;	/* indicate this is last token */
	} else {
		*p = '\0';
		savep = ++p;
	}
	return (retp);
}

/*
 *  matchname() parses a line of the /etc/nfssec.conf file
 *  and match the sc_name with the given name.
 *  If there is a match, it fills the information into the given
 *  pointer of the seconfig_t structure.
 *
 *  Returns TRUE if a match is found.
 */
static bool_t
matchname(char *line, char *name, seconfig_t *secp)
{
	char	*tok1,	*tok2;	/* holds a token from the line */
	char	*secname, *gss_mech, *gss_qop; /* pointer to a secmode name */

	if ((secname = gettoken(line, FALSE)) == NULL) {
		/* bad line */
		return (FALSE);
	}

	if (strcmp(secname, name) != 0) {
		return (FALSE);
	}

	tok1 = tok2 = NULL;
	if (((tok1 = gettoken(NULL, FALSE)) == NULL) ||
		((gss_mech = gettoken(NULL, FALSE)) == NULL) ||
		((gss_qop = gettoken(NULL, FALSE)) == NULL) ||
		((tok2 = gettoken(NULL, FALSE)) == NULL) ||
		((secp->sc_service = getvalue(tok2, sc_service))
			== SC_FAILURE)) {
		return (FALSE);
	}
	secp->sc_nfsnum = atoi(tok1);
	strcpy(secp->sc_name, secname);
	strcpy(secp->sc_gss_mech, gss_mech);
	secp->sc_gss_mech_type = NULL;
	if (secp->sc_gss_mech[0] != '-') {
	    if (!rpc_gss_mech_to_oid(gss_mech, &secp->sc_gss_mech_type) ||
		!rpc_gss_qop_to_num(gss_qop, gss_mech, &secp->sc_qop)) {
		return (FALSE);
	    }
	}

	return (TRUE);
}

/*
 *  matchnum() parses a line of the /etc/nfssec.conf file
 *  and match the sc_nfsnum with the given number.
 *  If it is a match, it fills the information in the given pointer
 *  of the seconfig_t structure.
 *
 *  Returns TRUE if a match is found.
 */
static bool_t
matchnum(char *line, int num, seconfig_t *secp)
{
	char	*tok1,	*tok2;	/* holds a token from the line */
	char	*secname, *gss_mech, *gss_qop;	/* pointer to a secmode name */

	if ((secname = gettoken(line, FALSE)) == NULL) {
		/* bad line */
		return (FALSE);
	}

	tok1 = tok2 = NULL;
	if ((tok1 = gettoken(NULL, FALSE)) == NULL) {
		/* bad line */
		return (FALSE);
	}

	if ((secp->sc_nfsnum = atoi(tok1)) != num) {
		return (FALSE);
	}

	if (((gss_mech = gettoken(NULL, FALSE)) == NULL) ||
		((gss_qop = gettoken(NULL, FALSE)) == NULL) ||
		((tok2 = gettoken(NULL, FALSE)) == NULL) ||
		((secp->sc_service = getvalue(tok2, sc_service))
			== SC_FAILURE)) {
		return (FALSE);
	}

	strcpy(secp->sc_name, secname);
	strcpy(secp->sc_gss_mech, gss_mech);
	if (secp->sc_gss_mech[0] != '-') {
	    if (!rpc_gss_mech_to_oid(gss_mech, &secp->sc_gss_mech_type) ||
		!rpc_gss_qop_to_num(gss_qop, gss_mech, &secp->sc_qop)) {
		return (FALSE);
	    }
	}

	return (TRUE);
}

/*
 *  Fill in the RPC Protocol security flavor number
 *  into the sc_rpcnum of seconfig_t structure.
 *
 *  Mainly to map NFS secmod number to RPCSEC_GSS if
 *  a mechanism name is specified.
 */
static void
get_rpcnum(seconfig_t *secp)
{
	if (secp->sc_gss_mech[0] != '-') {
		secp->sc_rpcnum = RPCSEC_GSS;
	} else {
		secp->sc_rpcnum = secp->sc_nfsnum;
	}
}

/*
 *  Parse a given hostname (nodename[.domain@realm]) to
 *  instant name (nodename[.domain]) and realm.
 *
 *  Assuming user has allocated the space for inst and realm.
 */
static int
parsehostname(char *hostname, char *inst, char *realm)
{
	char *h, *r;

	if (!hostname)
		return (0);

	h = (char *) strdup(hostname);
	if (!h) {
		syslog(LOG_ERR, "parsehostname: no memory\n");
		return (0);
	}

	r = (char *) strchr(h, '@');
	if (!r) {
		strcpy(inst, h);
		strcpy(realm, "");
	} else {
		*r++ = '\0';
		strcpy(inst, h);
		strcpy(realm, r);
	}
	free(h);
	return (1);
}

/*
 *  Get seconfig from /etc/nfssec.conf by name or by number or
 *  by descriptior.
 */
static int
get_seconfig(int whichway, char *name, int num,
		rpc_gss_service_t service, seconfig_t *entryp)
{
	static	mutex_t matching_lock = DEFAULTMUTEX;
	char	line[BUFSIZ];	/* holds each line of NFSSEC_CONF */
	FILE	*fp;		/* file stream for NFSSEC_CONF */

	if ((whichway == GETBYNAME) && (name == NULL))
		return (SC_NOTFOUND);

	if ((fp = fopen(NFSSEC_CONF, "r")) == NULL) {
		return (SC_OPENFAIL);
	}

	mutex_lock(&matching_lock);
	while (fgets(line, BUFSIZ, fp)) {
	    if (!(blank(line) || comment(line))) {
		switch (whichway) {
		    case GETBYNAME:
			if (matchname(line, name, entryp)) {
				goto found;
			}
			break;

		    case GETBYNUM:
			if (matchnum(line, num, entryp)) {
				goto found;
			}
			break;

		    default:
			break;
		}
	    }
	}
	mutex_unlock(&matching_lock);
	(void) fclose(fp);
	return (SC_NOTFOUND);

found:
	mutex_unlock(&matching_lock);
	(void) fclose(fp);
	(void) get_rpcnum(entryp);
	return (SC_NOERROR);
}


/*
 *  NFS project private API.
 *  Get a seconfig entry from /etc/nfssec.conf by nfs specific sec name,
 *  e.g. des, kerberos, krb5p, etc.
 */
int
nfs_getseconfig_byname(char *secmode_name, seconfig_t *entryp)
{
	if (!entryp)
		return (SC_NOMEM);

	return (get_seconfig(GETBYNAME, secmode_name, 0, rpc_gss_svc_none,
			entryp));
}

/*
 *  NFS project private API.
 *
 *  Get a seconfig entry from /etc/nfssec.conf by nfs specific sec number,
 *  e.g. AUTH_DES, AUTH_KERB, AUTH_KRB5_P, etc.
 */
int
nfs_getseconfig_bynumber(int nfs_secnum, seconfig_t *entryp)
{
	if (!entryp)
		return (SC_NOMEM);

	return (get_seconfig(GETBYNUM, NULL, nfs_secnum, rpc_gss_svc_none,
				entryp));
}

/*
 *  NFS project private API.
 *
 *  Get a seconfig_t entry used as the default for NFS operations.
 *  The default flavor entry is defined in /etc/nfssec.conf.
 *
 *  Assume user has allocate spaces for secp.
 */
int
nfs_getseconfig_default(seconfig_t *secp)
{
	if (secp == NULL)
		return (SC_NOMEM);

	return (nfs_getseconfig_byname("default", secp));
}


/*
 *  NFS project private API.
 *
 *  Free an sec_data structure.
 *  Free the parts that nfs_clnt_secdata allocates.
 */
void
nfs_free_secdata(sec_data_t *secdata)
{
	dh_k4_clntdata_t *dkdata;
	gss_clntdata_t *gdata;

	if (!secdata)
		return;

	switch (secdata->rpcflavor) {
	    case AUTH_UNIX:
		break;

	    case AUTH_DES:
	    case AUTH_KERB:
		/* LINTED pointer alignment */
		dkdata = (dh_k4_clntdata_t *) secdata->data;
		if (dkdata) {
			if (dkdata->netname)
				free(dkdata->netname);
			if (dkdata->syncaddr.buf)
				free(dkdata->syncaddr.buf);
			free(dkdata);
		}
		break;

	    case RPCSEC_GSS:
		/* LINTED pointer alignment */
		gdata = (gss_clntdata_t *) secdata->data;
		if (gdata) {
			if (gdata->mechanism.elements)
				free(gdata->mechanism.elements);
			free(gdata);
		}
		break;

	    default:
		break;
	}

	free(secdata);
}

/*
 *  Make an client side sec_data structure and fill in appropriate value
 *  based on its rpc security flavor.
 *
 *  It is caller's responsibility to allocate space for seconfig_t,
 *  and this routine will allocate space for the sec_data structure
 *  and related data field.
 *
 *  Return the sec_data_t on success.
 *  If fail, return NULL pointer.
 */
sec_data_t *
nfs_clnt_secdata(seconfig_t *secp, char *hostname, struct knetconfig *knconf,
		struct netbuf *syncaddr, int flags)
{
	char netname[MAXNETNAMELEN+1];
	sec_data_t *secdata;
	dh_k4_clntdata_t *dkdata;
	gss_clntdata_t *gdata;

	secdata = (sec_data_t *) malloc(sizeof (sec_data_t));
	if (!secdata) {
		syslog(LOG_ERR, "nfs_clnt_secdata: no memory\n");
		return (NULL);
	}
	memset(secdata, 0, sizeof (sec_data_t));

	secdata->secmod = secp->sc_nfsnum;
	secdata->rpcflavor = secp->sc_rpcnum;
	secdata->flags = flags;

	/*
	 *  Now, fill in the information for client side secdata :
	 *
	 *  For AUTH_UNIX, AUTH_DES
	 *  hostname can be in the form of
	 *    nodename or
	 *    nodename.domain
	 *
	 *  For AUTH_KERB, RPCSEC_GSS security flavor
	 *  hostname can be in the form of
	 *    nodename or
	 *    nodename.domain  or
	 *    nodename@realm (realm can be the same as the domain) or
	 *    nodename.domain@realm
	 */
	switch (secp->sc_rpcnum) {
	    case AUTH_UNIX:
		secdata->data = NULL;
		break;

	    case AUTH_DES:
	    case AUTH_KERB:
		if (secp->sc_rpcnum == AUTH_DES) {
		/*
		 *  If hostname is in the format of host.nisdomain
		 *  the netname will be constructed with
		 *  this nisdomain name rather than the default
		 *  domain of the machine.
		 */
		    if (!host2netname(netname, hostname, NULL)) {
			syslog(LOG_ERR, "host2netname: %s: unknown\n",
				hostname);
			goto err_out;
		    }
		} else {
			sprintf(netname, "nfs.%s", hostname);
		}
		dkdata = (dh_k4_clntdata_t *) malloc(sizeof (dh_k4_clntdata_t));
		if (!dkdata) {
		    syslog(LOG_ERR, "nfs_clnt_secdata: no memory\n");
		    goto err_out;
		}
		memset((char *)dkdata, 0, sizeof (dh_k4_clntdata_t));
		if ((dkdata->netname = strdup(netname)) == NULL) {
		    syslog(LOG_ERR, "nfs_clnt_secdata: no memory\n");
		    goto err_out;
		}
		dkdata->netnamelen = strlen(netname);
		dkdata->knconf = knconf;
		dkdata->syncaddr = *syncaddr;
		dkdata->syncaddr.buf = (char *) malloc(syncaddr->len);
		if (dkdata->syncaddr.buf == NULL) {
		    syslog(LOG_ERR, "nfs_clnt_secdata: no memory\n");
		    goto err_out;
		}
		(void) memcpy(dkdata->syncaddr.buf, syncaddr->buf,
						syncaddr->len);
		secdata->data = (caddr_t)dkdata;
		break;

	    case RPCSEC_GSS: {
		rpc_gss_OID gss_mech;

		if (secp->sc_gss_mech_type == NULL) {
		    syslog(LOG_ERR,
			"nfs_clnt_secdata: need mechanism information\n");
		    goto err_out;
		}

		gdata = (gss_clntdata_t *) malloc(sizeof (gss_clntdata_t));
		if (!gdata) {
		    syslog(LOG_ERR, "nfs_clnt_secdata: no memory\n");
		    goto err_out;
		}

		strcpy(gdata->uname, "nfs");
		if (!parsehostname(hostname, gdata->inst, gdata->realm)) {
		    syslog(LOG_ERR, "nfs_clnt_secdata: bad host name\n");
		    goto err_out;
		}

		gdata->mechanism.length = secp->sc_gss_mech_type->length;
		if (!(gdata->mechanism.elements =
			malloc(sizeof (secp->sc_gss_mech_type->length)))) {
			syslog(LOG_ERR, "nfs_clnt_secdata: no memory\n");
			goto err_out;
		}
		memcpy(gdata->mechanism.elements,
			secp->sc_gss_mech_type->elements,
			secp->sc_gss_mech_type->length);

		gdata->qop = secp->sc_qop;
		gdata->service = secp->sc_service;
		secdata->data = (caddr_t)gdata;
	    }
	    break;

	    default:
		syslog(LOG_ERR, "nfs_clnt_secdata: unknown flavor\n");
		goto err_out;
	}

	return (secdata);

err_out:
	free(secdata);
	return (NULL);
}

/*
 *  nfs_get_root_principal() maps a host name to its principal name
 *  based on the given security information.
 *
 *  input :  seconfig - security configuration information
 *	     host - the host name which could be in the following forms:
 *		node
 *		node.namedomain
 *		node@secdomain (e.g. kerberos realm is a secdomain)
 *		node.namedomain@secdomain
 *  output : rootname_p - address of the principal name for the host
 *
 *  Currently, this routine is only used by share program.
 *
 */
bool_t
nfs_get_root_principal(seconfig_t *seconfig, char *host, caddr_t *rootname_p)
{
	char netname[MAXNETNAMELEN+1], node[INST_SZ], secdomain[REALM_SZ];
	rpc_gss_principal_t gssname;

	switch (seconfig->sc_rpcnum) {
		case AUTH_DES:
		    if (!host2netname(netname, host, NULL)) {
			syslog(LOG_ERR,
			    "nfs_get_root_principal: unknown host: %s\n", host);
			return (FALSE);
		    }
		    *rootname_p = strdup(netname);
		    if (!*rootname_p) {
			syslog(LOG_ERR, "nfs_get_root_principal: no memory\n");
			return (FALSE);
		    }
		    break;

		case AUTH_KERB:
		    *rootname_p = strdup(host);
		    if (!*rootname_p) {
			syslog(LOG_ERR, "nfs_get_root_principal: no memory\n");
			return (FALSE);
		    }
		    break;

		case RPCSEC_GSS:

		    if (!parsehostname(host, node, secdomain)) {
			syslog(LOG_ERR,
			    "nfs_get_root_principal: bad host name\n");
			return (FALSE);
		    }
		    if (!rpc_gss_get_principal_name(&gssname,
				seconfig->sc_gss_mech, "root",
				node, secdomain)) {
			syslog(LOG_ERR,
	"nfs_get_root_principal: can not get principal name : %s\n", host);
			return (FALSE);
		    }

		    *rootname_p = (caddr_t) gssname;
		    break;

		default:
		    return (FALSE);
	}
	return (TRUE);
}


/*
 *  SYSLOG SC_* errors.
 */
void
nfs_syslog_scerr(int scerror)
{
	switch (scerror) {
	    case SC_NOMEM :
			syslog(LOG_ERR, "%s : no memory\n", NFSSEC_CONF);
			break;

	    case SC_OPENFAIL :
			syslog(LOG_ERR, "can not open %s\n", NFSSEC_CONF);
			break;

	    case SC_NOTFOUND :
			syslog(LOG_ERR, "no entry in %s\n", NFSSEC_CONF);
			break;

	    case SC_BADENTRIES :
			syslog(LOG_ERR, "bad entry in %s\n", NFSSEC_CONF);
			break;
	    default:
		break;
	}
}
