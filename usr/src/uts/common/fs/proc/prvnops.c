/*	Copyright (c) 1984,	 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)prvnops.c 1.78     96/10/03 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/pathname.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/mode.h>
#include <sys/poll.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <fs/fs_subr.h>
#include <vm/rm.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/hat.h>
#include <fs/proc/prdata.h>
#if i386
#include <sys/sysi86.h>
#endif

/*
 * Defined and initialized after all functions have been defined.
 */
extern struct vnodeops prvnodeops;

/*
 * Directory characteristics (patterned after the s5 file system).
 */
#define	PRROOTINO	2

#define	PRDIRSIZE	14
struct prdirect {
	u_short	d_ino;
	char	d_name[PRDIRSIZE];
};

#define	PRSDSIZE	(sizeof (struct prdirect))

/*
 * Directory characteristics.
 */
typedef struct prdirent {
	ino64_t		d_ino;		/* "inode number" of entry */
	off64_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[14];	/* name of file */
} prdirent_t;

/*
 * Contents of a /proc/<pid> directory.
 * Reuse d_ino field for the /proc file type.
 */
static prdirent_t piddir[] = {
	{ PR_PIDDIR,	 1 * sizeof (prdirent_t), sizeof (prdirent_t),
		"." },
	{ PR_PROCDIR,	 2 * sizeof (prdirent_t), sizeof (prdirent_t),
		".." },
	{ PR_AS,	 3 * sizeof (prdirent_t), sizeof (prdirent_t),
		"as" },
	{ PR_CTL,	 4 * sizeof (prdirent_t), sizeof (prdirent_t),
		"ctl" },
	{ PR_STATUS,	 5 * sizeof (prdirent_t), sizeof (prdirent_t),
		"status" },
	{ PR_LSTATUS,	 6 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lstatus" },
	{ PR_PSINFO,	 7 * sizeof (prdirent_t), sizeof (prdirent_t),
		"psinfo" },
	{ PR_LPSINFO,	 8 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lpsinfo" },
	{ PR_MAP,	 9 * sizeof (prdirent_t), sizeof (prdirent_t),
		"map" },
	{ PR_RMAP,	10 * sizeof (prdirent_t), sizeof (prdirent_t),
		"rmap" },
	{ PR_CRED,	11 * sizeof (prdirent_t), sizeof (prdirent_t),
		"cred" },
	{ PR_SIGACT,	12 * sizeof (prdirent_t), sizeof (prdirent_t),
		"sigact" },
	{ PR_AUXV,	13 * sizeof (prdirent_t), sizeof (prdirent_t),
		"auxv" },
	{ PR_USAGE,	14 * sizeof (prdirent_t), sizeof (prdirent_t),
		"usage" },
	{ PR_LUSAGE,	15 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lusage" },
	{ PR_PAGEDATA,	16 * sizeof (prdirent_t), sizeof (prdirent_t),
		"pagedata" },
	{ PR_WATCH,	17 * sizeof (prdirent_t), sizeof (prdirent_t),
		"watch" },
	{ PR_OBJECTDIR,	18 * sizeof (prdirent_t), sizeof (prdirent_t),
		"object" },
	{ PR_LWPDIR,	19 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lwp" },
#if defined(i386) || defined(__i386)
	{ PR_LDT,	20 * sizeof (prdirent_t), sizeof (prdirent_t),
		"ldt" },
#endif
};

#define	NPIDDIRFILES	(sizeof (piddir) / sizeof (piddir[0]) - 2)

/*
 * Contents of a /proc/<pid>/lwp/<lwpid> directory.
 */
static prdirent_t lwpiddir[] = {
	{ PR_LWPIDDIR,	 1 * sizeof (prdirent_t), sizeof (prdirent_t),
		"." },
	{ PR_LWPDIR,	 2 * sizeof (prdirent_t), sizeof (prdirent_t),
		".." },
	{ PR_LWPCTL,	 3 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lwpctl" },
	{ PR_LWPSTATUS,	 4 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lwpstatus" },
	{ PR_LWPSINFO,	 5 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lwpsinfo" },
	{ PR_LWPUSAGE,	 6 * sizeof (prdirent_t), sizeof (prdirent_t),
		"lwpusage" },
	{ PR_XREGS,	 7 * sizeof (prdirent_t), sizeof (prdirent_t),
		"xregs" },
#if defined(sparc) || defined(__sparc)
	{ PR_GWINDOWS,	 8 * sizeof (prdirent_t), sizeof (prdirent_t),
		"gwindows" },
#endif
};

#define	NLWPIDDIRFILES	(sizeof (lwpiddir) / sizeof (lwpiddir[0]) - 2)

/*
 * Span of entries in the array files (lstatus, lpsinfo, pusage).
 * We make the span larger than the size of the structure on purpose,
 * to make sure that programs cannot use the structure size by mistake.
 */
#define	LSPAN(type)	(round8(sizeof (type)) + 8)

static void rebuild_objdir(struct as *);

static int
propen(vnode_t **vpp, int flag, struct cred *cr)
{
	vnode_t *vp = *vpp;
	prnode_t *pnp = VTOP(vp);
	prcommon_t *pcp = pnp->pr_pcommon;
	proc_t *p;
	int error = 0;
	prnode_t *npnp = NULL;

	/*
	 * Nothing to do for the /proc directory itself.
	 */
	if (pnp->pr_type == PR_PROCDIR)
		return (0);

	/*
	 * If we are opening an underlying mapped object, reject opens
	 * for writing regardless of the objects's access modes.
	 */
	if (pnp->pr_type == PR_OBJECT) {
		vnode_t *rvp = pnp->pr_object;

		ASSERT(rvp != NULL);
		error = (flag & FWRITE)? EACCES : VOP_OPEN(&rvp, flag, cr);
		if (error == 0) {
			*vpp = rvp;
			pnp->pr_object = NULL;
			VN_RELE(vp);
		}
		return (error);
	}

	/*
	 * If we are opening the pagedata file, allocate a prnode now
	 * to avoid calling kmem_alloc() while holding p->p_lock.
	 */
	if (pnp->pr_type == PR_PAGEDATA || pnp->pr_type == PR_OPAGEDATA)
		npnp = prgetnode(pnp->pr_type);

	/*
	 * If the process exists, lock it now.
	 * Otherwise we have a race condition with prclose().
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL) {
		if (npnp != NULL)
			prfreenode(npnp);
		return (ENOENT);
	}
	ASSERT(p == pcp->prc_proc);
	ASSERT(p->p_flag & SPRLOCK);

	/*
	 * Maintain a count of opens for write.  Allow exactly one
	 * O_WRITE|O_EXCL request and fail subsequent ones.
	 * Don't fail opens of old (bletch!) /proc lwp files.
	 */
	if (flag & FWRITE) {
		if (pnp->pr_type == PR_LWPIDFILE)
			/* EMPTY */;
		else if (flag & FEXCL) {
			if (pcp->prc_writers > 0) {
				error = EBUSY;
				goto out;
			}
			/* semantic for old /proc interface */
			if (pnp->pr_type == PR_PIDFILE)
				pcp->prc_flags |= PRC_EXCL;
		} else if (pcp->prc_flags & PRC_EXCL) {
			ASSERT(pcp->prc_writers > 0);
			if (cr->cr_uid) {
				error = EBUSY;
				goto out;
			}
		}
		pcp->prc_writers++;
	}

	/*
	 * Keep a count of opens so that we can identify the last close.
	 */
	pcp->prc_opens++;

	/*
	 * Do file-specific things.
	 */
	switch (pnp->pr_type) {
	default:
		break;
	case PR_PAGEDATA:
	case PR_OPAGEDATA:
		/*
		 * Enable data collection for page data file;
		 * get unique id from the hat layer.
		 */
		{
			int id;

			/*
			 * Drop p->p_lock to call hat_startstat()
			 */
			mutex_exit(&p->p_lock);
			if ((p->p_flag & SSYS) || p->p_as == &kas ||
			    (id = hat_startstat(p->p_as)) == -1) {
				mutex_enter(&p->p_lock);
				error = ENOMEM;
			} else if (pnp->pr_hatid == 0) {
				mutex_enter(&p->p_lock);
				pnp->pr_hatid = (u_int)id;
			} else {
				mutex_enter(&p->p_lock);
				/*
				 * Use our newly allocated prnode.
				 */
				npnp->pr_hatid = (u_int)id;
				/*
				 * prgetnode() initialized most of the prnode.
				 * Duplicate the remainder.
				 */
				npnp->pr_ino = pnp->pr_ino;
				npnp->pr_common = pnp->pr_common;
				npnp->pr_pcommon = pnp->pr_pcommon;
				npnp->pr_parent = pnp->pr_parent;
				VN_HOLD(npnp->pr_parent);
				npnp->pr_index = pnp->pr_index;

				npnp->pr_next = p->p_plist;
				p->p_plist = PTOV(npnp);

				VN_RELE(PTOV(pnp));
				pnp = npnp;
				npnp = NULL;
				*vpp = PTOV(pnp);
			}
		}
		break;
	}

out:
	prunlock(pnp);

	if (npnp != NULL)
		prfreenode(npnp);
	return (error);
}

/* ARGSUSED */
static int
prclose(vnode_t *vp, int flag, int count, offset_t offset, struct cred *cr)
{
	prnode_t *pnp = VTOP(vp);
	prcommon_t *pcp = pnp->pr_pcommon;
	proc_t *p;
	kthread_t *t;
	vnode_t *xvp;
	user_t *up;
	int writers;

	/*
	 * Nothing to do for the /proc directory itself.
	 */
	if (pnp->pr_type == PR_PROCDIR)
		return (0);

	ASSERT(pnp->pr_type != PR_OBJECT);

	/*
	 * There is nothing more to do until the last close
	 * of the file table entry.
	 */
	if (count > 1)
		return (0);

	/*
	 * If the process exists, lock it now.
	 * Otherwise we have a race condition with propen().
	 * Hold pr_pidlock across the reference to pr_opens,
	 * and pr_writers in case there is no process anymore,
	 * to cover the case of concurrent calls to prclose()
	 * after the process has been reaped by freeproc().
	 */
	p = pr_p_lock(pnp);

	/*
	 * Decrement the count of opens and the count of opens for writing.
	 */
	--pcp->prc_opens;
	if ((flag & FWRITE) && --pcp->prc_writers == 0)
		pcp->prc_flags &= ~PRC_EXCL;
	mutex_exit(&pr_pidlock);

	/*
	 * If there is no process, there is nothing more to do.
	 */
	if (p == NULL)
		return (0);
	ASSERT(p == pcp->prc_proc);

	/*
	 * Do file-specific things.
	 */
	switch (pnp->pr_type) {
	default:
		break;
	case PR_PAGEDATA:
	case PR_OPAGEDATA:
		/*
		 * This is a page data file.
		 * Free the hat level statistics.
		 * Drop p->p_lock before calling hat_freestat().
		 */
		mutex_exit(&p->p_lock);
		if (p->p_as != &kas && pnp->pr_hatid != 0)
			hat_freestat(p->p_as, pnp->pr_hatid);
		mutex_enter(&p->p_lock);
		pnp->pr_hatid = 0;
		break;
	}

	/*
	 * Look through all the valid and invalid vnodes to
	 * determine if there are any outstanding writers.
	 */
	writers = 0;
	xvp = p->p_trace;
	do {
		writers += VTOP(xvp)->pr_common->prc_writers;
	} while ((xvp = VTOP(xvp)->pr_next) != NULL);

	/*
	 * On last close of all writable file descriptors,
	 * perform run-on-last-close and/or kill-on-last-close logic.
	 */
	if (writers == 0 &&
	    !(pcp->prc_flags & PRC_DESTROY) &&
	    p->p_stat != SZOMB &&
	    (p->p_flag & (SRUNLCL|SKILLCL))) {
		int killproc;

		/*
		 * Cancel any watchpoints currently in effect.
		 * The process might disappear during this operation.
		 */
		if (pr_cancel_watch(pnp) == NULL)
			return (0);
		/*
		 * If any tracing flags are set, clear them.
		 */
		if (p->p_flag & SPROCTR) {
			up = prumap(p);
			premptyset(&up->u_entrymask);
			premptyset(&up->u_exitmask);
			up->u_systrap = 0;
			prunmap(p);
		}
		premptyset(&p->p_sigmask);
		premptyset(&p->p_fltmask);
		killproc = (p->p_flag & SKILLCL);
		p->p_flag &= ~(SRUNLCL|SKILLCL|SPROCTR);
		/*
		 * Cancel any outstanding single-step requests.
		 */
		if ((t = p->p_tlist) != NULL) {
			/*
			 * Drop p_lock because prnostep() touches the stack.
			 * The loop is safe because the process is SPRLOCK'd.
			 */
			mutex_exit(&p->p_lock);
			do {
				prnostep(ttolwp(t));
			} while ((t = t->t_forw) != p->p_tlist);
			mutex_enter(&p->p_lock);
		}
		/*
		 * Set runnable all lwps stopped by /proc.
		 */
		if (killproc)
			sigtoproc(p, NULL, SIGKILL, 0);
		else
			allsetrun(p);
	}

	prunlock(pnp);
	return (0);
}

/*
 * Array of read functions, indexed by /proc file type.
 */
static int pr_read_inval(), pr_read_as(), pr_read_status(),
	pr_read_lstatus(), pr_read_psinfo(), pr_read_lpsinfo(),
	pr_read_map(), pr_read_rmap(), pr_read_cred(),
	pr_read_sigact(), pr_read_auxv(),
#if defined(i386) || defined(__i386)
	pr_read_ldt(),
#endif
	pr_read_usage(), pr_read_lusage(), pr_read_pagedata(),
	pr_read_watch(), pr_read_lwpstatus(), pr_read_lwpsinfo(),
	pr_read_lwpusage(), pr_read_xregs(),
#if defined(sparc) || defined(__sparc)
	pr_read_gwindows(),
#endif
	pr_read_piddir(), pr_read_pidfile(), pr_read_opagedata();

static int (*pr_read_function[PR_NFILES])() = {
	pr_read_inval,		/* /proc				*/
	pr_read_piddir,		/* /proc/<pid> (old /proc read())	*/
	pr_read_as,		/* /proc/<pid>/as			*/
	pr_read_inval,		/* /proc/<pid>/ctl			*/
	pr_read_status,		/* /proc/<pid>/status			*/
	pr_read_lstatus,	/* /proc/<pid>/lstatus			*/
	pr_read_psinfo,		/* /proc/<pid>/psinfo			*/
	pr_read_lpsinfo,	/* /proc/<pid>/lpsinfo			*/
	pr_read_map,		/* /proc/<pid>/map			*/
	pr_read_rmap,		/* /proc/<pid>/rmap			*/
	pr_read_cred,		/* /proc/<pid>/cred			*/
	pr_read_sigact,		/* /proc/<pid>/sigact			*/
	pr_read_auxv,		/* /proc/<pid>/auxv			*/
#if defined(i386) || defined(__i386)
	pr_read_ldt,		/* /proc/<pid>/ldt			*/
#endif
	pr_read_usage,		/* /proc/<pid>/usage			*/
	pr_read_lusage,		/* /proc/<pid>/lusage			*/
	pr_read_pagedata,	/* /proc/<pid>/pagedata			*/
	pr_read_watch,		/* /proc/<pid>/watch			*/
	pr_read_inval,		/* /proc/<pid>/object			*/
	pr_read_inval,		/* /proc/<pid>/object/xxx		*/
	pr_read_inval,		/* /proc/<pid>/lwp			*/
	pr_read_inval,		/* /proc/<pid>/lwp/<lwpid>		*/
	pr_read_inval,		/* /proc/<pid>/lwp/<lwpid>/lwpctl	*/
	pr_read_lwpstatus,	/* /proc/<pid>/lwp/<lwpid>/lwpstatus	*/
	pr_read_lwpsinfo,	/* /proc/<pid>/lwp/<lwpid>/lwpsinfo	*/
	pr_read_lwpusage,	/* /proc/<pid>/lwp/<lwpid>/lwpusage	*/
	pr_read_xregs,		/* /proc/<pid>/lwp/<lwpid>/xregs	*/
#if defined(sparc) || defined(__sparc)
	pr_read_gwindows,	/* /proc/<pid>/lwp/<lwpid>/gwindows	*/
#endif
	pr_read_pidfile,	/* old process file			*/
	pr_read_pidfile,	/* old lwp file				*/
	pr_read_opagedata,	/* old pagedata file			*/
};

/* ARGSUSED */
static int
prread(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
	prnode_t *pnp = VTOP(vp);

	ASSERT(pnp->pr_type < PR_NFILES);

	return (pr_read_function[pnp->pr_type](pnp, uiop));
}

/* ARGSUSED */
static int
pr_read_inval(prnode_t *pnp, struct uio *uiop)
{
	/*
	 * No read() on any /proc directory, use getdents(2) instead.
	 * Cannot read a control file either.
	 * An underlying mapped object file cannot get here.
	 */
	return (EINVAL);
}

/* ARGSUSED */
static int
pr_read_as(prnode_t *pnp, struct uio *uiop)
{
	int error;

	ASSERT(pnp->pr_type == PR_AS);

	if ((error = prlock(pnp, ZNO)) == 0) {
		proc_t *p = pnp->pr_common->prc_proc;
		struct as *as = p->p_as;

		/*
		 * /proc I/O cannot be done to a system process.
		 */
		if (!(p->p_flag & SSYS) && as != &kas) {
			/*
			 * We don't hold p_lock over an i/o operation because
			 * that could lead to deadlock with the clock thread.
			 */
			mutex_exit(&p->p_lock);
			error = prusrio(as, UIO_READ, uiop, 0);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
	}

	return (error);
}

/* ARGSUSED */
static int
pr_read_status(prnode_t *pnp, struct uio *uiop)
{
	pstatus_t *sp;
	int error;
	long count;

	ASSERT(pnp->pr_type == PR_STATUS);

	/*
	 * We kmem_alloc() the pstatus structure because
	 * it is so big it might blow the kernel stack.
	 */
	sp = kmem_alloc(sizeof (*sp), KM_SLEEP);
	if ((error = prlock(pnp, ZNO)) == 0) {
		prgetstatus(pnp->pr_common->prc_proc, sp);
		prunlock(pnp);
		count = sizeof (*sp) - uiop->uio_offset;
		if (count > 0)
			error = uiomove((char *)sp + uiop->uio_offset,
				count, UIO_READ, uiop);
	}
	kmem_free((caddr_t)sp, sizeof (*sp));
	return (error);
}

/* ARGSUSED */
static int
pr_read_lstatus(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	kthread_t *t;
	u_int size;
	prheader_t *php;
	lwpstatus_t *sp;
	int error;
	long count;
	int nlwp;

	ASSERT(pnp->pr_type == PR_LSTATUS);

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);
	p = pnp->pr_common->prc_proc;
	nlwp = p->p_lwpcnt;
	size = sizeof (prheader_t) + nlwp * LSPAN(lwpstatus_t);

	/* drop p->p_lock to do kmem_alloc(KM_SLEEP) */
	mutex_exit(&p->p_lock);
	php = kmem_zalloc(size, KM_SLEEP);
	mutex_enter(&p->p_lock);
	/* p->p_lwpcnt can't change while process is locked */
	ASSERT(nlwp == p->p_lwpcnt);

	php->pr_nent = nlwp;
	php->pr_entsize = LSPAN(lwpstatus_t);

	sp = (lwpstatus_t *)(php + 1);
	t = p->p_tlist;
	do {
		prgetlwpstatus(t, sp);
		sp = (lwpstatus_t *)((caddr_t)sp + LSPAN(lwpstatus_t));
	} while ((t = t->t_forw) != p->p_tlist);
	prunlock(pnp);

	count = size - uiop->uio_offset;
	if (count > 0)
		error = uiomove((caddr_t)php + uiop->uio_offset,
			count, UIO_READ, uiop);
	kmem_free((caddr_t)php, size);
	return (error);
}

/* ARGSUSED */
static int
pr_read_psinfo(prnode_t *pnp, struct uio *uiop)
{
	psinfo_t psinfo;
	proc_t *p;
	int error = 0;
	int count;

	ASSERT(pnp->pr_type == PR_PSINFO);

	/*
	 * We don't want the full treatment of prlock(pnp) here.
	 * This file is world-readable and never goes invalid.
	 * It doesn't matter if we are in the middle of an exec().
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		error = ENOENT;
	else {
		ASSERT(p == pnp->pr_common->prc_proc);
		prgetpsinfo(p, &psinfo);
		prunlock(pnp);
		if ((count = sizeof (psinfo) - uiop->uio_offset) > 0)
			error = uiomove((char *)&psinfo + uiop->uio_offset,
				count, UIO_READ, uiop);
	}
	return (error);
}

/* ARGSUSED */
static int
pr_read_lpsinfo(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	kthread_t *t;
	u_int size;
	prheader_t *php;
	lwpsinfo_t *sp;
	int error = 0;
	long count;
	int nlwp;

	ASSERT(pnp->pr_type == PR_LPSINFO);

	/*
	 * We don't want the full treatment of prlock(pnp) here.
	 * This file is world-readable and never goes invalid.
	 * It doesn't matter if we are in the middle of an exec().
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		return (ENOENT);
	ASSERT(p == pnp->pr_common->prc_proc);
	if ((nlwp = p->p_lwpcnt) == 0) {
		prunlock(pnp);
		return (ENOENT);
	}
	size = sizeof (prheader_t) + nlwp * LSPAN(lwpsinfo_t);

	/* drop p->p_lock to do kmem_alloc(KM_SLEEP) */
	mutex_exit(&p->p_lock);
	php = kmem_zalloc(size, KM_SLEEP);
	mutex_enter(&p->p_lock);
	/* p->p_lwpcnt can't change while process is locked */
	ASSERT(nlwp == p->p_lwpcnt);

	php->pr_nent = nlwp;
	php->pr_entsize = LSPAN(lwpsinfo_t);

	sp = (lwpsinfo_t *)(php + 1);
	t = p->p_tlist;
	do {
		prgetlwpsinfo(t, sp);
		sp = (lwpsinfo_t *)((caddr_t)sp + LSPAN(lwpsinfo_t));
	} while ((t = t->t_forw) != p->p_tlist);
	prunlock(pnp);

	count = size - uiop->uio_offset;
	if (count > 0)
		error = uiomove((caddr_t)php + uiop->uio_offset,
			count, UIO_READ, uiop);
	kmem_free((caddr_t)php, size);
	return (error);
}

static int
pr_read_map_common(prnode_t *pnp, struct uio *uiop, int reserved)
{
	proc_t *p;
	struct as *as;
	int nmaps;
	prmap_t *prmapp;
	size_t size;
	int error;
	char *start;
	int count;

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);

	p = pnp->pr_common->prc_proc;
	as = p->p_as;

	if ((p->p_flag & SSYS) || as == &kas) {
		prunlock(pnp);
		return (0);
	}

	mutex_exit(&p->p_lock);
	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
	nmaps = prgetmap(p, reserved, &prmapp, &size);
	AS_LOCK_EXIT(as, &as->a_lock);
	mutex_enter(&p->p_lock);
	prunlock(pnp);

	if ((unsigned)uiop->uio_offset < nmaps * sizeof (prmap_t)) {
		start = (char *)prmapp + uiop->uio_offset;
		count = nmaps * sizeof (prmap_t) - uiop->uio_offset;
		error = uiomove(start, count, UIO_READ, uiop);
	}

	kmem_free(prmapp, size);
	return (error);
}

static int
pr_read_map(prnode_t *pnp, struct uio *uiop)
{
	ASSERT(pnp->pr_type == PR_MAP);
	return (pr_read_map_common(pnp, uiop, 0));
}

static int
pr_read_rmap(prnode_t *pnp, struct uio *uiop)
{
	ASSERT(pnp->pr_type == PR_RMAP);
	return (pr_read_map_common(pnp, uiop, 1));
}

/* ARGSUSED */
static int
pr_read_cred(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	struct cred *cp;
	prcred_t *pcrp;
	int error;
	int i;
	char *start;
	int count;
	int ngroups;

	ASSERT(pnp->pr_type == PR_CRED);

	/*
	 * We kmem_alloc() the prcred_t structure because
	 * the number of supplementary groups is variable.
	 */
	pcrp = (prcred_t *)
	    kmem_alloc(sizeof (prcred_t) + sizeof (gid_t) * (ngroups_max - 1),
	    KM_SLEEP);

	if ((error = prlock(pnp, ZNO)) != 0)
		goto out;
	p = pnp->pr_common->prc_proc;
	ASSERT(p != NULL);

	mutex_enter(&p->p_crlock);
	cp = p->p_cred;
	pcrp->pr_euid = cp->cr_uid;
	pcrp->pr_ruid = cp->cr_ruid;
	pcrp->pr_suid = cp->cr_suid;
	pcrp->pr_egid = cp->cr_gid;
	pcrp->pr_rgid = cp->cr_rgid;
	pcrp->pr_sgid = cp->cr_sgid;
	pcrp->pr_ngroups = ngroups = umin(cp->cr_ngroups, ngroups_max);
	pcrp->pr_groups[0] = 0;		/* in case ngroups == 0 */
	for (i = 0; i < ngroups; i++)
		pcrp->pr_groups[i] = cp->cr_groups[i];
	mutex_exit(&p->p_crlock);
	prunlock(pnp);

	count = sizeof (prcred_t);
	if (ngroups > 1)
		count += sizeof (gid_t) * (ngroups - 1);
	if ((unsigned)uiop->uio_offset < count) {
		start = (char *)pcrp + uiop->uio_offset;
		count = count - uiop->uio_offset;
		error = uiomove(start, count, UIO_READ, uiop);
	}
out:
	kmem_free((caddr_t)pcrp,
	    sizeof (prcred_t) + sizeof (gid_t) * (ngroups_max - 1));
	return (error);
}

/* ARGSUSED */
static int
pr_read_sigact(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	struct sigaction *sap;
	int sig;
	int error;
	user_t *up;
	char *start;
	int count;

	ASSERT(pnp->pr_type == PR_SIGACT);

	/*
	 * We kmem_alloc() the sigaction array because
	 * it is so big it might blow the kernel stack.
	 */
	sap = (struct sigaction *)
	    kmem_alloc((NSIG-1) * sizeof (struct sigaction), KM_SLEEP);

	if ((error = prlock(pnp, ZNO)) != 0)
		goto out;
	p = pnp->pr_common->prc_proc;
	ASSERT(p != NULL);

	if ((unsigned)uiop->uio_offset >= (NSIG-1)*sizeof (struct sigaction)) {
		prunlock(pnp);
		goto out;
	}

	up = prumap(p);
	for (sig = 1; sig < NSIG; sig++)
		prgetaction(p, up, sig, &sap[sig-1]);
	prunmap(p);
	prunlock(pnp);

	start = (char *)sap + uiop->uio_offset;
	count = (NSIG-1) * sizeof (struct sigaction) - uiop->uio_offset;
	error = uiomove(start, count, UIO_READ, uiop);
out:
	kmem_free((caddr_t)sap, (NSIG-1) * sizeof (struct sigaction));
	return (error);
}

/* ARGSUSED */
static int
pr_read_auxv(prnode_t *pnp, struct uio *uiop)
{
	auxv_t auxv[NUM_AUX_VECTORS];
	proc_t *p;
	user_t *up;
	int error;
	char *start;
	int count;

	ASSERT(pnp->pr_type == PR_AUXV);

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);

	if ((unsigned)uiop->uio_offset >= sizeof (auxv)) {
		prunlock(pnp);
		return (0);
	}

	p = pnp->pr_common->prc_proc;
	up = prumap(p);
	bcopy((caddr_t)up->u_auxv, (caddr_t)auxv, sizeof (auxv));
	prunmap(p);
	prunlock(pnp);

	start = (char *)auxv + uiop->uio_offset;
	count = sizeof (auxv) - uiop->uio_offset;
	return (uiomove(start, count, UIO_READ, uiop));
}

#if defined(i386) || defined(__i386)
/* ARGSUSED */
static int
pr_read_ldt(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	struct ssd *ssd;
	u_int size;
	int error;
	char *start;
	int count;

	ASSERT(pnp->pr_type == PR_LDT);

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);
	p = pnp->pr_common->prc_proc;

	mutex_exit(&p->p_lock);
	mutex_enter(&p->p_ldtlock);
	size = prnldt(p) * sizeof (struct ssd);
	if ((unsigned)uiop->uio_offset >= size) {
		mutex_exit(&p->p_ldtlock);
		mutex_enter(&p->p_lock);
		prunlock(pnp);
		return (0);
	}

	ssd = kmem_alloc(size, KM_SLEEP);
	prgetldt(p, ssd);
	mutex_exit(&p->p_ldtlock);
	mutex_enter(&p->p_lock);
	prunlock(pnp);

	start = (char *)ssd + uiop->uio_offset;
	count = size - uiop->uio_offset;
	error = uiomove(start, count, UIO_READ, uiop);

	kmem_free((caddr_t)ssd, size);
	return (error);
}
#endif	/* i386 */

/* ARGSUSED */
static int
pr_read_usage(prnode_t *pnp, struct uio *uiop)
{
	prhusage_t prhusage;
	prhusage_t *pup = &prhusage;
	proc_t *p;
	kthread_t *t;
	char *start;
	int count;
	int was_disabled;

	ASSERT(pnp->pr_type == PR_USAGE);

	/*
	 * We don't want the full treatment of prlock(pnp) here.
	 * This file is world-readable and never goes invalid.
	 * It doesn't matter if we are in the middle of an exec().
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		return (ENOENT);
	ASSERT(p == pnp->pr_common->prc_proc);

	if ((unsigned)uiop->uio_offset >= sizeof (prhusage)) {
		prunlock(pnp);
		return (0);
	}

	was_disabled = !(p->p_flag & SMSACCT);

	bzero((caddr_t)pup, sizeof (prhusage_t));
	pup->pr_tstamp = gethrtime();

	pup->pr_count  = p->p_defunct;
	pup->pr_create = p->p_mstart;
	pup->pr_term   = p->p_mterm;

	pup->pr_rtime    = p->p_mlreal;
	pup->pr_utime    = p->p_acct[LMS_USER];
	pup->pr_stime    = p->p_acct[LMS_SYSTEM];
	pup->pr_ttime    = p->p_acct[LMS_TRAP];
	pup->pr_tftime   = p->p_acct[LMS_TFAULT];
	pup->pr_dftime   = p->p_acct[LMS_DFAULT];
	pup->pr_kftime   = p->p_acct[LMS_KFAULT];
	pup->pr_ltime    = p->p_acct[LMS_USER_LOCK];
	pup->pr_slptime  = p->p_acct[LMS_SLEEP];
	pup->pr_wtime    = p->p_acct[LMS_WAIT_CPU];
	pup->pr_stoptime = p->p_acct[LMS_STOPPED];

	pup->pr_minf  = p->p_ru.minflt;
	pup->pr_majf  = p->p_ru.majflt;
	pup->pr_nswap = p->p_ru.nswap;
	pup->pr_inblk = p->p_ru.inblock;
	pup->pr_oublk = p->p_ru.oublock;
	pup->pr_msnd  = p->p_ru.msgsnd;
	pup->pr_mrcv  = p->p_ru.msgrcv;
	pup->pr_sigs  = p->p_ru.nsignals;
	pup->pr_vctx  = p->p_ru.nvcsw;
	pup->pr_ictx  = p->p_ru.nivcsw;
	pup->pr_sysc  = p->p_ru.sysc;
	pup->pr_ioch  = p->p_ru.ioch;

	/*
	 * Add the usage information for each active lwp.
	 */
	if ((t = p->p_tlist) != NULL &&
	    !(pnp->pr_pcommon->prc_flags & PRC_DESTROY)) {
		do {
			if (t->t_proc_flag & TP_LWPEXIT)
				continue;
			pup->pr_count++;
			praddusage(t, pup);
		} while ((t = t->t_forw) != p->p_tlist);
	}

	/* if microstate accounting was disabled before, disabled it again */
	if (was_disabled)
		disable_msacct(p);
	prunlock(pnp);

	prcvtusage(&prhusage);
	start = (char *)&prhusage + uiop->uio_offset;
	count = sizeof (prhusage) - uiop->uio_offset;
	return (uiomove(start, count, UIO_READ, uiop));
}

/* ARGSUSED */
static int
pr_read_lusage(prnode_t *pnp, struct uio *uiop)
{
	int nlwp;
	prheader_t *php;
	prhusage_t *pup;
	u_int size;
	hrtime_t curtime;
	proc_t *p;
	kthread_t *t;
	int error;
	char *start;
	int count;
	int was_disabled;

	ASSERT(pnp->pr_type == PR_LUSAGE);

	/*
	 * We don't want the full treatment of prlock(pnp) here.
	 * This file is world-readable and never goes invalid.
	 * It doesn't matter if we are in the middle of an exec().
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		return (ENOENT);
	ASSERT(p == pnp->pr_common->prc_proc);
	if ((nlwp = p->p_lwpcnt) == 0) {
		prunlock(pnp);
		return (ENOENT);
	}

	was_disabled = !(p->p_flag & SMSACCT);
	size = sizeof (prheader_t) + (nlwp + 1) * LSPAN(prhusage_t);
	if ((unsigned)uiop->uio_offset >= size) {
		prunlock(pnp);
		return (0);
	}

	/* drop p->p_lock to do kmem_alloc(KM_SLEEP) */
	mutex_exit(&p->p_lock);
	php = kmem_zalloc(size, KM_SLEEP);
	mutex_enter(&p->p_lock);
	/* p->p_lwpcnt can't change while process is locked */
	ASSERT(nlwp == p->p_lwpcnt);

	php->pr_nent = nlwp + 1;
	php->pr_entsize = LSPAN(prhusage_t);

	curtime = gethrtime();

	/*
	 * First the summation over defunct lwps.
	 */
	pup = (prhusage_t *)(php + 1);
	pup->pr_count  = p->p_defunct;
	pup->pr_tstamp = curtime;
	pup->pr_create = p->p_mstart;
	pup->pr_term   = p->p_mterm;

	pup->pr_rtime    = p->p_mlreal;
	pup->pr_utime    = p->p_acct[LMS_USER];
	pup->pr_stime    = p->p_acct[LMS_SYSTEM];
	pup->pr_ttime    = p->p_acct[LMS_TRAP];
	pup->pr_tftime   = p->p_acct[LMS_TFAULT];
	pup->pr_dftime   = p->p_acct[LMS_DFAULT];
	pup->pr_kftime   = p->p_acct[LMS_KFAULT];
	pup->pr_ltime    = p->p_acct[LMS_USER_LOCK];
	pup->pr_slptime  = p->p_acct[LMS_SLEEP];
	pup->pr_wtime    = p->p_acct[LMS_WAIT_CPU];
	pup->pr_stoptime = p->p_acct[LMS_STOPPED];

	pup->pr_minf  = p->p_ru.minflt;
	pup->pr_majf  = p->p_ru.majflt;
	pup->pr_nswap = p->p_ru.nswap;
	pup->pr_inblk = p->p_ru.inblock;
	pup->pr_oublk = p->p_ru.oublock;
	pup->pr_msnd  = p->p_ru.msgsnd;
	pup->pr_mrcv  = p->p_ru.msgrcv;
	pup->pr_sigs  = p->p_ru.nsignals;
	pup->pr_vctx  = p->p_ru.nvcsw;
	pup->pr_ictx  = p->p_ru.nivcsw;
	pup->pr_sysc  = p->p_ru.sysc;
	pup->pr_ioch  = p->p_ru.ioch;

	prcvtusage(pup);

	/*
	 * Fill one prusage struct for each active lwp.
	 */
	if ((t = p->p_tlist) != NULL &&
	    !(pnp->pr_pcommon->prc_flags & PRC_DESTROY)) {
		do {
			ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
			ASSERT(nlwp > 0);
			--nlwp;
			pup = (prhusage_t *)((caddr_t)pup + LSPAN(prhusage_t));
			pup->pr_tstamp = curtime;
			prgetusage(t, pup);
			prcvtusage(pup);
		} while ((t = t->t_forw) != p->p_tlist);
	}
	ASSERT(nlwp == 0);

	/* if microstate accounting was disabled before, disabled it again */
	if (was_disabled)
		disable_msacct(p);
	prunlock(pnp);

	start = (caddr_t)php + uiop->uio_offset;
	count = size - uiop->uio_offset;
	error = uiomove(start, count, UIO_READ, uiop);
	kmem_free(php, size);
	return (error);
}

/* ARGSUSED */
static int
pr_read_pagedata(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	struct as *as;
	vnode_t *exec;
	int error;

	ASSERT(pnp->pr_type == PR_PAGEDATA);

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);

	p = pnp->pr_common->prc_proc;
	as = p->p_as;
	exec = p->p_exec;
	if ((p->p_flag & SSYS) || as == &kas) {
		prunlock(pnp);
		return (0);
	}

	mutex_exit(&p->p_lock);
	error = prpdread(as, exec, pnp->pr_hatid, uiop);
	mutex_enter(&p->p_lock);

	prunlock(pnp);
	return (error);
}

/* ARGSUSED */
static int
pr_read_opagedata(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	struct as *as;
	int error;

	ASSERT(pnp->pr_type == PR_OPAGEDATA);

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);

	p = pnp->pr_common->prc_proc;
	as = p->p_as;
	if ((p->p_flag & SSYS) || as == &kas) {
		prunlock(pnp);
		return (0);
	}

	mutex_exit(&p->p_lock);
	error = oprpdread(as, pnp->pr_hatid, uiop);
	mutex_enter(&p->p_lock);

	prunlock(pnp);
	return (error);
}

/* ARGSUSED */
static int
pr_read_watch(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	int error;
	char *start;
	int count;
	prwatch_t *Bpwp;
	u_int size;
	prwatch_t *pwp;
	int nwarea;
	struct watched_area *pwarea;

	ASSERT(pnp->pr_type == PR_WATCH);

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);

	p = pnp->pr_common->prc_proc;
	nwarea = p->p_nwarea;
	size = nwarea * sizeof (prwatch_t);
	if ((unsigned)uiop->uio_offset >= size) {
		prunlock(pnp);
		return (0);
	}

	/* drop p->p_lock to do kmem_alloc(KM_SLEEP) */
	mutex_exit(&p->p_lock);
	Bpwp = pwp = (prwatch_t *)kmem_zalloc(size, KM_SLEEP);
	mutex_enter(&p->p_lock);
	/* p->p_nwarea can't change while process is locked */
	ASSERT(nwarea == p->p_nwarea);

	/* gather the watched areas */
	for (pwarea = p->p_warea; nwarea != 0;
	    pwarea = pwarea->wa_forw, pwp++, nwarea--) {
		pwp->pr_vaddr = (uintptr_t)pwarea->wa_vaddr;
		pwp->pr_size = pwarea->wa_eaddr - pwarea->wa_vaddr;
		pwp->pr_wflags = pwarea->wa_flags;
	}

	prunlock(pnp);

	start = (char *)Bpwp + uiop->uio_offset;
	count = size - uiop->uio_offset;
	error = uiomove(start, count, UIO_READ, uiop);

	kmem_free(Bpwp, size);
	return (error);
}

/* ARGSUSED */
static int
pr_read_lwpstatus(prnode_t *pnp, struct uio *uiop)
{
	lwpstatus_t *sp;
	int error;
	char *start;
	int count;

	ASSERT(pnp->pr_type == PR_LWPSTATUS);

	/*
	 * We kmem_alloc() the lwpstatus structure because
	 * it is so big it might blow the kernel stack.
	 */
	sp = (lwpstatus_t *)kmem_alloc(sizeof (*sp), KM_SLEEP);

	if ((error = prlock(pnp, ZNO)) != 0)
		goto out;

	if ((unsigned)uiop->uio_offset >= sizeof (*sp)) {
		prunlock(pnp);
		goto out;
	}

	prgetlwpstatus(pnp->pr_common->prc_thread, sp);
	prunlock(pnp);

	start = (char *)sp + uiop->uio_offset;
	count = sizeof (*sp) - uiop->uio_offset;
	error = uiomove(start, count, UIO_READ, uiop);
out:
	kmem_free((caddr_t)sp, sizeof (*sp));
	return (error);
}

/* ARGSUSED */
static int
pr_read_lwpsinfo(prnode_t *pnp, struct uio *uiop)
{
	lwpsinfo_t lwpsinfo;
	proc_t *p;
	char *start;
	int count;

	ASSERT(pnp->pr_type == PR_LWPSINFO);

	/*
	 * We don't want the full treatment of prlock(pnp) here.
	 * This file is world-readable and never goes invalid.
	 * It doesn't matter if we are in the middle of an exec().
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		return (ENOENT);
	ASSERT(p == pnp->pr_common->prc_proc);
	if (pnp->pr_common->prc_thread == NULL) {
		prunlock(pnp);
		return (ENOENT);
	}

	if ((unsigned)uiop->uio_offset >= sizeof (lwpsinfo)) {
		prunlock(pnp);
		return (0);
	}

	prgetlwpsinfo(pnp->pr_common->prc_thread, &lwpsinfo);
	prunlock(pnp);

	start = (char *)&lwpsinfo + uiop->uio_offset;
	count = sizeof (lwpsinfo) - uiop->uio_offset;
	return (uiomove(start, count, UIO_READ, uiop));
}

/* ARGSUSED */
static int
pr_read_lwpusage(prnode_t *pnp, struct uio *uiop)
{
	prhusage_t prhusage;
	prhusage_t *pup = &prhusage;
	proc_t *p;
	char *start;
	int count;
	int was_disabled;

	ASSERT(pnp->pr_type == PR_LWPUSAGE);

	/*
	 * We don't want the full treatment of prlock(pnp) here.
	 * This file is world-readable and never goes invalid.
	 * It doesn't matter if we are in the middle of an exec().
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		return (ENOENT);
	ASSERT(p == pnp->pr_common->prc_proc);
	if (pnp->pr_common->prc_thread == NULL) {
		prunlock(pnp);
		return (ENOENT);
	}

	if ((unsigned)uiop->uio_offset >= sizeof (prhusage)) {
		prunlock(pnp);
		return (0);
	}

	was_disabled = !(p->p_flag & SMSACCT);

	bzero((caddr_t)pup, sizeof (prhusage_t));
	pup->pr_tstamp = gethrtime();

	prgetusage(pnp->pr_common->prc_thread, pup);

	/* if microstate accounting was disabled before, disabled it again */
	if (was_disabled)
		disable_msacct(p);
	prunlock(pnp);

	prcvtusage(&prhusage);
	start = (char *)&prhusage + uiop->uio_offset;
	count = sizeof (prhusage) - uiop->uio_offset;
	return (uiomove(start, count, UIO_READ, uiop));
}

static int
pr_read_xregs(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	kthread_t *t;
	int error;
	char *xreg;
	char *start;
	int count;
	u_int size;

	ASSERT(pnp->pr_type == PR_XREGS);

	size = prhasx()? prgetprxregsize() : 0;
	if ((unsigned)uiop->uio_offset >= size)
		return (0);

	xreg = kmem_zalloc(size, KM_SLEEP);

	if ((error = prlock(pnp, ZNO)) != 0)
		goto out;

	p = pnp->pr_common->prc_proc;
	t = pnp->pr_common->prc_thread;

	/* drop p->p_lock while (possibly) touching the stack */
	mutex_exit(&p->p_lock);
	prgetprxregs(ttolwp(t), xreg);
	mutex_enter(&p->p_lock);
	prunlock(pnp);

	start = (char *)xreg + uiop->uio_offset;
	count = size - uiop->uio_offset;
	error = uiomove(start, count, UIO_READ, uiop);
out:
	kmem_free(xreg, size);
	return (error);
}

#if defined(sparc) || defined(__sparc)
/* ARGSUSED */
static int
pr_read_gwindows(prnode_t *pnp, struct uio *uiop)
{
	proc_t *p;
	kthread_t *t;
	gwindows_t *gwp;
	int error;
	char *start;
	int count;
	u_int size;

	ASSERT(pnp->pr_type == PR_GWINDOWS);

	gwp = (gwindows_t *)kmem_zalloc(sizeof (gwindows_t), KM_SLEEP);

	if ((error = prlock(pnp, ZNO)) != 0)
		goto out;

	p = pnp->pr_common->prc_proc;
	t = pnp->pr_common->prc_thread;

	/*
	 * Drop p->p_lock while touching the stack.
	 * The SPRLOCK flag prevents the lwp from
	 * disappearing while we do this.
	 */
	mutex_exit(&p->p_lock);
	if ((size = prnwindows(ttolwp(t))) != 0)
		size = sizeof (gwindows_t) -
		    (SPARC_MAXREGWINDOW - size) * sizeof (struct rwindow);
	if ((unsigned)uiop->uio_offset >= size) {
		mutex_enter(&p->p_lock);
		prunlock(pnp);
		goto out;
	}
	prgetwindows(ttolwp(t), gwp);
	mutex_enter(&p->p_lock);
	prunlock(pnp);

	start = (char *)gwp + uiop->uio_offset;
	count = size - uiop->uio_offset;
	error = uiomove(start, count, UIO_READ, uiop);
out:
	kmem_free((caddr_t)gwp, sizeof (gwindows_t));
	return (error);
}
#endif	/* sparc */

static int
pr_read_piddir(prnode_t *pnp, struct uio *uiop)
{
	ASSERT(pnp->pr_type == PR_PIDDIR);
	ASSERT(pnp->pr_pidfile != NULL);

	/* use the underlying PR_PIDFILE to read the process */
	pnp = VTOP(pnp->pr_pidfile);
	ASSERT(pnp->pr_type == PR_PIDFILE);

	return (pr_read_pidfile(pnp, uiop));
}

static int
pr_read_pidfile(prnode_t *pnp, struct uio *uiop)
{
	int error;

	ASSERT(pnp->pr_type == PR_PIDFILE || pnp->pr_type == PR_LWPIDFILE);

	if ((error = prlock(pnp, ZNO)) == 0) {
		proc_t *p = pnp->pr_common->prc_proc;
		struct as *as = p->p_as;

		if ((p->p_flag & SSYS) || as == &kas) {
			/*
			 * /proc I/O cannot be done to a system process.
			 */
			error = EIO;	/* old /proc semantics */
		} else {
			/*
			 * We drop p_lock because we don't want to hold
			 * it over an I/O operation because that could
			 * lead to deadlock with the clock thread.
			 * The process will not disappear and its address
			 * space will not change because it is marked SPRLOCK.
			 */
			mutex_exit(&p->p_lock);
			error = prusrio(as, UIO_READ, uiop, 1);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
	}

	return (error);
}

/* ARGSUSED */
static int
prwrite(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
	prnode_t *pnp = VTOP(vp);
	int old = 0;
	int error;
	int resid;

	ASSERT(pnp->pr_type < PR_NFILES);

	/*
	 * Only a handful of /proc files are writable, enumerate them here.
	 */
	switch (pnp->pr_type) {
	case PR_PIDDIR:		/* directory write()s: visceral revulsion. */
		ASSERT(pnp->pr_pidfile != NULL);
		/* use the underlying PR_PIDFILE to write the process */
		vp = pnp->pr_pidfile;
		pnp = VTOP(vp);
		ASSERT(pnp->pr_type == PR_PIDFILE);
		/* FALLTHROUGH */
	case PR_PIDFILE:
	case PR_LWPIDFILE:
		old = 1;
		/* FALLTHROUGH */
	case PR_AS:
		if ((error = prlock(pnp, ZNO)) == 0) {
			proc_t *p = pnp->pr_common->prc_proc;
			struct as *as = p->p_as;

			if ((p->p_flag & SSYS) || as == &kas) {
				/*
				 * /proc I/O cannot be done to a system process.
				 */
				error = EIO;
			} else {
				/*
				 * See comments above (pr_read_pidfile)
				 * about this locking dance.
				 */
				mutex_exit(&p->p_lock);
				error = prusrio(as, UIO_WRITE, uiop, old);
				mutex_enter(&p->p_lock);
			}
			prunlock(pnp);
		}
		return (error);

	case PR_CTL:
	case PR_LWPCTL:
		resid = uiop->uio_resid;
		error = prwritectl(vp, uiop, cr);
		/*
		 * This hack makes sure that the EINTR is passed
		 * all the way back to the caller's write() call.
		 */
		if (error == EINTR)
			uiop->uio_resid = resid;
		return (error);

	default:
		return ((vp->v_type == VDIR)? EISDIR : EBADF);
	}
	/* NOTREACHED */
}

static int
prgetattr(vnode_t *vp, struct vattr *vap, int flags, struct cred *cr)
{
	prnode_t *pnp = VTOP(vp);
	proc_t *p;
	struct as *as;
	user_t *up;
	extern u_int nproc;

	/*
	 * Return all the attributes.  Should be refined so that it
	 * returns only those asked for.
	 *
	 * Most of this is complete fakery anyway.
	 */

	/*
	 * For files in the /proc/<pid>/object directory,
	 * return the attributes of the underlying object.
	 */
	if (pnp->pr_type == PR_OBJECT) {
		int error;

		ASSERT(pnp->pr_object != NULL);
		error = VOP_GETATTR(pnp->pr_object, vap, flags, cr);
		return (error);
	}

	bzero((caddr_t)vap, sizeof (*vap));
	/*
	 * Large Files: Internally proc now uses VPROC to indicate
	 * a proc file. Since we have been returning VREG through
	 * VOP_GETATTR() until now, we continue to do this so as
	 * not to break apps depending on this return value.
	 */
	vap->va_type = (vp->v_type == VPROC) ? VREG : vp->v_type;
	vap->va_mode = pnp->pr_mode;
	vap->va_fsid = vp->v_vfsp->vfs_dev;
	vap->va_blksize = DEV_BSIZE;
	vap->va_rdev = 0;
	vap->va_vcode = 0;

	if (pnp->pr_type == PR_PROCDIR) {
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_nlink = nproc + 2;
		vap->va_nodeid = (ino64_t)PRROOTINO;
		vap->va_atime = vap->va_mtime = vap->va_ctime = hrestime;
		vap->va_size = (v.v_proc + 2) * PRSDSIZE;
		vap->va_nblocks = btod(vap->va_size);
		return (0);
	}

	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		return (ENOENT);

	mutex_enter(&p->p_crlock);
	vap->va_uid = p->p_cred->cr_ruid;
	vap->va_gid = p->p_cred->cr_rgid;
	mutex_exit(&p->p_crlock);
	vap->va_nlink = 1;
	vap->va_nodeid = pnp->pr_ino;
	up = prumap(p);
	vap->va_atime.tv_sec = vap->va_mtime.tv_sec =
	    vap->va_ctime.tv_sec = up->u_start;
	vap->va_atime.tv_nsec = vap->va_mtime.tv_nsec =
	    vap->va_ctime.tv_nsec = 0;
	prunmap(p);

	switch (pnp->pr_type) {
	case PR_PIDDIR:
		vap->va_nlink = 4;
		vap->va_size = sizeof (piddir);
		break;
	case PR_OBJECTDIR:
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas)
			vap->va_size = 2 * PRSDSIZE;
		else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			if (as->a_updatedir)
				rebuild_objdir(as);
			vap->va_size = (as->a_sizedir + 2) * PRSDSIZE;
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		vap->va_nlink = 2;
		break;
	case PR_LWPDIR:
		vap->va_nlink =
			(p->p_tlist? p->p_tlist->t_back->t_dslot + 1 : 1) + 2;
		vap->va_size = vap->va_nlink * PRSDSIZE;
		break;
	case PR_LWPIDDIR:
		vap->va_nlink = 2;
		vap->va_size = sizeof (lwpiddir);
		break;
	case PR_AS:
	case PR_PIDFILE:
	case PR_LWPIDFILE:
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas)
			vap->va_size = 0;
		else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			vap->va_size = rm_assize(as);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		break;
	case PR_STATUS:
		vap->va_size = sizeof (pstatus_t);
		break;
	case PR_LSTATUS:
		vap->va_size = sizeof (prheader_t) +
			p->p_lwpcnt * LSPAN(lwpstatus_t);
		break;
	case PR_PSINFO:
		vap->va_size = sizeof (psinfo_t);
		break;
	case PR_LPSINFO:
		vap->va_size = sizeof (prheader_t) +
			p->p_lwpcnt * LSPAN(lwpsinfo_t);
		break;
	case PR_MAP:
	case PR_RMAP:
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas)
			vap->va_size = 0;
		else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			vap->va_size = prnsegs(as, pnp->pr_type == PR_RMAP) *
				sizeof (prmap_t);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		break;
	case PR_CRED:
		mutex_enter(&p->p_crlock);
		vap->va_size = sizeof (prcred_t);
		if (p->p_cred->cr_ngroups > 1)
			vap->va_size +=
			    (p->p_cred->cr_ngroups - 1) * sizeof (gid_t);
		mutex_exit(&p->p_crlock);
		break;
	case PR_SIGACT:
		vap->va_size = (NSIG-1) * sizeof (struct sigaction);
		break;
	case PR_AUXV:
		vap->va_size = NUM_AUX_VECTORS * sizeof (auxv_t);
		break;
#if defined(i386) || defined(__i386)
	case PR_LDT:
		mutex_enter(&p->p_ldtlock);
		vap->va_size = prnldt(p) * sizeof (struct ssd);
		mutex_exit(&p->p_ldtlock);
		break;
#endif
	case PR_USAGE:
		vap->va_size = sizeof (prusage_t);
		break;
	case PR_LUSAGE:
		vap->va_size = sizeof (prheader_t) +
			(p->p_lwpcnt + 1) * LSPAN(prhusage_t);
		break;
	case PR_PAGEDATA:
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas)
			vap->va_size = 0;
		else {
			/*
			 * We can drop p->p_lock before grabbing the
			 * address space lock because p->p_as will not
			 * change while the process is marked SPRLOCK.
			 */
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			vap->va_size = prpdsize(as);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		break;
	case PR_OPAGEDATA:
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas)
			vap->va_size = 0;
		else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			vap->va_size = oprpdsize(as);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		break;
	case PR_WATCH:
		vap->va_size = p->p_nwarea * sizeof (prwatch_t);
		break;
	case PR_LWPSTATUS:
		vap->va_size = sizeof (lwpstatus_t);
		break;
	case PR_LWPSINFO:
		vap->va_size = sizeof (lwpsinfo_t);
		break;
	case PR_LWPUSAGE:
		vap->va_size = sizeof (prusage_t);
		break;
	case PR_XREGS:
		if (prhasx())
			vap->va_size = prgetprxregsize();
		else
			vap->va_size = 0;
		break;
#if defined(sparc) || defined(__sparc)
	case PR_GWINDOWS:
	{
		int n;

		/*
		 * Drop p->p_lock while touching the stack.
		 * The SPRLOCK flag prevents the lwp from
		 * disappearing while we do this.
		 */
		mutex_exit(&p->p_lock);
		if ((n = prnwindows(ttolwp(pnp->pr_common->prc_thread))) == 0)
			vap->va_size = 0;
		else
			vap->va_size = sizeof (gwindows_t) -
			    (SPARC_MAXREGWINDOW - n) * sizeof (struct rwindow);
		mutex_enter(&p->p_lock);
		break;
	}
#endif
	case PR_CTL:
	case PR_LWPCTL:
	default:
		vap->va_size = 0;
		break;
	}

	prunlock(pnp);
	vap->va_nblocks = (fsblkcnt64_t)btod(vap->va_size);
	return (0);
}

