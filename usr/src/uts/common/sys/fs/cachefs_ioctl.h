/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FS_CACHEFS_IOCTL_H
#define	_SYS_FS_CACHEFS_IOCTL_H

#pragma ident	"@(#)cachefs_ioctl.h	1.18	96/02/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/* set of subcommands to CACHEFSIO_DCMD */
enum cfsdcmd_cmds {
	CFSDCMD_DAEMONID, CFSDCMD_STATEGET, CFSDCMD_STATESET,
	CFSDCMD_XWAIT, CFSDCMD_EXISTS, CFSDCMD_LOSTFOUND, CFSDCMD_GETINFO,
	CFSDCMD_CIDTOFID, CFSDCMD_GETATTRFID, CFSDCMD_GETATTRNAME,
	CFSDCMD_GETSTATS, CFSDCMD_ROOTFID,
	CFSDCMD_CREATE, CFSDCMD_REMOVE, CFSDCMD_LINK, CFSDCMD_RENAME,
	CFSDCMD_MKDIR, CFSDCMD_RMDIR, CFSDCMD_SYMLINK, CFSDCMD_SETATTR,
	CFSDCMD_SETSECATTR, CFSDCMD_PUSHBACK
};
typedef enum cfsdcmd_cmds cfsdcmd_cmds_t;

/* file system states passed to stateset, returned from stateget */
#define	CFS_FS_CONNECTED	0x00	/* fscache connected to backfs */
#define	CFS_FS_DISCONNECTED	0x01	/* fscache disconnected from backfs */
#define	CFS_FS_RECONNECTING	0x02	/* fscache is reconnecting to backfs */

/* bits returned by packinfo */
#define	CACHEFS_PACKED_FILE	1	/* file is marked as packed */
#define	CACHEFS_PACKED_DATA	2	/* file data is in the cache */
#define	CACHEFS_PACKED_NOCACHE	4	/* file marked as not for caching */

struct cachefsio_pack {
	char		p_name[MAXNAMELEN];	/* name of file */
	int		p_status;		/* status of operation */
};
typedef struct cachefsio_pack cachefsio_pack_t;

struct cachefsio_dcmd {
	cfsdcmd_cmds_t	 d_cmd;			/* cmd to execute */
	void		*d_sdata;		/* data for command */
	int		 d_slen;		/* len of data */
	void		*d_rdata;		/* data to return */
	int		 d_rlen;		/* len of data */
};
typedef struct cachefsio_dcmd cachefsio_dcmd_t;

struct cachefsio_getinfo {
	cfs_cid_t	gi_cid;			/* entry to lookup */
	int		gi_modified;		/* returns if modified data */
	vattr_t		gi_attr;		/* return file attributes */
	cfs_cid_t	gi_pcid;		/* returns the parent dir */
	u_long		gi_seq;			/* sequence number */
	char		gi_name[MAXNAMELEN];	/* returns name of file */
};
typedef struct cachefsio_getinfo cachefsio_getinfo_t;

struct cachefsio_lostfound_arg {
	cfs_cid_t	lf_cid;			/* file to move */
	char		lf_name[MAXNAMELEN];	/* suggested name */
};
typedef struct cachefsio_lostfound_arg cachefsio_lostfound_arg_t;

struct cachefsio_lostfound_return {
	char		lf_name[MAXNAMELEN];	/* returns actual name */
};
typedef struct cachefsio_lostfound_return cachefsio_lostfound_return_t;

