/*	Copyright (c) 1996 Sun Microsystems, Inc		*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1996, by Sun Microsystems, Inc.
 *		All rights reserved.
 *
 */

#ident	"@(#)ufs_directio.c 1.9 96/09/04 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/dnlc.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/filio.h>

#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_lockfs.h>
#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fsdir.h>
#ifdef QUOTA
#include <sys/fs/ufs_quota.h>
#endif
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_panic.h>
#include <sys/dirent.h>		/* must be AFTER <sys/fs/fsdir.h>! */
#include <sys/errno.h>

#include <sys/filio.h>		/* _FIOIO */

#include <vm/hat.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kmem.h>
#include <vm/rm.h>
#include <sys/swap.h>

#include <fs/fs_subr.h>

extern int	ufs_trans_directio;	/* see ufs_trans.c */

static void	*ufs_directio_zero_buf;
static int	ufs_directio_zero_len	= 8192;

int	ufs_directio_enabled = 1;	/* feature is enabled */

struct ufs_directio_kstats {
	u_long	logical_reads;
	u_long	phys_reads;
	u_long	hole_reads;
	u_long	nread;
	u_long	logical_writes;
	u_long	phys_writes;
	u_long	nwritten;
	u_long	nflushes;
} ufs_directio_kstats;

kstat_t	*ufs_directio_kstatsp;

void
ufs_directio_init()
{
	/*
	 * kstats
	 */
	ufs_directio_kstatsp = kstat_create("ufs directio", 0,
			"UFS DirectIO Stats", "ufs directio",
			KSTAT_TYPE_RAW, sizeof (ufs_directio_kstats),
			KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ufs_directio_kstatsp) {
		ufs_directio_kstatsp->ks_data = (void *)&ufs_directio_kstats;
		kstat_install(ufs_directio_kstatsp);
	}
	/*
	 * kzero is broken so we have to use a private buf of zeroes
	 */
	ufs_directio_zero_buf = kmem_zalloc(ufs_directio_zero_len, KM_SLEEP);
}

/*
 * Direct Write
 */
