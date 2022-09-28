/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident   "@(#)cachefs_dlog.c 1.28     96/06/20 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/mman.h>
#include <sys/tiuser.h>
#include <sys/pathname.h>
#include <sys/dirent.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/mount.h>
#include <sys/bootconf.h>
#include <sys/dnlc.h>
#include <sys/stat.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dlog.h>
#include "fs/fs_subr.h"

#define	offsetof(s, m)	(size_t)(&(((s *)0)->m))
static int cachefs_dlog_mapreserve(fscache_t *fscp, int size);

/*
 * Sets up for writing to the log files.
 */
int
cachefs_dlog_setup(fscache_t *fscp, int createfile)
{
	struct vattr vattr;
	int error = 0;
	int createdone = 0;
	int lookupdone = 0;
	long version = CFS_DLOG_VERSION;
	off_t offset;
	struct cfs_dlog_trailer trailer;

	cachefs_mutex_enter(&fscp->fs_dlock);

	/* all done if the log files already exist */
	if (fscp->fs_dlogfile) {
		ASSERT(fscp->fs_dmapfile);
		goto out;
	}

	/* see if the log file exists */
	error = VOP_LOOKUP(fscp->fs_fscdirvp, CACHEFS_DLOG_FILE,
	    &fscp->fs_dlogfile, NULL, 0, NULL, kcred);
	if (error && (createfile == 0))
		goto out;

	/* if the lookup failed then create file log files */
	if (error) {
		createdone++;

		vattr.va_mode = S_IFREG | 0666;
		vattr.va_uid = 0;
		vattr.va_gid = 0;
		vattr.va_type = VREG;
		vattr.va_mask = AT_TYPE|AT_MODE|AT_UID|AT_GID;
		error = VOP_CREATE(fscp->fs_fscdirvp, CACHEFS_DLOG_FILE,
		    &vattr, 0, 0666, &fscp->fs_dlogfile, kcred, 0);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_DLOG)
				printf("cachefs: log file create fail %d\n",
				    error);
#endif
			goto out;
		}

		/* write the version number into the log file */
		error = vn_rdwr(UIO_WRITE, fscp->fs_dlogfile, (caddr_t)&version,
		    sizeof (long), (offset_t)0, UIO_SYSSPACE, FSYNC,
		    RLIM_INFINITY, kcred, NULL);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_DLOG)
				printf("cachefs: log file init fail %d\n",
				    error);
#endif
			goto out;
		}

		vattr.va_mode = S_IFREG | 0666;
		vattr.va_uid = 0;
		vattr.va_gid = 0;
		vattr.va_type = VREG;
		vattr.va_mask = AT_TYPE|AT_MODE|AT_UID|AT_GID;
		error = VOP_CREATE(fscp->fs_fscdirvp, CACHEFS_DMAP_FILE,
		    &vattr, 0, 0666, &fscp->fs_dmapfile, kcred, 0);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_DLOG)
				printf("cachefs: map file create fail %d\n",
				    error);
#endif
			goto out;
		}

		fscp->fs_dlogoff = sizeof (long);
		fscp->fs_dlogseq = 0;
		fscp->fs_dmapoff = 0;
		fscp->fs_dmapsize = 0;
	}

	/*
	 * Else the lookup succeeded.
	 * Before mounting, fsck should have fixed any problems
	 * in the log file.
	 */
	else {
		lookupdone++;

		/* find the end of the log file */
		vattr.va_mask = AT_ALL;
		error = VOP_GETATTR(fscp->fs_dlogfile, &vattr, 0, kcred);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_DLOG)
				printf("cachefs: log file getattr fail %d\n",
				    error);