struct cachefsio_getattrfid {
	fid_t	cg_backfid;	/* backfs fid of file */
	cred_t	cg_cred;	/* creds */
	gid_t	cg_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_getattrfid cachefsio_getattrfid_t;

struct cachefsio_getattrname_arg {
	fid_t	cg_dir;			/* backfs fid of directory */
	char	cg_name[MAXNAMELEN];	/* name of file in directory cg_dir */
	cred_t	cg_cred;		/* creds */
	gid_t	cg_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef	struct cachefsio_getattrname_arg cachefsio_getattrname_arg_t;

struct cachefsio_getattrname_return {
	vattr_t	cg_attr;		/* returns attributes of file */
	fid_t	cg_fid;			/* returns fid of file */
};
typedef	struct cachefsio_getattrname_return cachefsio_getattrname_return_t;

struct cachefsio_getstats {
	long		gs_total;		/* total blocks */
	long		gs_gc;			/* number of gc blocks */
	long		gs_active;		/* number of active blocks */
	long		gs_packed;		/* number of packed blocks */
	long		gs_free;		/* number of free blocks */
	time_t		gs_gctime;		/* atime of front of gc list */
};
typedef struct cachefsio_getstats cachefsio_getstats_t;

struct cachefsio_create_arg {
	fid_t		cr_backfid;		/* backfs fid of directory */
	char		cr_name[MAXNAMELEN];	/* name of file to create */
	cfs_cid_t	cr_cid;			/* cid of file being created */
	vattr_t		cr_va;			/* attributes for create */
	int		cr_exclusive;		/* exclusive create or not */
	int		cr_mode;		/* mode */
	cred_t		cr_cred;		/* creds */
	gid_t		cr_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_create_arg cachefsio_create_arg_t;

struct cachefsio_create_return {
	fid_t		cr_newfid;		/* returns fid of new file */
	timestruc_t	cr_ctime;		/* returns new ctime */
	timestruc_t	cr_mtime;		/* returns new mtime */
};
typedef struct cachefsio_create_return cachefsio_create_return_t;

struct cachefsio_pushback_arg {
	cfs_cid_t	pb_cid;			/* file to push back */
	fid_t		pb_fid;			/* back fs fid to push to */
	cred_t		pb_cred;		/* creds */
	gid_t		pb_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_pushback_arg cachefsio_pushback_arg_t;

struct cachefsio_pushback_return {
	timestruc_t	pb_ctime;		/* returns new ctime */
	timestruc_t	pb_mtime;		/* returns new mtime */
};
typedef struct cachefsio_pushback_return cachefsio_pushback_return_t;

struct cachefsio_remove {
	cfs_cid_t	rm_cid;			/* cid of deleted file */
	fid_t		rm_fid;			/* fid of parent directory */
	char		rm_name[MAXNAMELEN];	/* name of file to remove */
	int		rm_getctime;		/* 1 means return new ctime */
	cred_t		rm_cred;		/* creds */
	gid_t		rm_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_remove cachefsio_remove_t;

struct cachefsio_link {
	fid_t		ln_dirfid;		/* backfid of parent dir */
	char		ln_name[MAXNAMELEN];	/* name of new link */
	fid_t		ln_filefid;		/* backfid of file to link to */
	cfs_cid_t	ln_cid;			/* cid of link */
	cred_t		ln_cred;		/* creds */
	gid_t		ln_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_link cachefsio_link_t;

struct cachefsio_rename_arg {
	fid_t		rn_olddir;		/* backfs fid of old dir */
	char		rn_oldname[MAXNAMELEN];	/* old name of file */
	fid_t		rn_newdir;		/* backfs fid of new dir */
	char		rn_newname[MAXNAMELEN];	/* new name of file */
	cfs_cid_t	rn_cid;			/* cid of renamed file */
	int		rn_del_getctime;	/* 1 means fill in del_ctime */
	cfs_cid_t	rn_del_cid;		/* cid of deleted file */
	cred_t		rn_cred;		/* creds */
	gid_t		rn_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_rename_arg cachefsio_rename_arg_t;

struct cachefsio_rename_return {
	timestruc_t	rn_ctime;		/* returns new file ctime */
	timestruc_t	rn_del_ctime;		/* returns new del file ctime */
};
typedef struct cachefsio_rename_return cachefsio_rename_return_t;

struct cachefsio_mkdir {
	fid_t		md_dirfid;		/* backfs fid of dir */
	char		md_name[MAXNAMELEN];	/* name of the new dir */
	cfs_cid_t	md_cid;			/* cid of dir being created */
	vattr_t		md_vattr;		/* attributes */
	cred_t		md_cred;		/* creds */
	gid_t		md_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_mkdir cachefsio_mkdir_t;

struct cachefsio_rmdir {
	fid_t		rd_dirfid;		/* backfs fid of dir */
	char		rd_name[MAXNAMELEN];	/* name of the dir to delete */
	cred_t		rd_cred;		/* creds */
	gid_t		rd_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_rmdir cachefsio_rmdir_t;

struct cachefsio_symlink_arg {
	fid_t		sy_dirfid;		/* backfs fid of dir */
	char		sy_name[MAXNAMELEN];	/* name of symlink to create */
	cfs_cid_t	sy_cid;			/* cid of symlink */
	char		sy_link[MAXPATHLEN];	/* contents of the symlink */
	vattr_t		sy_vattr;		/* attributes */
	cred_t		sy_cred;		/* creds */
	gid_t		sy_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_symlink_arg cachefsio_symlink_arg_t;

struct cachefsio_symlink_return {
	fid_t		sy_newfid;		/* returns fid of symlink */
	timestruc_t	sy_ctime;		/* returns new ctime */
	timestruc_t	sy_mtime;		/* returns new mtime */
};
typedef struct cachefsio_symlink_return cachefsio_symlink_return_t;

struct cachefsio_setattr_arg {
	fid_t		sa_backfid;		/* backfs fid of file */
	cfs_cid_t	sa_cid;			/* cid of file */
	vattr_t		sa_vattr;		/* attributes */
	int		sa_flags;		/* flags */
	cred_t		sa_cred;		/* creds */
	gid_t		sa_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_setattr_arg cachefsio_setattr_arg_t;

struct cachefsio_setattr_return {
	timestruc_t	sa_ctime;		/* returns new ctime */
	timestruc_t	sa_mtime;		/* returns new mtime */
};
typedef struct cachefsio_setattr_return cachefsio_setattr_return_t;

struct cachefsio_setsecattr_arg {
	fid_t		sc_backfid;		/* backfs fid of file */
	cfs_cid_t	sc_cid;			/* cid of file */
	u_long		sc_mask;		/* mask for setsec */
	int		sc_aclcnt;		/* count of ACLs */
	int		sc_dfaclcnt;		/* count of default ACLs */
	aclent_t	sc_acl[MAX_ACL_ENTRIES]; /* ACLs */
	cred_t		sc_cred;		/* creds */
	gid_t		sc_groups[NGROUPS_MAX_DEFAULT-1];
};
typedef struct cachefsio_setsecattr_arg cachefsio_setsecattr_arg_t;

struct cachefsio_setsecattr_return {
	timestruc_t	sc_ctime;		/* returns new ctime */
	timestruc_t	sc_mtime;		/* returns new mtime */
};
typedef struct cachefsio_setsecattr_return cachefsio_setsecattr_return_t;

int cachefs_pack(vnode_t *, char *, cred_t *);
int cachefs_unpack(vnode_t *, char *, cred_t *);
int cachefs_packinfo(vnode_t *dvp, char *name, int *statusp, cred_t *cr);
int cachefs_unpackall(vnode_t *);

int cachefs_io_daemonid(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_stateget(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_stateset(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_xwait(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_exists(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_lostfound(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_getinfo(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_cidtofid(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_getattrfid(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_getattrname(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_getstats(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_rootfid(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_create(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_remove(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_link(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_rename(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_mkdir(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_rmdir(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_symlink(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_setattr(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_setsecattr(vnode_t *vp, void *dinp, void *doutp);
int cachefs_io_pushback(vnode_t *vp, void *dinp, void *doutp);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FS_CACHEFS_IOCTL_H */
