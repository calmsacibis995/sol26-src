/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	(c) 1986-1991,1994-1996  Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef	_NFS_EXPORT_H
#define	_NFS_EXPORT_H

#pragma ident	"@(#)export.h	1.35	96/10/16 SMI"
/*	export.h 1.7 88/08/19 SMI */

#include <nfs/nfs_sec.h>
#include <rpcsvc/nfsauth_prot.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct secinfo {
	seconfig_t	s_secinfo;	/* /etc/nfssec.conf entry */
	unsigned	s_flags;	/* flags (see below) */
	int 		s_window;	/* window */
	int		s_rootcnt;	/* count of root names */
	caddr_t		*s_rootnames;	/* array of root names */
					/* they are strings for AUTH_DES and */
					/* rpc_gss_principal_t for RPCSEC_GSS */
};

/*
 * Per-mode flags
 */

#define	M_RO	0x01		/* exported ro to all */
#define	M_ROL	0x02		/* exported ro to all listed */
#define	M_RW	0x04		/* exported rw to all */
#define	M_RWL	0x08		/* exported ro to all listed */
#define	M_ROOT	0x10		/* root list is defined */

/*
 * The export information passed to exportfs() (Version 1)
 */
struct export {
	int		ex_version;	/* structure version */
	char		*ex_path;	/* exported path */
	int		ex_pathlen;	/* path length */
	int		ex_flags;	/* flags */
	unsigned	ex_anon;	/* uid for unauthenticated requests */
	int		ex_seccnt;	/* count of security modes */
	struct secinfo	*ex_secinfo;	/* security mode info */
	char *		ex_index;	/* index file for public filesystem */
};

/*
 * exported vfs flags.
 */

#define	EX_NOSUID	0x01	/* exported with unsetable set[ug]ids */
#define	EX_ACLOK	0x02	/* exported with maximal access if acl exists */
#define	EX_PUBLIC	0x04	/* exported with public filehandle */
#define	EX_NOSUB	0x08	/* no nfs_getfh or MCL below export point */
#define	EX_INDEX	0x10	/* exported with index file specified */

#ifdef	_KERNEL

/*
 * An authorization cache entry
 */
struct auth_cache {
	struct netbuf		auth_addr;
	int			auth_flavor;
	int			auth_access;
	long			auth_time;
	struct auth_cache	*auth_next;
};

#define	AUTH_TABLESIZE	32

#define	EXPTABLESIZE	16

/*
 * A node associated with an export entry on the
 * list of exported filesystems.
 *
 * The exportinfo structure is protected by the exi_lock.
 * You must have the writer lock to delete an exportinfo
 * structure from the list.
 */

struct exportinfo {
	struct export		exi_export;
	fsid_t			exi_fsid;
	struct fid		exi_fid;
	struct exportinfo	*exi_hash;
	fhandle_t		exi_fh;
	krwlock_t		exi_cache_lock;
	struct auth_cache	*exi_cache[AUTH_TABLESIZE];
};

#define	EQFSID(fsidp1, fsidp2)	\
	(((fsidp1)->val[0] == (fsidp2)->val[0]) && \
	    ((fsidp1)->val[1] == (fsidp2)->val[1]))

#define	EQFID(fidp1, fidp2)	\
	((fidp1)->fid_len == (fidp2)->fid_len && \
	    nfs_fhbcmp((char *)(fidp1)->fid_data, (char *)(fidp2)->fid_data, \
	    (uint_t)(fidp1)->fid_len) == 0)

/*
 * Returns true iff exported filesystem is read-only to the given host.
 *
 * Note:  this macro should be as fast as possible since it's called
 * on each NFS modification request.
 */
#define	rdonly(exi, req)  (nfsauth_access(exi, req) & NFSAUTH_RO)

extern int	nfs_fhhash(fsid_t *, fid_t *);
extern int	nfs_fhbcmp(char *, char *, int);
extern int	nfs_exportinit(void);
extern int	makefh(fhandle_t *, struct vnode *, struct exportinfo *);
extern int	makefh3(nfs_fh3 *, struct vnode *, struct exportinfo *);
extern vnode_t *nfs_fhtovp(fhandle_t *, struct exportinfo *);
extern vnode_t *nfs3_fhtovp(nfs_fh3 *, struct exportinfo *);
extern struct	exportinfo *findexport(fsid_t *, struct fid *);
extern struct	exportinfo *checkexport(fsid_t *, struct fid *);
extern void	export_rw_exit(void);
extern struct exportinfo *nfs_vptoexi(vnode_t *, vnode_t *, cred_t *, int *);

/*
 * "public" and default (root) location for public filehandle
 */
extern struct exportinfo *exi_public, *exi_root;
extern fhandle_t nullfh2;	/* for comparing V2 filehandles */

/*
 * Two macros for identifying public filehandles.
 * A v2 public filehandle is 32 zero bytes.
 * A v3 public filehandle is zero length.
 */
#define	PUBLIC_FH2(fh, exi) \
	((fh)->fh_fsid.val[1] == 0 && (exi == exi_public) && \
	bcmp((caddr_t) (fh), (caddr_t) &nullfh2, \
	sizeof (fhandle_t)) == 0)

#define	PUBLIC_FH3(fh, exi) \
	((fh)->fh3_length == 0 && exi == exi_public)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_EXPORT_H */
