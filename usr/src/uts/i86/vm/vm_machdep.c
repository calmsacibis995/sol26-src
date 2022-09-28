/*
 * Copyright (c) 1987, 1992 by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)vm_machdep.c 1.30     96/08/01 SMI"

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
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/debug.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <vm/mach_page.h>

#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/memlist.h>

#include <vm/hat_i86.h>
#include <sys/x86_archext.h>
#include <sys/cmn_err.h>

u_int
page_num_pagesizes() {
	return ((x86_feature & X86_LARGEPAGE) ? 2 : 1);
}

u_int
page_get_pagesize(u_int n) {

	if (n > 1)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);

	return ((n == 0) ? PAGESIZE : FOURMB_PAGESIZE);
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
 * choose an address for the user.  We will pick an address
 * range which is the lowest available that is above 0x80000000.
 * The algorithm used for cache consistency on machines with virtual
 * address caches is such that offset 0 in the vnode is always
 * on a shm_alignment-aligned address.  Unfortunately, this
 * means that vnodes which are demand paged will not be mapped
 * cache consistently with the executable images.  When the
 * cache alignment for a given object is inconsistent, the
 * lower level code must manage the translations so that this
 * is not seen here (at the cost of efficiency, of course).
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

	base = (caddr_t)(((u_int)p->p_brkbase + FOURMB_PAGESIZE)
		& ~FOURMB_PAGEOFFSET);
	slen = (caddr_t)KERNELBASE - base;
	len = (len + PAGEOFFSET) & PAGEMASK;
	if (len >= FOURMB_PAGESIZE) {
		/*
		 * we need to return 4MB aligned address
		 */
		len += FOURMB_PAGESIZE;
		align = 1;
	} else {

		/*
		 * Redzone for each side of the request. This is done to leave
		 * one page unmapped between segments. This is not required, but
		 * it's useful for the user because if their program strays
		 * across a segment boundary, it will catch a fault
		 * immediately making debugging a little easier.
		 */
		len += 2 * PAGESIZE;
		align = 0;
	}

#ifdef NEVER
	if (vac && align)
		len += 2 * shm_alignment;
#endif NEVER

	/*
	 * Look for a large enough hole starting above p->p_brkbase
	 * After finding it, use the lower part.
	 */
	if (as_gap(as, len, &base, &slen, AH_LO, (caddr_t)NULL) == 0) {
		base = base + slen - len;
		if (align)
			addr = (caddr_t)(((u_int)base + FOURMB_PAGESIZE) &
			    ~FOURMB_PAGEOFFSET);
		else
			addr = base + PAGESIZE;
#ifdef NEVER
		/*
		 * If this remains "non-generic" code, we will not use it
		 * because we do not expect to see a virtual address cache.
		 * This code is being left here in case this module
		 *  becomes a candidate for being generic.
		 */
		if (vac && align) {
			/*
			 * Adjust up to the next shm_alignment boundary, then
			 * up by the pos offset in shm_alignment from there.
			 */
			addr = (caddr_t)roundup((u_int)addr, shm_alignment);
			addr += (int)(off & (shm_alignment - 1));
		}
#endif NEVER
		*addrp = addr;
	} else {
		*addrp = ((caddr_t)NULL);	/* no more virtual space */
	}
}

/*
 * Determine whether [base, base+len] contains a mapable range of
 * addresses at least minlen long. base and len are adjusted if
 * required to provide a mapable range.
 */
/*ARGSUSED3*/
int
valid_va_range(register caddr_t *basep, register u_int *lenp,
	register u_int minlen, register int dir)
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
 * Invalidate a single page in the TLB.
 * For 386s (machines earlier than the 486), there is no way of
 * invalidating a single page, so we have to invalidate the whole TLB.
 */
void
mmu_tlbflush_entry(addr)
	caddr_t addr;
{
	if ((cputype & CPU_ARCH) == I86_386_ARCH)
		mmu_tlbflush_all();
	else
		invlpg(addr);
}

/*
 * Return 1 if the page frame is onboard memory, else 0.
 */
int
pf_is_memory(pf)
	u_int pf;
{
	extern struct memlist *phys_install;
	extern int address_in_memlist(struct memlist *mp, caddr_t va,
		u_int len);

	return (address_in_memlist(phys_install, (caddr_t)ptob(pf), 1));
}

