/*
 *	nfs_tbind.h
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ifndef	_NFS_TBIND_H
#define	_NFS_TBIND_H

#pragma ident	"@(#)nfs_tbind.h	1.1	96/07/09 SMI"

#include <netconfig.h>
#include <netdir.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * nfs library routines
 */
extern int nfslib_transport_open(struct netconfig *);
extern int nfslib_bindit(struct netconfig *, struct netbuf **,
			struct nd_hostserv *, int);
extern void nfslib_log_tli_error(char *, int, struct netconfig *);

#ifdef __cplusplus
}
#endif

#endif	/* _NFS_TBIND_H */