static int
praccess(vnode_t *vp, int mode, int flags, struct cred *cr)
{
	prnode_t *pnp = VTOP(vp);
	int vmode;
	proc_t *p;
	int error = 0;
	vnode_t *xvp;

	if ((mode & VWRITE) && (vp->v_vfsp->vfs_flag & VFS_RDONLY))
		return (EROFS);

	switch (pnp->pr_type) {
	case PR_PROCDIR:
		break;

	case PR_OBJECT:
		/* Disallow write access to the underlying objects */
		if (mode & VWRITE)
			return (EACCES);
		return (VOP_ACCESS(pnp->pr_object, mode, flags, cr));

	case PR_PSINFO:		/* these files can read by anyone */
	case PR_LPSINFO:
	case PR_LWPSINFO:
	case PR_LWPDIR:
	case PR_LWPIDDIR:
	case PR_USAGE:
	case PR_LUSAGE:
	case PR_LWPUSAGE:
		p = pr_p_lock(pnp);
		mutex_exit(&pr_pidlock);
		if (p == NULL)
			return (ENOENT);
		prunlock(pnp);
		break;

	default:
		/*
		 * Except for the world-readable files above,
		 * only /proc/pid exists if the process is a zombie.
		 */
		if ((error = prlock(pnp,
		    (pnp->pr_type == PR_PIDDIR)? ZYES : ZNO)) != 0)
			return (error);
		p = pnp->pr_common->prc_proc;
		if (cr->cr_uid != 0) {
			mutex_enter(&p->p_crlock);
			if (cr->cr_uid != p->p_cred->cr_ruid ||
			    cr->cr_uid != p->p_cred->cr_suid ||
			    cr->cr_gid != p->p_cred->cr_rgid ||
			    cr->cr_gid != p->p_cred->cr_sgid)
				error = EACCES;
			mutex_exit(&p->p_crlock);
		}

		if (error || cr->cr_uid == 0 ||
		    (p->p_flag & SSYS) || p->p_as == &kas ||
		    (xvp = p->p_exec) == NULL)
			prunlock(pnp);
		else {
			/*
			 * Determine if the process's executable is readable.
			 * We have to drop p->p_lock before the VOP operation.
			 */
			VN_HOLD(xvp);
			prunlock(pnp);
			error = VOP_ACCESS(xvp, VREAD, 0, cr);
			VN_RELE(xvp);
		}
		if (error)
			return (error);
		break;
	}
	vmode = pnp->pr_mode;
	/*
	 * Visceral revulsion:  For compatibility with old /proc,
	 * allow the /proc/<pid> directory to be opened for writing.
	 */
	if (pnp->pr_type == PR_PIDDIR)
		vmode |= VWRITE;
	if (cr->cr_uid != 0 && (vmode & mode) != mode)
		error = EACCES;
	return (error);
}