#ifdef notdef
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
	register struct pte *fpte;

	if ((size % PAGESIZE) != 0)
		panic("pagemove");

	for (fpte = &Sysmap[mmu_btop((int)from - SYSBASE)]; size > 0;
	    size -= PAGESIZE, from += PAGESIZE,
	    to += PAGESIZE, fpte += CLSIZE) {
		u_int tpf;

		/*
		 * We save the old page frame info and unmap
		 * the old address "from" before we set up the
		 * new mapping to new address "to" to avoid
		 * VAC conflicts
		 */
		tpf = MAKE_PFNUM(fpte);
		segkmem_mapout(&kvseg, (caddr_t)from, PAGESIZE);
		segkmem_mapin(&kvseg, (caddr_t)to, PAGESIZE,
		    PROT_READ | PROT_WRITE, tpf, 0);
	}
}
#endif /* notdef */

#ifdef notdef
/*
 * Check for valid program size
 */
/*ARGSUSED*/
chksize(ts, ds, ss)
	unsigned ts, ds, ss;
{
	/*
	 * Most of the checking is done by the as layer routines, we
	 * simply check the resource limits for data and stack here.
	 */
	if (ctob(ds) > (rlim_t)U_CURLIMIT(&u, RLIMIT_DATA) ||
	    ctob(ss) > (rlim_t)U_CURLIMIT(&u, RLIMIT_STACK)) {
		return (ENOMEM);
	}

	return (0);
}
#endif /* notdef */

/*
 * Routine used to check to see if an a.out can be executed
 * by the current machine/architecture.
 */
chkaout(exp)
	struct exdata *exp;
{

	if (exp->ux_mach == M_SPARC)
		return (0);
	else
		return (ENOEXEC);
}

/*
 * The following functions return information about an a.out
 * which is used when a program is executed.
 */

#ifdef notdef
/*
 * Return the size of the text segment.
 */
long
getts(exp)
	struct exec *exp;
{

	return ((long)clrnd(btoc(exp->a_text)));
}
#endif /* notdef */

#ifdef notdef
/*
 * Return the size of the data segment.
 */
long
getds(exp)
	struct exec *exp;
{

	return ((long)clrnd(btoc(exp->a_data + exp->a_bss)));
}
#endif /* notdef */

/*
 * Return the load memory address for the data segment.
 */
caddr_t
getdmem(exp)
	struct exec *exp;
{
	/*
	 * XXX - Sparc Reference Hack approaching
	 * Remember that we are loading
	 * 8k executables into a 4k machine
	 * DATA_ALIGN == 2 * NBPG
	 */
	if (exp->a_text)
		return ((caddr_t)(roundup(USRTEXT + exp->a_text, DATA_ALIGN)));
	else
		return ((caddr_t)USRTEXT);
}

/*
 * Return the starting disk address for the data segment.
 */
u_int
getdfile(struct exec *exp)
{

	if (exp->a_magic == ZMAGIC)
		return (exp->a_text);
	else
		return (sizeof (struct exec) + exp->a_text);
}

/*
 * Return the load memory address for the text segment.
 */

/*ARGSUSED*/
caddr_t
gettmem(exp)
	struct exec *exp;
{

	return ((caddr_t)USRTEXT);
}

/*
 * Return the file byte offset for the text segment.
 */
u_int
gettfile(struct exec *exp)
{

	if (exp->a_magic == ZMAGIC)
		return (0);
	else
		return (sizeof (struct exec));
}

void
getexinfo(
	struct exdata *edp_in,
	struct exdata *edp_out,
	int *pagetext,
	int *pagedata)
{
	*edp_out = *edp_in;	/* structure copy */

	if ((edp_in->ux_mag == ZMAGIC) &&
	    ((edp_in->vp->v_flag & VNOMAP) == 0)) {
		*pagetext = 1;
		*pagedata = 1;
	} else {
		*pagetext = 0;
		*pagedata = 0;
	}
}

/*
 * initialized by page_coloring_init()
 */
static u_int	page_colors = 1;
static u_int	page_colors_mask;

/*
 * page freelist and cachelist are hashed into bins based on color
 */