#endif
			goto out;
		}
		ASSERT(vattr.va_size <= MAXOFF_T);
		fscp->fs_dlogoff = (off_t)vattr.va_size;

		offset = vattr.va_size - sizeof (struct cfs_dlog_trailer);
		/*
		 * The last record in the dlog file is a trailer record
		 * that contains the last sequence number used. This is
		 * used to reset the sequence number when a logfile already
		 * exists.
		 */
		error = vn_rdwr(UIO_READ, fscp->fs_dlogfile, (caddr_t)&trailer,
		    sizeof (struct cfs_dlog_trailer), (offset_t)offset,
		    UIO_SYSSPACE, FSYNC, RLIM_INFINITY, kcred, NULL);
		if (error == 0) {
			if (trailer.dl_op == CFS_DLOG_TRAILER) {
				fscp->fs_dlogseq = trailer.dl_seq;
				/*
				 * Set the offset of the next record to be
				 * written, to over write the current
				 * trailer.
				 */
				fscp->fs_dlogoff = offset;
			} else {
#ifdef CFSDEBUG
				CFS_DEBUG(CFSDEBUG_DLOG) {
					cmn_err(CE_WARN,
					    "cachefs: can't find dlog trailer");
					cmn_err(CE_WARN,
					    "cachefs: fsck required");
				}
#endif /* CFSDEBUG */
				fscp->fs_dlogseq = vattr.va_size;
			}
		} else {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_DLOG)
				cmn_err(CE_WARN,
				    "cachefs: error reading dlog trailer");
#endif /* CFSDEBUG */
			fscp->fs_dlogseq = vattr.va_size;
		}


		error = VOP_LOOKUP(fscp->fs_fscdirvp, CACHEFS_DMAP_FILE,
		    &fscp->fs_dmapfile, NULL, 0, NULL, kcred);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_DLOG)
				printf("cachefs: map file lookup fail %d\n",
				    error);
#endif
			goto out;
		}

		vattr.va_mask = AT_ALL;
		error = VOP_GETATTR(fscp->fs_dmapfile, &vattr, 0, kcred);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_DLOG)
				printf("cachefs: map file getattr fail %d\n",
				    error);
#endif
			goto out;
		}
		fscp->fs_dmapoff = (off_t)vattr.va_size;
		fscp->fs_dmapsize = (off_t)vattr.va_size;
	}

out:
	if (error) {
		if (createdone) {
			if (fscp->fs_dlogfile) {
				VN_RELE(fscp->fs_dlogfile);
				fscp->fs_dlogfile = NULL;
				VOP_REMOVE(fscp->fs_fscdirvp, CACHEFS_DLOG_FILE,
				    kcred);
			}
			if (fscp->fs_dmapfile) {
				VN_RELE(fscp->fs_dmapfile);
				fscp->fs_dmapfile = NULL;
				VOP_REMOVE(fscp->fs_fscdirvp, CACHEFS_DMAP_FILE,
				    kcred);
			}
		}
		if (lookupdone) {
			if (fscp->fs_dlogfile) {
				VN_RELE(fscp->fs_dlogfile);
				fscp->fs_dlogfile = NULL;
			}
			if (fscp->fs_dmapfile) {
				VN_RELE(fscp->fs_dmapfile);
				fscp->fs_dmapfile = NULL;
			}
		}
	}

	cachefs_mutex_exit(&fscp->fs_dlock);
	return (error);
}

/*
 * Drops reference to the log file.
 */
void
cachefs_dlog_teardown(fscache_t *fscp)
{
	vattr_t va;
	int error;

	cachefs_mutex_enter(&fscp->fs_dlock);

	/* clean up the log file */
	if (fscp->fs_dlogfile) {
		VN_RELE(fscp->fs_dlogfile);
		fscp->fs_dlogfile = NULL;
	}

	/* clean up the map file */
	if (fscp->fs_dmapfile) {
		/* set the map file to the actual size needed */
		va.va_mask = AT_SIZE;
		va.va_size = fscp->fs_dmapoff;
		error = VOP_SETATTR(fscp->fs_dmapfile, &va, 0, kcred);
		if (error) {
#ifdef CFSDEBUG
			cmn_err(CE_WARN, "cachefs: map setattr failed %d",
			    error);
#endif
		}

		VN_RELE(fscp->fs_dmapfile);
		fscp->fs_dmapfile = NULL;
	}
	cachefs_mutex_exit(&fscp->fs_dlock);
}

/*
 * Outputs a dlog message to the log file.
 */
