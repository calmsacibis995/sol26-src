/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)vm_machdep.c 1.34     96/04/23 SMI"

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
#include <sys/cmn_err.h>
#include <sys/vmsystm.h>
#include <sys/bitmap.h>
#include <sys/debug.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <vm/mach_page.h>

#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/atomic_prim.h>
#include <sys/elf_SPARC.h>
#include <sys/machsystm.h>

#include <vm/hat_sfmmu.h>

#include <sys/memnode.h>

#include <sys/kmem.h>

extern	u_int	shm_alignment;	/* VAC address consistency modulus */

extern int vac_size;
extern int vac_shift;

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
map_addr(caddr_t *addrp, u_int len, offset_t off, int align)
{
	map_addr_proc(addrp, len, off, align, curproc);
}

/*
 * map_addr_proc() is the routine called when the system is to
 * chose an address for the user.  We will pick an address
 * range which is just below the current stack limit.  The
 * algorithm used for cache consistency on machines with virtual
 * address caches is such that offset 0 in the vnode is always
 * on a shm_alignment'ed aligned address.  Unfortunately, this
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
map_addr_proc(caddr_t *addrp, u_int len, offset_t off, int align,
		struct proc *p)
{
	register struct as *as = p->p_as;
	register caddr_t addr;
	caddr_t base;
	u_int slen;
	u_int align_amount;

	base = p->p_brkbase;
	slen = (caddr_t)USRSTACK - base
	    - (((rlim_t)U_CURLIMIT(&u, RLIMIT_STACK) + PAGEOFFSET) &
	    PAGEMASK);
	len = (len + PAGEOFFSET) & PAGEMASK;

	/*
	 * Redzone for each side of the request. This is done to leave
	 * one page unmapped between segments. This is not required, but
	 * it's useful for the user because if their program strays across
	 * a segment boundary, it will catch a fault immediately making
	 * debugging a little easier.
	 */
	len += (2 * PAGESIZE);

	/*
	 *  If the request is larger than the size of a particular
	 *  mmu level, then we use that level to map the request.
	 *  But this requires that both the virtual and the physical
	 *  addresses be aligned with respect to that level, so we
	 *  do the virtual bit of nastiness here.
	 */
	if (len >= MMU_PAGESIZE4M) {  /* 4MB mappings */
		align_amount = MMU_PAGESIZE4M;
	} else if (len >= MMU_PAGESIZE512K) { /* 512KB mappings */
		align_amount = MMU_PAGESIZE512K;
	} else if (len >= MMU_PAGESIZE64K) { /* 64KB mappings */
		align_amount = MMU_PAGESIZE64K;
	} else  {
		/*
		 * Align virtual addresses on a 64K boundary to ensure
		 * that ELF shared libraries are mapped with the appropriate
		 * alignment constraints by the run-time linker.
		 */
		align_amount = ELF_SPARC_MAXPGSZ;
	}

#ifdef VAC
	if (vac && align)
		if (align_amount < shm_alignment)
			align_amount = shm_alignment;
#endif

	len += align_amount;

	/*
	 * Look for a large enough hole starting below the stack limit.
	 * After finding it, use the upper part.  Addition of PAGESIZE is
	 * for the redzone as described above.
	 */
	if (as_gap(as, len, &base, &slen, AH_HI, (caddr_t)NULL) == 0) {
		caddr_t as_addr;

		addr = base + slen - len + PAGESIZE;
		as_addr = addr;
		/*
		 * Round address DOWN to the alignment amount,
		 * add the offset, and if this address is less
		 * than the original address, add alignment amount.
		 */
		addr = (caddr_t)((u_int)addr & (~(align_amount - 1)));
		addr += (int)(off & (align_amount - 1));
		if (addr < as_addr)
			addr += align_amount;

		ASSERT(addr <= (as_addr + align_amount));
		ASSERT(((u_int)addr & (align_amount - 1)) ==
			((u_int)(off & (align_amount - 1))));
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
/* ARGSUSED */
int
valid_va_range(caddr_t *basep, u_int *lenp, u_int minlen, int dir)
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
int
valid_usr_range(caddr_t addr, size_t len)
{
	caddr_t eaddr = addr + len;

	if (eaddr <= addr || addr >= (caddr_t)USRSTACK ||
	    eaddr > (caddr_t)USRSTACK)
		return (0);
	return (1);
}

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

/*
 * Return the size of the text segment.
 */
long
getts(exp)
	struct exec *exp;
{

	return ((long)clrnd(btoc(exp->a_text)));
}

/*
 * Return the size of the data segment.
 */
long
getds(exp)
	struct exec *exp;
{

	return ((long)clrnd(btoc(exp->a_data + exp->a_bss)));
}

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
gettmem(struct exec *exp)
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
getexinfo(edp_in, edp_out, pagetext, pagedata)
	struct exdata *edp_in, *edp_out;
	int *pagetext;
	int *pagedata;
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
 * Array of page sizes.
 */
typedef struct hw_pagesize {
	size_t	size;
	size_t	shift;
} hw_pagesize_t;

static hw_pagesize_t hw_page_array[] = {
	{MMU_PAGESIZE,		MMU_PAGESHIFT},
	{MMU_PAGESIZE64K,	MMU_PAGESHIFT64K},
	{MMU_PAGESIZE512K,	MMU_PAGESHIFT512K},
	{MMU_PAGESIZE4M,	MMU_PAGESHIFT4M},
	{0, 0}
};

u_int
page_num_pagesizes()
{
	return (MMU_PAGE_SIZES);
}

size_t
page_get_pagesize(u_int n)
{

	if (n >= MMU_PAGE_SIZES)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);

	return (hw_page_array[n].size);
}

static size_t
page_get_shift(u_int n)
{

	if (n >= MMU_PAGE_SIZES)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);

	return (hw_page_array[n].shift);
}

#define	PNUM_SIZE(size_code)						\
	(hw_page_array[size_code].size >> hw_page_array[0].shift)

/*
 * Anchored in the table below are counters used to keep track
 * of free contiguous physical memory. Each element of the table contains
 * the array of counters, the size of array which is allocated during
 * startup based on physmax and a shift value used to convert a pagenum
 * into a counter array index or vice versa. The table has page size
 * for rows and region size for columns:
 *
 *	page_counters[page_size][region_size]
 *
 *	page_size: 	TTE size code of pages on page_size freelist.
 *
 *	region_size:	TTE size code of a candidate larger page made up
 *			made up of contiguous free page_size pages.
 *
 * As you go across a page_size row increasing region_size each
 * element keeps track of how many (region_size - 1) size groups
 * made up of page_size free pages can be coalesced into a
 * regsion_size page. Yuck! Lets try an example:
 *
 * 	page_counters[1][3] is the table element used for identifying
 *	candiate 4M pages from contiguous pages off the 64K free list.
 *	Each index in the page_counters[1][3].array spans 4M. Its the
 *	number of free 512K size (regsion_size - 1) groups of contiguous
 *	64K free pages.	So when page_counters[1][3].counters[n] == 8
 *	we know we have a candiate 4M page made up of 512K size groups
 *	of 64K free pages.
 *
 * NOTE:It may seem that basing the size of the array on physmax would
 *	waste memory depending how the memory banks are populated.
 *	On our large server machines this is not true.
 *	At power on physical memory is moved around such that it starts
 *	at page zero and there are no holes.
 */