/*
 * Array of lookup functions, indexed by /proc file type.
 */
static vnode_t *pr_lookup_notdir(), *pr_lookup_procdir(), *pr_lookup_piddir(),
	*pr_lookup_objectdir(), *pr_lookup_lwpdir(), *pr_lookup_lwpiddir();

static vnode_t *(*pr_lookup_function[PR_NFILES])() = {
	pr_lookup_procdir,	/* /proc				*/
	pr_lookup_piddir,	/* /proc/<pid>				*/
	pr_lookup_notdir,	/* /proc/<pid>/as			*/
	pr_lookup_notdir,	/* /proc/<pid>/ctl			*/
	pr_lookup_notdir,	/* /proc/<pid>/status			*/
	pr_lookup_notdir,	/* /proc/<pid>/lstatus			*/
	pr_lookup_notdir,	/* /proc/<pid>/psinfo			*/
	pr_lookup_notdir,	/* /proc/<pid>/lpsinfo			*/
	pr_lookup_notdir,	/* /proc/<pid>/map			*/
	pr_lookup_notdir,	/* /proc/<pid>/rmap			*/
	pr_lookup_notdir,	/* /proc/<pid>/cred			*/
	pr_lookup_notdir,	/* /proc/<pid>/sigact			*/
	pr_lookup_notdir,	/* /proc/<pid>/auxv			*/
#if defined(i386) || defined(__i386)
	pr_lookup_notdir,	/* /proc/<pid>/ldt			*/
#endif
	pr_lookup_notdir,	/* /proc/<pid>/usage			*/
	pr_lookup_notdir,	/* /proc/<pid>/lusage			*/
	pr_lookup_notdir,	/* /proc/<pid>/pagedata			*/
	pr_lookup_notdir,	/* /proc/<pid>/watch			*/
	pr_lookup_objectdir,	/* /proc/<pid>/object			*/
	pr_lookup_notdir,	/* /proc/<pid>/object/xxx		*/
	pr_lookup_lwpdir,	/* /proc/<pid>/lwp			*/
	pr_lookup_lwpiddir,	/* /proc/<pid>/lwp/<lwpid>		*/
	pr_lookup_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpctl	*/
	pr_lookup_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpstatus	*/
	pr_lookup_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpsinfo	*/
	pr_lookup_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpusage	*/
	pr_lookup_notdir,	/* /proc/<pid>/lwp/<lwpid>/xregs	*/
#if defined(sparc) || defined(__sparc)
	pr_lookup_notdir,	/* /proc/<pid>/lwp/<lwpid>/gwindows	*/
#endif
	pr_lookup_notdir,	/* old process file			*/
	pr_lookup_notdir,	/* old lwp file				*/
	pr_lookup_notdir,	/* old pagedata file			*/
};