#define	PAGE_COLORS_MAX	256

static	page_t *page_freelists[PAGE_COLORS_MAX];
static	page_t *page_cachelists[PAGE_COLORS_MAX];

/*
 * There are at most 256 colors/bins.  Spread them out under a
 * couple of locks.  There are mutexes for both the page freelist
 * and the page cachelist.
 */

#define	PC_SHIFT	(4)
#define	NPC_MUTEX	(PAGE_COLORS_MAX/(1 << PC_SHIFT))

static kmutex_t	fpc_mutex[NPC_MUTEX];
static kmutex_t	cpc_mutex[NPC_MUTEX];

#if	defined(COLOR_STATS) || defined(DEBUG)

#define	COLOR_STATS_INC(x) (x)++;
#define	COLOR_STATS_DEC(x) (x)--;

static	u_int	pf_size[PAGE_COLORS_MAX];
static	u_int	pc_size[PAGE_COLORS_MAX];

static	u_int	sys_nak_bins[PAGE_COLORS_MAX];
static	u_int	sys_req_bins[PAGE_COLORS_MAX];

#else	COLOR_STATS

#define	COLOR_STATS_INC(x)
#define	COLOR_STATS_DEC(x)

#endif	COLOR_STATS


#define	PP_2_BIN(pp) (((machpage_t *)(pp))->p_pagenum & page_colors_mask)

#define	PC_BIN_MUTEX(bin, list)	((list == PG_FREE_LIST)? \
	&fpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))] :	\
	&cpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))])

/*
 * hash `as' and `vaddr' to get a bin.
 * sizeof(struct as) is 60.
 * shifting down by 4 bits will cause consecutive as's to be offset by ~3.
 */
#define	AS_2_BIN(as, vaddr) \
	((((u_int)(vaddr) >> PAGESHIFT) + ((u_int)(as) >> 3)) \
	& page_colors_mask)


#define	VADDR_2_BIN(as, vaddr) AS_2_BIN(as, vaddr)

static page_t *
page_4mb_try(u_int b_pfn)
{
	int	i;
	page_t  *pplist = NULL, *pp;
	extern page_t *page_numtopp_nolock(u_int pfn);

	/*
	 * Make sure we are called with a pfn that is 4MB aligned.
	 */
	if (! IS_P2ALIGNED(b_pfn << PAGESHIFT, FOURMB_PAGESIZE)) {
		cmn_err(CE_PANIC, "page_4mb_try: pfn %d not 4mb aligned",
			b_pfn);
	}

	for (i = b_pfn; i < (b_pfn + NPTEPERPT); i++) {

		pp = page_numtopp_nolock(i);

		if ((pp == NULL) || (!page_trylock(pp, SE_EXCL)))
			goto failed;
		if (!PP_ISFREE(pp) || !PP_ISAGED(pp)) {
			page_unlock(pp);
			goto failed;
		}
		page_list_sub(PG_FREE_LIST, pp);
		PP_CLRFREE(pp);
		PP_CLRAGED(pp);
		PP_SETREF(pp);
		page_list_concat(&pplist, &pp);
	}

	ASSERT(IS_P2ALIGNED(((machpage_t *)pplist)->p_pagenum << PAGESHIFT,
	    FOURMB_PAGESIZE));
	ASSERT(((machpage_t *)pplist->p_prev)->p_pagenum
		== (((machpage_t *)pplist)->p_pagenum + (NPTEPERPT - 1)));

	return (pplist);
failed:
	while (pplist) {
		pp = pplist;
		page_sub(&pplist, pp);
		PP_SETFREE(pp);
		PP_CLRALL(pp);
		PP_SETAGED(pp);
		page_list_add(PG_FREE_LIST, pp, PG_LIST_TAIL);
		page_unlock(pp);
	}
	return (NULL);
}

static page_t *
page_get_4mb_page(void) {

	u_int	start_pfn, pfn;
	page_t *pplist;

	static u_int last_pfn = 0;

	/*
	 * XXX I assume this is because you know that the
	 * first 4MB of physical memory is not free?
	 */
	start_pfn = FOURMB_PAGESIZE / PAGESIZE;

	if (last_pfn == 0) {
		struct memseg *tseg;

		for (tseg = memsegs; tseg; tseg = tseg->next)
				if (tseg->pages_end > last_pfn)
					last_pfn = tseg->pages_end;
	}

	for (pfn = start_pfn; pfn < last_pfn; pfn += NPTEPERPT) {
		if (pplist = page_4mb_try(pfn)) {
			return (pplist);
		}
	}
	return (NULL);
}