typedef struct HW_PAGE_MAP {
	char	*counters;	/* counter array */
	size_t	size;		/* size of array above */
	int	shift;		/* shift for pnum/array index conversion */
} hw_page_map_t;

static hw_page_map_t
page_counters[MAX_MEM_NODES][MMU_PAGE_SIZES][MMU_PAGE_SIZES];

#define	PAGE_COUNTERS(mnode, pg_sz, rg_sz, idx)				\
	(page_counters[mnode][pg_sz][rg_sz].counters[idx])

#define	PAGE_COUNTERS_SHIFT(mnode, pg_sz, rg_sz) 			\
	(page_counters[mnode][pg_sz][rg_sz].shift)

#define	PAGE_COUNTERS_SIZE(mnode, pg_sz, rg_sz) 			\
	(page_counters[mnode][pg_sz][rg_sz].size)

#define	PNUM_TO_IDX(p_sz, region_sz, pnum)				\
	(PFN_2_MEM_NODE_OFF(pnum) >>					\
		(int)PAGE_COUNTERS_SHIFT(PFN_2_MEM_NODE(pnum),		\
			p_sz, region_sz))

#define	IDX_TO_PNUM(mnode, p_sz, region_sz, index) 			\
	MEM_NODE_2_PFN(mnode, ((int)index <<				\
		(int)PAGE_COUNTERS_SHIFT(mnode, p_sz, region_sz)))

/*
 * For sfmmu each larger page size is 8 times the size of previous
 * size page.
 */
#define	FULL_REGION_CNT(p_sz, r_sz)	(8)

/*
 * initialized by page_coloring_init()
 */
static u_int page_colors = 0;
static u_int page_colors_mask = 0;
static u_int vac_colors = 0;
static u_int vac_colors_mask = 0;

extern int do_pg_coloring;
extern int do_virtual_coloring;
extern int ecache_linesize;
int consistent_coloring;

/*
 * page freelist and cachelist are hashed into bins based on color
 */
#define	PAGE_COLORS_MAX8K	(512)
#define	PAGE_COLORS_MAX64K	(PAGE_COLORS_MAX8K   >> 3)
#define	PAGE_COLORS_MAX512K	(PAGE_COLORS_MAX64K  >> 3)
#define	PAGE_COLORS_MAX4MB	(PAGE_COLORS_MAX512K >> 3)

/*
 * Per page size free lists. Allocated dynamically.
 */
static page_t **page_freelists[MAX_MEM_NODES][MMU_PAGE_SIZES];

#define	PAGE_FREELISTS(mnode, pgsz, color) \
	((*(page_freelists[mnode][pgsz] + (color))))

/*
 * Counters for number of free pages for each size
 * free list.
 */
#ifdef DEBUG

u_int	page_freemem[MAX_MEM_NODES][MMU_PAGE_SIZES];

#define	FREEMEM_INC(mnode, sz)						\
	(atomic_add_word(&page_freemem[mnode][sz], 1, NULL))
#define	FREEMEM_DEC(mnode, sz)						\
	(atomic_add_word(&page_freemem[mnode][sz], -1, NULL))

#else

#define	FREEMEM_INC(mnode, sz)	{}
#define	FREEMEM_DEC(mnode, sz)	{}

#endif /* DEBUG */

/*
 * For now there is only a single size cache list.
 * Allocated dynamically.
 */
static	page_t **page_cachelists[MAX_MEM_NODES];

#define	PAGE_CACHELISTS(mnode, color) \
	((*(page_cachelists[mnode] + (color))))

/*
 * There are at most 512 colors/bins.  Spread them out under a
 * couple of locks.  There are mutexes for both the page freelist
 * and the page cachelist.
 */
#define	PC_SHIFT	(4)
#define	NPC_MUTEX	(PAGE_COLORS_MAX8K/(1 << PC_SHIFT))

static kmutex_t	fpc_mutex[MAX_MEM_NODES][NPC_MUTEX];
static kmutex_t	cpc_mutex[MAX_MEM_NODES][NPC_MUTEX];

#define	PP_2_BIN(pp)							\
	(((pp->p_pagenum) & page_colors_mask) >>			\
	(hw_page_array[pp->p_cons].shift - hw_page_array[0].shift))

#define	PP_2_MEM_NODE(pp)	(PFN_2_MEM_NODE((pp)->p_pagenum))

#define	PC_BIN_MUTEX(mnode, bin, list) ((list == PG_FREE_LIST)?		\
	&fpc_mutex[mnode][(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))] :		\
	&cpc_mutex[mnode][(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))])

/*
 * We can only allow a single thread to update a counter within
 * a 4M region of phisical memory. That is the finest granularity
 * possible since the counter values are dependent on each other
 * as you move accross region sizes.
 */
static kmutex_t	ctr_mutex[MAX_MEM_NODES][NPC_MUTEX];

#define	PP_CTR_MUTEX(pp)						\
	&ctr_mutex[PP_2_MEM_NODE(pp)]					\
		[((pp->p_pagenum >> (MMU_PAGESHIFT4M - MMU_PAGESHIFT))	\
			& (NPC_MUTEX-1))]

/*
 * Called by startup().
 * Size up the per page size free list counters based on physmax
 * of each node.
 */
u_int
page_ctrs_sz()
{
	int	npgs;
	int	p, r;
	int	mnode;
	u_int	ctrs_sz = 0;

	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		npgs = mem_node_config[mnode].physmax;

		for (p = 0; p < MMU_PAGE_SIZES; p++) {
			for (r = p + 1; r < MMU_PAGE_SIZES; r++) {

				if (mem_node_config[mnode].exists) {
					PAGE_COUNTERS_SHIFT(mnode, p, r) =
						TTE_BSZS_SHIFT(r);
					PAGE_COUNTERS_SIZE(mnode, p, r) =
						(npgs >> PAGE_COUNTERS_SHIFT(
							mnode, p, r)) + 1;
					ctrs_sz +=
						PAGE_COUNTERS_SIZE(mnode, p, r);
				} else {
					page_counters[mnode][p][r].counters =
						NULL;
					/*
					 * Paranoid. If some mem_node does not
					 * exist but there's a bug that
					 * references its page we want to help
					 * panic right away. Shift of pfn by
					 * 31 right should give an index of 0
					 * and crash on NULL pointer.
					 */
					PAGE_COUNTERS_SHIFT(mnode, p, r) = 31;
					PAGE_COUNTERS_SIZE(mnode, p, r) = 0;
				}
			}
		}
	}

	/*
	 * add some slop for roundups. page_ctrs_alloc will roundup the start
	 * address of the counters to ecache_linesize boundary for every
	 * memory node.
	 */
	return (ctrs_sz + MAX_MEM_NODES * ecache_linesize);
}

caddr_t
page_ctrs_alloc(caddr_t alloc_base)
{

	int	mnode;
	int	p, r;

	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {

		if (mem_node_config[mnode].exists == 0)
			continue;

		for (p = 0; p < MMU_PAGE_SIZES; p++) {
			for (r = p + 1; r < MMU_PAGE_SIZES; r++) {
				page_counters[mnode][p][r].counters =
					(caddr_t)alloc_base;
				alloc_base += page_counters[mnode][p][r].size;
			}
		}

		alloc_base = (caddr_t)roundup((u_int)alloc_base,
			ecache_linesize);
	}

	return (alloc_base);
}

