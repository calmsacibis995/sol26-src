/*
 * Copyright (c) 1995,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * nfs_sec.h, NFS specific security service information.
 */

#ifndef	_NFS_SEC_H
#define	_NFS_SEC_H

#pragma ident	"@(#)nfs_sec.h	1.10	96/09/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <rpc/rpcsec_gss.h>

#define	NFSSEC_CONF	"/etc/nfssec.conf"
#define	SC_FAILURE	-1

/*
 *  Errors for the nfssec_*
 */
#define	SC_NOERROR	0
#define	SC_NOMEM	1
#define	SC_OPENFAIL	2
#define	SC_NOTFOUND	3
#define	SC_BADENTRIES	4	/* Bad entries in nfssec.conf file */

typedef struct {
	char		sc_name[MAX_NAME_LEN];
	int		sc_nfsnum;
	int		sc_rpcnum;
	char		sc_gss_mech[MAX_NAME_LEN];
	rpc_gss_OID	sc_gss_mech_type;
	u_int		sc_qop;
	rpc_gss_service_t	sc_service;
} seconfig_t;


extern int nfs_getseconfig_default(seconfig_t *);
extern int nfs_getseconfig_byname(char *, seconfig_t *);
extern int nfs_getseconfig_bynumber(int, seconfig_t *);
extern int nfs_getseconfig_bydesc(char *, char *, rpc_gss_service_t,
				seconfig_t *);
extern sec_data_t *nfs_clnt_secdata(seconfig_t *, char *, struct knetconfig *,
				struct netbuf *, int);
extern void nfs_free_secdata(sec_data_t *);
extern void nfs_syslog_scerr(int);

#ifdef	__cplusplus
}
#endif

#endif	/* !_NFS_SEC_H */