static off_t
cachefs_dlog_output(fscache_t *fscp, cfs_dlog_entry_t *entp, u_long *seqp)
{
	int error;
	off_t offset;
	int xx;
	u_long seq;
	int len;
	struct cfs_dlog_trailer *trail;

	ASSERT(entp->dl_len <= CFS_DLOG_ENTRY_MAXSIZE);

	if (fscp->fs_dlogfile == NULL) {
		error = cachefs_dlog_setup(fscp, 1);
		if (error) {
			offset = 0;
			goto out;
		}
	}

	/* round up length to a 4 byte boundary */
	len = entp->dl_len;
	xx = len & 0x03;
	if (xx) {
		xx = 4 - xx;
		bzero((caddr_t)entp + len, xx);
		len += xx;
		entp->dl_len = len;
	}

	/* XXX turn this on/off in sync with code in cachefs_dlog_setsecattr */
#if 0
	/* XXX debugging hack, round up to 16 byte boundary */
	len = entp->dl_len;
	xx = 16 - (len & 0x0f);
	bcopy((caddr_t)"UUUUUUUUUUUUUUUU", (caddr_t)entp + len, xx);
	len += xx;
	entp->dl_len = len;
#endif

	/*
	 * All functions which allocate a dlog entry buffer must be sure
	 * to allocate space for the trailer record. The trailer record,
	 * is always located at the end of the log file. It contains the
	 * highest sequence number used. This allows cachefs_dlog_setup()
	 * to reset the sequence numbers properly when the log file
	 * already exists.
	 */
	trail = (struct cfs_dlog_trailer *)((int)entp + entp->dl_len);
	trail->dl_len = sizeof (struct cfs_dlog_trailer);
	trail->dl_op = CFS_DLOG_TRAILER;
	trail->dl_valid = CFS_DLOG_VAL_COMMITTED;
	cachefs_mutex_enter(&fscp->fs_dlock);
	ASSERT(fscp->fs_dlogfile);

	/* get a sequence number for this log entry */
	seq = fscp->fs_dlogseq + 1;
	if (seq == 0) {
		cachefs_mutex_exit(&fscp->fs_dlock);
		offset = 0;
#ifdef CFSDEBUG
		cmn_err(CE_WARN, "cachefs: logging failed, seq overflow");
#endif
		goto out;
	}
	fscp->fs_dlogseq++;
	trail->dl_seq = fscp->fs_dlogseq;

	/* add the sequence number to the record */
	entp->dl_seq = seq;

	/* get offset into file to write record */
	offset = fscp->fs_dlogoff;

	/* try to write the record to the log file */
	/*
	 * NOTE This write will over write the previous trailer record and
	 * will add a new trailer record. This is done with a single
	 * write for performance reasons.
	 */
	error = vn_rdwr(UIO_WRITE, fscp->fs_dlogfile, (caddr_t)entp,
	    entp->dl_len+trail->dl_len, (offset_t)offset, UIO_SYSSPACE, FSYNC,
	    RLIM_INFINITY, kcred, NULL);

	if (error) {
		offset = 0;
		cmn_err(CE_WARN, "cachefs: logging failed (%d)", error);
	} else {
		fscp->fs_dlogoff += entp->dl_len;

		/* get offset of valid field */
		offset += offsetof(struct cfs_dlog_entry, dl_valid);
	}

	cachefs_mutex_exit(&fscp->fs_dlock);

	/* return sequence number used if requested */
	if (seqp)
		*seqp = seq;

out:
	return (offset);
}

/*
 * Commmits a previously written dlog message.
 */
int
cachefs_dlog_commit(fscache_t *fscp, off_t offset, int error)
{
	cfs_dlog_val_t valid;

	if (error)
		valid = CFS_DLOG_VAL_ERROR;
	else
		valid = CFS_DLOG_VAL_COMMITTED;

	error = vn_rdwr(UIO_WRITE, fscp->fs_dlogfile,
	    (caddr_t)&valid, sizeof (valid), (offset_t)offset,
	    UIO_SYSSPACE, FSYNC, RLIM_INFINITY, kcred, NULL);

	if (error)
		cmn_err(CE_WARN, "cachefs: logging commit failed (%d)", error);
	return (error);
}