/* ARGSUSED */
static int
prlookup(vnode_t *dp, char *comp, vnode_t **vpp, struct pathname *pathp,
	int flags, vnode_t *rdir, struct cred *cr)
{
	prnode_t *pnp = VTOP(dp);
	int error;

	ASSERT(dp->v_type == VDIR);
	ASSERT(pnp->pr_type < PR_NFILES);

	if (pnp->pr_type == PR_OBJECTDIR &&
	    (error = praccess(dp, VEXEC, 0, cr)) != 0)
		return (error);

	if (pnp->pr_type != PR_PROCDIR && strcmp(comp, "..") == 0) {
		VN_HOLD(pnp->pr_parent);
		*vpp = pnp->pr_parent;
		return (0);
	}

	if (*comp == '\0' ||
	    strcmp(comp, ".") == 0 || strcmp(comp, "..") == 0) {
		VN_HOLD(dp);
		*vpp = dp;
		return (0);
	}

	*vpp = (pr_lookup_function[pnp->pr_type](dp, comp));

	return ((*vpp == NULL) ? ENOENT : 0);
}

/* ARGSUSED */
static vnode_t *
pr_lookup_notdir(vnode_t *dp, char *comp)
{
	return (NULL);
}

/*
 * Find or construct a process vnode for the given pid.
 */
