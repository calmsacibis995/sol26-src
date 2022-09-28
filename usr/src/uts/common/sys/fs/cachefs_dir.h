/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FS_CACHEFS_DIR_H
#define	_SYS_FS_CACHEFS_DIR_H

#pragma ident	"@(#)cachefs_dir.h	1.16	96/07/01 SMI"

#include <sys/types.h>
#include <sys/fs/cachefs_fs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct c_dirent {
	u_int		d_length;	/* entry length */
	u_int		d_flag;		/* entry flags */
	cfs_cid_t	d_id;		/* file id */
	offset_t	d_offset;	/* disk offset of this entry */
	struct fid 	d_cookie;	/* back fid */
	u_short		d_namelen;	/* name length, without null */
	char		d_name[1];	/* name */
};

#define	C_DIRSIZ(dp) \
	(((dp)->d_namelen + sizeof (struct c_dirent) + 7) & ~7)

#define	CDE_SIZE(NM) \
	((strlen(NM) + sizeof (struct c_dirent) + 7) & ~7)

/*
 * Various flags stored in c_dirent flag field.
 */
#define	CDE_VALID	0x1		/* entry is valid */
#define	CDE_COMPLETE	0x2		/* entry is complete */


#if defined(_KERNEL) && defined(__STDC__)
int cachefs_dir_look(cnode_t *dcp, char *nm, fid_t *cookiep, u_int *flagp,
    u_offset_t *d_offsetp, cfs_cid_t *cidp);
int cachefs_dir_new(cnode_t *dcp, cnode_t *cp);
int cachefs_dir_enter(cnode_t *dcp, char *nm, fid_t *cookiep, cfs_cid_t *cidp,
    int issync);
int cachefs_dir_rmentry(cnode_t *dcp, char *nm);
void cachefs_dir_modentry(cnode_t *dcp, u_offset_t offset, fid_t *cookiep,
    cfs_cid_t *cidp);
int cachefs_dir_read(struct cnode *dcp, struct uio *uiop, int *eofp);
int cachefs_dir_fill(cnode_t *dcp, cred_t *cr);
int cachefs_dir_empty(cnode_t *dcp);
int cachefs_async_populate_dir(struct cachefs_populate_req *, cred_t *,
    vnode_t *, vnode_t *);

#endif /* defined(_KERNEL) && defined(__STDC__) */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_FS_CACHEFS_DIR_H */
