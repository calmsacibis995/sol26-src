/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)vm_machdep.c	1.28	96/07/01 SMI"

/*
 * UNIX machine dependent virtual memory support.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/vm.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/cred.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg_kmem.h>

#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/obpdefs.h>
#include <sys/bootconf.h>

#include <vm/hat_ppcmmu.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/elf_ppc.h>

u_int
page_num_pagesizes() {
	return (1);
}

u_int
page_get_pagesize(u_int n) {

	if (n != 0)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);

	return (MMU_PAGESIZE);
}

/*
 * Handle a pagefault.
 */
faultcode_t
pagefault(addr, type, rw, iskernel)
	register caddr_t addr;
	register enum fault_type type;
	register enum seg_rw rw;
	register int iskernel;
{
	register struct as *as;
	register struct proc *p;
	register faultcode_t res;
	caddr_t base;
	u_int len;
	int err;

	if (iskernel) {
		as = &kas;
	} else {
		p = curproc;
		as = p->p_as;
	}

	/*
	 * Dispatch pagefault.
	 */
	res = as_fault(as->a_hat, as, addr, 1, type, rw);

	/*
	 * If this isn't a potential unmapped hole in the user's
	 * UNIX data or stack segments, just return status info.
	 */
	if (!(res == FC_NOMAP && iskernel == 0))
		return (res);

	/*
	 * Check to see if we happened to faulted on a currently unmapped
	 * part of the UNIX data or stack segments.  If so, create a zfod
	 * mapping there and then try calling the fault routine again.
	 */
	base = p->p_brkbase;
	len = p->p_brksize;

	if (addr < base || addr >= base + len) {		/* data seg? */
		base = (caddr_t)((caddr_t)USRSTACK - p->p_stksize);
		len = p->p_stksize;
		if (addr < base || addr >= (caddr_t)USRSTACK) {	/* stack seg? */
			/* not in either UNIX data or stack segments */
			return (FC_NOMAP);
		}
	}

	/* the rest of this function implements a 3.X 4.X 5.X compatibility */
	/* This code is probably not needed anymore */

	/* expand the gap to the page boundaries on each side */
	len = (((u_int)base + len + PAGEOFFSET) & PAGEMASK) -
	    ((u_int)base & PAGEMASK);
	base = (caddr_t)((u_int)base & PAGEMASK);

	as_rangelock(as);
	if (as_gap(as, NBPG, &base, &len, AH_CONTAIN, addr) != 0) {
		/*
		 * Since we already got an FC_NOMAP return code from
		 * as_fault, there must be a hole at `addr'.  Therefore,
		 * as_gap should never fail here.
		 */
		panic("pagefault as_gap");
	}

	err = as_map(as, base, len, segvn_create, zfod_argsp);
	as_rangeunlock(as);
	if (err)
		return (FC_MAKE_ERR(err));

	return (as_fault(as->a_hat, as, addr, 1, F_INVAL, rw));
}

/*ARGSUSED*/
void
map_addr(addrp, len, off, align)
	caddr_t *addrp;
	register u_int len;
	offset_t off;
	int align;
{
	map_addr_proc(addrp, len, off, align, curproc);
}

/*
 * map_addr_proc() is the routine called when the system is to
 * chose an address for the user.  We will pick an address range which is
 * just below the user text or below the current stack limit.
 *
 * As per PowerPC ABI the regions (0x10000 to USRTEXT) and (user_stack_limit
 * to program_break_address) can be used for dynamic segments. Using the
 * address space just below the USRTEXT (or whatever the text origin of the
 * process) is an optimization to have the relative addresses within a 32M
 * range (limited by the relative address branching on PPC). So, the allocation
 * is done in the following regions in that order:
 *
 *	Region 1: 0x10000 to USERTEXT
 *		Allocation is from high end to low end.
 *	Region 2: program_break_address to user_stack_limit
 *		Allocation is from high end to low end.
 *
 * addrp is a value/result parameter.
 *	On input it is a hint from the user to be used in a completely
 *	machine dependent fashion.  We decide to completely ignore this hint.
 *
 *	On output it is NULL if no address can be found in the current
 *	processes address space or else an address that is currently
 *	not mapped for len bytes with a page of red zone on either side.
 *	If align is true, then the selected address will obey the alignment
 *	constraints of a vac machine based on the given off value.
 */
/*ARGSUSED*/
void
map_addr_proc(addrp, len, off, align, p)
	caddr_t *addrp;
	register u_int len;
	offset_t off;
	int align;
	struct proc *p;
{
	register struct as *as = p->p_as;
	register caddr_t addr;
	caddr_t base;
	u_int slen;
	u_int align_amount;

	len = (len + PAGEOFFSET) & PAGEMASK;