static vnode_t *
pr_lookup_procdir(vnode_t *dp, char *comp)
{
	pid_t pid;
	prnode_t *pnp;
	prcommon_t *pcp;
	vnode_t *vp;
	proc_t *p;
	int c;

	ASSERT(VTOP(dp)->pr_type == PR_PROCDIR);

	pid = 0;
	while ((c = *comp++) != '\0') {
		if (c < '0' || c > '9')
			return (NULL);
		pid = 10*pid + c - '0';
		if (pid > MAXPID)
			return (NULL);
	}

	pnp = prgetnode(PR_PIDDIR);

	mutex_enter(&pidlock);
	if ((p = prfind(pid)) == NULL || p->p_stat == SIDL) {
		mutex_exit(&pidlock);
		prfreenode(pnp);
		return (NULL);
	}
	ASSERT(p->p_stat != 0);
	mutex_enter(&p->p_lock);
	mutex_exit(&pidlock);

	if ((vp = p->p_trace) != NULL &&
	    !(VTOP(VTOP(vp)->pr_pidfile)->pr_flags & PR_INVAL)) {
		VN_HOLD(vp);
		prfreenode(pnp);
		mutex_exit(&p->p_lock);
		return (vp);
	}

	/*
	 * prgetnode() initialized most of the prnode.
	 * Finish the job.
	 */
	pnp->pr_pcommon = pcp = pnp->pr_common;
	pnp->pr_parent = dp;
	VN_HOLD(dp);
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		pcp->prc_flags |= PRC_SYS;
	if (p->p_stat == SZOMB)
		pcp->prc_flags |= PRC_DESTROY;
	pcp->prc_proc = p;
	pcp->prc_pid = p->p_pid;
	pcp->prc_slot = p->p_slot;
	pnp->pr_ino = pmkino(0, pcp->prc_slot, PR_PIDDIR);
	/*
	 * Link in the old, invalid directory vnode so we
	 * can later determine the last close of the file.
	 */
	pnp->pr_next = p->p_trace;
	p->p_trace = dp = PTOV(pnp);

	/*
	 * Kludge for old /proc: initialize the PR_PIDFILE as well.
	 */
	vp = pnp->pr_pidfile;
	pnp = VTOP(vp);
	pnp->pr_ino = ptoi(pcp->prc_pid);
	pnp->pr_common = pcp;
	pnp->pr_pcommon = pcp;
	pnp->pr_parent = dp;
	pnp->pr_next = p->p_plist;
	p->p_plist = vp;

	mutex_exit(&p->p_lock);
	return (dp);
}

/* ARGSUSED */
static vnode_t *
pr_lookup_piddir(vnode_t *dp, char *comp)
{
	prnode_t *dpnp = VTOP(dp);
	vnode_t *vp;
	prnode_t *pnp;
	proc_t *p;
	prdirent_t *dirp;
	int i;
	enum prnodetype type;

	ASSERT(dpnp->pr_type == PR_PIDDIR);

	for (i = 0; i < NPIDDIRFILES; i++) {
		/* Skip "." and ".." */
		dirp = &piddir[i+2];
		if (strcmp(comp, dirp->d_name) == 0)
			break;
	}

	if (i >= NPIDDIRFILES)
		return (NULL);

	type = dirp->d_ino;
	pnp = prgetnode(type);

	p = pr_p_lock(dpnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL) {
		prfreenode(pnp);
		return (NULL);
	}
	if (dpnp->pr_pcommon->prc_flags & PRC_DESTROY) {
		switch (type) {
		case PR_PSINFO:
		case PR_USAGE:
			break;
		default:
			prunlock(dpnp);
			prfreenode(pnp);
			return (NULL);
		}
	}

	mutex_enter(&dpnp->pr_mutex);

	if ((vp = dpnp->pr_files[i]) != NULL &&
	    !(VTOP(vp)->pr_flags & PR_INVAL)) {
		VN_HOLD(vp);
		mutex_exit(&dpnp->pr_mutex);
		prunlock(dpnp);
		prfreenode(pnp);
		return (vp);
	}

	/*
	 * prgetnode() initialized most of the prnode.
	 * Finish the job.
	 */
	pnp->pr_common = dpnp->pr_common;
	pnp->pr_pcommon = dpnp->pr_pcommon;
	pnp->pr_parent = dp;
	pnp->pr_ino = pmkino(0, pnp->pr_pcommon->prc_slot, type);
	VN_HOLD(dp);
	pnp->pr_index = i;

	dpnp->pr_files[i] = vp = PTOV(pnp);

	/*
	 * Link new vnode into list of all /proc vnodes for the process.
	 */
	if (vp->v_type == VPROC) {
		pnp->pr_next = p->p_plist;
		p->p_plist = vp;
	}
	mutex_exit(&dpnp->pr_mutex);
	prunlock(dpnp);
	return (vp);
}