/*
 * Adjust page counters if memory should suddenly be
 * plugged in. This function assumes the system
 * has been quiesced befor being called.
 */
u_int
page_ctrs_adjust(int mnode)
{
	int	npgs;
	int	p, r;
	u_int	pcsz = 0;
	caddr_t	tmp;

	npgs = roundup(mem_node_config[mnode].physmax, 8);

	for (p = 0; p < MMU_PAGE_SIZES; p++) {
		for (r = p + 1; r < MMU_PAGE_SIZES; r++) {

			PAGE_COUNTERS_SHIFT(mnode, p, r) =
			    TTE_BSZS_SHIFT(r);
			pcsz =
			    (npgs >> PAGE_COUNTERS_SHIFT(mnode, p, r)) + 1;

			if ((tmp = (char *)kmem_alloc(pcsz, KM_NOSLEEP))) {

				if (PAGE_COUNTERS_SIZE(mnode, p, r) != 0) {
					bcopy(
					    page_counters[mnode][p][r].counters,
					    tmp,
					    PAGE_COUNTERS_SIZE(mnode, p, r));
					page_counters[mnode][p][r].counters =
					    tmp;
				}
				PAGE_COUNTERS_SIZE(mnode, p, r) = pcsz;

			} else {
				return (ENOMEM);
			}
		}
	}

	return (0);
} 


caddr_t
ndata_alloc_page_freelists(caddr_t alloc_base)
{
	int	mnode;
	int	psz;

	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		if (mem_node_config[mnode].exists == 0) {
			page_cachelists[mnode] = NULL;
			for (psz  = 0; psz < MMU_PAGE_SIZES; psz++)
				page_freelists[mnode][psz] = NULL;
		} else {
			/*
			 * We only support small pages in the cachelist.
			 */
			page_cachelists[mnode] = (page_t **)alloc_base;
			alloc_base += sizeof (machpage_t *) * PAGE_COLORS_MAX8K;
			/*
			 * Allocate freelists bins for all supported page
			 * sizes.
			 */
			for (psz  = 0; psz < MMU_PAGE_SIZES; psz++) {
				page_freelists[mnode][psz] =
					(page_t **)alloc_base;
				alloc_base +=
					sizeof (machpage_t *) *
					(PAGE_COLORS_MAX8K >> (3 * psz));
			}
			alloc_base = (caddr_t)roundup((u_int)alloc_base,
				ecache_linesize);
		}
	}
	return (alloc_base);
}

/*
 * Local functions prototypes.
 */

static	void page_ctr_add(machpage_t *);
static	void page_ctr_sub(machpage_t *);
static	u_int page_convert_color(u_char, u_char, u_int);
#ifdef DEBUG
static void CHK_LPG(machpage_t *, u_char);
#endif
static void page_freelist_lock(int);
static void page_freelist_unlock(int);
static machpage_t *page_cons_create(machpage_t *);
static int page_promote(u_int, u_char, u_char);
static int page_demote(u_int, u_char, u_char);
static int page_freelist_fill(u_char, int, int);
static page_t *page_get_mnode_freelist(u_int, u_char, u_int, int);
static page_t *page_get_mnode_cachelist(u_int, u_int, int);


/*
 * Functions to adjust region counters for each size free list.
 */
static void
page_ctr_add(machpage_t *pp)
{
	kmutex_t	*lock = PP_CTR_MUTEX(pp);
	int 		p, r, idx;
	u_int		pfnum;
	int		mnode = PP_2_MEM_NODE(pp);

	ASSERT(pp->p_cons >= 0 && pp->p_cons < MMU_PAGE_SIZES);
	ASSERT(se_excl_assert(&pp->genp_selock));

	/*
	 * p is the current page size.
	 * r is the region size.
	 */
	p = pp->p_cons;
	r = p + 1;
	pfnum = pp->p_pagenum;

	FREEMEM_INC(mnode, p);

	/*
	 * Increment the count of free pages for the current
	 * region. Continue looping up in region size incrementing
	 * count if the preceeding region is full.
	 */
	mutex_enter(lock);
	while (r < MMU_PAGE_SIZES) {
		idx = PNUM_TO_IDX(p, r, pfnum);

		ASSERT(PAGE_COUNTERS(mnode, p, r, idx) < FULL_REGION_CNT(p, r));

		if (++PAGE_COUNTERS(mnode, p, r, idx) !=
			FULL_REGION_CNT(p, r)) {
			break;
		}
		r++;
	}
	mutex_exit(lock);
}

static void
page_ctr_sub(machpage_t *pp)
{
	kmutex_t	*lock = PP_CTR_MUTEX(pp);
	int 		p, r, idx;
	u_int		pfnum;
	int		mnode = PP_2_MEM_NODE(pp);

	ASSERT(pp->p_cons >= 0 && pp->p_cons < MMU_PAGE_SIZES);
	ASSERT(se_excl_assert(&pp->genp_selock));

	/*
	 * p is the current page size.
	 * r is the region size.
	 */
	p = pp->p_cons;
	r = p + 1;
	pfnum = pp->p_pagenum;

	FREEMEM_DEC(mnode, p);

	/*
	 * Decrement the count of free pages for the current
	 * region. Continue looping up in region size decrementing
	 * count if the preceeding region was full.
	 */
	mutex_enter(lock);
	while (r < MMU_PAGE_SIZES) {
		idx = PNUM_TO_IDX(p, r, pfnum);

		ASSERT(PAGE_COUNTERS(mnode, p, r, idx) > 0);

		if (--PAGE_COUNTERS(mnode, p, r, idx) !=
			FULL_REGION_CNT(p, r) - 1) {
			break;
		}
		r++;
	}
	mutex_exit(lock);
}

/*
 * Function to get an ecache color bin: F(as, cnt, vcolor).
 * the goal of this function is to:
 * - to spread a processes' physical pages across the entire ecache to
 *	maximize its use.
 * - to minimize vac flushes caused when we reuse a physical page on a
 *	different vac color than it was previously used.
 * - to prevent all processes to use the same exact colors and trash each
 *	other.
 *
 * cnt is a bin ptr kept on a per as basis.  As we page_create we increment
 * the ptr so we spread out the physical pages to cover the entire ecache.
 * The virtual color is made a subset of the physical color in order to
 * in minimize virtual cache flushing.
 * We add in the as to spread out different as.  This happens when we
 * initialize the start count value.
 * sizeof(struct as) is 60 so we shift by 3 to get into the bit range
 * that will tend to change.  For example, on spitfire based machines
 * (vcshft == 1) contigous as are spread bu ~6 bins.
 * vcshft provides for proper virtual color alignment.
 * In theory cnt should be updated using cas only but if we are off by one
 * or 2 it is no big deal.
 * We also keep a start value which is used to randomize on what bin we
 * start counting when it is time to start another loop. This avoids
 * contigous allocations of ecache size to point to the same bin.
 * Why 3? Seems work ok. Better than 7 or anything larger.
 */
#define	PGCLR_LOOPFACTOR 3

/*
 * This AS_2_BIN macro is supplied so a different kernel can be made
 * for compiler folks in the attempt to get very reproducible results
 * (Giving up some performance in the process of course).
 */