int
ufs_directio_write(struct inode *ip, uio_t *uio, cred_t *cr, int *statusp)
{
	long		resid;
	u_offset_t	size, uoff;
	rlim64_t	limit = uio->uio_llimit;
	int		on, n, error, len, has_holes;
	daddr_t		bn;
	u_int		cnt;
	struct fs	*fs;
	vnode_t		*vp;
	uio_t		phys_uio;
	iovec_t		phys_iov, *iov;
	struct ufsvfs	*ufsvfsp = ip->i_ufsvfs;

	/*
	 * assume that directio isn't possible (normal case)
	 */
	*statusp = DIRECTIO_FAILURE;

	/*
	 * Don't go direct
	 */
	if (ufs_directio_enabled == 0)
		return (0);

	/*
	 * mapped file; nevermind
	 */
	if (ip->i_mapcnt)
		return (0);

	/*
	 * not on old Logging UFS
	 */
	if ((ufs_trans_directio == 0) && TRANS_ISTRANS(ip->i_ufsvfs))
		return (0);

	/*
	 * CAN WE DO DIRECT IO?
	 */
	uoff = uio->uio_loffset;
	resid = uio->uio_resid;

	/*
	 * beyond limit
	 */
	if (uoff + resid > limit)
		resid = limit - uoff;

	/*
	 * must be sector aligned
	 */
	if ((uoff & (u_offset_t)(DEV_BSIZE - 1)) || (resid & (DEV_BSIZE - 1)))
		return (0);

	/*
	 * must be short aligned and sector aligned
	 */
	iov = uio->uio_iov;
	cnt = uio->uio_iovcnt;
	while (cnt--) {
		if (((u_int)iov->iov_len & (DEV_BSIZE - 1)) != 0)
			return (0);
		if ((intptr_t)(iov++->iov_base) & 1)
			return (0);
	}

	/*
	 * SHOULD WE DO DIRECT IO?
	 */
	size = ip->i_size;
	has_holes = -1;

	/*
	 * only on regular files; no metadata
	 */
	if (((ip->i_mode & IFMT) != IFREG) || ip->i_ufsvfs->vfs_qinod == ip)
		return (0);

	/*
	 * Synchronous, allocating writes run very slow in Direct-Mode
	 * 	XXX - can be fixed with bmap_write changes for large writes!!!
	 *	XXX - can be fixed for updates to "almost-full" files
	 *	XXX - WARNING - system hangs if bmap_write() has to
	 * 			allocate lots of pages since pageout
	 * 			suspends on locked inode
	 */
	if (ip->i_flag & ISYNC) {
		if ((uoff + resid) > size)
			return (0);
		has_holes = bmap_has_holes(ip);
		if (has_holes)
			return (0);
	}

	/*
	 * DIRECTIO
	 */

	fs = ip->i_fs;
	/*
	 * allocate space
	 */
	do {
		on = (int)blkoff(fs, uoff);
		n = (int)MIN(fs->fs_bsize - on, resid);
		if ((uoff + n) > ip->i_size) {
			error = bmap_write(ip, uoff, (int)(on + n),
				    (int)(uoff & (offset_t)MAXBOFFSET) == 0,
			    cr);
			if (error)
				break;
			ip->i_size = uoff + n;
			ip->i_flag |= IATTCHG;
		} else if (n == MAXBSIZE) {
			error = bmap_write(ip, uoff, (int)(on + n), 1, cr);
		} else {
			if (has_holes < 0)
				has_holes = bmap_has_holes(ip);
			if (has_holes) {
				u_int	blk_size;
				u_offset_t offset;

				offset = uoff & (offset_t)fs->fs_bmask;
				blk_size = (int)blksize(fs, ip,
				    (daddr_t)lblkno(fs, offset));
				error = bmap_write(ip, uoff, blk_size, 0, cr);
			} else
				error = 0;
		}
		if (error)
			break;
		uoff += n;
		resid -= n;
		/*
		 * if file has grown larger than 2GB, set flag
		 * in superblock if not already set
		 */
		if ((ip->i_size > MAXOFF_T) &&
		    !(fs->fs_flags & FSLARGEFILES)) {
			ASSERT(ufsvfsp->vfs_lfflags & UFS_LARGEFILES);
			mutex_enter(&ufsvfsp->vfs_lock);
			fs->fs_flags |= FSLARGEFILES;
			ufs_sbwrite(ufsvfsp);
			mutex_exit(&ufsvfsp->vfs_lock);
		}
	} while (resid);

	if (error) {
		/*
		 * restore original state
		 */
		if (resid) {
			if (size == ip->i_size)
				return (0);
			(void) ufs_itrunc(ip, size, 0, cr);
		}
		/*
		 * try non-directio path
		 */
		return (0);
	}

	/*
	 * get rid of cached pages
	 */
	vp = ITOV(ip);
	if (vp->v_pages != NULL) {
		ASSERT(ip->i_owner == NULL);
		ip->i_owner = curthread;
		(void) VOP_PUTPAGE(vp, (offset_t)0, (u_int)0, B_INVAL, cr);
		ip->i_owner = NULL;
		ufs_directio_kstats.nflushes++;
	}
	if (vp->v_pages)
		return (0);

	*statusp = DIRECTIO_SUCCESS;
	error = 0;
	resid = uio->uio_resid;
	ufs_directio_kstats.logical_writes++;
	while (error == 0 && resid && uio->uio_iovcnt) {
		uoff = uio->uio_loffset;
		iov = uio->uio_iov;
		cnt = (int)MIN(iov->iov_len, resid);
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		len = (int)blkroundup(fs, cnt);
		error = bmap_read(ip, uoff, &bn, &len);
		if (error)
			break;
		if (bn == UFS_HOLE || len == 0)
			break;

		cnt = MIN(cnt, len);

		ufs_directio_kstats.phys_writes++;
		ufs_directio_kstats.nwritten += cnt;

		phys_iov.iov_base = iov->iov_base;
		phys_iov.iov_len = cnt;

		phys_uio.uio_iov = &phys_iov;
		phys_uio.uio_iovcnt = 1;
		phys_uio.uio_loffset = ldbtob(bn);
		phys_uio.uio_resid = phys_iov.iov_len;
		phys_uio.uio_segflg = uio->uio_segflg;
		phys_uio.uio_fmode = uio->uio_fmode;
		phys_uio.uio_llimit = uio->uio_llimit;
		error = cdev_write(ip->i_dev, &phys_uio, cr);
		if (phys_uio.uio_resid) {
			if (error == 0)
				error = EIO;
			cnt -= phys_uio.uio_resid;
		}
		iov->iov_len -= cnt;
		iov->iov_base += cnt;
		uio->uio_loffset += cnt;
		uio->uio_resid -= cnt;
		resid -= cnt;
	}
	ip->i_flag |= IUPD | ICHG;
	TRANS_INODE(ip->i_ufsvfs, ip);
	if (resid) {
		if (size == ip->i_size)
			return (error);
		if (uio->uio_loffset > size)
			size = uio->uio_loffset;
		(void) ufs_itrunc(ip, size, 0, cr);
		return (error);
	}
	return (error);
}