/* ARGSUSED */
static vnode_t *
pr_lookup_objectdir(vnode_t *dp, char *comp)
{
	prnode_t *dpnp = VTOP(dp);
	prnode_t *pnp;
	proc_t *p;
	struct seg *seg;
	struct as *as;
	struct vnode *vp;
	struct vattr vattr;

	ASSERT(VTOP(dp)->pr_type == PR_OBJECTDIR);

	pnp = prgetnode(PR_OBJECT);

	if (prlock(dpnp, ZNO) != 0) {
		prfreenode(pnp);
		return (NULL);
	}
	p = dpnp->pr_common->prc_proc;
	if ((p->p_flag & SSYS) || (as = p->p_as) == &kas) {
		prunlock(dpnp);
		prfreenode(pnp);
		return (NULL);
	}

	/*
	 * We drop p_lock before grabbing the address space lock
	 * in order to avoid a deadlock with the clock thread.
	 * The process will not disappear and its address space
	 * will not change because it is marked SPRLOCK.
	 */
	mutex_exit(&p->p_lock);
	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	if ((seg = AS_SEGP(as, as->a_segs)) == NULL) {
		vp = NULL;
		goto out;
	}
	if (strcmp(comp, "a.out") == 0) {
		vp = p->p_exec;
		goto out;
	}
	do {
		/*
		 * Manufacture a filename for the "object" directory.
		 */
		vattr.va_mask = AT_FSID|AT_NODEID;
		if (seg->s_ops == &segvn_ops &&
		    SEGOP_GETVP(seg, seg->s_base, &vp) == 0 &&
		    vp != NULL && vp->v_type == VREG &&
		    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
			char name[64];

			if (vp == p->p_exec)	/* "a.out" */
				continue;
			pr_object_name(name, vp, &vattr);
			if (strcmp(name, comp) == 0)
				goto out;
		}
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	vp = NULL;
out:
	if (vp != NULL)
		VN_HOLD(vp);
	AS_LOCK_EXIT(as, &as->a_lock);
	mutex_enter(&p->p_lock);
	prunlock(dpnp);

	if (vp == NULL)
		prfreenode(pnp);
	else {
		/*
		 * Fill in the prnode so future references will
		 * be able to find the underlying object's vnode.
		 * Don't link this prnode into the list of all
		 * prnodes for the process; this is a one-use node.
		 * Its use is entirely to catch and fail opens for writing.
		 */
		pnp->pr_object = vp;
		vp = PTOV(pnp);
	}

	return (vp);
}

/*
 * Find or construct an lwp vnode for the given lwpid.
 */
static vnode_t *
pr_lookup_lwpdir(vnode_t *dp, char *comp)
{
	u_int tid;	/* same type as t->t_tid */
	prnode_t *dpnp = VTOP(dp);
	prnode_t *pnp;
	prcommon_t *pcp;
	vnode_t *vp;
	proc_t *p;
	kthread_t *t;
	int c;

	ASSERT(dpnp->pr_type == PR_LWPDIR);

	tid = 0;
	while ((c = *comp++) != '\0') {
		u_int otid;

		if (c < '0' || c > '9')
			return (NULL);
		otid = tid;
		tid = 10*tid + c - '0';
		if (tid/10 != otid)	/* integer overflow */
			return (NULL);
	}

	pnp = prgetnode(PR_LWPIDDIR);

	p = pr_p_lock(dpnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL) {
		prfreenode(pnp);
		return (NULL);
	}

	if ((t = p->p_tlist) != NULL) {
		do {
			if (t->t_tid == tid)
				break;
		} while ((t = t->t_forw) != p->p_tlist);
	}

	if (t == NULL || t->t_tid != tid || ttolwp(t) == NULL) {
		prunlock(dpnp);
		prfreenode(pnp);
		return (NULL);
	}
	ASSERT(t->t_state != TS_FREE);

	if ((vp = t->t_trace) != NULL) {
		VN_HOLD(vp);
		prunlock(dpnp);
		prfreenode(pnp);
		return (vp);
	}

	/*
	 * prgetnode() initialized most of the prnode.
	 * Finish the job.
	 */
	pnp->pr_pcommon = dpnp->pr_pcommon;
	pnp->pr_parent = dp;
	VN_HOLD(dp);
	pcp = pnp->pr_common;
	pcp->prc_flags |= PRC_LWP;
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		pcp->prc_flags |= PRC_SYS;
	if ((t->t_flag & T_LWPZOMB) ||
	    (t->t_proc_flag & TP_LWPEXIT) ||
	    t->t_state == TS_ZOMB)
		pcp->prc_flags |= PRC_DESTROY;
	pcp->prc_proc = p;
	pcp->prc_pid = p->p_pid;
	pcp->prc_slot = p->p_slot;
	pcp->prc_thread = t;
	pcp->prc_tid = t->t_tid;
	pcp->prc_tslot = t->t_dslot;
	pnp->pr_ino = pmkino(pcp->prc_tslot, pcp->prc_slot, PR_LWPIDDIR);
	t->t_trace = vp = PTOV(pnp);
	prunlock(dpnp);
	return (vp);
}

/* ARGSUSED */
static vnode_t *
pr_lookup_lwpiddir(vnode_t *dp, char *comp)
{
	prnode_t *dpnp = VTOP(dp);
	vnode_t *vp;
	prnode_t *pnp;
	proc_t *p;
	prdirent_t *dirp;
	int i;
	enum prnodetype type;

	ASSERT(dpnp->pr_type == PR_LWPIDDIR);

	for (i = 0; i < NLWPIDDIRFILES; i++) {
		/* Skip "." and ".." */
		dirp = &lwpiddir[i+2];
		if (strcmp(comp, dirp->d_name) == 0)
			break;
	}

	if (i >= NLWPIDDIRFILES)
		return (NULL);

	type = dirp->d_ino;
	pnp = prgetnode(type);

	p = pr_p_lock(dpnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL) {
		prfreenode(pnp);
		return (NULL);
	}
	if (dpnp->pr_common->prc_thread == NULL) {
		prunlock(dpnp);
		prfreenode(pnp);
		return (NULL);
	}

	mutex_enter(&dpnp->pr_mutex);

	if ((vp = dpnp->pr_files[i]) != NULL &&
	    !(VTOP(vp)->pr_flags & PR_INVAL)) {
		VN_HOLD(vp);
		mutex_exit(&dpnp->pr_mutex);
		prunlock(dpnp);
		prfreenode(pnp);
		return (vp);
	}

	/*
	 * prgetnode() initialized most of the prnode.
	 * Finish the job.
	 */
	pnp->pr_common = dpnp->pr_common;
	pnp->pr_pcommon = dpnp->pr_pcommon;
	pnp->pr_parent = dp;
	pnp->pr_ino = pmkino(pnp->pr_common->prc_tslot,
		pnp->pr_common->prc_slot, type);
	VN_HOLD(dp);
	pnp->pr_index = i;

	dpnp->pr_files[i] = vp = PTOV(pnp);

	/*
	 * Link new vnode into list of all /proc vnodes for the process.
	 */
	if (vp->v_type == VPROC) {
		pnp->pr_next = p->p_plist;
		p->p_plist = vp;
	}
	mutex_exit(&dpnp->pr_mutex);
	prunlock(dpnp);
	return (vp);
}

/*
 * Construct an lwp vnode for the old /proc interface.
 * We stand on our head to make the /proc plumbing correct.
 */
vnode_t *
prlwpnode(pid_t pid, u_int tid)
{
	char comp[12];
	vnode_t *dp;
	vnode_t *vp;
	prnode_t *pnp;
	prcommon_t *pcp;
	proc_t *p;

	/*
	 * Lookup the /proc/<pid>/lwp/<lwpid> directory vnode.
	 * We are guaranteed that the new /proc is mounted.
	 */
	dp = PTOV(&prrootnode);
	VN_HOLD(dp);

	(void) pr_utos(pid, comp, sizeof (comp));
	vp = pr_lookup_procdir(dp, comp);
	VN_RELE(dp);
	if ((dp = vp) == NULL)
		return (NULL);
	vp = pr_lookup_piddir(dp, "lwp");
	VN_RELE(dp);
	if ((dp = vp) == NULL)
		return (NULL);
	(void) pr_utos(tid, comp, sizeof (comp));
	vp = pr_lookup_lwpdir(dp, comp);
	VN_RELE(dp);
	if ((dp = vp) == NULL)
		return (NULL);

	pnp = prgetnode(PR_LWPIDFILE);
	vp = PTOV(pnp);

	/*
	 * prgetnode() initialized most of the prnode.
	 * Finish the job.
	 */
	pcp = VTOP(dp)->pr_common;
	pnp->pr_ino = ptoi(pcp->prc_pid);
	pnp->pr_common = pcp;
	pnp->pr_pcommon = VTOP(dp)->pr_pcommon;
	pnp->pr_parent = dp;
	/*
	 * Link new vnode into list of all /proc vnodes for the process.
	 */
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL) {
		VN_RELE(dp);
		prfreenode(pnp);
		vp = NULL;
	} else {
		pnp->pr_next = p->p_plist;
		p->p_plist = vp;
		prunlock(pnp);
	}

	return (vp);
}

#if defined(DEBUG)

static	kmutex_t prnode_mutex;	/* really should be mutex_init()'d */
static	long nprnode, nprcommon;

#define	INCREMENT(x)	\
	(mutex_enter(&prnode_mutex), x++, mutex_exit(&prnode_mutex))
#define	DECREMENT(x)	\
	(mutex_enter(&prnode_mutex), x--, mutex_exit(&prnode_mutex))

#else

#define	INCREMENT(x)
#define	DECREMENT(x)

#endif	/* DEBUG */

/*
 * New /proc vnode required; allocate it and fill in most of the fields.
 */
prnode_t *
prgetnode(prnodetype_t type)
{
	prnode_t *pnp;
	prcommon_t *pcp;
	vnode_t *vp;
	unsigned nfiles;

	INCREMENT(nprnode);
	pnp = (prnode_t *)kmem_zalloc(sizeof (prnode_t), KM_SLEEP);

	mutex_init(&pnp->pr_mutex, "prnode mutex", MUTEX_DEFAULT, DEFAULT_WT);
	pnp->pr_type = type;

	vp = PTOV(pnp);
	mutex_init(&vp->v_lock, "procfs v_lock", MUTEX_DEFAULT, DEFAULT_WT);
	vp->v_flag = VNOCACHE|VNOMAP|VNOSWAP|VNOMOUNT;
	vp->v_count = 1;
	vp->v_op = &prvnodeops;
	vp->v_vfsp = procvfs;
	vp->v_type = VPROC;
	vp->v_data = (caddr_t)pnp;
	cv_init(&vp->v_cv, "procfs v_cv", CV_DEFAULT, NULL);

	switch (type) {
	case PR_PIDDIR:
	case PR_LWPIDDIR:
		/*
		 * We need a prcommon and a files array for each of these.
		 */
		INCREMENT(nprcommon);

		pcp = (prcommon_t *)kmem_zalloc(sizeof (prcommon_t), KM_SLEEP);
		pnp->pr_common = pcp;
		mutex_init(&pcp->prc_mutex, "prcommon prc_mutex", MUTEX_DEFAULT,
		    DEFAULT_WT);
		cv_init(&pcp->prc_wait, "prcommon prc_wait", CV_DEFAULT,
		    NULL);

		nfiles = (type == PR_PIDDIR)? NPIDDIRFILES : NLWPIDDIRFILES;
		pnp->pr_files = (vnode_t **)
		    kmem_zalloc(nfiles * sizeof (vnode_t *), KM_SLEEP);

		vp->v_type = VDIR;
		/*
		 * Mode should be read-search by all, but we cannot so long
		 * as we must support compatibility mode with old /proc.
		 * Make /proc/<pid> be read by owner only, search by all.
		 * Make /proc/<pid>/lwp/<lwpid> read-search by all.  Also,
		 * set VDIROPEN on /proc/<pid> so it can be opened for writing.
		 */
		if (type == PR_PIDDIR) {
			/* kludge for old /proc interface */
			prnode_t *xpnp = prgetnode(PR_PIDFILE);
			pnp->pr_pidfile = PTOV(xpnp);
			pnp->pr_mode = 0511;
			vp->v_flag |= VDIROPEN;
		} else {
			pnp->pr_mode = 0555;
		}

		break;

	case PR_OBJECTDIR:
		vp->v_type = VDIR;
		pnp->pr_mode = 0500;	/* read-search by owner only */
		break;

	case PR_LWPDIR:
		vp->v_type = VDIR;
		pnp->pr_mode = 0555;	/* read-search by all */
		break;

	case PR_AS:
		pnp->pr_mode = 0600;	/* read-write by owner only */
		break;

	case PR_CTL:
	case PR_LWPCTL:
		pnp->pr_mode = 0200;	/* write-only by owner only */
		break;

	case PR_PIDFILE:
	case PR_LWPIDFILE:
		pnp->pr_mode = 0600;	/* read-write by owner only */
		break;

	case PR_PSINFO:
	case PR_LPSINFO:
	case PR_LWPSINFO:
	case PR_USAGE:
	case PR_LUSAGE:
	case PR_LWPUSAGE:
		pnp->pr_mode = 0444;	/* read-only by all */
		break;

	default:
		pnp->pr_mode = 0400;	/* read-only by owner only */
		break;
	}

	return (pnp);
}

/*
 * Free the storage obtained from prgetnode().
 */
void
prfreenode(prnode_t *pnp)
{
	vnode_t *vp = PTOV(pnp);
	prcommon_t *pcp;
	unsigned nfiles;

	mutex_destroy(&pnp->pr_mutex);
	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);

	switch (pnp->pr_type) {
	case PR_PIDDIR:
		/* kludge for old /proc interface */
		if (pnp->pr_pidfile != NULL) {
			prfreenode(VTOP(pnp->pr_pidfile));
			pnp->pr_pidfile = NULL;
		}
		/* FALLTHROUGH */
	case PR_LWPIDDIR:
		/*
		 * We allocated a prcommon and a files array for each of these.
		 */
		pcp = pnp->pr_common;
		ASSERT(pcp->prc_opens == 0 && pcp->prc_writers == 0);
		mutex_destroy(&pcp->prc_mutex);
		cv_destroy(&pcp->prc_wait);
		kmem_free((caddr_t)pcp, sizeof (prcommon_t));

		nfiles = (pnp->pr_type == PR_PIDDIR)?
		    NPIDDIRFILES : NLWPIDDIRFILES;
		kmem_free((caddr_t)pnp->pr_files, nfiles * sizeof (vnode_t *));

		DECREMENT(nprcommon);
	}
	kmem_free((caddr_t)pnp, sizeof (*pnp));

	DECREMENT(nprnode);
}

/*
 * Array of readdir functions, indexed by /proc file type.
 */
static int pr_readdir_notdir(), pr_readdir_procdir(), pr_readdir_piddir(),
	pr_readdir_objectdir(), pr_readdir_lwpdir(), pr_readdir_lwpiddir();

static int (*pr_readdir_function[PR_NFILES])() = {
	pr_readdir_procdir,	/* /proc				*/
	pr_readdir_piddir,	/* /proc/<pid>				*/
	pr_readdir_notdir,	/* /proc/<pid>/as			*/
	pr_readdir_notdir,	/* /proc/<pid>/ctl			*/
	pr_readdir_notdir,	/* /proc/<pid>/status			*/
	pr_readdir_notdir,	/* /proc/<pid>/lstatus			*/
	pr_readdir_notdir,	/* /proc/<pid>/psinfo			*/
	pr_readdir_notdir,	/* /proc/<pid>/lpsinfo			*/
	pr_readdir_notdir,	/* /proc/<pid>/map			*/
	pr_readdir_notdir,	/* /proc/<pid>/rmap			*/
	pr_readdir_notdir,	/* /proc/<pid>/cred			*/
	pr_readdir_notdir,	/* /proc/<pid>/sigact			*/
	pr_readdir_notdir,	/* /proc/<pid>/auxv			*/
#if defined(i386) || defined(__i386)
	pr_readdir_notdir,	/* /proc/<pid>/ldt			*/
#endif
	pr_readdir_notdir,	/* /proc/<pid>/usage			*/
	pr_readdir_notdir,	/* /proc/<pid>/lusage			*/
	pr_readdir_notdir,	/* /proc/<pid>/pagedata			*/
	pr_readdir_notdir,	/* /proc/<pid>/watch			*/
	pr_readdir_objectdir,	/* /proc/<pid>/object			*/
	pr_readdir_notdir,	/* /proc/<pid>/object/xxx		*/
	pr_readdir_lwpdir,	/* /proc/<pid>/lwp			*/
	pr_readdir_lwpiddir,	/* /proc/<pid>/lwp/<lwpid>		*/
	pr_readdir_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpctl	*/
	pr_readdir_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpstatus	*/
	pr_readdir_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpsinfo	*/
	pr_readdir_notdir,	/* /proc/<pid>/lwp/<lwpid>/lwpusage	*/
	pr_readdir_notdir,	/* /proc/<pid>/lwp/<lwpid>/xregs	*/
#if defined(sparc) || defined(__sparc)
	pr_readdir_notdir,	/* /proc/<pid>/lwp/<lwpid>/gwindows	*/
#endif
	pr_readdir_notdir,	/* old process file			*/
	pr_readdir_notdir,	/* old lwp file				*/
	pr_readdir_notdir,	/* old pagedata file			*/
};

/* ARGSUSED */
static int
prreaddir(vnode_t *vp, struct uio *uiop, struct cred *cr, int *eofp)
{
	prnode_t *pnp = VTOP(vp);

	ASSERT(pnp->pr_type < PR_NFILES);

	return (pr_readdir_function[pnp->pr_type](pnp, uiop, eofp));
}

/* ARGSUSED */
static int
pr_readdir_notdir(prnode_t *pnp, struct uio *uiop, int *eofp)
{
	return (ENOTDIR);
}