#define	AS_2_BIN(as, cnt, addr,  bin)					\
	if (consistent_coloring) {					\
		bin = ((ulong)addr >> MMU_PAGESHIFT) &			\
			page_colors_mask;				\
	} else {							\
		cnt = as_color_bin(as);					\
		/* make sure physical color aligns with vac color */	\
		while ((cnt & vac_colors_mask) !=			\
		    addr_to_vcolor(addr)) {				\
			cnt++;						\
		}							\
		bin = cnt = cnt & page_colors_mask;			\
		/* update per as page coloring fields */		\
		cnt = (cnt + 1) & page_colors_mask;			\
		if (cnt == (as_color_start(as) & page_colors_mask)) {	\
			cnt = as_color_start(as) = as_color_start(as) +	\
				PGCLR_LOOPFACTOR; 			\
		}							\
		as_color_bin(as) = cnt & page_colors_mask;		\
	}								\
	ASSERT(bin <= page_colors_mask);

u_int
get_color_start(struct as *as)
{
	return ((((u_int)as >> 3) << (vac_shift - MMU_PAGESHIFT)) &&
		page_colors_mask);
}

static u_int
page_convert_color(u_char cur_szc, u_char new_szc, u_int color)
{
	int shift;

	if (cur_szc > new_szc) {
		shift = page_get_shift(cur_szc) - page_get_shift(new_szc);
		return (color << shift);
	} else if (cur_szc < new_szc) {
		shift = page_get_shift(new_szc) - page_get_shift(cur_szc);
		return (color >> shift);
	}
	return (color);
}

#ifdef DEBUG

static void
CHK_LPG(machpage_t *pp, u_char szc)
{

	int npgs = page_get_pagesize(pp->p_cons) >> PAGESHIFT;

	if (npgs  == 1) {
		ASSERT(pp->p_cons == 0);
		ASSERT(PP2MACHPP(pp->genp_next) == pp);
		ASSERT(PP2MACHPP(pp->genp_prev) == pp);
		return;
	}

	ASSERT(PP2MACHPP(pp->genp_vpnext) == pp || pp->genp_vpnext == NULL);
	ASSERT(PP2MACHPP(pp->genp_vpprev) == pp || pp->genp_vpprev == NULL);

	ASSERT(IS_P2ALIGNED((pp->p_pagenum << PAGESHIFT),
			page_get_pagesize(pp->p_cons)));
	ASSERT(pp->p_pagenum == (PP2MACHPP(pp->genp_next)->p_pagenum - 1));
	ASSERT(PP2MACHPP(pp->genp_prev)->p_pagenum ==
		(pp->p_pagenum + (npgs - 1)));

	/*
	 * Check list of pages.
	 */
	while (npgs--) {
		if (npgs != 0) {
			ASSERT(pp->p_pagenum ==
				PP2MACHPP(pp->genp_next)->p_pagenum - 1);
		}
		ASSERT(pp->p_cons == szc);
		ASSERT(PP_ISFREE(MACHPP2PP(pp)));
		ASSERT(PP_ISAGED(MACHPP2PP(pp)));
		ASSERT(PP2MACHPP(pp->genp_vpnext) == pp ||
			pp->genp_vpnext == NULL);
		ASSERT(PP2MACHPP(pp->genp_vpprev) == pp ||
			pp->genp_vpprev == NULL);
		ASSERT(pp->genp_vnode  == NULL);

		pp = PP2MACHPP(pp->genp_next);
	}
}

#else /* !DEBUG */

#define	CHK_LPG(pp, szc)	{}

#endif /* DEBUG */

static void
page_freelist_lock(int mnode)
{
	int i;

	for (i = 0; i < NPC_MUTEX; i++) {
		mutex_enter(&fpc_mutex[mnode][i]);
		mutex_enter(&cpc_mutex[mnode][i]);
	}
}

static void
page_freelist_unlock(int mnode)
{
	int i;

	for (i = 0; i < NPC_MUTEX; i++) {
		mutex_exit(&fpc_mutex[mnode][i]);
		mutex_exit(&cpc_mutex[mnode][i]);
	}
}

/*
 * Manages freed up constituent pages. The upper layers
 * are going to free the pages up one PAGESIZE page
 * at a time. We wait until we see all the constituent
 * pages for the large page then gather them up and free
 * the entire large page at once.
 *
 * RFE: When we have the new page_create_ru and truly
 *	big pages we can delete this code.
 */

static 	kmutex_t conlist_mutex;
static 	page_t *page_conslists[MMU_PAGE_SIZES]; /* staging list */
int	cons_sema;	/* counting semaphore to trigger */
			/* calls page_cons_create 	*/

#define	PAGE_CONLISTS(szc) (page_conslists[szc])
#define	PFN_BASE(pfnum, szc)  (pfnum & ~((1 << (szc * 3)) - 1))
#define	CONS_LPFN(pfnum, szc)					\
	(PFN_BASE(pfnum, szc) != PFN_BASE((pfnum + 1), szc))

#ifdef DEBUG
static u_longlong_t page_cons_creates[MAX_MEM_NODES][MMU_PAGE_SIZES];
#endif

/*
 * Called with last_pp when caller believes that all the constituent
 * pages of a large page are free on the constituent page list and
 * can be grouped back toghther and placed on the large page free list.
 *
 * Caller must protect constituent list.
 */
machpage_t *
page_cons_create(machpage_t *last_pp)
{
	page_t	*plist = NULL, *pp;
	u_int	npgs, pfnum, szc;

	szc	= last_pp->p_cons;
	pfnum	= PFN_BASE(last_pp->p_pagenum, szc);
	npgs 	= page_get_pagesize(last_pp->p_cons) / PAGESIZE;

#ifdef DEBUG
	atomic_add_ext(&page_cons_creates[PFN_2_MEM_NODE(pfnum)][szc], 1, NULL);
#endif
	/*
	 * Gather up all the constituent pages to be linked
	 * together and placed back on the large page freelist.
	 * The common case is that they are all here.
	 * But while gathering them if we find that one is missing
	 * then stop and place them all back on the constituent
	 * list to try again later.
	 */
	while (npgs--) {

		pp = MACHPP2PP(page_numtopp_nolock(pfnum++));
		ASSERT(pp != NULL);

		if (!PP2MACHPP(pp)->p_conslist) {

			/*
			 * There're not all free.
			 * Put em back and try again
			 * later.
			 */
			while (plist) {
				/*
				 * last_pp is already locked since
				 * we are here via page_free().
				 */
				pp = plist;
				if (pp != MACHPP2PP(last_pp)) {
					if (!page_trylock(MACHPP2PP(pp),
					    SE_EXCL)) {
						cmn_err(CE_PANIC,
					"page_cons_create: trylock failed");
					}
				}
				page_sub(&plist, pp);
				PP2MACHPP(pp)->p_conslist = 1;
				page_add(&PAGE_CONLISTS(PP2MACHPP(pp)->p_cons),
					pp);
				if (pp != MACHPP2PP(last_pp)) {
					page_unlock(pp);
				}
			}
			return (NULL);
		}

		ASSERT(PP_ISFREE(pp));
		ASSERT(PP2MACHPP(pp)->p_mapping == NULL);
		ASSERT(PP2MACHPP(pp)->genp_vnode == NULL);
		ASSERT(PP2MACHPP(pp)->p_cons == szc);

		page_sub(&PAGE_CONLISTS(PP2MACHPP(pp)->p_cons), pp);
		PP2MACHPP(pp)->p_conslist = 0;
		page_list_concat(&plist, &pp);
	}
	return (PP2MACHPP(plist));
}