	/*
	 * Redzone for each side of the request. This is done to leave
	 * one page unmapped between segments. This is not required, but
	 * it's useful for the user because if their program strays across
	 * a segment boundary, it will catch a fault immediately making
	 * debugging a little easier.
	 */
	len += 2 * PAGESIZE;

	/*
	 * Use ELF_PPC_MAXPGSZ (64k) alignment for better ld.so.1 behavior.
	 *
	 * (The PPC ABI specifies 64k for p_align field of program header
	 * for shared objects. We do the alignement here so that the
	 * run-time linker does not have to do additional system
	 * calls (i.e mmap/munmap) to enforce alignment on the objects.)
	 */
	align_amount = ELF_PPC_MAXPGSZ;
	len += align_amount;

	/*
	 * Look for a large enough hole starting below the start of user
	 * text origin. After finding it, use the upper part.  Addition of
	 * PAGESIZE is for the redzone as described above.
	 */
	base = (caddr_t)0x10000;
	if (p->p_brkbase > base) {
		slen = (u_int)p->p_brkbase - (u_int)base;
		if (as_gap(as, len, &base, &slen, AH_LO, (caddr_t)NULL) == 0) {
			caddr_t as_addr;

			addr = base + slen - len  + PAGESIZE;
			as_addr = addr;

			/*
			 * Round address DOWN to the alignment amount,
			 * add the offset, and if this address is less
			 * than the original address, add alignment amount.
			 */
			addr = (caddr_t)((u_int)addr & ~(align_amount - 1));
			addr += off & (align_amount - 1);
			if (addr < as_addr)
				addr += align_amount;
			*addrp = addr;
			return;
		}
	}

	/*
	 * Look for a large enough hole starting below the stack limit.
	 * After finding it, use the upper part.  Addition of PAGESIZE is
	 * for the redzone as described above.
	 */
	base = p->p_brkbase;
	slen = (caddr_t)USRSTACK - base
	    - (((rlim_t)U_CURLIMIT(&u, RLIMIT_STACK) +
	    PAGEOFFSET) & PAGEMASK);

	if (as_gap(as, len, &base, &slen, AH_HI, (caddr_t)NULL) == 0) {
		caddr_t as_addr;

		addr = base + slen - len  + PAGESIZE;
		as_addr = addr;

		/*
		 * Round address DOWN to the alignment amount,
		 * add the offset, and if this address is less
		 * than the original address, add alignment amount.
		 */
		addr = (caddr_t)((u_int)addr & ~(align_amount - 1));
		addr += off & (align_amount - 1);
		if (addr < as_addr)
			addr += align_amount;
		*addrp = addr;
	}
	else
		*addrp = ((caddr_t)NULL);	/* no more virtual space */
}

/*
 * Determine whether [base, base+len] contains a mapable range of
 * addresses at least minlen long. base and len are adjusted if
 * required to provide a mapable range.
 */
/* ARGSUSED3 */
int
valid_va_range(basep, lenp, minlen, dir)
	register caddr_t *basep;
	register u_int *lenp;
	register u_int minlen;
	register int dir;
{
	register caddr_t hi, lo;

	lo = *basep;
	hi = lo + *lenp;

	/*
	 * If hi rolled over the top, try cutting back.
	 */
	if (hi < lo) {
		if (0 - (u_int)lo + (u_int)hi < minlen)
			return (0);
		if (0 - (u_int)lo < minlen)
			return (0);
		*lenp = 0 - (u_int)lo;
	} else if (hi - lo < minlen)
		return (0);
	return (1);
}

/*
 * Determine whether [addr, addr+len] are valid user addresses.
 */