/* ARGSUSED */
static int
pr_readdir_procdir(prnode_t *pnp, struct uio *uiop, int *eofp)
{
	/* bp holds one dirent64 structure */
	longlong_t bp[DIRENT64_RECLEN(PNSIZ) / sizeof (longlong_t)];
	struct dirent64 *dirent = (struct dirent64 *)bp;
	int reclen;
	int i;
	int oresid;
	off_t off;
	int error;

	ASSERT(pnp->pr_type == PR_PROCDIR);

	if (uiop->uio_offset < 0 ||
	    uiop->uio_resid <= 0 ||
	    (uiop->uio_offset % PRSDSIZE) != 0)
		return (EINVAL);
	oresid = uiop->uio_resid;
	bzero(bp, sizeof (bp));

	/*
	 * Loop until user's request is satisfied or until all processes
	 * have been examined.
	 */
	for (; uiop->uio_resid > 0; uiop->uio_offset = off + PRSDSIZE) {
		if ((off = uiop->uio_offset) == 0) {	/* "." */
			dirent->d_ino = (ino64_t)PRROOTINO;
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '\0';
			reclen = DIRENT64_RECLEN(1);
		} else if (off == PRSDSIZE) {		/* ".." */
			dirent->d_ino = (ino64_t)PRROOTINO;
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '.';
			dirent->d_name[2] = '\0';
			reclen = DIRENT64_RECLEN(2);
		} else {
			u_int pid;
			int pslot;
			/*
			 * Stop when entire proc table has been examined.
			 */
			proc_t *p;
			if ((i = (off-2*PRSDSIZE)/PRSDSIZE) >= v.v_proc)
				break;
			mutex_enter(&pidlock);
			if ((p = pid_entry(i)) == NULL || p->p_stat == SIDL) {
				mutex_exit(&pidlock);
				continue;
			}
			ASSERT(p->p_stat != 0);
			pid = p->p_pid;
			pslot = p->p_slot;
			mutex_exit(&pidlock);
			dirent->d_ino = pmkino(0, pslot, PR_PIDDIR);
			(void) pr_utos(pid, dirent->d_name, PNSIZ+1);
			reclen = DIRENT64_RECLEN(PNSIZ);
		}
		dirent->d_off = (offset_t)(uiop->uio_offset + PRSDSIZE);
		dirent->d_reclen = (u_short)reclen;
		if (reclen > uiop->uio_resid) {
			/*
			 * Error if no entries have been returned yet.
			 */
			if (uiop->uio_resid == oresid)
				return (EINVAL);
			break;
		}
		/*
		 * uiomove() updates both resid and offset by the same
		 * amount.  But we want offset to change in increments
		 * of PRSDSIZE, which is different from the number of bytes
		 * being returned to the user.  So we set uio_offset
		 * separately, ignoring what uiomove() does.
		 */
		if (error = uiomove((caddr_t)dirent, reclen, UIO_READ, uiop))
			return (error);
	}
	if (eofp)
		*eofp = ((uiop->uio_offset-2*PRSDSIZE)/PRSDSIZE >= v.v_proc);
	return (0);
}

/* ARGSUSED */
static int
pr_readdir_piddir(prnode_t *pnp, struct uio *uiop, int *eofp)
{
	int zombie = ((pnp->pr_pcommon->prc_flags & PRC_DESTROY) != 0);
	prdirent_t dirent;
	prdirent_t *dirp;
	off_t off;
	int error;

	ASSERT(pnp->pr_type == PR_PIDDIR);

	if (uiop->uio_offset < 0 ||
	    uiop->uio_offset % sizeof (prdirent_t) != 0 ||
	    uiop->uio_resid < sizeof (prdirent_t))
		return (EINVAL);
	if (pnp->pr_pcommon->prc_proc == NULL)
		return (ENOENT);
	if (uiop->uio_offset >= sizeof (piddir))
		goto out;

	/*
	 * Loop until user's request is satisfied, omitting some
	 * files along the way if the process is a zombie.
	 */
	for (dirp = &piddir[uiop->uio_offset / sizeof (prdirent_t)];
	    uiop->uio_resid >= sizeof (prdirent_t) &&
	    dirp < &piddir[NPIDDIRFILES+2];
	    uiop->uio_offset = off + sizeof (prdirent_t), dirp++) {
		off = uiop->uio_offset;
		if (zombie) {
			switch (dirp->d_ino) {
			case PR_PIDDIR:
			case PR_PROCDIR:
			case PR_PSINFO:
			case PR_USAGE:
				break;
			default:
				continue;
			}
		}
		bcopy(dirp, &dirent, sizeof (prdirent_t));
		if (dirent.d_ino == PR_PROCDIR)
			dirent.d_ino = PRROOTINO;
		else
			dirent.d_ino = pmkino(0, pnp->pr_pcommon->prc_slot,
					dirent.d_ino);
		if ((error = uiomove((caddr_t)&dirent, sizeof (prdirent_t),
		    UIO_READ, uiop)) != 0)
			return (error);
	}
out:
	if (eofp)
		*eofp = (uiop->uio_offset >= sizeof (piddir));
	return (0);
}

static void
rebuild_objdir(struct as *as)
{
	struct seg *seg;
	struct vnode *vp;
	struct vattr vattr;
	vnode_t **dir;
	size_t nalloc;
	size_t nentries;
	int i, j;
	int nold, nnew;

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));

	if (as->a_updatedir == 0 && as->a_objectdir != NULL)
		return;
	as->a_updatedir = 0;

	if ((nalloc = as->a_nsegs) == 0 ||
	    (seg = AS_SEGP(as, as->a_segs)) == NULL)	/* can't happen? */
		return;

	/*
	 * Allocate space for the new object directory.
	 * (This is usually about two times too many entries.)
	 */
	nalloc = (nalloc + 0xf) & ~0xf;		/* multiple of 16 */
	dir = kmem_zalloc(nalloc * sizeof (vnode_t *), KM_SLEEP);

	/* fill in the new directory with desired entries */
	nentries = 0;
	do {
		vattr.va_mask = AT_FSID|AT_NODEID;
		if (seg->s_ops == &segvn_ops &&
		    SEGOP_GETVP(seg, seg->s_base, &vp) == 0 &&
		    vp != NULL && vp->v_type == VREG &&
		    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
			for (i = 0; i < nentries; i++)
				if (vp == dir[i])
					break;
			if (i == nentries) {
				ASSERT(nentries < nalloc);
				dir[nentries++] = vp;
			}
		}
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	if (as->a_objectdir == NULL) {	/* first time */
		as->a_objectdir = dir;
		as->a_sizedir = nalloc;
		return;
	}

	/*
	 * Null out all of the defunct entries in the old directory.
	 */
	nold = 0;
	nnew = nentries;
	for (i = 0; i < as->a_sizedir; i++) {
		if ((vp = as->a_objectdir[i]) != NULL) {
			for (j = 0; j < nentries; j++) {
				if (vp == dir[j]) {
					dir[j] = NULL;
					nnew--;
					break;
				}
			}
			if (j == nentries)
				as->a_objectdir[i] = NULL;
			else
				nold++;
		}
	}

	if (nold + nnew > as->a_sizedir) {
		/*
		 * Reallocate the old directory to have enough
		 * space for the old and new entries combined.
		 * Round up to the next multiple of 16.
		 */
		size_t newsize = (nold + nnew + 0xf) & ~0xf;
		vnode_t **newdir = kmem_zalloc(newsize * sizeof (vnode_t *),
					KM_SLEEP);
		bcopy((caddr_t)as->a_objectdir, (caddr_t)newdir,
			as->a_sizedir * sizeof (vnode_t *));
		kmem_free(as->a_objectdir, as->a_sizedir * sizeof (vnode_t *));
		as->a_objectdir = newdir;
		as->a_sizedir = newsize;
	}

	/*
	 * Move all new entries to the old directory and
	 * deallocate the space used by the new directory.
	 */
	if (nnew) {
		for (i = 0, j = 0; i < nentries; i++) {
			if ((vp = dir[i]) == NULL)
				continue;
			for (; j < as->a_sizedir; j++) {
				if (as->a_objectdir[j] != NULL)
					continue;
				as->a_objectdir[j++] = vp;
				break;
			}
		}
	}
	kmem_free(dir, nalloc * sizeof (vnode_t *));
}

/*
 * Return the vnode from a slot in the process's object directory.
 * The caller must have locked the process's address space.
 * The only caller is below, in pr_readdir_objectdir().
 */
static vnode_t *
obj_entry(struct as *as, int slot)
{
	ASSERT(AS_LOCK_HELD(as, &as->a_lock));
	if (as->a_objectdir == NULL)
		return (NULL);
	ASSERT(slot < as->a_sizedir);
	return (as->a_objectdir[slot]);
}

/* ARGSUSED */
static int
pr_readdir_objectdir(prnode_t *pnp, struct uio *uiop, int *eofp)
{
	/* bp holds one dirent64 structure */
	longlong_t bp[DIRENT64_RECLEN(64) / sizeof (longlong_t)];
	struct dirent64 *dirent = (struct dirent64 *)bp;
	int reclen;
	int i;
	int oresid;
	off_t off;
	int error;
	int pslot;
	int objdirsize;
	proc_t *p;
	struct as *as;
	struct vnode *vp;

	ASSERT(pnp->pr_type == PR_OBJECTDIR);

	if (uiop->uio_offset < 0 ||
	    uiop->uio_resid <= 0 ||
	    (uiop->uio_offset % PRSDSIZE) != 0)
		return (EINVAL);
	oresid = uiop->uio_resid;
	bzero(bp, sizeof (bp));

	if ((error = prlock(pnp, ZNO)) != 0)
		return (error);
	p = pnp->pr_common->prc_proc;
	pslot = p->p_slot;

	/*
	 * We drop p_lock before grabbing the address space lock
	 * in order to avoid a deadlock with the clock thread.
	 * The process will not disappear and its address space
	 * will not change because it is marked SPRLOCK.
	 */
	mutex_exit(&p->p_lock);

	if ((p->p_flag & SSYS) || (as = p->p_as) == &kas) {
		as = NULL;
		objdirsize = 0;
	} else {
		AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
		if (as->a_updatedir)
			rebuild_objdir(as);
		objdirsize = as->a_sizedir;
	}

	/*
	 * Loop until user's request is satisfied or until
	 * all mapped objects have been examined.
	 */
	for (; uiop->uio_resid > 0; uiop->uio_offset = off + PRSDSIZE) {
		if ((off = uiop->uio_offset) == 0) {	/* "." */
			dirent->d_ino = pmkino(0, pslot, PR_OBJECTDIR);
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '\0';
			reclen = DIRENT64_RECLEN(1);
		} else if (off == PRSDSIZE) {	/* ".." */
			dirent->d_ino = pmkino(0, pslot, PR_PIDDIR);
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '.';
			dirent->d_name[2] = '\0';
			reclen = DIRENT64_RECLEN(2);
		} else {
			struct vattr vattr;
			/*
			 * Stop when all objects have been reported.
			 */
			if ((i = (off-2*PRSDSIZE)/PRSDSIZE) >= objdirsize)
				break;
			if ((vp = obj_entry(as, i)) == NULL)
				continue;
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (VOP_GETATTR(vp, &vattr, 0, CRED()) != 0)
				continue;
			if (vp == p->p_exec)
				strcpy(dirent->d_name, "a.out");
			else
				pr_object_name(dirent->d_name, vp, &vattr);
			dirent->d_ino = vattr.va_nodeid;
			reclen = DIRENT64_RECLEN(strlen(dirent->d_name));
		}
		dirent->d_off = uiop->uio_offset + PRSDSIZE;
		dirent->d_reclen = (u_short)reclen;
		if (reclen > uiop->uio_resid) {
			/*
			 * Error if no entries have been returned yet.
			 */
			if (uiop->uio_resid == oresid)
				error = EINVAL;
			break;
		}
		/*
		 * Drop the address space lock to do the uiomove().
		 */
		if (as != NULL)
			AS_LOCK_EXIT(as, &as->a_lock);
		/*
		 * uiomove() updates both resid and offset by the same
		 * amount.  But we want offset to change in increments
		 * of PRSDSIZE, which is different from the number of bytes
		 * being returned to the user.  So we set uio_offset
		 * separately, ignoring what uiomove() does.
		 */
		error = uiomove((caddr_t)dirent, reclen, UIO_READ, uiop);
		if (as != NULL) {
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			if (as->a_updatedir) {
				rebuild_objdir(as);
				objdirsize = as->a_sizedir;
			}
		}
		if (error)
			break;
	}
	if (error == 0 && eofp)
		*eofp = ((uiop->uio_offset-2*PRSDSIZE)/PRSDSIZE >= objdirsize);

	if (as != NULL)
		AS_LOCK_EXIT(as, &as->a_lock);
	mutex_enter(&p->p_lock);
	prunlock(pnp);
	return (error);
}

/*
 * Return the thread from a slot in the process's thread directory.
 * The kthread_t argument is the last thread found (optimization).
 * The caller must have locked the process via /proc.
 * The only caller is below, in pr_readdir_lwpdir().
 */
static kthread_t *
thr_entry(proc_t *p, kthread_t *t, int slot)
{
	ASSERT(p->p_flag & SPRLOCK);

	if (t != NULL) {
		do {
			if (slot <= t->t_dslot) {
				if (slot == t->t_dslot)
					return (t);
				break;
			}
		} while ((t = t->t_forw) != p->p_tlist);
	}

	return (NULL);
}