/*
 * Reserves space in the map file.
 */
static int
cachefs_dlog_mapreserve(fscache_t *fscp, int size)
{
	int error = 0;
	int len;
	char *bufp;

	if (fscp->fs_dmapfile == NULL) {
		error = cachefs_dlog_setup(fscp, 1);
		if (error) {
			return (error);
		}
	}

	cachefs_mutex_enter(&fscp->fs_dlock);
	ASSERT(fscp->fs_dmapoff <= fscp->fs_dmapsize);
	ASSERT(fscp->fs_dmapfile);

	if ((fscp->fs_dmapoff + size) > fscp->fs_dmapsize) {
		/* reserve 20% for optimal hashing */
		size += MAXBSIZE / 5;

		/* grow file by a MAXBSIZE chunk */
		len = MAXBSIZE;
		ASSERT((fscp->fs_dmapoff + size) < (fscp->fs_dmapsize + len));

		bufp = (char *)cachefs_kmem_zalloc(len, KM_SLEEP);
		error = vn_rdwr(UIO_WRITE, fscp->fs_dmapfile, (caddr_t)bufp,
			len, (offset_t)fscp->fs_dmapsize, UIO_SYSSPACE, FSYNC,
			RLIM_INFINITY, kcred, NULL);
		if (error == 0) {
			fscp->fs_dmapoff += size;
			fscp->fs_dmapsize += len;
		} else {
			cmn_err(CE_WARN, "cachefs: logging secondary "
			    "failed (%d)", error);
		}
		cachefs_kmem_free((caddr_t)bufp, len);
	} else {
		fscp->fs_dmapoff += size;
	}
	cachefs_mutex_exit(&fscp->fs_dlock);
	return (error);
}

/*
 * Reserves space for one cid mapping in the mapping file.
 */
int
cachefs_dlog_cidmap(fscache_t *fscp)
{
	int error;
	error = cachefs_dlog_mapreserve(fscp,
	    sizeof (struct cfs_dlog_mapping_space));
	return (error);
}

off_t
cachefs_dlog_setattr(fscache_t *fscp, struct vattr *vap, int flags,
    cnode_t *cp, cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_setattr *up;
	int	len;
	off_t offset;

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_SETATTR;
	up = &entp->dl_u.dl_setattr;
	up->dl_attrs = *vap;
	up->dl_flags = flags;
	up->dl_cid = cp->c_id;
	up->dl_times.tm_mtime = cp->c_metadata.md_vattr.va_mtime;
	up->dl_times.tm_ctime = cp->c_metadata.md_vattr.va_ctime;

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* Calculate the length of this record */
	entp->dl_len = (((caddr_t)&up->dl_cred) + len) - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
/*ARGSUSED*/
cachefs_dlog_setsecattr(fscache_t *fscp, vsecattr_t *vsec, int flags,
    cnode_t *cp, cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_setsecattr *up;
	int alen, clen, len;
	off_t offset = 0;
	aclent_t *aclp;

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));

	/* paranoia */
	ASSERT((vsec->vsa_mask & VSA_ACL) || (vsec->vsa_aclcnt == 0));
	ASSERT((vsec->vsa_mask & VSA_DFACL) || (vsec->vsa_dfaclcnt == 0));
	if ((vsec->vsa_mask & VSA_ACL) == 0)
		vsec->vsa_aclcnt = 0;
	if ((vsec->vsa_mask & VSA_DFACL) == 0)
		vsec->vsa_dfaclcnt = 0;

	/* calculate length of ACL and cred data */
	alen = sizeof (aclent_t) * (vsec->vsa_aclcnt + vsec->vsa_dfaclcnt);
	clen = sizeof (cred_t) + ((cr->cr_ngroups - 1) * sizeof (gid_t));

	/*
	 * allocate entry.  ACLs may be up to 24k currently, but they
	 * usually won't, so we don't want to make cfs_dlog_entry_t
	 * too big.  so, we must compute the length here.
	 */

	len = sizeof (*entp) - sizeof (*up);
	len += sizeof (*up) - sizeof (up->dl_buffer);
	len += alen + (clen - sizeof (up->dl_cred));

