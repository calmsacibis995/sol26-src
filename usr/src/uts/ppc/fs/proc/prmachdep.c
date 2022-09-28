/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)prmachdep.c	1.43	96/06/21 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/psw.h>
#include <sys/pcb.h>
#include <sys/buf.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/cpuvar.h>

#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>

#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/map.h>
#include <sys/vmmac.h>
#include <sys/mman.h>
#include <sys/vmparam.h>
#include <sys/archsystm.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_kp.h>
#include <vm/page.h>

#include <fs/proc/prdata.h>

int	prnwatch = 10000;	/* maximum number of watched areas */

/*
 * Force a thread into the kernel if it is not already there.
 * This is a no-op on uniprocessors.
 */
/* ARGSUSED */
void
prpokethread(kthread_t *t)
{
#ifdef MP
	if (t->t_state == TS_ONPROC && t->t_cpu != CPU)
		poke_cpu(t->t_cpu->cpu_id);
#endif /* MP */
}

/*
 * Map a target process's u-block in and out.  prumap() makes it addressable
 * (if necessary) and returns a pointer to it.
 */
struct user *
prumap(register proc_t *p)
{
	return (PTOU(p));
}

/* ARGSUSED */
void
prunmap(proc_t *p)
{
	/*
	 * With paged u-blocks, there's nothing to do in order to unmap.
	 */
}

/*
 * Return general registers.
 */
void
prgetprregs(register klwp_t *lwp, register prgregset_t prp)
{
	register struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	bcopy((caddr_t)r, (caddr_t)prp, sizeof (prgregset_t));
}

/*
 * Set general registers.
 * (Note: This can be an alias to setgregs().)
 */
void
prsetprregs(lwp, prp)
	register klwp_t *lwp;
	register prgregset_t prp;
{
	(void) setgregs(lwp, prp);
}

/*
 * Get the syscall return values for the lwp.
 */
int
prgetrvals(klwp_t *lwp, long *rval1, long *rval2)
{
	struct regs *r = lwptoregs(lwp);

	if (r->r_cr & CR0_SO)
		return (r->r_r3);
	*rval1 = r->r_r3;
	*rval2 = r->r_r4;
	return (0);
}

/*
 * Return the value of the PC from the supplied register set.
 */
prgreg_t
prgetpc(prp)
	prgregset_t prp;
{
	return ((prgreg_t)prp[R_PC]);
}

/*
 * Does the system support floating-point, either through hardware
 * or by trapping and emulating floating-point machine instructions?
 */
int
prhasfp()
{
	extern int fpu_exists;

	return (fpu_exists);
}

/*
 * Get floating-point registers.
 */
void
prgetprfpregs(lwp, pfp)
	register klwp_t *lwp;
	register prfpregset_t *pfp;
{
	bzero((caddr_t)pfp, sizeof (prfpregset_t));
	(void) getfpregs(lwp, pfp);
}

/*
 * Set floating-point registers.
 * (Note: This can be an alias to setfpregs().)
 */
void
prsetprfpregs(lwp, pfp)
	register klwp_t *lwp;
	register prfpregset_t *pfp;
{
	(void) setfpregs(lwp, pfp);
}

/*
 * Does the system support extra register state?
 */
int
prhasx(void)
{
	return (0);
}

/*
 * Get the size of the extra registers.
 */
int
prgetprxregsize(void)
{
	return (0);
}

/*
 * Get extra registers.
 */
/*ARGSUSED*/
void
prgetprxregs(lwp, prx)
	register klwp_t *lwp;
	register caddr_t prx;
{
	/* no extra registers */
}

/*
 * Set extra registers.
 */
/*ARGSUSED*/
void
prsetprxregs(lwp, prx)
	register klwp_t *lwp;
	register caddr_t prx;
{
	/* no extra registers */
}

#if 0 /* USL, not Solaris */
/*
 * Get debug registers.
 */