/*
 * Take a particular page off of whatever freelist the page is claimed to be on.
 */
void
page_list_sub(int list, page_t *pp)
{
	u_int		bin;
	kmutex_t	*pcm;
	page_t		**ppp;

	ASSERT(se_excl_assert(&pp->p_selock));
	ASSERT(PP_ISFREE(pp));

	bin = PP_2_BIN(pp);
	pcm = PC_BIN_MUTEX(bin, list);

	if (list == PG_FREE_LIST) {
		ppp = &page_freelists[bin];
		COLOR_STATS_DEC(pf_size[bin]);
		ASSERT(PP_ISAGED(pp));

		ASSERT(page_pptonum(pp) <= physmax);
	} else {
		ppp = &page_cachelists[bin];
		COLOR_STATS_DEC(pc_size[bin]);
		ASSERT(PP_ISAGED(pp) == 0);
	}

	mutex_enter(pcm);
	page_sub(ppp, pp);
	mutex_exit(pcm);
}

void
page_list_add(int list, page_t *pp, int where)
{
	page_t		**ppp;
	kmutex_t	*pcm;
	u_int		bin;
	u_int		*pc_stats;

	ASSERT(se_excl_assert(&pp->p_selock));
	ASSERT(PP_ISFREE(pp));
	ASSERT(!hat_page_is_mapped(pp));

	bin = PP_2_BIN(pp);
	pcm = PC_BIN_MUTEX(bin, list);

	if (list == PG_FREE_LIST) {
		ASSERT(PP_ISAGED(pp));
		/* LINTED */
		ASSERT(pc_stats = &pf_size[bin]);  /* the `=' is correct */
		ppp = &page_freelists[bin];

		ASSERT(page_pptonum(pp) <= physmax);
	} else {
		ASSERT(pp->p_vnode);
		ASSERT((pp->p_offset & 0xfff) == 0);
		/* LINTED */
		ASSERT(pc_stats = &pc_size[bin]);  /* the `=' is correct */
		ppp = &page_cachelists[bin];
	}

	mutex_enter(pcm);
	COLOR_STATS_INC(*pc_stats);
	page_add(ppp, pp);

	if (where == PG_LIST_TAIL) {
		*ppp = (*ppp)->p_next;
	}
	mutex_exit(pcm);

	/*
	 * It is up to the caller to unlock the page!
	 */
	ASSERT(se_excl_assert(&pp->p_selock));
}


/*
 * When a bin is empty, and we can't satisfy a color request correctly,
 * we scan.  If we assume that the programs have reasonable spatial
 * behavior, then it will not be a good idea to use the adjacent color.
 * Using the adjacent color would result in virtually adjacent addresses
 * mapping into the same spot in the cache.  So, if we stumble across
 * an empty bin, skip a bunch before looking.  After the first skip,
 * then just look one bin at a time so we don't miss our cache on
 * every look. Be sure to check every bin.  Page_create() will panic
 * if we miss a page.
 *
 * This also explains the `<=' in the for loops in both page_get_freelist()
 * and page_get_cachelist().  Since we checked the target bin, skipped
 * a bunch, then continued one a time, we wind up checking the target bin
 * twice to make sure we get all of them bins.
 */
#define	BIN_STEP	19

/*
 * Find the `best' page on the freelist for this (vp,off) (as,vaddr) pair.
 *
 * Does its own locking and accounting.
 * If PG_MATCH_COLOR is set, then NULL will be returned if there are no
 * pages of the proper color even if there are pages of a different color.
 *
 * Finds a page, removes it, THEN locks it.
 */