void
page_list_add(int list, page_t *gen_pp, int where)
{
	machpage_t	*pp = PP2MACHPP(gen_pp), *pplist = NULL;
	page_t		**ppp;
	kmutex_t	*pcm;
	u_int		bin, mnode;

	ASSERT(se_excl_assert(&pp->genp_selock));
	ASSERT(PP_ISFREE(MACHPP2PP(pp)));
	ASSERT(pp->p_mapping == NULL);

	/*
	 * PAGESIZE case.
	 */
	if (pp->p_cons == 0) {

		bin = PP_2_BIN(pp);
		mnode = PP_2_MEM_NODE(pp);
		pcm = PC_BIN_MUTEX(mnode, bin, list);

		if (list == PG_FREE_LIST) {
			ASSERT(PP_ISAGED(MACHPP2PP(pp)));
			ppp = &PAGE_FREELISTS(mnode, 0, bin);
		} else {
			ASSERT(pp->genp_vnode);
			ASSERT((pp->genp_offset & 0xfff) == 0);
			ASSERT(pp->p_cons == 0);
			ppp = &PAGE_CACHELISTS(mnode, bin);
		}

		mutex_enter(pcm);
		page_add(ppp, MACHPP2PP(pp));

		if (where == PG_LIST_TAIL) {
			*ppp = (*ppp)->p_next;
		}

		/*
		 * Add counters before releasing
		 * pcm mutex to avoid a race with
		 * page_freelist_coalesce and
		 * page_freelist_fill.
		 */
		page_ctr_add(pp);
		mutex_exit(pcm);

		/*
		 * It is up to the caller to unlock the page!
		 */
		ASSERT(se_excl_assert(&pp->genp_selock));
		return;
	}

	/*
	 * Large page case.
	 *
	 * This is a constituent page of a larger page so
	 * we place it on a temporary staging list until we
	 * can free them all up in one chunk. Hopefully this
	 * will go away someday when we have true big pages.
	 *
	 * Most of the time the pages will be freed in
	 * order and all I have to do is try a coalesce
	 * them when I see the last constituent page of the
	 * large page. The problem is that it's possible the
	 * pager can get ahead of me and trigger the freeing
	 * of the last constituent page before all others have
	 * been freed. For this reason I have a counting
	 * semaphore to indicate I now have or have seen a last
	 * constituent page. So I only have to check when the
	 * semaphore > 0. A counting semaphore also handles the
	 * case where two or more threads are freeing a large
	 * page and the pager gets ahead of both of them. This
	 * shouldn't be expensive since its rare the pager will
	 * get ahead of us.
	 */
	mutex_enter(&conlist_mutex);

	page_add(&PAGE_CONLISTS(pp->p_cons), MACHPP2PP(pp));
	pp->p_conslist = 1;
	if (CONS_LPFN(pp->p_pagenum, pp->p_cons)) {
		cons_sema++;
	}

	if (cons_sema) {
		pplist = page_cons_create(pp);
		if (pplist) {
			CHK_LPG(pplist, pp->p_cons);

			cons_sema--;

			bin = PP_2_BIN(pplist);
			mnode = PP_2_MEM_NODE(pp);
			pcm = PC_BIN_MUTEX(mnode, bin, PG_FREE_LIST);

			/*
			 * If pp is the base constituent page of this
			 * large page then it is already locked.
			 */
			if (pp != pplist) {
				if (!page_trylock(MACHPP2PP(pplist), SE_EXCL)) {
					cmn_err(CE_PANIC,
						"page_list_add: pp %x failed",
						pplist);
				}
			}

			mutex_enter(pcm);
			page_vpadd(&PAGE_FREELISTS(mnode, pp->p_cons, bin),
				MACHPP2PP(pplist));
			/*
			 * Add counters before releasing
			 * pcm mutex to avoid a race with
			 * page_freelist_coalesce and
			 * page_freelist_fill.
			 */
			page_ctr_add(pplist);
			mutex_exit(pcm);

			if (pp != pplist) {
				page_unlock(MACHPP2PP(pplist));
			}
		}
	}
	mutex_exit(&conlist_mutex);
}

/*
 * Take a particular page off of whatever freelist the page
 * is claimed to be on.
 */
void
page_list_sub(int list, page_t *gen_pp)
{
	machpage_t	*pp = PP2MACHPP(gen_pp);
	u_int		bin;
	u_int		mnode;
	kmutex_t	*pcm;
	page_t		**ppp;

	ASSERT(se_excl_assert(&pp->genp_selock));
	ASSERT(PP_ISFREE(MACHPP2PP(pp)));
	if (pp->p_cons) {
		cmn_err(CE_PANIC,
		    "page_list_sub: 0x%p pp part of large page", gen_pp);
	}

	bin = PP_2_BIN(pp);
	mnode = PP_2_MEM_NODE(pp);
	pcm = PC_BIN_MUTEX(mnode, bin, list);

	if (list == PG_FREE_LIST) {
		ASSERT(PP_ISAGED(MACHPP2PP(pp)));
		ppp = &PAGE_FREELISTS(mnode, pp->p_cons, bin);
	} else {
		ASSERT(!PP_ISAGED(MACHPP2PP(pp)));
		ppp = &PAGE_CACHELISTS(mnode, bin);
		ASSERT(PP_ISAGED(MACHPP2PP(pp)) == 0);
	}

	mutex_enter(pcm);
	page_sub(ppp, MACHPP2PP(pp));
	/*
	 * Subtract counters before releasing pcm mutex
	 * to avid a race with page_freelist_coalesce.
	 */
	page_ctr_sub(pp);
	mutex_exit(pcm);

}

int page_promote_err, page_demote_err;

/*
 * Create a single larger page from smaller contiguous pages.
 * Pages involved are on the freelist before and after operation.
 * The caller is responsible for locking the freelist.
 * Returns 1 on success 0, on failure.
 *
 * RFE: For performance pass in pp instead of pfnum so
 * 	we can avoid excessive calls to page_numtopp_nolock().
 *	This would depend on an assumption that all contiguous
 *	pages are in the same memseg so we can just add/dec
 *	our pp.
 *
 * Lock odering:
 *
 *	There is a potential but rare deadlock situation
 *	for page promotion and demotion operations. The problem
 *	is there are two paths into the freelist manager and
 *	they have different lock orders:
 *
 *	page_create()
 *		lock freelist
 *		page_lock(EXCL)
 *		unlock freelist
 *		return
 *		caller drops page_lock
 *
 *	page_free() and page_reclaim()
 *		caller grabs page_lock(EXCL)
 *
 *		lock freelist
 *		unlock freelist
 *		drop page_lock
 *
 *	What prevents a thread in page_creat() from deadlocking
 *	with a thread freeing or reclaiming the same page is the
 *	page_trylock() in page_get_freelist(). If the trylock fails
 *	it skips the page.
 *
 *	The lock ordering for promotion and demotion is the same as
 *	for page_create(). Since the same deadlock could occur during
 *	page promotion and demotion and freeing or reclaiming a page
 *	we might have to fail the operation and undo what have done so
 *	far. Again this is rare.
 */