#if 0
	/* make up for weird behavior in cachefs_dlog_output */
	/* XXX turn this on/off in sync with code in cachefs_dlog_output */
	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(len + 32 + sizeof (struct cfs_dlog_trailer),
	    KM_SLEEP);
#else
	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(len, KM_SLEEP);
#endif

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_SETSECATTR;

	up = &entp->dl_u.dl_setsecattr;
	up->dl_cid = cp->c_id;

	up->dl_times.tm_mtime = cp->c_metadata.md_vattr.va_mtime;
	up->dl_times.tm_ctime = cp->c_metadata.md_vattr.va_ctime;

	/* get the creds */
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, clen);

	/* mask and counts */
	up->dl_mask = vsec->vsa_mask;
	up->dl_aclcnt = vsec->vsa_aclcnt;
	up->dl_dfaclcnt = vsec->vsa_dfaclcnt;

	/* get the acls themselves */
	aclp = (aclent_t *)(((caddr_t)(&up->dl_cred)) + clen);
	if (vsec->vsa_aclcnt > 0) {
		bcopy((caddr_t) vsec->vsa_aclentp, (caddr_t) aclp,
		    vsec->vsa_aclcnt * sizeof (aclent_t));
		aclp += vsec->vsa_aclcnt;
	}
	if (vsec->vsa_dfaclcnt > 0) {
		bcopy((caddr_t) vsec->vsa_dfaclentp, (caddr_t) aclp,
		    vsec->vsa_dfaclcnt * sizeof (aclent_t));
	}

	entp->dl_len = len;

	offset = cachefs_dlog_output(fscp, entp, NULL);

#if 0
	/* XXX turn on/off in sync with code in cachefs_dlog_output */
	cachefs_kmem_free((caddr_t) entp, len + 32 +
	    sizeof (struct cfs_dlog_trailer));
#else
	cachefs_kmem_free((caddr_t) entp, len);
#endif

	return (offset);
}

