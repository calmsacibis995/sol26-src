/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)watchpoint.c	1.2	96/09/12 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/regset.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/prsystm.h>
#include <sys/buf.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/cpuvar.h>

#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/watchpoint.h>

#include <sys/mman.h>
#include <vm/as.h>
#include <vm/seg.h>

/*
 * Map the 'rw' argument to a protection flag.
 */
static int
rw_to_prot(enum seg_rw rw)
{
	switch (rw) {
	case S_EXEC:
		return (PROT_EXEC);
	case S_READ:
		return (PROT_READ);
	case S_WRITE:
		return (PROT_WRITE);
	}
	return (0);	/* can't happen */
}

/*
 * Map the 'rw' argument to an index into an array of exec/read/write things.
 * The index follows the precedence order:  exec .. read .. write
 */
static int
rw_to_index(enum seg_rw rw)
{
	switch (rw) {
	default:	/* default case "can't happen" */
	case S_EXEC:
		return (0);
	case S_READ:
		return (1);
	case S_WRITE:
		return (2);
	}
}

/*
 * Map an index back to a seg_rw.
 */
static enum seg_rw S_rw[3] = {
	S_EXEC,
	S_READ,
	S_WRITE,
};

#define	X	0
#define	R	1
#define	W	2
#define	sum(a)	(a[X] + a[R] + a[W])

/*
 * Common code for pr_mappage() and pr_unmappage().
 */
static int
pr_do_mappage(caddr_t addr, u_int size, int mapin, enum seg_rw rw, int kernel)
{
	proc_t *p = curproc;
	struct as *as = p->p_as;
	char *eaddr = addr + size;
	int prot_rw = rw_to_prot(rw);
	int xrw = rw_to_index(rw);
	int rv = 0;
	struct watched_page *pwp;
	u_int prot;

	ASSERT(as != &kas);

startover:
	ASSERT(rv == 0);
	if ((pwp = as->a_wpage) == NULL)
		return (0);

	/*
	 * as->a_wpage and its linked list can only be changed while
	 * the process is totally stopped.  Don't grab p_lock here.
	 * Holding p_lock while grabbing the address space lock
	 * leads to deadlocks with the clock thread.
	 */
	do {
		if (eaddr <= pwp->wp_vaddr)
			break;
		if (addr >= pwp->wp_vaddr + PAGESIZE)
			continue;

		/*
		 * If the requested protection has not been
		 * removed, we need not remap this page.
		 */
		prot = pwp->wp_prot;
		if (kernel || (prot & PROT_USER))
			if (prot & prot_rw)
				continue;
		/*
		 * If the requested access does not exist in the page's
		 * original protections, we need not remap this page.
		 * If the page does not exist yet, we can't test it.
		 */
		if ((prot = pwp->wp_oprot) != 0) {
			if (!(kernel || (prot & PROT_USER)))
				continue;
			if (!(prot & prot_rw))
				continue;
		}

		if (mapin) {
			/*
			 * Before mapping the page in, ensure that
			 * all other lwps are held in the kernel.
			 */
			if (p->p_mapcnt == 0) {
				if (holdwatch() == 0) {
					/*
					 * We stopped in holdwatch().
					 * Start all over again because the
					 * watched page list may have changed.
					 */
					goto startover;
				}
				ASSERT(p->p_mapcnt == 0);
			}
			/* pr_do_mappage() is single-threaded now. */
			p->p_mapcnt++;
		}

		addr = pwp->wp_vaddr;
		rv++;

		prot = pwp->wp_prot;
		if (mapin) {
			if (kernel)
				pwp->wp_kmap[xrw]++;
			else
				pwp->wp_umap[xrw]++;
			pwp->wp_flags |= WP_NOWATCH;
			if (pwp->wp_kmap[X] + pwp->wp_umap[X])
				/* cannot have exec-only protection */
				prot |= PROT_READ|PROT_EXEC;
			if (pwp->wp_kmap[R] + pwp->wp_umap[R])
				prot |= PROT_READ;
			if (pwp->wp_kmap[W] + pwp->wp_umap[W])
				/* cannot have write-only protection */
				prot |= PROT_READ|PROT_WRITE;
#if 0	/* damned broken mmu feature! */
			if (sum(pwp->wp_umap) == 0)
				prot &= ~PROT_USER;
#endif
		} else {
			ASSERT(pwp->wp_flags & WP_NOWATCH);
			if (kernel) {
				ASSERT(pwp->wp_kmap[xrw] != 0);
				--pwp->wp_kmap[xrw];
			} else {
				ASSERT(pwp->wp_umap[xrw] != 0);
				--pwp->wp_umap[xrw];
			}
			if (sum(pwp->wp_kmap) + sum(pwp->wp_umap) == 0)
				pwp->wp_flags &= ~WP_NOWATCH;
			else {
				if (pwp->wp_kmap[X] + pwp->wp_umap[X])
					/* cannot have exec-only protection */
					prot |= PROT_READ|PROT_EXEC;
				if (pwp->wp_kmap[R] + pwp->wp_umap[R])
					prot |= PROT_READ;
				if (pwp->wp_kmap[W] + pwp->wp_umap[W])
					/* cannot have write-only protection */
					prot |= PROT_READ|PROT_WRITE;
#if 0	/* damned broken mmu feature! */
				if (sum(pwp->wp_umap) == 0)
					prot &= ~PROT_USER;
#endif
			}
		}

		if (pwp->wp_oprot != 0) {	/* if page exists */
			struct seg *seg;
			u_int oprot;

			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			seg = as_segat(as, addr);
			ASSERT(seg != NULL);
			SEGOP_GETPROT(seg, addr, 0, &oprot);
			if (prot != oprot)
				(void) SEGOP_SETPROT(seg, addr, PAGESIZE, prot);
			AS_LOCK_EXIT(as, &as->a_lock);
		}

		/*
		 * When all pages are mapped back to their normal state,
		 * continue the other lwps.
		 */
		if (!mapin) {
			ASSERT(p->p_mapcnt > 0);
			if (--p->p_mapcnt == 0) {
				mutex_enter(&p->p_lock);
				continuelwps(p);
				mutex_exit(&p->p_lock);
				/* pr_do_mappage() is multi-threaded now. */
			}
		}
	} while ((pwp = pwp->wp_forw) != as->a_wpage);

	return (rv);
}