void
prgetdbregs(lwp, db)
	register klwp_t *lwp;
	register dbregset_t *db;
{
	register struct pcb *pcb = &lwp->lwp_pcb;

	/* Copy the debug regs if they are in use by the process. */
	if (pcb->pcb_flags & PCB_DEBUG_EN)
		bcopy((caddr_t)&pcb->pcb_dregs, (caddr_t)db,
			sizeof (dbregset_t));
	else
		bzero((caddr_t)&db, sizeof (dbregset_t));
}
#endif

#if 0 /* USL, not Solaris */
/*
 * Set debug registers.
 */
void
prsetdbregs(lwp, db)
	register klwp_t *lwp;
	register dbregset_t *db;
{
	register struct pcb *pcb = &lwp->lwp_pcb;

	if (pcb->pcb_flags & PCB_DEBUG_EN)
		bcopy((caddr_t)db, (caddr_t)&pcb->pcb_dregs,
			sizeof (dbregset_t));
	else {
		/* Setup context structure for debug regs */
		/* XXXPPC installctx() needs to be done */
		pcb->pcb_flags |= PCB_DEBUG_EN;
	}
}
#endif

/*
 * Return the base (lower limit) of the process stack.
 */
caddr_t
prgetstackbase(p)
	register proc_t *p;
{
	return ((caddr_t)USRSTACK - p->p_stksize);
}

/*
 * Return the "addr" field for pr_addr in prpsinfo_t.
 * This is a vestige of the past, so whatever we return is OK.
 */
caddr_t
prgetpsaddr(p)
	register proc_t *p;
{
	return ((caddr_t)p);
}

/*
 * Arrange to single-step the lwp.
 * Note: All PowerPC implementation may not support MSR_SE bit. If it is
 *	 not supported then debuggers using /proc should know it.
 */
void
prstep(klwp_t *lwp, int watchstep)
{
	register struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	if (watchstep)
		lwp->lwp_pcb.pcb_flags |= PCB_WATCH_STEP;
	else
		lwp->lwp_pcb.pcb_flags |= PCB_NORMAL_STEP;

	r->r_msr |= MSR_SE; /* set the single step tracing in MSR */
}

/*
 * Undo prstep().
 * Note: All PowerPC implementation may not support MSR_SE bit. If it is
 *	 not supported then debuggers using /proc should know it.
 */
void
prnostep(klwp_t *lwp)
{
	register struct regs *r = lwptoregs(lwp);

	ASSERT(ttolwp(curthread) == lwp ||
	    MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	r->r_msr &= ~MSR_SE; /* turn off single step tracing in MSR */
	lwp->lwp_pcb.pcb_flags &= ~(PCB_NORMAL_STEP|PCB_WATCH_STEP);
}

/*
 * Return non-zero if a single-step is in effect.
 */
int
prisstep(klwp_t *lwp)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	return ((lwp->lwp_pcb.pcb_flags &
		(PCB_NORMAL_STEP|PCB_WATCH_STEP)) != 0);
}

/*
 * Set the PC to the specified virtual address.
 */
void
prsvaddr(lwp, vaddr)
	klwp_t *lwp;
	caddr_t vaddr;
{
	register struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	r->r_pc = (int)vaddr;
}

/* XXX -- belongs in some header file */
extern caddr_t ppmapin(struct page *, u_int, caddr_t);
extern int pf_is_memory(u_int pfnum);

/*
 * Map address "addr" in address space "as" into a kernel virtual address.
 * The memory is guaranteed to be resident and locked down.
 */
caddr_t
prmapin(as, addr, writing)
	struct as *as;
	caddr_t addr;
	int writing;
{
	u_int pfnum;
	page_t *pp;
	caddr_t kaddr;
	u_long x;

	/*
	 * XXX - Because of past mistakes, we have bits being returned
	 * by getpfnum that are actually the page type bits of the pte.
	 * When the object we are trying to map is a memory page with
	 * a page structure everything is ok and we can use the optimal
	 * method, ppmapin.  Otherwise, we have to do something special.
	 */
	pfnum = hat_getpfnum(as->a_hat, addr);
	if (pf_is_memory(pfnum)) {
		pp = page_numtopp_nolock(pfnum);
		if (pp != NULL) {
			kaddr = ppmapin(pp, writing ?
				(PROT_READ | PROT_WRITE) : PROT_READ,
				(caddr_t)-1);
			return (kaddr + ((int)addr & PAGEOFFSET));
		}
	}

	/*
	 * Oh well, we didn't have a page struct for the object we were
	 * trying to map in; ppmapin doesn't handle devices, but allocating a
	 * slot from kernelmap allows ppmapout to free virutal space when done.
	 */
	x = rmalloc_wait(kernelmap, (long)CLSIZE);
	kaddr = kmxtob(x);

	hat_devload(kas.a_hat, kaddr, MMU_PAGESIZE, pfnum,
		writing ? (PROT_READ | PROT_WRITE) : PROT_READ, 0);

	return (kaddr + ((int)addr & PAGEOFFSET));
}

