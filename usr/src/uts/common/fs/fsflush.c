/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/* 	Copyright (c) 1993  Sun Microsystems, Inc	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */
#ident	"@(#)fsflush.c	1.37	96/10/17 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/tuneable.h>
#include <sys/inline.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/var.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/swap.h>
#include <sys/vm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/sysinfo.h>
#include <sys/callb.h>

#include <vm/hat.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_kmem.h>

int doiflush = 1;	/* non-zero to turn inode flushing on */
int dopageflush = 1;	/* non-zero to turn page flushing on */

kcondvar_t fsflush_cv;
static kmutex_t fsflush_lock;	/* just for the cv_wait */

/*
 * As part of file system hardening, this daemon is awakened
 * every second to flush cached data which includes the
 * buffer cache, the inode cache and mapped pages.
 */
void
fsflush()
{
	register struct buf *bp, *dwp;
	register struct hbuf *hp;
	register autoup;
	register page_t *pp = page_first();
	unsigned int i, ix, nscan, pcount, icount, count = 0;
	callb_cpr_t cprinfo;
	u_int		bcount;
	kmutex_t	*hmp;

	proc_fsflush = ttoproc(curthread);
	proc_fsflush->p_cstime = 0;
	proc_fsflush->p_stime =  0;
	proc_fsflush->p_cutime =  0;
	proc_fsflush->p_utime = 0;
	bcopy("fsflush", u.u_psargs, 8);
	bcopy("fsflush", u.u_comm, 7);

	mutex_init(&fsflush_lock, "fsflush_lock", MUTEX_DEFAULT, DEFAULT_WT);

	if (v.v_autoup <= 0)
		cmn_err(CE_PANIC, "v.v_autoup <= 0");
	if (tune.t_fsflushr <= 0)
		cmn_err(CE_PANIC, "tune.t_fsflushr <= 0");
	autoup = v.v_autoup * hz;
	icount = v.v_autoup / tune.t_fsflushr;
	CALLB_CPR_INIT(&cprinfo, &fsflush_lock, callb_generic_cpr, "fsflush");
loop:
	mutex_enter(&fsflush_lock);
	CALLB_CPR_SAFE_BEGIN(&cprinfo);
	cv_wait(&fsflush_cv, &fsflush_lock);		/* wait for clock */
	CALLB_CPR_SAFE_END(&cprinfo, &fsflush_lock);
	mutex_exit(&fsflush_lock);

	/*
	 * Put this inside the loop as memory may have been increased
	 * through hotplugging.
	 */
	nscan = (total_pages * (tune.t_fsflushr))/v.v_autoup;

	/*
	 * Write back all old B_DELWRI buffers on the freelist.
	 */
	bcount = 0;
	for (ix = 0; ix < v.v_hbuf; ix++) {

		hp = &hbuf[ix];
		dwp = (struct buf *)&dwbuf[ix];

		bcount += (hp->b_length);

		if (dwp->av_forw == dwp) {
			continue;
		}

		hmp = &hbuf[ix].b_lock;
		mutex_enter(hmp);
		bp = dwp->av_forw;

		/*
		 * Go down only on the delayed write lists.
		 */
		while (bp != dwp) {

			ASSERT(bp->b_flags & B_DELWRI);

			if ((bp->b_flags & B_DELWRI) &&
			    (lbolt - bp->b_start >= autoup) &&
			    sema_tryp(&bp->b_sem)) {
				bp->b_flags |= B_ASYNC;
				hp->b_length--;
				notavail(bp);
				mutex_exit(hmp);
				bwrite(bp);
				mutex_enter(hmp);
				bp = dwp->av_forw;
			} else {
				bp = bp->av_forw;
			}
		}
		mutex_exit(hmp);
	}

	/*
	 *
	 * There is no need to wakeup any thread waiting on bio_mem_cv
	 * since brelse will wake them up as soon as IO is complete.
	 */
	bfreelist.b_bcount = bcount;

	if (!dopageflush)
		goto iflush_out;

	/*
	 * Flush dirty pages.
	 */
	pcount = 0;
	while (pcount++ <= nscan) {
		struct vnode *vp;
		u_offset_t offset;
		int	mod;

		pp = page_next(pp);

		/*
		 * Skip pages associated with the kernel vnode since
		 * they are always "exclusively" locked or pages which are
		 * on the free list.
		 *
		 * NOTE:  These optimizations assume that reads are atomic.
		 */
		if (pp->p_vnode == &kvp || PP_ISFREE(pp) ||
		    !se_nolock(&pp->p_selock))
			continue;

		/*
		 * Reject pages that can't be "exclusively"
		 * locked or are already free.  After we
		 * lock the page, check the free bit again.
		 */
		if (!page_trylock(pp, SE_EXCL)) {
			continue;
		} else if (PP_ISFREE(pp)) {
			page_unlock(pp);
			continue;
		}
		vp = pp->p_vnode;

		/*
		 * Skip the page if it has no vnode or
		 * if it is associated with a swap device.
		 */
		if (vp == NULL || (vp->v_flag & VISSWAP)) {
			page_unlock(pp);
			continue;
		}

		/*
		 * Is this page involved in some I/O? shared?
		 *
		 * The page_struct_lock need not be acquired to
		 * examine these fields since the page has an
		 * "exclusive" lock.
		 */
		if (pp->p_lckcnt != 0 || pp->p_cowcnt != 0) {
			page_unlock(pp);
			continue;
		}

		if (vp->v_type == VCHR) {
			cmn_err(CE_PANIC, "vp->v_type == VCHR");
		}

		/*
		 * Check the modified bit. Leave the bits alone in hardware
		 * (they will be modified if we do the putpage).
		 */
		mod = (hat_pagesync(pp, HAT_SYNC_DONTZERO | HAT_SYNC_STOPON_MOD)
			& P_MOD);

		if (mod) {
			offset = pp->p_offset;
			/*
			 * Hold the vnode before releasing the page lock
			 * to prevent it from being freed and re-used by
			 * some other thread.
			 */
			VN_HOLD(vp);
			page_unlock(pp);

			VOP_PUTPAGE(vp, offset,
			    PAGESIZE, B_ASYNC, kcred);
			VN_RELE(vp);
		} else {
			page_unlock(pp);
		}
	}

iflush_out:
	if (!doiflush)
		goto loop;

	/*
	 * Flush cached attribute information (e.g. inodes).
	 */
	if (count++ >= icount) {
		count = 0;

		/*
		 * Sync back cached data.
		 */
		for (i = 1; i < nfstype; i++) {
			RLOCK_VFSSW();
			if (vfssw[i].vsw_vfsops != NULL)
				(void) (*vfssw[i].vsw_vfsops->vfs_sync)(NULL,
					SYNC_ATTR, kcred);
			RUNLOCK_VFSSW();
		}
	}
	goto loop;
}