/*
 * Restore the original page protections on an address range.
 * If 'kernel' is non-zero, just do it for the kernel.
 * pr_mappage() returns non-zero if it actually changed anything.
 *
 * pr_mappage() and pr_unmappage() must be executed in matched pairs,
 * but pairs may be nested within other pairs.  The reference counts
 * sort it all out.  See pr_do_mappage(), above.
 */
int
pr_mappage(caddr_t addr, u_int size, enum seg_rw rw, int kernel)
{
	return (pr_do_mappage(addr, size, 1, rw, kernel));
}

/*
 * Set the modified page protections on a watched page.
 * Inverse of pr_mappage().
 * Needs to be called only if pr_mappage() returned non-zero.
 */
void
pr_unmappage(caddr_t addr, u_int size, enum seg_rw rw, int kernel)
{
	(void) pr_do_mappage(addr, size, 0, rw, kernel);
}

/*
 * Function called by an lwp after it resumes from stop().
 */
void
setallwatch()
{
	struct as *as = curproc->p_as;
	struct watched_page *pwp;
	struct seg *seg;
	caddr_t vaddr;
	u_int prot;
	u_int npage;

	ASSERT(MUTEX_NOT_HELD(&curproc->p_lock));

	if ((pwp = as->a_wpage) == NULL)
		return;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
	ASSERT(pwp == as->a_wpage);

	for (npage = as->a_nwpage; npage; npage--) {
		vaddr = pwp->wp_vaddr;
		if ((pwp->wp_flags & WP_SETPROT) &&
		    (seg = as_segat(as, vaddr)) != NULL) {
			ASSERT(!(pwp->wp_flags & WP_NOWATCH));
			prot = pwp->wp_prot;
			pwp->wp_flags &= ~WP_SETPROT;
			(void) SEGOP_SETPROT(seg, vaddr, PAGESIZE, prot);
		}

		if (pwp->wp_read + pwp->wp_write + pwp->wp_exec != 0)
			pwp = pwp->wp_forw;
		else {
			/*
			 * No watched areas remain in this page.
			 * Free the watched_page structure.
			 */
			struct watched_page *next = pwp->wp_forw;
			as->a_nwpage--;
			if (as->a_wpage == pwp)
				as->a_wpage = next;
			if (as->a_wpage == pwp) {
				as->a_wpage = next = NULL;
				ASSERT(as->a_nwpage == 0);
			} else {
				remque(pwp);
				ASSERT(as->a_nwpage > 0);
			}
			kmem_free(pwp, sizeof (struct watched_page));
			pwp = next;
		}
	}

	AS_LOCK_EXIT(as, &as->a_lock);
}