/* ARGSUSED */
static int
pr_readdir_lwpdir(prnode_t *pnp, struct uio *uiop, int *eofp)
{
	/* bp holds one dirent64 structure */
	longlong_t bp[DIRENT64_RECLEN(PLNSIZ) / sizeof (longlong_t)];
	struct dirent64 *dirent = (struct dirent64 *)bp;
	int reclen;
	int i;
	int oresid;
	off_t off;
	int error = 0;
	proc_t *p;
	kthread_t *tx;
	int pslot;
	int lwpdirsize;

	ASSERT(pnp->pr_type == PR_LWPDIR);

	if (uiop->uio_offset < 0 ||
	    uiop->uio_resid <= 0 ||
	    (uiop->uio_offset % PRSDSIZE) != 0)
		return (EINVAL);
	oresid = uiop->uio_resid;
	bzero(bp, sizeof (bp));

	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);
	if (p == NULL)
		return (ENOENT);
	ASSERT(p == pnp->pr_common->prc_proc);
	pslot = p->p_slot;
	if (p->p_tlist == NULL)
		lwpdirsize = 0;
	else
		lwpdirsize = p->p_tlist->t_back->t_dslot + 1;
	/*
	 * Drop p->p_lock so we can safely do uiomove().
	 * The lwp directory will not change because
	 * we have the process locked with SPRLOCK.
	 */
	mutex_exit(&p->p_lock);

	/*
	 * Loop until user's request is satisfied or until all lwps
	 * have been examined.
	 */
	tx = p->p_tlist;
	for (; uiop->uio_resid > 0; uiop->uio_offset = off + PRSDSIZE) {
		if ((off = uiop->uio_offset) == 0) {	/* "." */
			dirent->d_ino = pmkino(0, pslot, PR_LWPDIR);
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '\0';
			reclen = DIRENT64_RECLEN(1);
		} else if (off == PRSDSIZE) { /* ".." */
			dirent->d_ino = pmkino(0, pslot, PR_PIDDIR);
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '.';
			dirent->d_name[2] = '\0';
			reclen = DIRENT64_RECLEN(2);
		} else {
			kthread_t *t;
			u_int tid;
			int tslot;
			/*
			 * Stop when all lwps have been reported.
			 */
			if ((i = (off-2*PRSDSIZE)/PRSDSIZE) >= lwpdirsize)
				break;
			if ((t = thr_entry(p, tx, i)) == NULL)
				continue;
			tx = t;		/* remember for next time around */
			tid = t->t_tid;
			tslot = t->t_dslot;
			dirent->d_ino = pmkino(tslot, pslot, PR_LWPIDDIR);
			(void) pr_utos(tid, dirent->d_name, PLNSIZ+1);
			reclen = DIRENT64_RECLEN(PLNSIZ);
		}
		dirent->d_off = uiop->uio_offset + PRSDSIZE;
		dirent->d_reclen = (u_short)reclen;
		if (reclen > uiop->uio_resid) {
			/*
			 * Error if no entries have been returned yet.
			 */
			if (uiop->uio_resid == oresid)
				error = EINVAL;
			break;
		}
		/*
		 * uiomove() updates both resid and offset by the same
		 * amount.  But we want offset to change in increments
		 * of PRSDSIZE, which is different from the number of bytes
		 * being returned to the user.  So we set uio_offset
		 * separately, ignoring what uiomove() does.
		 */
		if (error = uiomove((caddr_t)dirent, reclen, UIO_READ, uiop))
			break;
	}
	if (error == 0 && eofp)
		*eofp = ((uiop->uio_offset-2*PRSDSIZE)/PRSDSIZE >= lwpdirsize);

	mutex_enter(&p->p_lock);
	prunlock(pnp);
	return (error);
}

/* ARGSUSED */
static int
pr_readdir_lwpiddir(prnode_t *pnp, struct uio *uiop, int *eofp)
{
	prdirent_t mylwpiddir[NLWPIDDIRFILES+2];
	prdirent_t *dirp;
	char *start;
	int count;
	int error;
	int pslot;
	int tslot;

	ASSERT(pnp->pr_type == PR_LWPIDDIR);

	if (uiop->uio_offset < 0 ||
	    uiop->uio_offset % sizeof (prdirent_t) != 0 ||
	    uiop->uio_resid < sizeof (prdirent_t))
		return (EINVAL);
	if (pnp->pr_pcommon->prc_proc == NULL ||
	    pnp->pr_common->prc_thread == NULL)
		return (ENOENT);
	if (uiop->uio_offset >= sizeof (mylwpiddir))
		goto out;

	pslot = pnp->pr_pcommon->prc_slot;
	tslot = pnp->pr_common->prc_tslot;
	start = (char *)mylwpiddir + uiop->uio_offset;
	count = min(uiop->uio_resid, sizeof (mylwpiddir) - uiop->uio_offset);
	count = (count / sizeof (prdirent_t)) * sizeof (prdirent_t);
	if (count <= 0)
		return (EINVAL);

	bcopy((caddr_t)lwpiddir, (caddr_t)mylwpiddir, sizeof (mylwpiddir));
	for (dirp = mylwpiddir; dirp < &mylwpiddir[NLWPIDDIRFILES+2]; dirp++) {
		if (dirp->d_ino == PR_LWPDIR)
			dirp->d_ino = pmkino(0, pslot, dirp->d_ino);
		else
			dirp->d_ino = pmkino(tslot, pslot, dirp->d_ino);
	}
	if (error = uiomove(start, count, UIO_READ, uiop))
		return (error);
out:
	if (eofp)
		*eofp = (uiop->uio_offset >= sizeof (mylwpiddir));
	return (0);
}

/* ARGSUSED */
static int
prfsync(vnode_t *vp, int syncflag, struct cred *cr)
{
	return (0);
}

/*
 * Utility: remove a /proc vnode from a linked list, threaded through pr_next.
 * Return 1 (true) if the vnode was found on the list, else 0 (false).
 */
static int
pr_list_unlink(vnode_t *vp, vnode_t **listp)
{
	prnode_t *pnp = VTOP(vp);
	vnode_t *pvp;
	prnode_t *ppnp;

	if ((pvp = *listp) == NULL)
		return (0);
	if (pvp == vp)
		*listp = pnp->pr_next;
	else {
		for (ppnp = VTOP(pvp); ppnp->pr_next != vp; ppnp = VTOP(pvp))
			if ((pvp = ppnp->pr_next) == NULL)
				return (0);
		ppnp->pr_next = pnp->pr_next;
	}
	pnp->pr_next = NULL;
	return (1);
}

/* ARGSUSED */
static void
prinactive(vnode_t *vp, struct cred *cr)
{
	prnode_t *pnp = VTOP(vp);
	proc_t *p;
	kthread_t *t;
	vnode_t *dp = NULL;
	prnode_t *dpnp = NULL;
	vnode_t *ovp = NULL;
	prnode_t *opnp = NULL;

	if (pnp->pr_type == PR_OBJECT) {
		/* This is not linked into the usual lists */
		ASSERT(vp->v_count == 1);
		if (pnp->pr_object)
			VN_RELE(pnp->pr_object);
		prfreenode(pnp);
		return;
	}

	mutex_enter(&pr_pidlock);
	if (pnp->pr_pcommon == NULL)
		p = NULL;
	else if ((p = pnp->pr_pcommon->prc_proc) != NULL)
		mutex_enter(&p->p_lock);
	mutex_enter(&vp->v_lock);

	if (pnp->pr_type == PR_PROCDIR || vp->v_count > 1) {
		vp->v_count--;
		mutex_exit(&vp->v_lock);
		if (p != NULL)
			mutex_exit(&p->p_lock);
		mutex_exit(&pr_pidlock);
		return;
	}

	if (pnp->pr_parent != NULL) {
		dp = pnp->pr_parent;
		pnp->pr_parent = NULL;
		dpnp = VTOP(dp);
		mutex_enter(&dpnp->pr_mutex);
	}

	if (pnp->pr_type != PR_PIDFILE &&
	    pnp->pr_type != PR_LWPIDFILE &&
	    pnp->pr_type != PR_OPAGEDATA &&
	    dpnp != NULL && dpnp->pr_files != NULL &&
	    dpnp->pr_files[pnp->pr_index] == vp)
		dpnp->pr_files[pnp->pr_index] = NULL;

	ASSERT(vp->v_count == 1);

	/*
	 * If we allocated an old /proc/pid node, free it too.
	 */
	if (pnp->pr_pidfile != NULL) {
		ASSERT(pnp->pr_type == PR_PIDDIR);
		ovp = pnp->pr_pidfile;
		opnp = VTOP(ovp);
		ASSERT(opnp->pr_type == PR_PIDFILE);
		pnp->pr_pidfile = NULL;
	}

	mutex_exit(&pr_pidlock);

	if (p != NULL) {
		/*
		 * Remove the vnodes from the list of
		 * all /proc vnodes for the process.
		 */
		if (vp->v_type == VDIR) {
			if (pr_list_unlink(vp, &p->p_trace))
				/* EMPTY */;
			else if ((t = pnp->pr_common->prc_thread) != NULL)
				(void) pr_list_unlink(vp, &t->t_trace);
		} else {
			(void) pr_list_unlink(vp, &p->p_plist);
		}
		if (ovp != NULL) {
			(void) pr_list_unlink(ovp, &p->p_plist);
		}
		mutex_exit(&p->p_lock);
	}

	mutex_exit(&vp->v_lock);

	if (opnp != NULL)
		prfreenode(opnp);
	prfreenode(pnp);
	if (dpnp != NULL) {
		mutex_exit(&dpnp->pr_mutex);
		VN_RELE(dp);
	}
}

/* ARGSUSED */
static int
prseek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{
	return (0);
}

/*
 * Return the answer requested to poll().
 * POLLIN, POLLRDNORM, and POLLOUT are recognized as in fs_poll().
 * In addition, these have special meaning for /proc files:
 *	POLLPRI		process or lwp stopped on an event of interest
 *	POLLERR		/proc file descriptor is invalid
 *	POLLHUP		process or lwp has terminated
 */
static int
prpoll(vnode_t *vp, short events, int anyyet, short *reventsp,
	pollhead_t **phpp)
{
	prnode_t *pnp = VTOP(vp);
	prcommon_t *pcp = pnp->pr_common;
	pollhead_t *php = &pcp->prc_pollhead;
	proc_t *p;
	short revents;
	int error;
	int lockstate;

	ASSERT(pnp->pr_type < PR_NFILES);

	/*
	 * Support for old /proc interface.
	 */
	if (pnp->pr_pidfile != NULL) {
		vp = pnp->pr_pidfile;
		pnp = VTOP(vp);
		ASSERT(pnp->pr_type == PR_PIDFILE);
		ASSERT(pnp->pr_common == pcp);
	}

	*reventsp = revents = 0;
	*phpp = (pollhead_t *)NULL;

	if (vp->v_type == VDIR) {
		*reventsp |= POLLNVAL;
		return (0);
	}

	lockstate = pollunlock(php);	/* avoid deadlock with prnotify() */

	if ((error = prlock(pnp, ZNO)) != 0) {
		pollrelock(php, lockstate);
		switch (error) {
		case ENOENT:		/* process or lwp died */
			*reventsp = POLLHUP;
			error = 0;
			break;
		case EAGAIN:		/* invalidated */
			*reventsp = POLLERR;
			error = 0;
			break;
		}
		return (error);
	}

	/*
	 * We have the process marked locked (SPRLOCK) and we are holding
	 * its p->p_lock.  We want to unmark the process but retain
	 * exclusive control w.r.t. other /proc controlling processes
	 * before reacquiring the polling locks.
	 *
	 * prunmark() does this for us.  It unmarks the process
	 * but retains p->p_lock so we still have exclusive control.
	 * We will drop p->p_lock at the end to relinquish control.
	 *
	 * We cannot call prunlock() at the end to relinquish control
	 * because prunlock(), like prunmark(), may drop and reacquire
	 * p->p_lock and that would lead to a lock order violation
	 * w.r.t. the polling locks we are about to reacquire.
	 */
	p = pcp->prc_proc;
	ASSERT(p != NULL);
	prunmark(p);

	pollrelock(php, lockstate);	/* reacquire dropped poll locks */

	if ((p->p_flag & SSYS) || p->p_as == &kas)
		revents = POLLNVAL;
	else {
		short ev;

		if ((ev = (events & (POLLIN|POLLRDNORM))) != 0)
			revents |= ev;
		/*
		 * POLLWRNORM (same as POLLOUT) really should not be
		 * used to indicate that the process or lwp stopped.
		 * However, USL chose to use POLLWRNORM rather than
		 * POLLPRI to indicate this, so we just accept either
		 * requested event to indicate stopped.  (grr...)
		 */
		if ((ev = (events & (POLLPRI|POLLOUT|POLLWRNORM))) != 0) {
			kthread_t *t;

			if (pcp->prc_flags & PRC_LWP) {
				t = pcp->prc_thread;
				ASSERT(t != NULL);
				thread_lock(t);
			} else {
				t = prchoose(p);	/* returns locked t */
				ASSERT(t != NULL);
			}

			if (ISTOPPED(t) || VSTOPPED(t))
				revents |= ev;
			thread_unlock(t);
		}
	}

	*reventsp = revents;
	if (!anyyet && revents == 0) {
		/*
		 * Arrange to wake up the polling lwp when
		 * the target process/lwp stops or terminates
		 * or when the file descriptor becomes invalid.
		 */
		pcp->prc_flags |= PRC_POLL;
		*phpp = php;
	}
	mutex_exit(&p->p_lock);
	return (0);
}

/* ARGSUSED */
static int
prcreate(struct vnode *dvp,
	char *name,
	struct vattr *vap,
	enum vcexcl excl,
	int mode,
	struct vnode **vpp,
	struct cred *cr,
	int flag)
{
	return (EACCES);
}

/* in prioctl.c */
extern int prioctl(struct vnode *, int, intptr_t, int, struct cred *, int *);

/*
 * /proc vnode operations vector
 */
struct vnodeops prvnodeops = {
	propen,
	prclose,
	prread,
	prwrite,
	prioctl,
	fs_setfl,
	prgetattr,
	fs_nosys,	/* setattr */
	praccess,
	prlookup,
	prcreate,	/* create */
	fs_nosys,	/* remove */
	fs_nosys,	/* link */
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	prreaddir,
	fs_nosys,	/* symlink */
	fs_nosys,	/* readlink */
	prfsync,
	prinactive,
	fs_nosys,	/* fid */
	fs_rwlock,
	fs_rwunlock,
	prseek,
	fs_cmp,
	fs_nosys,	/* frlock */
	fs_nosys,	/* space */
	fs_nosys,	/* realvp */
	fs_nosys,	/* getpage */
	fs_nosys,	/* putpage */
	fs_nosys_map,	/* map */
	fs_nosys_addmap, /* addmap */
	fs_nosys,	/* delmap */
	prpoll,
	fs_nosys,	/* dump */
	fs_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_nodispose,	/* dispose */
	fs_nosys,	/* setsecattr */
	fs_fab_acl,	/* getsecattr */
	fs_nosys	/* shrlock */
};