valid_usr_range(addr, len)
	register caddr_t addr;
	register u_int len;
{
	register caddr_t eaddr = addr + len;

	if (eaddr <= addr || addr >= (caddr_t)KERNELBASE ||
	    eaddr > (caddr_t)KERNELBASE)
		return (0);
	return (1);
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of PAGESIZE.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	int size;
{
	register struct spte *fpte;

	if ((size % PAGESIZE) != 0)
		panic("pagemove");

	for (fpte = &Sysmap[mmu_btop((int)from - SYSBASE)]; size > 0;
	    size -= PAGESIZE, from += PAGESIZE,
	    to += PAGESIZE, fpte += CLSIZE) {
		u_int tpf;

		ASSERT(spte_valid(fpte));
		/*
		 * We save the old page frame info and unmap
		 * the old address "from" before we set up the
		 * new mapping to new address "to" to avoid
		 * VAC conflicts
		 */
		tpf = fpte->spte_ppn;
		segkmem_mapout(&kvseg, (caddr_t)from, PAGESIZE);
		segkmem_mapin(&kvseg, (caddr_t)to, PAGESIZE,
		    PROT_READ | PROT_WRITE, tpf, 0);
	}
}

/*
 * Return 1 if the page frame is onboard DRAM memory, else 0.
 */
int
pf_is_memory(u_int pf)
{
	extern struct memlist *phys_install;

	return (address_in_memlist(phys_install, (caddr_t)ptob(pf), 1));
}

static page_t *page_freelist;
static page_t   *page_cachelist;
static kmutex_t freelist_lock;
static kmutex_t cachelist_lock;

/*
 * Just called this because all the other silly platforms us
 * this name.  Here, it just needs to set up the mutexes.
 */
void
page_coloring_init()
{
	mutex_init(&freelist_lock, "page_freelist", MUTEX_DEFAULT, NULL);
	mutex_init(&cachelist_lock, "page_cachelist", MUTEX_DEFAULT, NULL);
}

/* ARGSUSED */
page_t *
page_get_freelist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	size_t size,
	u_int flags)
{
	page_t		*pp, *first_pp;

	if (size != MMU_PAGESIZE)
		return (NULL);

	mutex_enter(&freelist_lock);
	pp = page_freelist;
	if (pp != NULL) {
		ASSERT(PP_ISFREE(pp));
		ASSERT(PP_ISAGED(pp));
		ASSERT(pp->p_vnode == NULL);
		ASSERT(pp->p_hash == NULL);
		ASSERT(pp->p_offset == (u_offset_t)-1);
		first_pp = pp;
		/*
		 * Walk down the freelist
		 */
		while (!page_trylock(pp, SE_EXCL)) {
			pp = pp->p_next;
			ASSERT(PP_ISFREE(pp));
			ASSERT(PP_ISAGED(pp));
			ASSERT(pp->p_vnode == NULL);
			ASSERT(pp->p_hash == NULL);
			ASSERT(pp->p_offset == (u_offset_t)-1);
			if (pp == first_pp) {
				pp = NULL;
				break;
			}
		}
		if (pp != NULL) {
			page_sub(&page_freelist, pp);
			if ((PP_ISFREE(pp) == 0) ||
			    (PP_ISAGED(pp) == 0)) {
				cmn_err(CE_PANIC,
				    "free page is not. pp %x", pp);
			}
			mutex_exit(&freelist_lock);
			return (pp);
		}
	}
	mutex_exit(&freelist_lock);
	return (pp);
}

/* ARGSUSED */
page_t *
page_get_cachelist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	u_int flags)
{
	page_t		*pp;
	page_t		*first_pp;

	mutex_enter(&cachelist_lock);
	if ((pp = page_cachelist) != NULL) {
		first_pp = pp;
		ASSERT(pp->p_vnode);
		ASSERT(PP_ISAGED(pp) == 0);
		while (!page_trylock(pp, SE_EXCL)) {
			pp = pp->p_next;
			if (pp == first_pp) {
				/*
				 * Searched the complete list!
				 * And all of them (might only
				 * be one) are locked. This can
				 * happen because these pages
				 * can also be found via the
				 * hash list.  When they are,
				 * they are locked first, then
				 * removed.  We give up and let
				 * the other thread run.
				 */
				pp = NULL;
				break;
			}
			ASSERT(pp->p_vnode);
			ASSERT(PP_ISFREE(pp));
			ASSERT(PP_ISAGED(pp) == 0);
		}
		if (pp) {
			/*
			 * found and locked a page.
			 * pull it off the list.
			 */
			ASSERT(se_excl_assert(&pp->p_selock));
			page_sub(&page_cachelist, pp);
		}
	}
	mutex_exit(&cachelist_lock);
	return (pp);
}

void
page_list_add(int list, page_t *pp, int where)
{
	kmutex_t	*lm;
	page_t		**ppp;

	ASSERT(se_excl_assert(&pp->p_selock));
	ASSERT(PP_ISFREE(pp));
	ASSERT(((machpage_t *)pp)->p_mapping == NULL);

	if (list == PG_FREE_LIST) {
		ASSERT(PP_ISAGED(pp));
		ppp = &page_freelist;
		lm = &freelist_lock;
	} else {
		ASSERT(pp->p_vnode != NULL);
		ppp = &page_cachelist;
		lm = &cachelist_lock;
	}

	mutex_enter(lm);
	page_add(ppp, pp);
	if (where == PG_LIST_TAIL) {
		*ppp = (*ppp)->p_next;
	}
	mutex_exit(lm);

	/*
	 * It is up to the caller to unlock the page!
	 */
	ASSERT(se_excl_assert(&pp->p_selock));
}

void
page_list_sub(int list, page_t *pp)
{
	kmutex_t	*lm;
	page_t		**ppp;

	ASSERT(se_excl_assert(&pp->p_selock));
	ASSERT(PP_ISFREE(pp));

	if (list == PG_FREE_LIST) {
		ppp = &page_freelist;
		lm = &freelist_lock;
	} else {
		ppp = &page_cachelist;
		lm = &cachelist_lock;
	}

	mutex_enter(lm);
	page_sub(ppp, pp);
	mutex_exit(lm);
}