/*
 * trap() calls here to determine if a fault is in a watched page.
 * We return nonzero if this is true and the load/store would fail.
 */
int
pr_is_watchpage(caddr_t addr, enum seg_rw rw)
{
	register struct as *as = curproc->p_as;
	register struct watched_page *pwp;
	u_int prot;
	int rv = 0;

	switch (rw) {
	case S_READ:
	case S_WRITE:
	case S_EXEC:
		break;
	default:
		return (0);
	}

	/*
	 * as->a_wpage and the linked list of pwp's can only
	 * be modified while the process is totally stopped.
	 * We need, and should use, no locks here.
	 */

	if (as != &kas && (pwp = as->a_wpage) != NULL) {
		do {
			if (addr < pwp->wp_vaddr)
				break;
			if (addr < pwp->wp_vaddr + PAGESIZE) {
				/*
				 * If page doesn't exist yet, forget it.
				 */
				if (pwp->wp_oprot == 0)
					break;
				prot = pwp->wp_prot;
				switch (rw) {
				case S_READ:
					rv = ((prot & (PROT_USER|PROT_READ))
						!= (PROT_USER|PROT_READ));
					break;
				case S_WRITE:
					rv = ((prot & (PROT_USER|PROT_WRITE))
						!= (PROT_USER|PROT_WRITE));
					break;
				case S_EXEC:
					rv = ((prot & (PROT_USER|PROT_EXEC))
						!= (PROT_USER|PROT_EXEC));
					break;
				}
				break;
			}
		} while ((pwp = pwp->wp_forw) != as->a_wpage);
	}

	return (rv);
}

/*
 * trap() calls here to determine if a fault is a watchpoint.
 */
int
pr_is_watchpoint(caddr_t *paddr, int *pta, int size, size_t *plen,
	enum seg_rw rw)
{
	proc_t *p = curproc;
	caddr_t addr = *paddr;
	caddr_t eaddr = addr + size;
	register struct watched_area *pwa;
	int rv = 0;
	int ta = 0;
	size_t len = 0;

	switch (rw) {
	case S_READ:
	case S_WRITE:
	case S_EXEC:
		break;
	default:
		*pta = 0;
		return (0);
	}

	/*
	 * p->p_warea and its linked list is protected by p->p_lock.
	 */
	mutex_enter(&p->p_lock);

	if ((pwa = p->p_warea) != NULL) {
		do {
			if (eaddr <= pwa->wa_vaddr)
				break;
			if (addr < pwa->wa_eaddr) {
				switch (rw) {
				case S_READ:
					if (pwa->wa_flags & WA_READ)
						rv = TRAP_RWATCH;
					break;
				case S_WRITE:
					if (pwa->wa_flags & WA_WRITE)
						rv = TRAP_WWATCH;
					break;
				case S_EXEC:
					if (pwa->wa_flags & WA_EXEC)
						rv = TRAP_XWATCH;
					break;
				}
				if (addr < pwa->wa_vaddr)
					addr = pwa->wa_vaddr;
				len = pwa->wa_eaddr - addr;
				if (pwa->wa_flags & WA_TRAPAFTER)
					ta = 1;
				break;
			}
		} while ((pwa = pwa->wa_forw) != p->p_warea);
	}