off_t
cachefs_dlog_create(fscache_t *fscp, cnode_t *pcp, char *nm,
    vattr_t *vap, int excl, int mode, cnode_t *cp, int exists, cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_create *up;
	int len;
	caddr_t curp;
	off_t offset;

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_CREATE;
	up = &entp->dl_u.dl_create;
	up->dl_parent_cid = pcp->c_id;
	up->dl_new_cid = cp->c_id;
	up->dl_attrs = *vap;
	up->dl_excl = excl;
	up->dl_mode = mode;
	up->dl_exists = exists;
	bzero((caddr_t)&up->dl_fid, sizeof (fid_t));
	if (exists) {
		up->dl_times.tm_mtime = cp->c_metadata.md_vattr.va_mtime;
		up->dl_times.tm_ctime = cp->c_metadata.md_vattr.va_ctime;
	} else {
		up->dl_times.tm_ctime.tv_sec = 0;
		up->dl_times.tm_ctime.tv_nsec = 0;
		up->dl_times.tm_mtime.tv_sec = 0;
		up->dl_times.tm_mtime.tv_nsec = 0;
	}

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* store the created name */
	len = strlen(nm) + 1;
	bcopy(nm, curp, len);

	/* calculate the length of this record */
	entp->dl_len = (curp + len) - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
cachefs_dlog_remove(fscache_t *fscp, cnode_t *pcp, char *nm, cnode_t *cp,
    cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_remove *up;
	int len;
	caddr_t curp;
	off_t offset;

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_REMOVE;
	up = &entp->dl_u.dl_remove;
	up->dl_parent_cid = pcp->c_id;
	up->dl_child_cid = cp->c_id;
	up->dl_times.tm_mtime = cp->c_metadata.md_vattr.va_mtime;
	up->dl_times.tm_ctime = cp->c_metadata.md_vattr.va_ctime;

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* store the removed name */
	len = strlen(nm) + 1;
	bcopy(nm, curp, len);

	/* calculate the length of this record */
	entp->dl_len = (curp + len) - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
cachefs_dlog_link(fscache_t *fscp, cnode_t *pcp, char *nm, cnode_t *cp,
    cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_link *up;
	int len;
	caddr_t curp;
	off_t offset;

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_LINK;
	up = &entp->dl_u.dl_link;
	up->dl_parent_cid = pcp->c_id;
	up->dl_child_cid = cp->c_id;
	up->dl_times.tm_mtime = cp->c_metadata.md_vattr.va_mtime;
	up->dl_times.tm_ctime = cp->c_metadata.md_vattr.va_ctime;

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* store the link name */
	len = strlen(nm) + 1;
	bcopy(nm, curp, len);

	/* calculate the length of this record */
	entp->dl_len = (curp + len) - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
cachefs_dlog_rename(fscache_t *fscp, cnode_t *odcp, char *onm, cnode_t *ndcp,
    char *nnm, cred_t *cr, cnode_t *cp, cnode_t *delcp)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_rename *up;
	int len;
	caddr_t curp;
	off_t offset;

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_RENAME;
	up = &entp->dl_u.dl_rename;
	up->dl_oparent_cid = odcp->c_id;
	up->dl_nparent_cid = ndcp->c_id;
	up->dl_child_cid = cp->c_id;
	up->dl_times.tm_mtime = cp->c_metadata.md_vattr.va_mtime;
	up->dl_times.tm_ctime = cp->c_metadata.md_vattr.va_ctime;
	if (delcp) {
		up->dl_del_cid = delcp->c_id;
		up->dl_del_times.tm_mtime = delcp->c_metadata.md_vattr.va_mtime;
		up->dl_del_times.tm_ctime = delcp->c_metadata.md_vattr.va_ctime;
	} else {
		up->dl_del_cid.cid_fileno = 0;
		up->dl_del_cid.cid_flags = 0;
		up->dl_del_times.tm_mtime.tv_sec = 0;
		up->dl_del_times.tm_mtime.tv_nsec = 0;
		up->dl_del_times.tm_ctime.tv_sec = 0;
		up->dl_del_times.tm_ctime.tv_nsec = 0;
	}

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* store the old name */
	len = strlen(onm) + 1;
	bcopy(onm, curp, len);

	/* store the new name */
	curp += len;
	len = strlen(nnm) + 1;
	bcopy(nnm, curp, len);

	/* calculate the length of this record */
	entp->dl_len = (curp + len) - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
cachefs_dlog_mkdir(fscache_t *fscp, cnode_t *pcp, cnode_t *cp, char *nm,
    vattr_t *vap, cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_mkdir *up;
	int len;
	caddr_t curp;
	off_t offset;

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_MKDIR;
	up = &entp->dl_u.dl_mkdir;
	up->dl_parent_cid = pcp->c_id;
	up->dl_child_cid = cp->c_id;
	up->dl_attrs = *vap;
	bzero((caddr_t)&up->dl_fid, sizeof (fid_t));

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* store the new directory name */
	len = strlen(nm) + 1;
	bcopy(nm, curp, len);

	/* calculate the length of this record */
	entp->dl_len = (curp + len) - (caddr_t)entp;

	/* write the record in the dlog */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
cachefs_dlog_rmdir(fscache_t *fscp, cnode_t *pcp, char *nm, cnode_t *cp,
    cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_rmdir *up;
	int len;
	caddr_t curp;
	off_t offset;

	/* if not a local dir, log the cid to fid mapping */
	if ((cp->c_id.cid_flags & CFS_CID_LOCAL) == 0) {
		if (cachefs_dlog_mapfid(fscp, cp))
			return (0);
		if (cachefs_dlog_cidmap(fscp))
			return (0);
	}

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_RMDIR;
	up = &entp->dl_u.dl_rmdir;
	up->dl_parent_cid = pcp->c_id;

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* store the created name */
	len = strlen(nm) + 1;
	bcopy(nm, curp, len);

	/* calculate the length of this record */
	entp->dl_len = (curp + len) - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
cachefs_dlog_symlink(fscache_t *fscp, cnode_t *pcp, cnode_t *cp, char *lnm,
    vattr_t *vap, char *tnm, cred_t *cr)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_symlink *up;
	int len;
	caddr_t curp;
	off_t offset;

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_SYMLINK;
	up = &entp->dl_u.dl_symlink;
	up->dl_parent_cid = pcp->c_id;
	up->dl_child_cid = cp->c_id;
	up->dl_attrs = *vap;
	up->dl_times.tm_ctime.tv_sec = 0;
	up->dl_times.tm_ctime.tv_nsec = 0;
	up->dl_times.tm_mtime.tv_sec = 0;
	up->dl_times.tm_mtime.tv_nsec = 0;
	bzero((caddr_t)&up->dl_fid, sizeof (fid_t));

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* store the link name */
	len = strlen(lnm) + 1;
	bcopy(lnm, curp, len);

	/* store new name */
	curp += len;
	len = strlen(tnm) + 1;
	bcopy(tnm, curp, len);

	/* calculate the length of this record */
	entp->dl_len = (curp + len) - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

off_t
cachefs_dlog_modify(fscache_t *fscp, cnode_t *cp, cred_t *cr, u_long *seqp)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_modify *up;
	off_t offset;
	u_long seq;
	caddr_t curp;
	int len;

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_CRASH;
	entp->dl_op = CFS_DLOG_MODIFIED;
	up = &entp->dl_u.dl_modify;
	up->dl_cid = cp->c_id;
	up->dl_times.tm_mtime = cp->c_metadata.md_vattr.va_mtime;
	up->dl_times.tm_ctime = cp->c_metadata.md_vattr.va_ctime;
	up->dl_next = 0;

	/* store the cred info */
	len = sizeof (cred_t) + (cr->cr_ngroups - 1) * sizeof (gid_t);
	bcopy((caddr_t)cr, (caddr_t)&up->dl_cred, len);

	/* find the address in buffer past where the creds are stored */
	curp = ((caddr_t)&up->dl_cred) + len;

	/* calculate the length of this record */
	entp->dl_len = curp - (caddr_t)entp;

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, &seq);

	/* return sequence number */
	*seqp = seq;

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset);
}

int
cachefs_dlog_mapfid(fscache_t *fscp, cnode_t *cp)
{
	struct cfs_dlog_entry *entp;
	struct cfs_dlog_mapfid *up;
	off_t offset;

	entp = (cfs_dlog_entry_t *)
	    cachefs_kmem_alloc(sizeof (cfs_dlog_entry_t), KM_SLEEP);

	entp->dl_valid = CFS_DLOG_VAL_COMMITTED;
	entp->dl_op = CFS_DLOG_MAPFID;
	up = &entp->dl_u.dl_mapfid;
	up->dl_cid = cp->c_id;
	up->dl_fid = cp->c_cookie;

	/* calculate the length of this record */
	entp->dl_len = (caddr_t)up - (caddr_t)entp + sizeof (*up);

	/* write the record in the log */
	offset = cachefs_dlog_output(fscp, entp, NULL);

	cachefs_kmem_free((caddr_t)entp, sizeof (cfs_dlog_entry_t));
	return (offset == 0);
}

/* Returns the next sequence number, 0 if an error */
u_long
cachefs_dlog_seqnext(fscache_t *fscp)
{
	int error;
	u_long seq;

	if (fscp->fs_dlogfile == NULL) {
		error = cachefs_dlog_setup(fscp, 1);
		if (error)
			return (0);
	}

	cachefs_mutex_enter(&fscp->fs_dlock);
	ASSERT(fscp->fs_dlogfile);

	/* get a sequence number for this log entry */
	seq = fscp->fs_dlogseq + 1;
	if (seq == 0) {
#ifdef CFSDEBUG
		cmn_err(CE_WARN, "cachefs: logging failed, seq overflow 2.");
#endif
	} else {
		fscp->fs_dlogseq++;
	}
	cachefs_mutex_exit(&fscp->fs_dlock);
	return (seq);
}