static int
page_promote(u_int pfnum, u_char cur_szc, u_char new_szc)
{
	machpage_t *pp, *pplist, *tpp;
	int	cur_npgs, new_npgs, npgs, coalesces, bin;
	int	mnode;

	ASSERT(new_szc > cur_szc);

	/*
	 * Number of pages per constituent page.
	 */
	cur_npgs = btop(page_get_pagesize(cur_szc));
	new_npgs = btop(page_get_pagesize(new_szc));

	/*
	 * Loop around coalescing the smaller pages
	 * into a big page.
	 */
	coalesces = new_npgs / cur_npgs;
	pplist = NULL;
	while (coalesces--) {

		pp = PP2MACHPP(page_numtopp_nolock(pfnum));
		pfnum += PNUM_SIZE(cur_szc);
		ASSERT(pp != NULL);

		/*
		 * This is where a rare but potential deadlock
		 * could occur. See comment above.
		 */
		if (!page_trylock(MACHPP2PP(pp), SE_EXCL)) {
			goto fail_promote;
		}

		ASSERT(PP_ISFREE(MACHPP2PP(pp)));

		/*
		 * Remove from the freelist.
		 */
		bin = PP_2_BIN(pp);
		mnode = PP_2_MEM_NODE(pp);
		if (PP_ISAGED(MACHPP2PP(pp))) {
			if (pp->p_cons) {
				page_vpsub(&PAGE_FREELISTS(mnode, cur_szc, bin),
					MACHPP2PP(pp));
			} else {
				ASSERT(cur_szc == 0);
				page_sub(&PAGE_FREELISTS(mnode, 0, bin),
					MACHPP2PP(pp));
			}
		} else {
			ASSERT(pp->p_cons == 0);
			ASSERT(cur_szc == 0);
			page_sub(&PAGE_CACHELISTS(mnode, bin), MACHPP2PP(pp));

			/*
			 * Since this page comes from the
			 * cachelist, we must destory the
			 * vnode association.
			 */
			page_hashout(MACHPP2PP(pp), (kmutex_t *)NULL);
			PP_SETAGED(MACHPP2PP(pp));
		}
		CHK_LPG(pp, cur_szc);
		page_ctr_sub(pp);

		ASSERT(pp->p_cons == cur_szc);

		/*
		 * Concatenate the smaller page(s) onto
		 * the large page list.
		 */
		npgs = cur_npgs;
		tpp = pp;
		while (npgs) {

			/*
			 * Simple check avoids having to drop the lock
			 * from above and picking it back up. Especially
			 * when cur_npgs == 1.
			 *
			 * XXX  page_numtopp() could srew us here. It
			 * 	calls page_lock() with P_RECLAIM. Currently
			 *	this doesn't seem to be a problem. Maybe in
			 *	the future we should just teach new callers
			 *	that want a constituent page to demote it
			 *	first or take the whole large page?
			 */
			if (npgs != cur_npgs &&
			    !page_trylock(MACHPP2PP(tpp), SE_EXCL)) {
				cmn_err(CE_PANIC,
					"page_promote: page_trylock failed");
			}
			ASSERT(tpp->p_cons == cur_szc);
			tpp->p_cons = new_szc;
			page_unlock(MACHPP2PP(tpp));
			tpp = PP2MACHPP(tpp->genp_next);
			npgs--;
		}
		page_list_concat((page_t **)&pplist, (page_t **)&pp);
	}

	/*
	 * Now place the new large page on the freelist.
	 * Its ok to panic here. Since we succeeded
	 * in locking the page during promotion.
	 *
	 * XXX: see note above.
	 */
	if (!page_trylock(MACHPP2PP(pplist), SE_EXCL)) {
		cmn_err(CE_PANIC,
		    "page_promote: trylock failed page %x", pp);
	}
	CHK_LPG(pplist, new_szc);

	bin = PP_2_BIN(pplist);
	mnode = PP_2_MEM_NODE(pplist);
	page_vpadd(&PAGE_FREELISTS(mnode, new_szc, bin), MACHPP2PP(pplist));

	page_ctr_add(pplist);
	page_unlock(MACHPP2PP(pplist));
	return (1);

fail_promote:
	/*
	 * Someone must have been still freeing or
	 * reclaiming the page. To prevent a deadlock
	 * undo what we have done sofar and return failure.
	 * Since failure only can happen on the new
	 * pagesize boundaries the code is similar
	 * to page_demote().
	 */
	while (pplist) {
		machpage_t *npplist;
		int	n;

		pp = pplist;
		if (cur_npgs == 1) {
			page_sub((page_t **)&pplist, MACHPP2PP(pp));
			pp->p_cons = cur_szc;
			bin = PP_2_BIN(pp);
			mnode = PP_2_MEM_NODE(pp);
			page_add(&PAGE_FREELISTS(mnode, 0, bin), MACHPP2PP(pp));
			page_ctr_add(pp);
			page_unlock(MACHPP2PP(pp));
		} else {
			page_list_break((page_t **)&pplist, (page_t **)&npplist,
				cur_npgs);

			pp = pplist;
			n = cur_npgs;
			while (n--) {
				pp->p_cons = cur_szc;
				pp = PP2MACHPP(pp->genp_next);
			}
			CHK_LPG(pplist, new_szc);

			bin = PP_2_BIN(pplist);
			mnode = PP_2_MEM_NODE(pplist);
			page_vpadd(&PAGE_FREELISTS(mnode, new_szc, bin),
				MACHPP2PP(pplist));

			page_ctr_add(pplist);
			page_unlock(MACHPP2PP(pplist));
			pplist = npplist;
		}
	}
	page_promote_err++;
	return (0);
}

/*
 * Break up a large page into smaller size pages.
 * Pages involved are on the freelist before and after operation.
 * The caller is responsible for locking.
 * Returns 1 on success 0, on failure.
 */
static int
page_demote(u_int pfnum, u_char cur_szc, u_char new_szc)
{

	machpage_t	*pp, *pplist, *npplist;
	int	npgs, bin, n;
	int	mnode;

	ASSERT(cur_szc != 0);
	ASSERT(new_szc < cur_szc);

	pplist = PP2MACHPP(page_numtopp_nolock(pfnum));
	ASSERT(pplist != NULL);

	/*
	 * If the page is already locked then fail the
	 * demote to prevent deadlock.
	 */
	if (!page_trylock(MACHPP2PP(pplist), SE_EXCL)) {
		page_demote_err++;
		return (0);
	}

	ASSERT(pplist->p_cons == cur_szc);

	bin = PP_2_BIN(pplist);
	mnode = PP_2_MEM_NODE(pplist);
	page_vpsub(&PAGE_FREELISTS(mnode, cur_szc, bin), MACHPP2PP(pplist));

	CHK_LPG(pplist, cur_szc);
	page_ctr_sub(pplist);
	page_unlock(MACHPP2PP(pplist));

	/*
	 * Number of PAGESIZE pages for smaller new_szc
	 * page.
	 */
	npgs = btop(page_get_pagesize(new_szc));

	while (pplist) {
		pp = pplist;

		if (!page_trylock(MACHPP2PP(pp), SE_EXCL)) {
			cmn_err(CE_PANIC, "page_demote: page_trylock failed");
		}
		ASSERT(pp->p_cons == cur_szc);

		/*
		 * We either break it up into PAGESIZE pages or larger.
		 */
		if (npgs == 1) {	/* PAGESIZE case */
			page_sub((page_t **)&pplist, MACHPP2PP(pp));
			ASSERT(pp->p_cons == cur_szc);
			ASSERT(new_szc == 0);
			pp->p_cons = new_szc;
			bin = PP_2_BIN(pp);
			mnode = PP_2_MEM_NODE(pp);
			page_add(&PAGE_FREELISTS(mnode, 0, bin), MACHPP2PP(pp));
			page_ctr_add(pp);
			page_unlock(MACHPP2PP(pp));
		} else {

			/*
			 * Break down into smaller lists of pages.
			 */
			page_list_break((page_t **)&pplist, (page_t **)&npplist,
				npgs);

			pp = pplist;
			n = npgs;
			while (n--) {
				ASSERT(pp->p_cons == cur_szc);
				pp->p_cons = new_szc;
				pp = PP2MACHPP(pp->genp_next);
			}

			CHK_LPG(pplist, new_szc);

			bin = PP_2_BIN(pplist);
			mnode = PP_2_MEM_NODE(pp);
			page_vpadd(&PAGE_FREELISTS(mnode, new_szc, bin),
				MACHPP2PP(pplist));

			page_ctr_add(pplist);
			page_unlock(MACHPP2PP(pplist));
			pplist = npplist;
		}
	}
	return (1);
}