	mutex_exit(&p->p_lock);

	*paddr = addr;
	*pta = ta;
	if (plen != NULL)
		*plen = len;
	return (rv);
}


/*
 * Set up to perform a single-step at user level for the
 * case of a trapafter watchpoint.  Called from trap().
 */
void
do_watch_step(caddr_t vaddr, int sz, enum seg_rw rw, int watchcode, greg_t pc)
{
	register klwp_t *lwp = ttolwp(curthread);
	struct lwp_watch *pw = &lwp->lwp_watch[rw_to_index(rw)];

	/*
	 * Check to see if we are already performing this special
	 * watchpoint single-step.  We must not do pr_mappage() twice.
	 */
	if (pw->wpaddr != NULL) {
		ASSERT(lwp->lwp_watchtrap != 0);
		ASSERT(pw->wpaddr == vaddr &&
		    pw->wpsize == sz &&
		    pw->wpcode == watchcode &&
		    pw->wppc == pc);
	} else {
		int mapped = pr_mappage(vaddr, sz, rw, 0);
		ASSERT(mapped != 0);
		prstep(lwp, 1);
		lwp->lwp_watchtrap = 1;
		pw->wpaddr = vaddr;
		pw->wpsize = sz;
		pw->wpcode = watchcode;
		pw->wppc = pc;
	}
}

/*
 * Undo the effects of do_watch_step().
 * Called from trap() after the single-step is finished.
 * Also called from issig_forreal() and stop() with a NULL
 * argument to avoid having these things set more than once.
 */
int
undo_watch_step(k_siginfo_t *sip)
{
	register klwp_t *lwp = ttolwp(curthread);
	int fault = 0;

	if (lwp->lwp_watchtrap) {
		struct lwp_watch *pw = lwp->lwp_watch;
		int i;

		for (i = 0; i < 3; i++, pw++) {
			if (pw->wpaddr == NULL)
				continue;
			pr_unmappage(pw->wpaddr, pw->wpsize, S_rw[i], 0);
			if (pw->wpcode != 0) {
				if (sip != NULL) {
					sip->si_signo = SIGTRAP;
					sip->si_code = pw->wpcode;
					sip->si_addr = pw->wpaddr;
					sip->si_trapafter = 1;
					sip->si_pc = (caddr_t)pw->wppc;
				}
				fault = FLTWATCH;
				pw->wpcode = 0;
			}
			pw->wpaddr = NULL;
			pw->wpsize = 0;
		}
		lwp->lwp_watchtrap = 0;
	}

	return (fault);
}

/*
 * Handle a watchpoint that occurs while doing copyin()
 * or copyout() in a system call.
 * Return non-zero if the fault or signal is cleared
 * by a debugger while the lwp is stopped.
 */
static int
sys_watchpoint(caddr_t addr, int watchcode, int ta)
{
	extern greg_t getuserpc(void);	/* XXX header file */
	k_sigset_t smask;
	register proc_t *p = ttoproc(curthread);
	register klwp_t *lwp = ttolwp(curthread);
	register sigqueue_t *sqp;
	int rval;

	/* assert no locks are held */
	/* ASSERT(curthread->t_nlocks == 0); */

	sqp = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);
	sqp->sq_info.si_signo = SIGTRAP;
	sqp->sq_info.si_code = watchcode;
	sqp->sq_info.si_addr = addr;
	sqp->sq_info.si_trapafter = ta;
	sqp->sq_info.si_pc = (caddr_t)getuserpc();

	mutex_enter(&p->p_lock);

	/* this will be tested and cleared by the caller */
	lwp->lwp_sysabort = 0;

	if (prismember(&p->p_fltmask, FLTWATCH)) {
		lwp->lwp_curflt = (u_char)FLTWATCH;
		lwp->lwp_siginfo = sqp->sq_info;
		stop(PR_FAULTED, FLTWATCH);
		if (lwp->lwp_curflt == 0) {
			mutex_exit(&p->p_lock);
			kmem_free(sqp, sizeof (sigqueue_t));
			return (1);
		}
		lwp->lwp_curflt = 0;
	}

	/*
	 * post the SIGTRAP signal.
	 * Block all other signals so we only stop showing SIGTRAP.
	 */
	if (sigismember(&curthread->t_hold, SIGTRAP) ||
	    sigismember(&p->p_ignore, SIGTRAP)) {
		/* SIGTRAP is blocked or ignored, forget the rest. */
		mutex_exit(&p->p_lock);
		kmem_free(sqp, sizeof (sigqueue_t));
		return (0);
	}
	sigdelq(p, curthread, SIGTRAP);
	sigaddqa(p, curthread, sqp);
	smask = curthread->t_hold;
	sigfillset(&curthread->t_hold);
	sigdiffset(&curthread->t_hold, &cantmask);
	sigdelset(&curthread->t_hold, SIGTRAP);
	mutex_exit(&p->p_lock);

	rval = ((ISSIG_FAST(curthread, lwp, p, FORREAL))? 0 : 1);

	/* restore the original signal mask */
	mutex_enter(&p->p_lock);
	curthread->t_hold = smask;
	mutex_exit(&p->p_lock);

	return (rval);
}