/*
 * Unmap address "addr" in address space "as"; inverse of prmapin().
 */
/* ARGSUSED */
void
prmapout(as, addr, vaddr, writing)
	struct as *as;
	caddr_t addr;
	caddr_t vaddr;
	int writing;
{
	extern void ppmapout(caddr_t);

	vaddr = (caddr_t)((long)vaddr & PAGEMASK);
	ppmapout(vaddr);
}

/*
 * Make sure the lwp is in an orderly state
 * for inspection by a debugger through /proc.
 * Called from stop().
 */
/* ARGSUSED */
void
prstop(int why, int what)
{
	register klwp_t *lwp = ttolwp(curthread);
	register struct regs *r = lwptoregs(lwp);
	proc_t *p = lwptoproc(lwp);
	int mapped;

	/*
	 * Make sure we don't deadlock on a recursive call to prstop().
	 * stop() tests the lwp_nostop flag.
	 */
	ASSERT(lwp->lwp_nostop == 0);
	lwp->lwp_nostop = 1;

	mapped = 0;
	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage((caddr_t)r->r_pc,
			sizeof (lwp->lwp_pcb.pcb_instr), S_READ, 1);
	if (_copyin((caddr_t)r->r_pc, (caddr_t)&lwp->lwp_pcb.pcb_instr,
	    sizeof (lwp->lwp_pcb.pcb_instr)) == 0)
		lwp->lwp_pcb.pcb_flags |= PCB_INSTR_VALID;
	else {
		lwp->lwp_pcb.pcb_flags &= ~PCB_INSTR_VALID;
		lwp->lwp_pcb.pcb_instr = 0;
	}
	if (mapped)
		pr_unmappage((caddr_t)r->r_pc,
			sizeof (lwp->lwp_pcb.pcb_instr), S_READ, 1);
	(void) save_syscall_args();

	ASSERT(lwp->lwp_nostop == 1);
	lwp->lwp_nostop = 0;
}

/*
 * Fetch the user-level instruction on which the lwp is stopped.
 * It was saved by the lwp itself, in prstop().
 * Return non-zero if the instruction is valid.
 */
int
prfetchinstr(lwp, ip)
	register klwp_t *lwp;
	register u_long *ip;
{
	*ip = (u_long)lwp->lwp_pcb.pcb_instr;
	return (lwp->lwp_pcb.pcb_flags & PCB_INSTR_VALID);
}

/*
 * Called from trap() when a load or store instruction
 * falls in a watched page but is not a watchpoint.
 * We emulate the instruction in the kernel.
 */
/* ARGSUSED */
int
pr_watch_emul(struct regs *rp, caddr_t addr, enum seg_rw rw)
{
#ifdef	SOMEDAY
	int res;
	proc_t *p = curproc;
	char *badaddr = (caddr_t)(-1);
	int mapped;

	/* prevent recursive calls to pr_watch_emul() */
	ASSERT(!(curthread->t_flag & T_WATCHPT));
	curthread->t_flag |= T_WATCHPT;

	mapped = pr_mappage(addr, 8, rw, 1);
	res = do_unaligned(rp, &badaddr);
	if (mapped)
		pr_unmappage(addr, 8, rw, 1);

	curthread->t_flag &= ~T_WATCHPT;
	if (res == SIMU_SUCCESS) {
		/* adjust the pc */
		return (1);
	}
#endif
	return (0);
}