/*
 * Coalesce as many contiguous pages into the largest
 * possible supported page sizes.
 */
void
page_freelist_coalesce(int mnode)
{
	int 	p, r, idx, pfnum, full, len;

	/*
	 * Lock the entire freelist and coalesce what we can.
	 *
	 * I choose to always promote to the largest page possible
	 * first to reduce the number of page promotions.
	 *
	 * RFE: For performance maybe we can do something less
	 *	brutal than locking the entire freelist. So far
	 * 	this doesn't seem to be a peformance problem?
	 */
	page_freelist_lock(mnode);
	for (p = 0; p < MMU_PAGE_SIZES; p++) {
		for (r = MMU_PAGE_SIZES - 1; r > p; r--) {

			full = FULL_REGION_CNT(p, r);
			len  = PAGE_COUNTERS_SIZE(mnode, p, r);

			for (idx = 0; idx < len; idx++) {
				if (PAGE_COUNTERS(mnode, p, r, idx) == full) {
					pfnum = IDX_TO_PNUM(mnode, p, r, idx);
					(void) page_promote(pfnum, p, r);
				}
			}
		}
	}
	page_freelist_unlock(mnode);
}

/*
 * This is where all polices for moving pages around
 * to different page size free lists is implemented.
 * Returns 1 on success 0, on failure.
 *
 * So far these are the prorities for this alogrithm in descending
 * order:
 *
 *	1) Minimize fragmentation
 *		 At startup place as many pages on the
 *		 4M free list as possible. (done in startup)
 *
 *	2) When servicing a request try to do so with a free page
 *	   from next size up. Helps defer fragmentation as long
 *	   as possible.
 *
 *	3) Page coalesce on demand. Only when a freelist
 *	   larger than PAGESIZE is empty and step 2
 *	   will not work since all larger size lists are
 *	   also empty.
 */
static int
page_freelist_fill(u_char size, int color, int mnode)
{
	u_char next_size = size + 1;
	int 	bin;
	machpage_t *pp;

	ASSERT(size < MMU_PAGE_SIZES);

	/*
	 * First try to break up a larger page to fill
	 * current size freelist.
	 */
	page_freelist_lock(mnode);
	while (next_size < MMU_PAGE_SIZES) {

		bin = page_convert_color(size, next_size, color);
		pp = PP2MACHPP(PAGE_FREELISTS(mnode, next_size, bin));
		while (pp) {

			ASSERT(pp->p_cons == next_size);

			/*
			 * Try and demote this page.
			 */
			if (page_demote(pp->p_pagenum, pp->p_cons, size)) {
				page_freelist_unlock(mnode);
				return (1);
			}

			/*
			 * Page demotion failed. If there are
			 * no more pages of this color then
			 * goto the next size up.
			 */
			pp = PP2MACHPP(pp->genp_vpnext);
			if (pp == PP2MACHPP(PAGE_FREELISTS(mnode,
				next_size, bin))) {
				break;
			}
		}
		next_size++;
	}
	page_freelist_unlock(mnode);

	/*
	 * Ok that didn't work. Time to get nasty
	 * and coalesce.
	 */
	if (size != 0) {
		page_freelist_coalesce(mnode);
	}

	if (PAGE_FREELISTS(mnode, size, color)) {
		return (1);
	} else {
		return (0);
	}
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
#define	BIN_STEP	20

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
	int	cpu_mnode = CPUID_2_MEM_NODE(CPU->cpu_id);
	int	mnode;
	page_t	*pp;

	u_int		bin;
	u_char		szc;
	int		colorcnt;

	/*
	 * Convert size to page size code. For now
	 * we only allow two sizes to be used.
	 */
	switch (size) {
		case MMU_PAGESIZE:
			szc = 0;
			break;

		case MMU_PAGESIZE64K:
			return (NULL);

		case MMU_PAGESIZE512K:
			return (NULL);

		case MMU_PAGESIZE4M:
			szc = 3;
			break;

		default:
			cmn_err(CE_PANIC,
			    "page_get_freelist: illegal page size request");
	}

	AS_2_BIN(as, colorcnt, vaddr, bin);

	/*
	 * AS_2_BIN() gave us an 8k color. Might need to convert it.
	 */
	if (szc) {
		bin = page_convert_color(0, szc, bin);
	}

	/*
	 * Try local memory node first.
	 */
	ASSERT(mem_node_config[cpu_mnode].exists == 1);
	pp = page_get_mnode_freelist(bin, szc, flags, cpu_mnode);
	/* LINTED */
	if ((MAX_MEM_NODES == 1) || pp || (flags & PG_MATCH_COLOR))
		return (pp);

	/*
	 * Try local cachelist before remote freelist for small pages.
	 * Don't need to do it for larger ones cause page_freelist_coalesce()
	 * already failed there anyway.
	 */
	if (size == MMU_PAGESIZE) {
		pp = page_get_mnode_cachelist(bin, flags, cpu_mnode);
		if (pp) {
			page_hashout(pp, (kmutex_t *)NULL);
			PP_SETAGED(pp);
			return (pp);
		}
	}

	/* Now try remote freelists */
	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		if ((mnode == cpu_mnode) ||
			(mem_node_config[mnode].exists == 0))
			continue;

		pp = page_get_mnode_freelist(bin, szc, flags, mnode);
		if (pp)
			return (pp);
	}

	return (NULL);
}