/*
 * Wrappers for the copyin()/copyout() functions to deal
 * with watchpoints that fire while in system calls.
 */

int
watch_copyin(caddr_t uaddr, caddr_t kaddr, size_t count)
{
	return (watch_xcopyin(uaddr, kaddr, count)? -1 : 0);
}

int
watch_xcopyin(caddr_t uaddr, caddr_t kaddr, size_t count)
{
	klwp_t *lwp = ttolwp(curthread);
	int error = 0;

	while (count && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		int ta;
		int mapped;

		if ((part = PAGESIZE - (((u_int)uaddr) & PAGEOFFSET)) > count)
			part = count;

		if (!pr_is_watchpage(uaddr, S_READ))
			watchcode = 0;
		else {
			vaddr = uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
				part, &len, S_READ);
			if (watchcode && ta == 0)
				part = vaddr - uaddr;
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage(uaddr, part, S_READ, 1);
			error = _xcopyin(uaddr, kaddr, part);
			if (mapped)
				pr_unmappage(uaddr, part, S_READ, 1);
			uaddr += part;
			kaddr += part;
			count -= part;
		}
		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (count && watchcode && ta && len > part && error == 0) {
			len -= part;
			if ((part = PAGESIZE) > count)
				part = count;
			if (part > len)
				part = len;
			mapped = pr_mappage(uaddr, part, S_READ, 1);
			error = _xcopyin(uaddr, kaddr, part);
			if (mapped)
				pr_unmappage(uaddr, part, S_READ, 1);
			uaddr += part;
			kaddr += part;
			count -= part;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}
	}
	return (error);
}

int
watch_copyout(caddr_t kaddr, caddr_t uaddr, size_t count)
{
	return (watch_xcopyout(kaddr, uaddr, count)? -1 : 0);
}

int
watch_xcopyout(caddr_t kaddr, caddr_t uaddr, size_t count)
{
	klwp_t *lwp = ttolwp(curthread);
	int error = 0;

	while (count && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		int ta;
		int mapped;

		if ((part = PAGESIZE - (((u_int)uaddr) & PAGEOFFSET)) > count)
			part = count;

		if (!pr_is_watchpage(uaddr, S_WRITE))
			watchcode = 0;
		else {
			vaddr = uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
				part, &len, S_WRITE);
			if (watchcode) {
				if (ta == 0)
					part = vaddr - uaddr;
				else {
					len += vaddr - uaddr;
					if (part > len)
						part = len;
				}
			}
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage(uaddr, part, S_WRITE, 1);
			error = _xcopyout(kaddr, uaddr, part);
			if (mapped)
				pr_unmappage(uaddr, part, S_WRITE, 1);
			uaddr += part;
			kaddr += part;
			count -= part;
		}

		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (count && watchcode && ta && len > part && error == 0) {
			len -= part;
			if ((part = PAGESIZE) > count)
				part = count;
			if (part > len)
				part = len;
			mapped = pr_mappage(uaddr, part, S_WRITE, 1);
			error = _xcopyout(kaddr, uaddr, part);
			if (mapped)
				pr_unmappage(uaddr, part, S_WRITE, 1);
			uaddr += part;
			kaddr += part;
			count -= part;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}
	}
	return (error);
}

