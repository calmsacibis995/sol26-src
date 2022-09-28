// ------------------------------------------------------------
//
//			cfsd_kmod.h
//
// Include file for the cfsd_kmod class.
//

#pragma ident   "@(#)cfsd_kmod.h 1.10     95/10/20 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

#ifndef CFSD_KMOD
#define	CFSD_KMOD

class cfsd_kmod {
private:
	RWCString		i_path;		// path to root of file system
	int			i_fd;		// file descriptor of i_path
	char			i_fidbuf[1024];	// for formatted fid

	void i_format_fid(const fid_t *fidp);
	void i_print_cred(const cred_t *credp);
	void i_print_attr(const vattr_t *vattrp);
	int  i_doioctl(enum cfsdcmd_cmds cmd, void *sdata, int slen,
	    void *rdata, int rlen);

public:
	cfsd_kmod();
	~cfsd_kmod();
	int kmod_setup(const char *path);
	void kmod_shutdown();
	int kmod_xwait();
	int kmod_stateget();
	int kmod_stateset(int state);
	int kmod_exists(cfs_cid *cidp);
	int kmod_lostfound(cfs_cid *cidp, const char *namep, char *newnamep);
	int kmod_lostfoundall();
	int kmod_rofs();
	int kmod_rootfid(fid_t *fidp);
	int kmod_getstats(cachefsio_getstats_t *);
	int kmod_getinfo(cfs_cid_t *filep, cachefsio_getinfo *infop);
	int kmod_cidtofid(cfs_cid *cidp, fid_t *fidp);
	int kmod_getattrfid(fid_t *fidp, cred_t *credp, vattr_t *vattrp);
	int kmod_getattrname(fid_t *dirp, const char *name, cred_t *credp,
		vattr_t *vattrp, fid_t *filep);
	int kmod_create(fid_t *dirp, const char *namep, const cfs_cid_t *cidp,
		vattr_t *vattrp, int exclusive, int mode, cred_t *credp,
		fid_t *newfidp, timestruc_t *ctimep, timestruc_t *mtimep);
	int kmod_pushback(cfs_cid *filep, fid_t *fidp, cred_t *credp,
		timestruc_t *ctimep, timestruc_t *mtimep, int update);
	int kmod_rename(fid_t *olddir, const char *oldname, fid_t *newdir,
		const char *newname, const cfs_cid_t *cidp, cred_t *credp,
		timestruc_t *ctimep, timestruc_t *delctimep,
		const cfs_cid_t *delcidp);
	int kmod_setattr(fid_t *fidp, const cfs_cid_t *cidp, vattr_t *vattrp,
		int flags,
		cred_t *credp, timestruc_t *ctimep, timestruc_t *mtimep);
	int kmod_setsecattr(fid_t *fidp, const cfs_cid_t *cidp, u_long mask,
		int aclcnt, int dfaclcnt, const aclent_t *acl, cred_t *credp,
		timestruc_t *ctimep, timestruc_t *mtimep);
	int kmod_remove(const fid_t *fidp, const cfs_cid_t *cidp,
		const char *namep, const cred_t *credp, timestruc_t *ctimep);
	int kmod_link(const fid_t *dirfidp, const char *namep,
		const fid_t *filefidp, const cfs_cid_t *cidp,
		const cred_t *credp, timestruc_t *ctimep);
	int kmod_mkdir(const fid_t *dirfidp, const char *namep,
		const cfs_cid_t *cidp,
		const vattr_t *vattrp, const cred_t *credp, fid_t *newfidp);
	int kmod_rmdir(const fid_t *dirfidp, const char *namep,
		const cred_t *credp);
	int kmod_symlink(const fid_t *dirfidp, const char *namep,
		const cfs_cid_t *cidp,
		const char *linkvalp, const vattr_t *vattrp,
		const cred_t *credp,
		fid_t *newfidp, timestruc_t *ctimep, timestruc_t *mtimep);
};

#endif /* CFSD_KMOD */