/*ARGSUSED*/
static page_t *
page_get_mnode_freelist(
	u_int bin,
	u_char szc,
	u_int flags,
	int mnode)
{
	kmutex_t	*pcm;
	int		i, fill_tryed = 0;
	page_t		*pp, *first_pp;
	u_int		bin_marker;
	int		colors;

	/*
	 * Set how many physical colors for this page size.
	 */
	colors = page_convert_color(0, szc, page_colors - 1) + 1;

	ASSERT(colors <= page_colors);
	ASSERT(colors);
	ASSERT((colors & (colors - 1)) == 0);

	ASSERT(bin < colors);

	/*
	 * Only hold one freelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 */
	for (i = 0; i <= colors; i++) {
try_again:
		ASSERT(bin < colors);
		if (PAGE_FREELISTS(mnode, szc, bin)) {
			pcm = PC_BIN_MUTEX(mnode, bin, PG_FREE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = PAGE_FREELISTS(mnode, szc, bin)) {
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
				ASSERT(PP2MACHPP(pp)->p_cons == szc);
				ASSERT(PFN_2_MEM_NODE(
					PP2MACHPP(pp)->p_pagenum) == mnode);

				first_pp = pp;
				/*
				 * Walk down the hash chain.
				 */
				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;

					ASSERT(PP_ISFREE(pp));
					ASSERT(PP_ISAGED(pp));
					ASSERT(pp->p_vnode == NULL);
					ASSERT(pp->p_hash == NULL);
					ASSERT(pp->p_offset == (u_offset_t)-1);
					ASSERT(PP2MACHPP(pp)->p_cons == szc);
					ASSERT(PFN_2_MEM_NODE(
						PP2MACHPP(pp)->p_pagenum) ==
							mnode);

					if (pp == first_pp) {
						pp = NULL;
						break;
					}
				}

				if (pp) {
					ASSERT(PP2MACHPP(pp)->p_cons == szc);
					if (szc == 0) {
						page_sub(&PAGE_FREELISTS(mnode,
							szc,
							bin), pp);
					} else {
						page_t	*tpp;
						page_vpsub(&PAGE_FREELISTS(
							mnode, szc, bin), pp);
						CHK_LPG(PP2MACHPP(pp), szc);

						/*
						 * Pages must be returned
						 * locked. That includes all
						 * constituent pages of a
						 * large page.
						 * XXX: Watch out for
						 * 	page_numtopp().
						 */
						tpp = pp->p_next;
						while (tpp != pp) {
							if (!page_trylock(tpp,
							    SE_EXCL)) {
							    panic("trylock");
							}
							tpp = tpp->p_next;
						}
					}
					page_ctr_sub(PP2MACHPP(pp));

					if ((PP_ISFREE(pp) == 0) ||
					    (PP_ISAGED(pp) == 0)) {
						cmn_err(CE_PANIC,
						    "free page is not. pp %x",
						    pp);
					}
					mutex_exit(pcm);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}

		/*
		 * Wow! The bin was empty. First try and satisfy
		 * the request by breaking up or coalescing pages from
		 * a different size freelist of the correct color that
		 * satisfies the ORIGINAL color requested. If that
		 * fails then try pages of the same size but different
		 * colors assuming we are not called with
		 * PG_MATCH_COLOR.
		 */
		if (!fill_tryed) {
			fill_tryed = 1;
			if (page_freelist_fill(szc, bin, mnode)) {
				goto try_again;
			}
		}


		if (flags & PG_MATCH_COLOR) {
			break;
		}

		/*
		 * Select next color bin to try.
		 */
		if (szc == 0) {
			/*
			 * PAGESIZE page case.
			 */
			if (i == 0) {
				bin = (bin + BIN_STEP) & page_colors_mask;
				bin_marker = bin;
			} else {
				bin = (bin +  vac_colors) & page_colors_mask;
				if (bin == bin_marker) {
					bin = (bin + 1) & page_colors_mask;
					bin_marker = bin;
				}
			}
		} else {
			/*
			 * Large page case.
			 */
			bin = (bin + 1) & (colors - 1);
		}
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
	int	cpu_mnode = CPUID_2_MEM_NODE(CPU->cpu_id);
	int	mnode;
	page_t	*pp;
	u_int	bin;
	int	colorcnt;

	AS_2_BIN(as, colorcnt, vaddr, bin);
	ASSERT(bin <= page_colors_mask);

	/*
	 * Try local memory node first.
	 */
	ASSERT(mem_node_config[cpu_mnode].exists == 1);
	pp = page_get_mnode_cachelist(bin, flags, cpu_mnode);
	/* LINTED */
	if ((MAX_MEM_NODES == 1) || pp || (flags & PG_MATCH_COLOR))
		return (pp);

	/* Now try remote cachelists */
	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		if ((mnode == cpu_mnode) ||
			(mem_node_config[mnode].exists == 0))
			continue;

		pp = page_get_mnode_cachelist(bin, flags, mnode);
		if (pp)
			return (pp);
	}

	return (NULL);

}

/*ARGSUSED*/
static page_t *
page_get_mnode_cachelist(
	u_int bin,
	u_int flags,
	int mnode)
{
	kmutex_t	*pcm;
	int		i;
	page_t		*pp;
	page_t		*first_pp;
	u_int		bin_marker;

	/*
	 * Only hold one cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 */

	for (i = 0; i <= page_colors; i++) {
		if (PAGE_CACHELISTS(mnode, bin)) {
			pcm = PC_BIN_MUTEX(mnode, bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = PAGE_CACHELISTS(mnode, bin)) {
				first_pp = pp;
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);
				ASSERT(PP2MACHPP(pp)->p_cons == 0);
				ASSERT(PFN_2_MEM_NODE(
					PP2MACHPP(pp)->p_pagenum) == mnode);
				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;
					ASSERT(PP2MACHPP(pp)->p_cons == 0);
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
					ASSERT(PFN_2_MEM_NODE(
						PP2MACHPP(pp)->p_pagenum) ==
							mnode);
				}

				if (pp) {
					/*
					 * Found and locked a page.
					 * Pull it off the list.
					 */
					page_sub(&PAGE_CACHELISTS(mnode, bin),
						pp);
					/*
					 * Subtract counters before releasing
					 * pcm mutex to avoid a race with
					 * page_freelist_coalesce and
					 * page_freelist_fill.
					 */
					page_ctr_sub(PP2MACHPP(pp));
					mutex_exit(pcm);
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISAGED(pp) == 0);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}
		/*
		 * Wow! The bin was empty or no page could be locked.
		 * If only the proper bin is to be checked, get out
		 * now. */
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		if (i == 0) {
			bin = (bin + BIN_STEP) & page_colors_mask;
			bin_marker = bin;
		} else {
			bin = (bin +  vac_colors) & page_colors_mask;
			if (bin == bin_marker) {
				bin = (bin + 1) & page_colors_mask;
				bin_marker = bin;
			}
		}
		bin &= page_colors_mask;
	}
	return (NULL);
}

/*
 * Called once at startup from kphysm_init() -- before memialloc()
 * is invoked to do the 1st page_free()/page_freelist_add().
 *
 * initializes page_colors and page_colors_mask
 * based on the cache size of the boot CPU.
 *
 * Also initializes the counter locks.
 */
void
page_coloring_init()
{
	u_int i;
	char buffer[100];
	int mnode;

	extern int ecache_size;			/* from obp properties */

	if (do_pg_coloring == 0)
		return;

	page_colors = ecache_size / MMU_PAGESIZE;
	page_colors_mask = page_colors - 1;

	vac_colors = vac_size / MMU_PAGESIZE;
	vac_colors_mask = vac_colors -1;

	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++)
		for (i = 0; i < NPC_MUTEX; i++) {
			(void) sprintf(buffer, "fpc lock %d %d", mnode, i);
			mutex_init(&fpc_mutex[mnode][i],
				buffer, MUTEX_DEFAULT, NULL);

			(void) sprintf(buffer, "cpc lock %d %d", mnode, i);
			mutex_init(&cpc_mutex[mnode][i],
				buffer, MUTEX_DEFAULT, NULL);

			(void) sprintf(buffer,
				"page ctrs lock %d %d", mnode, i);
			mutex_init(&ctr_mutex[mnode][i],
				buffer, MUTEX_DEFAULT, NULL);

		}
	mutex_init(&conlist_mutex, "con page free mutex",
		MUTEX_DEFAULT, NULL);
}