int
watch_copyinstr(char *uaddr, char *kaddr, size_t maxlength, size_t *lencopied)
{
	klwp_t *lwp = ttolwp(curthread);
	size_t resid;
	int error = 0;

	if ((resid = maxlength) == 0)
		return (ENAMETOOLONG);

	while (resid && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		size_t size;
		int ta;
		int mapped;

		if ((part = PAGESIZE - (((u_int)uaddr) & PAGEOFFSET)) > resid)
			part = resid;

		if (!pr_is_watchpage(uaddr, S_READ))
			watchcode = 0;
		else {
			vaddr = uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
				part, &len, S_READ);
			if (watchcode) {
				if (ta == 0)
					part = vaddr - uaddr;
				else {
					len += vaddr - uaddr;
					if (part > len)
						part = len;
				}
			}
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage(uaddr, part, S_READ, 1);
			error = _copyinstr(uaddr, kaddr, part, &size);
			if (mapped)
				pr_unmappage(uaddr, part, S_READ, 1);
			uaddr += size;
			kaddr += size;
			resid -= size;
			if (watchcode &&
			    (uaddr < vaddr || kaddr[-1] == '\0'))
				break;	/* didn't reach the watched area */
		}

		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (resid && watchcode && ta && len > part && error == 0 &&
		    size == part && kaddr[-1] != '\0') {
			len -= part;
			if ((part = PAGESIZE) > resid)
				part = resid;
			if (part > len)
				part = len;
			mapped = pr_mappage(uaddr, part, S_READ, 1);
			error = _copyinstr(uaddr, kaddr, part, &size);
			if (mapped)
				pr_unmappage(uaddr, part, S_READ, 1);
			uaddr += size;
			kaddr += size;
			resid -= size;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}

		if (part != 0 && (size < part || kaddr[-1] == '\0'))
			break;
	}

	if (lencopied)
		*lencopied = maxlength - resid;
	return (error);
}

int
watch_copyoutstr(char *kaddr, char *uaddr, size_t maxlength, size_t *lencopied)
{
	klwp_t *lwp = ttolwp(curthread);
	size_t resid;
	int error = 0;

	if ((resid = maxlength) == 0)
		return (ENAMETOOLONG);

	while (resid && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		size_t size;
		int ta;
		int mapped;

		if ((part = PAGESIZE - (((u_int)uaddr) & PAGEOFFSET)) > resid)
			part = resid;

		if (!pr_is_watchpage(uaddr, S_WRITE))
			watchcode = 0;
		else {
			vaddr = uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
				part, &len, S_WRITE);
			if (watchcode && ta == 0)
				part = vaddr - uaddr;
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage(uaddr, part, S_WRITE, 1);
			error = _copyoutstr(kaddr, uaddr, part, &size);
			if (mapped)
				pr_unmappage(uaddr, part, S_WRITE, 1);
			uaddr += size;
			kaddr += size;
			resid -= size;
			if (watchcode &&
			    (uaddr < vaddr || kaddr[-1] == '\0'))
				break;	/* didn't reach the watched area */
		}

		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (resid && watchcode && ta && len > part && error == 0 &&
		    size == part && kaddr[-1] != '\0') {
			len -= part;
			if ((part = PAGESIZE) > resid)
				part = resid;
			if (part > len)
				part = len;
			mapped = pr_mappage(uaddr, part, S_WRITE, 1);
			error = _copyoutstr(kaddr, uaddr, part, &size);
			if (mapped)
				pr_unmappage(uaddr, part, S_WRITE, 1);
			uaddr += size;
			kaddr += size;
			resid -= size;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}

		if (part != 0 && (size < part || kaddr[-1] == '\0'))
			break;
	}

	if (lencopied)
		*lencopied = maxlength - resid;
	return (error);
}