/*ARGSUSED*/
page_t *
page_get_freelist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	size_t size,
	u_int flags)
{
	u_int		bin;
	kmutex_t	*pcm;
	int		i;
	page_t		*pp, *first_pp;

	/*
	 * For now we only handle 4MB and 4k pages.
	 */
	switch (size) {
		case MMU_PAGESIZE:
			break;
		case FOURMB_PAGESIZE:
			return (page_get_4mb_page());

		default:
			cmn_err(CE_PANIC,
		"page_get_freelist: illegal size request for i86 platform");
	}
	/*
	 * Only hold one freelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = VADDR_2_BIN(as, vaddr);

	for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_freelists[bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_FREE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_freelists[bin]) {
				/*
				 * These were set before the page
				 * was put on the free list,
				 * they must still be set.
				 */
				ASSERT(PP_ISFREE(pp));
				ASSERT(PP_ISAGED(pp));
				ASSERT(pp->p_vnode == NULL);
				ASSERT(pp->p_hash == NULL);
				ASSERT(pp->p_offset == (u_offset_t)-1);
				first_pp = pp;
				/*
				 * Walk down the hash chain
				 */

				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;

					ASSERT(PP_ISFREE(pp));
					ASSERT(PP_ISAGED(pp));
					ASSERT(pp->p_vnode == NULL);
					ASSERT(pp->p_hash == NULL);
					ASSERT(pp->p_offset == -1);

					if (pp == first_pp) {
						pp = NULL;
						break;
					}
				}

				if (pp != NULL) {
				    COLOR_STATS_DEC(pf_size[bin]);
				    page_sub(&page_freelists[bin], pp);

				    ASSERT(page_pptonum(pp) <= physmax);

				    if ((PP_ISFREE(pp) == 0) ||
					(PP_ISAGED(pp) == 0)) {
					cmn_err(CE_PANIC,
					    "free page is not. pp %x", (int)pp);
				    }
				    mutex_exit(pcm);
				    return (pp);
				}
			}
			mutex_exit(pcm);
		}

		/*
		 * Wow! The bin was empty.
		 */
		COLOR_STATS_INC(sys_nak_bins[bin]);
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	}
	return (NULL);
}

/*
 * Find the `best' page on the cachelist for this (vp,off) (as,vaddr) pair.
 *
 * Does its own locking.
 * If PG_MATCH_COLOR is set, then NULL will be returned if there are no
 * pages of the proper color even if there are pages of a different color.
 * Otherwise, scan the bins for ones with pages.  For each bin with pages,
 * try to lock one of them.  If no page can be locked, try the
 * next bin.  Return NULL if a page can not be found and locked.
 *
 * Finds a pages, TRYs to lock it, then removes it.
 */
/*ARGSUSED*/
page_t *
page_get_cachelist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	u_int flags)
{
	kmutex_t	*pcm;
	int		i;
	page_t		*pp;
	page_t		*first_pp;
	int		bin;

	/*
	 * Only hold one cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = VADDR_2_BIN(as, vaddr);

	for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_cachelists[bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_cachelists[bin]) {
				first_pp = pp;
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);
				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;
					if (pp == first_pp) {
						/*
						 * We have searched the
						 * complete list!
						 * And all of them (might
						 * only be one) are locked.
						 * This can happen since
						 * these pages can also be
						 * found via the hash list.
						 * When found via the hash
						 * list, they are locked
						 * first, then removed.
						 * We give up to let the
						 * other thread run.
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
					 * Found and locked a page.
					 * Pull it off the list.
					 */
					COLOR_STATS_DEC(pc_size[bin]);
					page_sub(&page_cachelists[bin], pp);
					mutex_exit(pcm);
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISAGED(pp) == 0);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}
		COLOR_STATS_INC(sys_nak_bins[bin]);
		/*
		 * Wow! The bin was empty or no page could be locked.
		 * If only the proper bin is to be checked, get out
		 * now.
		 */
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	}


	return (NULL);
}
int	x86_l2cache = 512 * 1024;

/*
 * page_coloring_init()
 * called once at startup from kphysm_init() -- before memialloc()
 * is invoked to do the 1st page_free()/page_freelist_add().
 *
 * initializes page_colors and page_colors_mask
 */
void
page_coloring_init()
{
	u_int colors;


	colors = x86_l2cache/ PAGESIZE;	/* 256 */
	if (colors > PAGE_COLORS_MAX - 1)
		colors = PAGE_COLORS_MAX - 1;
	page_colors = colors;
	page_colors_mask = colors - 1;
}