/*
 * Direct Read
 */
int
ufs_directio_read(struct inode *ip, uio_t *uio, cred_t *cr, int *statusp)
{
	long		resid, nzero;
	u_offset_t	size, uoff;
	int		error, len;
	u_int		cnt;
	struct fs	*fs;
	vnode_t		*vp;
	daddr_t		bn;
	uio_t		phys_uio;
	iovec_t		phys_iov, *iov;

	/*
	 * assume that directio isn't possible (normal case)
	 */
	*statusp = DIRECTIO_FAILURE;

	/*
	 * Don't go direct
	 */
	if (ufs_directio_enabled == 0)
		return (0);

	/*
	 * mapped file; nevermind
	 */
	if (ip->i_mapcnt)
		return (0);

	/*
	 * CAN WE DO DIRECT IO?
	 */
	/*
	 * must be sector aligned
	 */
	uoff = uio->uio_loffset;
	resid = uio->uio_resid;
	if ((uoff & (u_offset_t)(DEV_BSIZE - 1)) || (resid & (DEV_BSIZE - 1)))
		return (0);
	/*
	 * must be short aligned and sector aligned
	 */
	iov = uio->uio_iov;
	cnt = uio->uio_iovcnt;
	while (cnt--) {
		if (((u_int)iov->iov_len & (DEV_BSIZE - 1)) != 0)
			return (0);
		if ((intptr_t)(iov++->iov_base) & 1)
			return (0);
	}

	/*
	 * DIRECTIO
	 */
	fs = ip->i_fs;

	/*
	 * don't read past EOF
	 */
	size = ip->i_size;
	if ((uoff + resid) > size)
		if (uoff > size)
			resid = 0;
		else {
			resid = size - uoff;
			/*
			 * recheck sector alignment
			 */
			if (resid & (DEV_BSIZE - 1))
				return (0);
		}

	/*
	 * get rid of cached pages
	 */
	vp = ITOV(ip);
	if (vp->v_pages != NULL) {
		rw_exit(&ip->i_contents);
		rw_enter(&ip->i_contents, RW_WRITER);
		ASSERT(ip->i_owner == NULL);
		ip->i_owner = curthread;
		(void) VOP_PUTPAGE(vp, (offset_t)0, (u_int)0, B_INVAL, cr);
		ip->i_owner = NULL;
		ufs_directio_kstats.nflushes++;
	}
	if (vp->v_pages)
		return (0);

	*statusp = DIRECTIO_SUCCESS;
	error = 0;
	ufs_directio_kstats.logical_reads++;
	while (error == 0 && resid && uio->uio_iovcnt) {
		uoff = uio->uio_loffset;
		iov = uio->uio_iov;
		cnt = (u_int)MIN(iov->iov_len, resid);
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		len = (int)blkroundup(fs, cnt);
		error = bmap_read(ip, uoff, &bn, &len);
		if (error)
			break;
		if (bn == UFS_HOLE) {
			cnt = (u_int)MIN(fs->fs_bsize - (long)blkoff(fs, uoff),
				    cnt);
			ufs_directio_kstats.hole_reads++;
		} else {
			cnt = MIN(cnt, len);
			ufs_directio_kstats.phys_reads++;
		}

		ufs_directio_kstats.nread += cnt;

		phys_iov.iov_base = iov->iov_base;
		phys_iov.iov_len = cnt;

		phys_uio.uio_iov = &phys_iov;
		phys_uio.uio_iovcnt = 1;
		phys_uio.uio_loffset = ldbtob(bn);
		phys_uio.uio_resid = phys_iov.iov_len;
		phys_uio.uio_segflg = uio->uio_segflg;
		phys_uio.uio_fmode = uio->uio_fmode;
		phys_uio.uio_llimit = uio->uio_llimit;
		if (bn == UFS_HOLE) {
			while (error == 0 && phys_uio.uio_resid) {
				nzero = MIN(phys_iov.iov_len,
						ufs_directio_zero_len);
				error = uiomove(ufs_directio_zero_buf, nzero,
						UIO_READ, &phys_uio);
			}
		} else
			error = cdev_read(ip->i_dev, &phys_uio, cr);
		if (phys_uio.uio_resid) {
			if (error == 0)
				error = EIO;
			cnt -= phys_uio.uio_resid;
		}
		iov->iov_len -= cnt;
		iov->iov_base += cnt;
		uio->uio_loffset += cnt;
		uio->uio_resid -= cnt;
		resid -= cnt;
	}
	return (error);
}