/*
 * Utility function for watch_*() functions below.
 * Return true if the object at [addr, addr+size) falls in a watched page.
 */
static int
is_watched(caddr_t addr, u_int size, enum seg_rw rw)
{
	caddr_t eaddr = addr + size - 1;

	return (pr_is_watchpage(addr, rw) ||
	    (((uintptr_t)addr & PAGEMASK) != ((uintptr_t)eaddr & PAGEMASK) &&
	    pr_is_watchpage(eaddr, rw)));
}

int
watch_fubyte(caddr_t addr)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (char), S_READ))
			return (_fubyte(addr));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (char), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (char), S_READ, 1);
			rv = _fubyte(addr);
			if (mapped)
				pr_unmappage(addr, sizeof (char), S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_fuibyte(caddr_t addr)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (char), S_READ))
			return (_fuibyte(addr));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (char), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (char), S_READ, 1);
			rv = _fuibyte(addr);
			if (mapped)
				pr_unmappage(addr, sizeof (char), S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_fusword(caddr_t addr)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (short), S_READ))
			return (_fusword(addr));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (short), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (short), S_READ, 1);
			rv = _fusword(addr);
			if (mapped)
				pr_unmappage(addr, sizeof (short), S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_fuword(int *addr)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (int), S_READ))
			return (_fuword(addr));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (int), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (int),
				S_READ, 1);
			rv = _fuword(addr);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (int),
					S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_fuiword(int *addr)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (int), S_READ))
			return (_fuiword(addr));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (int), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (int),
				S_READ, 1);
			rv = _fuiword(addr);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (int),
					S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_subyte(caddr_t addr, char value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (char), S_WRITE))
			return (_subyte(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (char), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (char), S_WRITE, 1);
			rv = _subyte(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (char), S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_suibyte(caddr_t addr, char value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (char), S_WRITE))
			return (_suibyte(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (char), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (char), S_WRITE, 1);
			rv = _suibyte(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (char), S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_susword(caddr_t addr, int value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (short), S_WRITE))
			return (_susword(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (short), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (short), S_WRITE, 1);
			rv = _susword(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (short), S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_suword(int *addr, int value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (int), S_WRITE))
			return (_suword(addr, value));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (int), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (int),
				S_WRITE, 1);
			rv = _suword(addr, value);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (int),
					S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_suiword(int *addr, int value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (int), S_WRITE))
			return (_suiword(addr, value));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (int), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (int),
				S_WRITE, 1);
			rv = _suiword(addr, value);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (int),
					S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

#if defined(__sparcv9cpu)

longlong_t
watch_fuextword(u_longlong_t *addr)
{
	extern longlong_t _fuextword(u_longlong_t *);
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	longlong_t rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (longlong_t), S_READ))
			return (_fuextword(addr));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (longlong_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (longlong_t),
				S_READ, 1);
			rv = _fuextword(addr);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (longlong_t),
					S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

longlong_t
watch_fuiextword(u_longlong_t *addr)
{
	extern longlong_t _fuiextword(u_longlong_t *);
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	longlong_t rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (longlong_t), S_READ))
			return (_fuiextword(addr));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (longlong_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (longlong_t),
				S_READ, 1);
			rv = _fuiextword(addr);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (longlong_t),
					S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_suextword(u_longlong_t *addr, longlong_t value)
{
	extern int _suextword(u_longlong_t *, longlong_t);
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (longlong_t), S_WRITE))
			return (_suextword(addr, value));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (longlong_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (longlong_t),
				S_WRITE, 1);
			rv = _suextword(addr, value);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (longlong_t),
					S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

int
watch_suiextword(u_longlong_t *addr, longlong_t value)
{
	extern int _suiextword(u_longlong_t *, longlong_t);
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched((caddr_t)addr, sizeof (longlong_t), S_WRITE))
			return (_suiextword(addr, value));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (longlong_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (longlong_t),
				S_WRITE, 1);
			rv = _suiextword(addr, value);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (longlong_t),
					S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

#endif	/* defined(__sparcv9cpu) */
