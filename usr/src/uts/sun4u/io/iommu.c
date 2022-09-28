/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)iommu.c	1.62	96/09/23 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>

#include <sys/ddidmareq.h>
#include <sys/sysiosbus.h>
#include <sys/iommu.h>
#include <sys/iocache.h>
#include <sys/dvma.h>

#include <vm/as.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/hat_sfmmu.h>
#include <sys/machparam.h>

/* Useful debugging Stuff */
#include <sys/nexusdebug.h>
#include <sys/debug.h>
/* Bitfield debugging definitions for this file */
#define	IOMMU_GETDVMAPAGES_DEBUG	0x1
#define	IOMMU_DMAMAP_DEBUG		0x2
#define	IOMMU_DMAMCTL_DEBUG		0x4
#define	IOMMU_DMAMCTL_SYNC_DEBUG	0x8
#define	IOMMU_DMAMCTL_HTOC_DEBUG	0x10
#define	IOMMU_DMAMCTL_KVADDR_DEBUG	0x20
#define	IOMMU_DMAMCTL_NEXTWIN_DEBUG	0x40
#define	IOMMU_DMAMCTL_NEXTSEG_DEBUG	0x80
#define	IOMMU_DMAMCTL_MOVWIN_DEBUG	0x100
#define	IOMMU_DMAMCTL_REPWIN_DEBUG	0x200
#define	IOMMU_DMAMCTL_GETERR_DEBUG	0x400
#define	IOMMU_DMAMCTL_COFF_DEBUG	0x800
#define	IOMMU_DMAMCTL_DMA_FREE_DEBUG	0x1000
#define	IOMMU_REGISTERS_DEBUG		0x2000
#define	IOMMU_DMA_SETUP_DEBUG		0x4000
#define	IOMMU_DMA_UNBINDHDL_DEBUG	0x8000
#define	IOMMU_DMA_BINDHDL_DEBUG		0x10000
#define	IOMMU_DMA_WIN_DEBUG		0x20000
#define	IOMMU_DMA_ALLOCHDL_DEBUG	0x40000
#define	IOMMU_DMA_LIM_SETUP_DEBUG	0x80000
#define	IOMMU_FASTDMA_RESERVE		0x100000
#define	IOMMU_FASTDMA_LOAD		0x200000
#define	IOMMU_INTER_INTRA_XFER		0x400000
#define	IOMMU_TTE			0x800000
#define	IOMMU_TLB			0x1000000
#define	IOMMU_FASTDMA_SYNC		0x2000000

/* Turn on if you need to keep track of outstanding IOMMU usage */
/* #define	IO_MEMUSAGE */
/* Turn on to debug IOMMU unmapping code */
/* #define	IO_MEMDEBUG */

static int iommu_map_window(ddi_dma_impl_t *, u_long, u_long);

static struct dvma_ops iommu_dvma_ops = {
	DVMAO_REV,
	iommu_dvma_kaddr_load,
	iommu_dvma_unload,
	iommu_dvma_sync
};

extern void *sbusp;		/* sbus soft state hook */
extern caddr_t iommu_tsb_vaddr[];
extern int iommu_tsb_alloc_size[];
static char *mapstr = "sbus map space";
/*
 * This is the number of pages that a mapping request needs before we force
 * the TLB flush code to use diagnostic registers.  This value was determined
 * through a series of test runs measuring dma mapping settup performance.
 */
int tlb_flush_using_diag = 16;

int sysio_iommu_tsb_sizes[] = {
	IOMMU_TSB_SIZE_8M,
	IOMMU_TSB_SIZE_16M,
	IOMMU_TSB_SIZE_32M,
	IOMMU_TSB_SIZE_64M,
	IOMMU_TSB_SIZE_128M,
	IOMMU_TSB_SIZE_256M,
	IOMMU_TSB_SIZE_512M,
	IOMMU_TSB_SIZE_1G
};

int
iommu_init(struct sbus_soft_state *softsp, caddr_t address)
{
	int i;
#ifdef	DEBUG
	debug_info = 1;
	debug_print_level = 0;
#endif

	/*
	 * Simply add each registers offset to the base address
	 * to calculate the already mapped virtual address of
	 * the device register...
	 *
	 * define a macro for the pointer arithmetic; all registers
	 * are 64 bits wide and are defined as u_ll_t's.
	 */

#define	REG_ADDR(b, o)	(u_ll_t *)((caddr_t)(b) + (o))

	softsp->iommu_ctrl_reg = REG_ADDR(address, OFF_IOMMU_CTRL_REG);
	softsp->tsb_base_addr = REG_ADDR(address, OFF_TSB_BASE_ADDR);
	softsp->iommu_flush_reg = REG_ADDR(address, OFF_IOMMU_FLUSH_REG);
	softsp->iommu_tlb_tag = REG_ADDR(address, OFF_IOMMU_TLB_TAG);
	softsp->iommu_tlb_data = REG_ADDR(address, OFF_IOMMU_TLB_DATA);

#undef	REG_ADDR

	mutex_init(&softsp->dma_pool_lock, "sbus dma pool lock",
		MUTEX_DEFAULT, NULL);

	mutex_init(&softsp->intr_poll_list_lock, "sbus intr_poll list lock",
		MUTEX_DEFAULT, NULL);

	/* Set up the DVMA resource sizes */
	softsp->iommu_dvma_size = iommu_tsb_alloc_size[softsp->upa_id]
	    << IOMMU_TSB_TO_RNG;
	softsp->iommu_dvma_base = 0 - softsp->iommu_dvma_size;
	softsp->iommu_dvma_pagebase = iommu_btop(softsp->iommu_dvma_base);

	/* initialize the DVMA resource map */
	softsp->dvmamap = kmem_zalloc(sizeof (struct map)
	    * SBUSMAP_FRAG(iommu_btop(softsp->iommu_dvma_size)), KM_NOSLEEP);

	if (softsp->dvmamap == NULL) {
		cmn_err(CE_WARN, "sbus_attach: kmem_zalloc failed\n");
		return (DDI_FAILURE);
	}

	mapinit(softsp->dvmamap, (long) iommu_btop(softsp->iommu_dvma_size),
	    (u_long) softsp->iommu_dvma_pagebase,
	    mapstr, SBUSMAP_FRAG(iommu_btop(softsp->iommu_dvma_size)));

	softsp->dma_reserve = SBUSMAP_MAXRESERVE;

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	mutex_init(&softsp->iomemlock, "DMA iomem lock", MUTEX_DEFAULT, NULL);
	softsp->iomem = (struct io_mem_list *) 0;
#endif /* DEBUG && IO_MEMUSAGE */
	/*
	 * Get the base address of the TSB table and store it in the hardware
	 */
	if (!iommu_tsb_vaddr[softsp->upa_id]) {
		cmn_err(CE_WARN, "Unable to retrieve IOMMU array.");
		return (DDI_FAILURE);
	}

	softsp->soft_tsb_base_addr = (u_ll_t *)iommu_tsb_vaddr[softsp->upa_id];

	/*
	 * We plan on the PROM flushing all TLB entries.  If this is not the
	 * case, this is where we should flush the hardware TLB.
	 */

	/* Set the IOMMU registers */
	(void) iommu_resume_init(softsp);

	/* check the convenient copy of TSB base, and flush write buffers */
	if (*softsp->tsb_base_addr !=
	    va_to_pa((caddr_t)softsp->soft_tsb_base_addr))
		return (DDI_FAILURE);

	softsp->sbus_io_lo_pfn = 0xffffffffu;
	softsp->sbus_io_hi_pfn = 0;
	for (i = 0; i < sysio_pd_getnrng(softsp->dip); i++) {
		struct rangespec *rangep;
		u_ll_t addr;
		u_int hipfn, lopfn;

		rangep = sysio_pd_getrng(softsp->dip, i);
		addr = (u_ll_t) ((u_ll_t) rangep->rng_bustype << 32);
		addr |= (u_ll_t) rangep->rng_offset;
		lopfn = (u_int) (addr >> MMU_PAGESHIFT);
		addr += (u_ll_t) (rangep->rng_size - 1);
		hipfn = (u_int) (addr >> MMU_PAGESHIFT);

		softsp->sbus_io_lo_pfn = (lopfn < softsp->sbus_io_lo_pfn) ?
		    lopfn : softsp->sbus_io_lo_pfn;

		softsp->sbus_io_hi_pfn = (hipfn > softsp->sbus_io_hi_pfn) ?
		    hipfn : softsp->sbus_io_hi_pfn;
	}

	DPRINTF(IOMMU_REGISTERS_DEBUG, ("IOMMU Control reg: 0x%x, IOMMU TSB "
	    "base reg: 0x%x, IOMMU flush reg: 0x%x TSB base addr 0x%x\n",
	    softsp->iommu_ctrl_reg, softsp->tsb_base_addr,
	    softsp->iommu_flush_reg, softsp->soft_tsb_base_addr));

	return (DDI_SUCCESS);
}

/*
 * Initialize iommu hardware registers when the system is being resumed.
 * (Subset of iommu_init())
 */
int
iommu_resume_init(struct sbus_soft_state *softsp)
{
	int i;
	u_int tsb_size;

	/*
	 * Reset the base address of the TSB table in the hardware
	 */
	*softsp->tsb_base_addr = va_to_pa((caddr_t)softsp->soft_tsb_base_addr);

	/*
	 * Figure out the correct size of the IOMMU TSB entries.  If we
	 * end up with a size smaller than that needed for 8M of IOMMU
	 * space, default the size to 8M.  XXX We could probably panic here
	 */
	i = sizeof (sysio_iommu_tsb_sizes) / sizeof (sysio_iommu_tsb_sizes[0])
	    - 1;

	while (i > 0) {
		if (iommu_tsb_alloc_size[softsp->upa_id] >=
		    sysio_iommu_tsb_sizes[i])
			break;
		i--;
	}

	tsb_size = i;

	/* OK, lets flip the "on" switch of the IOMMU */
	*softsp->iommu_ctrl_reg = (u_ll_t)(tsb_size << TSB_SIZE_SHIFT
	    | IOMMU_ENABLE | IOMMU_DIAG_ENABLE);

	return (DDI_SUCCESS);
}

#define	ALIGN_REQUIRED(align)	(align != (u_int) -1)
#define	COUNTER_RESTRICTION(cntr)	(cntr != (u_int) -1)
#define	SEG_ALIGN(addr, seg)		(iommu_btop(((((addr) + (u_long) 1) +  \
					    (seg)) & ~(seg))))
#define	WITHIN_DVMAMAP(page)	\
	((page >= softsp->iommu_dvma_pagebase) && \
	(page - softsp->iommu_dvma_pagebase \
	< iommu_btop(softsp->iommu_dvma_size)))

/*ARGSUSED*/
u_long
getdvmapages(int npages, u_long addrlo, u_long addrhi, u_int align,
	u_int cntr, int cansleep, struct map *dvmamap,
	struct sbus_soft_state *softsp)
{
	u_long alo = iommu_btop(addrlo);
	u_long ahi, amax, amin, aseg;
	u_long addr = 0;

	if (addrhi != (u_long) -1) {
		/*
		 * -1 is our magic NOOP for no high limit. If it's not -1,
		 * make addrhi 1 bigger since ahi is a non-inclusive limit,
		 * but addrhi is an inclusive limit.
		 */
		addrhi++;
		amax = iommu_btop(addrhi);
	} else {
		amax = iommu_btop(addrhi) + 1;
	}
	/*
	 * If we have a counter restriction we adjust ahi to the
	 * minimum of the maximum address and the end of the
	 * current segment. Actually it is the end+1 since ahi
	 * is always excluding. We then allocate dvma space out
	 * of a segment instead from the whole map. If the allocation
	 * fails we try the next segment.
	 */
	if (COUNTER_RESTRICTION(cntr)) {
		u_long a;

		if (WITHIN_DVMAMAP(alo)) {
			a = addrlo;
		} else {
			a = softsp->iommu_dvma_base;
		}
		/*
		 * check for wrap around
		 */
		if (a + (u_long) 1 + cntr <= a) {
			ahi = iommu_btop((u_long) -1) + 1;
		} else {
			ahi = SEG_ALIGN(a, cntr);
		}
		ahi = min(amax, ahi);
		aseg = ahi;
		amin = alo;
	} else {
		ahi = amax;
	}

	/*
	 * we may have a 'constrained' allocation;
	 * if so, we have to search dvmamap for a piece
	 * that fits the constraints.
	 */
	if (WITHIN_DVMAMAP(alo) || WITHIN_DVMAMAP(ahi) ||
	    COUNTER_RESTRICTION(cntr)) {
		register struct map *mp;
		/*
		 * Search for a piece that will fit.
		 */
		mutex_enter(&maplock(dvmamap));
again:
		for (mp = mapstart(dvmamap); mp->m_size; mp++) {
			u_int ok, end;

			end = mp->m_addr + mp->m_size;

			if (alo < mp->m_addr) {
				if (ahi >= end)
					ok = (mp->m_size >= npages);
				else {
					end = ahi;
					ok = (mp->m_addr + npages <= ahi);
				}
				addr = mp->m_addr;
			} else {
				if (ahi >= end)
					ok = (alo + npages <= end);
				else {
					end = ahi;
					ok = (alo + npages <= ahi);
				}
				addr = alo;
			}

			DPRINTF(IOMMU_DMAMAP_DEBUG, ("Map range %x:%x alo %x "
			    "ahi %x addr %x end %x\n", mp->m_addr,
			    mp->m_addr + mp->m_size, alo, ahi, addr, end));

			/* If we have a valid region, we're done */
			if (ok)
				break;

		}

		if (mp->m_size != 0) {
			addr = rmget(dvmamap, (long)npages, addr);

		} else {
			addr = 0;
		}

		if (addr == 0) {
			/*
			 * If we have a counter restriction we walk the
			 * dvma space in segments at a time. If we
			 * reach the last segment we reset alo and ahi
			 * to the original values. This allows us to
			 * walk the segments again in case we have to
			 * switch to unaligned mappings or we were out
			 * of resources.
			 */
			if (COUNTER_RESTRICTION(cntr)) {
				if (ahi < amax) {
					alo = ahi;
					ahi = min(amax,
						ahi + mmu_btopr(cntr));
					goto again;
				} else {
					/*
					 * reset alo and ahi in case we
					 * have to walk the segments again
					 */
					alo = amin;
					ahi = aseg;
				}
			}
		}

		if (addr == 0 && cansleep) {
			DPRINTF(IOMMU_DMAMAP_DEBUG, ("getdvmapages: sleep on "
			    "constrained alloc\n"));

			mapwant(dvmamap) = 1;
			cv_wait(&map_cv(dvmamap), &maplock(dvmamap));
			goto again;
		}

		mutex_exit(&maplock(dvmamap));

	} else {
		if (cansleep) {
			addr = rmalloc_wait(dvmamap, npages);

		} else {
			addr = rmalloc(dvmamap, npages);

		}
	}

	if (addr) {
		addr = iommu_ptob(addr);
	}

	return (addr);
}


static void
iommu_tlb_flush(struct sbus_soft_state *softsp, u_long addr, int npages)
{
	volatile u_ll_t tmpreg;

	if (npages == 1) {
		*softsp->iommu_flush_reg = (u_ll_t) addr;
		tmpreg = *softsp->sbus_ctrl_reg;
	} else {
		volatile u_ll_t *vaddr_reg, *valid_bit_reg;
		u_long hiaddr, ioaddr;
		int i, do_flush = 0;

		hiaddr = addr + (npages * IOMMU_PAGESIZE);
		for (i = 0, vaddr_reg = softsp->iommu_tlb_tag,
		    valid_bit_reg = softsp->iommu_tlb_data;
		    i < IOMMU_TLB_ENTRIES; i++, vaddr_reg++, valid_bit_reg++) {

			tmpreg = *vaddr_reg;
			ioaddr = (u_long) tmpreg << IOMMU_PAGESHIFT;
#ifdef DEBUG
			{
			u_int hi, lo;
			hi = (u_int)(tmpreg >> 32);
			lo = (u_int)(tmpreg & 0xffffffff);
			DPRINTF(IOMMU_TLB, ("Vaddr reg 0x%x, "
			    "TLB vaddr reg hi0x%x lo0x%x, IO addr 0x%x "
			    "Base addr 0x%x, Hi addr 0x%x\n",
			    vaddr_reg, hi, lo, ioaddr, addr, hiaddr));
			}
#endif /* DEBUG */
			if (ioaddr >= addr && ioaddr <= hiaddr) {
				tmpreg = *valid_bit_reg;
#ifdef DEBUG
				{
				u_int hi, lo;
				hi = (u_int)(tmpreg >> 32);
				lo = (u_int)(tmpreg & 0xffffffff);
				DPRINTF(IOMMU_TLB, ("Valid reg addr 0x%x, "
				    "TLB valid reg hi0x%x lo0x%x\n",
				    valid_bit_reg, hi, lo));
				}
#endif /* DEBUG */
				if (tmpreg & IOMMU_TLB_VALID) {
					*softsp->iommu_flush_reg = (u_ll_t)
					    ioaddr;
					do_flush = 1;
				}
			}
		}

		if (do_flush)
			tmpreg = *softsp->sbus_ctrl_reg;
	}
}


/*
 * Shorthand defines
 */

#define	DMAOBJ_PP_PP	dmao_obj.pp_obj.pp_pp
#define	DMAOBJ_PP_OFF	dmao_ogj.pp_obj.pp_offset
#define	ALO		dma_lim->dlim_addr_lo
#define	AHI		dma_lim->dlim_addr_hi
#define	CMAX		dma_lim->dlim_cntr_max
#define	OBJSIZE		dmareq->dmar_object.dmao_size
#define	ORIGVADDR	dmareq->dmar_object.dmao_obj.virt_obj.v_addr
#define	DIRECTION	(mp->dmai_rflags & DDI_DMA_RDWR)
#define	IOTTE_NDX(vaddr, base) (base + \
		(int) (iommu_btop((vaddr & ~IOMMU_PAGEMASK) - \
		softsp->iommu_dvma_base)))
/*
 * If DDI_DMA_PARTIAL flag is set and the request is for
 * less than MIN_DVMA_WIN_SIZE, it's not worth the hassle so
 * we turn off the DDI_DMA_PARTIAL flag
 */
#define	MIN_DVMA_WIN_SIZE	(128)

/* ARGSUSED */
void
iommu_remove_mappings(ddi_dma_impl_t *mp)
{

#if	defined(DEBUG) && defined(IO_MEMDEBUG)
	register u_int npages;
	register u_long ioaddr;
	volatile u_ll_t *iotte_ptr;
	u_long ioaddr = mp->dmai_mapping & ~IOMMU_PAGEOFFSET;
	u_int npages = mp->dmai_ndvmapages;
	struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;
	struct sbus_soft_state *softsp = mppriv->softsp;

#if	defined(IO_MEMUSAGE)
	struct io_mem_list **prevp, *walk;
#endif /* DEBUG && IO_MEMUSAGE */

	ASSERT(softsp != NULL);
	/*
	 * Run thru the mapped entries and free 'em
	 */

	ioaddr = mp->dmai_mapping & ~IOMMU_PAGEOFFSET;
	npages = mp->dmai_ndvmapages;

#if	defined(IO_MEMUSAGE)
	mutex_enter(&softsp->iomemlock);
	prevp = &softsp->iomem;
	walk = softsp->iomem;

	while (walk) {
		if (walk->ioaddr == ioaddr) {
			*prevp = walk->next;
			break;
		}

		prevp = &walk->next;
		walk = walk->next;
	}
	mutex_exit(&softsp->iomemlock);

	kmem_free(walk->pfn, sizeof (u_int) * (npages + 1));
	kmem_free(walk, sizeof (struct io_mem_list));
#endif /* IO_MEMUSAGE */

	iotte_ptr = IOTTE_NDX(ioaddr,
			softsp->soft_tsb_base_addr);

	while (npages) {
		DPRINTF(IOMMU_DMAMCTL_DEBUG,
		    ("dma_mctl: freeing virt "
			"addr 0x%x, with IOTTE index 0x%x.\n",
			ioaddr, iotte_ptr));
		*iotte_ptr = (u_ll_t)0;	/* unload tte */
		iommu_tlb_flush(softsp, ioaddr, 1);
		npages--;
		ioaddr += IOMMU_PAGESIZE;
		iotte_ptr++;
	}
#endif	/* DEBUG && IO_MEMDEBUG */

}


int
iommu_create_vaddr_mappings(ddi_dma_impl_t *mp, u_long addr)
{
	extern struct as kas;
	u_int pfn;
	struct as *as = 0;
	register int npages;
	register u_long ioaddr;
	u_long offset;
	volatile u_ll_t *iotte_ptr;
				/* Set Valid and Cache for mem xfer */
	u_ll_t tmp_iotte_flag =
	    IOTTE_VALID | IOTTE_CACHE | IOTTE_WRITE | IOTTE_STREAM;
	int rval = DDI_DMA_MAPPED;
	struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;
	struct sbus_soft_state *softsp = mppriv->softsp;
	int diag_tlb_flush;
	ASSERT(softsp != NULL);

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	struct io_mem_list *iomemp;
	u_int *pfnp;
#endif /* DEBUG && IO_MEMUSAGE */

	offset = mp->dmai_mapping & IOMMU_PAGEOFFSET;
	npages = iommu_btopr(mp->dmai_size + offset);
	ioaddr = mp->dmai_mapping & ~IOMMU_PAGEOFFSET;
	iotte_ptr = IOTTE_NDX(ioaddr, softsp->soft_tsb_base_addr);
	diag_tlb_flush = npages > tlb_flush_using_diag ? 1 : 0;

	as = mp->dmai_object.dmao_obj.virt_obj.v_as;
	if (as == (struct as *) 0)
		as = &kas;

	/*
	 * Set the per object bits of the TTE here. We optimize this for
	 * the memory case so that the while loop overhead is minimal.
	 */
	/* Turn on NOSYNC if we need consistent mem */
	if (mp->dmai_rflags & DDI_DMA_CONSISTENT) {
		mp->dmai_rflags |= DMP_NOSYNC;
		tmp_iotte_flag ^= IOTTE_STREAM;
	/* Set streaming mode if not consistent mem */
	} else if (softsp->stream_buf_off) {
		tmp_iotte_flag ^= IOTTE_STREAM;
	}

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	iomemp = kmem_alloc(sizeof (struct io_mem_list),
	    KM_SLEEP);
	iomemp->rdip = mp->dmai_rdip;
	iomemp->ioaddr = ioaddr;
	iomemp->addr = addr;
	iomemp->npages = npages;
	pfnp = iomemp->pfn = kmem_zalloc(
	    sizeof (u_int) * (npages + 1), KM_SLEEP);
#endif /* DEBUG && IO_MEMUSAGE */
	/*
	 * Grab the mappings from the dmmu and stick 'em into the
	 * iommu.
	 */
	ASSERT(npages > 0);

	/* If we're going to flush the TLB using diag mode, do it now. */
	if (diag_tlb_flush)
		iommu_tlb_flush(softsp, ioaddr, npages);

	do {
		u_ll_t iotte_flag;
		extern int pf_is_memory(u_int);

		iotte_flag = tmp_iotte_flag;

		/*
		 * Fetch the pfn for the DMA object
		 */

		ASSERT(as);
		pfn = hat_getpfnum(as->a_hat, (caddr_t) addr);
		ASSERT(pfn != (u_int) -1);

		if (!pf_is_memory(pfn)) {
			/* DVMA'ing to IO space */

			/* Turn off cache bit if set */
			if (iotte_flag & IOTTE_CACHE)
				iotte_flag ^= IOTTE_CACHE;

			/* Turn off stream bit if set */
			if (iotte_flag & IOTTE_STREAM)
				iotte_flag ^= IOTTE_STREAM;

			if (IS_INTRA_SBUS(softsp, pfn)) {
				/* Intra sbus transfer */

				/* Turn on intra flag */
				iotte_flag |= IOTTE_INTRA;
#ifdef DEBUG
				{
				u_int hi, lo;
				hi = (u_int)(iotte_flag >> 32);
				lo = (u_int)(iotte_flag & 0xffffffff);
				DPRINTF(IOMMU_INTER_INTRA_XFER, (
				    "Intra xfer "
				    "pfnum 0x%x TTE hi0x%x lo0x%x\n",
				    pfn, hi, lo));
				}
#endif /* DEBUG */
			} else {
				extern int pf_is_dmacapable(u_int);

				if (pf_is_dmacapable(pfn) == 1) {
/* EMPTY */
#ifdef DEBUG
					{
					u_int hi, lo;
					hi = (u_int)(iotte_flag >> 32);
					lo = (u_int)(iotte_flag &
					    0xffffffff);
					DPRINTF(IOMMU_INTER_INTRA_XFER,
					    ("Inter xfer pfnum 0x%x "
					    "TTE hi 0x%x lo 0x%x\n",
					    pfn, hi, lo));
					}
#endif /* DEBUG */
				} else {
					rval = DDI_DMA_NOMAPPING;
#if	defined(DEBUG) && defined(IO_MEMDEBUG)
					goto bad;
#endif /* DEBUG && IO_MEMDEBUG */
				}
			}
		}
		addr += IOMMU_PAGESIZE;

#ifdef DEBUG
		{
		u_int hi, lo;
		hi = (u_int)(iotte_flag >> 32);
		lo = (u_int)(iotte_flag & 0xffffffff);
		DPRINTF(IOMMU_TTE, ("vaddr mapping: TTE index 0x%x, pfn 0x%x, "
		    "tte flag hi 0x%x lo 0x%x, addr 0x%x, ioaddr 0x%x\n",
		    iotte_ptr, pfn, hi, lo, addr, ioaddr));
		}
#endif /* DEBUG */

		/* Flush the IOMMU TLB before loading a new mapping */
		if (!diag_tlb_flush)
			iommu_tlb_flush(softsp, ioaddr, 1);

		/* Set the hardware IO TTE */
		*iotte_ptr = ((u_longlong_t)pfn << IOMMU_PAGESHIFT) |
			iotte_flag;	/* load tte */

		ioaddr += IOMMU_PAGESIZE;
		npages--;
		iotte_ptr++;
#if	defined(DEBUG) && defined(IO_MEMUSAGE)
		*pfnp = pfn;
		pfnp++;
#endif /* DEBUG && IO_MEMUSAGE */
	} while (npages > 0);

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	mutex_enter(&softsp->iomemlock);
	iomemp->next = softsp->iomem;
	softsp->iomem = iomemp;
	mutex_exit(&softsp->iomemlock);
#endif /* DEBUG && IO_MEMUSAGE */

	return (rval);

#if	defined(DEBUG) && defined(IO_MEMDEBUG)
bad:
	/* If we fail a mapping, free up any mapping resources used */
	iommu_remove_mappings(mp);
	return (rval);
#endif /* DEBUG && IO_MEMDEBUG */
}


int
iommu_create_pp_mappings(ddi_dma_impl_t *mp, page_t *pp, page_t **pplist)
{
	u_int pfn;
	register int npages;
	register u_long ioaddr;
	u_long offset;
	volatile u_ll_t *iotte_ptr;
				/* Set Valid and Cache for mem xfer */
	u_ll_t tmp_iotte_flag =
	    IOTTE_VALID | IOTTE_CACHE | IOTTE_WRITE | IOTTE_STREAM;
	int rval = DDI_DMA_MAPPED;
	struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;
	struct sbus_soft_state *softsp = mppriv->softsp;
	int diag_tlb_flush;
#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	struct io_mem_list *iomemp;
	u_int *pfnp;
	int diag_tlb_flush;
#endif /* DEBUG && IO_MEMUSAGE */

	ASSERT(softsp != NULL);

	offset = mp->dmai_mapping & IOMMU_PAGEOFFSET;
	npages = iommu_btopr(mp->dmai_size + offset);
	ioaddr = mp->dmai_mapping & ~IOMMU_PAGEOFFSET;
	iotte_ptr = IOTTE_NDX(ioaddr, softsp->soft_tsb_base_addr);
	diag_tlb_flush = npages > tlb_flush_using_diag ? 1 : 0;

	/*
	 * Set the per object bits of the TTE here. We optimize this for
	 * the memory case so that the while loop overhead is minimal.
	 */
	/* Turn on NOSYNC if we need consistent mem */
	if (mp->dmai_rflags & DDI_DMA_CONSISTENT) {
		mp->dmai_rflags |= DMP_NOSYNC;
		tmp_iotte_flag ^= IOTTE_STREAM;
	/* Set streaming mode if not consistent mem */
	} else if (softsp->stream_buf_off) {
		tmp_iotte_flag ^= IOTTE_STREAM;
	}

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	iomemp = kmem_alloc(sizeof (struct io_mem_list),
	    KM_SLEEP);
	iomemp->rdip = mp->dmai_rdip;
	iomemp->ioaddr = ioaddr;
	iomemp->npages = npages;
	pfnp = iomemp->pfn = kmem_zalloc(
	    sizeof (u_int) * (npages + 1), KM_SLEEP);
#endif /* DEBUG && IO_MEMUSAGE */
	/*
	 * Grab the mappings from the dmmu and stick 'em into the
	 * iommu.
	 */
	ASSERT(npages > 0);

	/* If we're going to flush the TLB using diag mode, do it now. */
	if (diag_tlb_flush)
		iommu_tlb_flush(softsp, ioaddr, npages);

	do {
		u_ll_t iotte_flag;

		iotte_flag = tmp_iotte_flag;

		/*
		 * First, fetch the pte(s) we're interested in.
		 */
		if (pp) {
			pfn = page_pptonum(pp);
			pp = pp->p_next;
		} else {
			pfn = page_pptonum(*pplist);
			pplist++;
		}

#ifdef DEBUG
		{
		u_int hi, lo;
		hi = (u_int)(iotte_flag >> 32);
		lo = (u_int)(iotte_flag & 0xffffffff);
		DPRINTF(IOMMU_TTE, ("pp mapping TTE index 0x%x, pfn 0x%x, "
		    "tte flag hi 0x%x lo 0x%x, ioaddr 0x%x\n", iotte_ptr,
		    pfn, hi, lo, ioaddr));
		}
#endif /* DEBUG */

		/* Flush the IOMMU TLB before loading a new mapping */
		if (!diag_tlb_flush)
			iommu_tlb_flush(softsp, ioaddr, 1);

		/* Set the hardware IO TTE */
		*iotte_ptr = ((u_longlong_t)pfn << IOMMU_PAGESHIFT) |
			iotte_flag;	/* load tte */

		/*
		 * adjust values of interest
		 */
		ioaddr += IOMMU_PAGESIZE;
		npages--;
		iotte_ptr++;
#if	defined(DEBUG) && defined(IO_MEMUSAGE)
		*pfnp = pfn;
		pfnp++;
#endif /* DEBUG && IO_MEMUSAGE */
	} while (npages > 0);

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	mutex_enter(&softsp->iomemlock);
	iomemp->next = softsp->iomem;
	softsp->iomem = iomemp;
	mutex_exit(&softsp->iomemlock);
#endif /* DEBUG && IO_MEMUSAGE */

	return (rval);
}


int
iommu_dma_lim_setup(dev_info_t *dip, dev_info_t *rdip,
    struct sbus_soft_state *softsp, u_int *burstsizep, u_int burstsize64,
    u_int *minxferp, u_int dma_flags)
{

	/* Take care of 64 byte limits. */
	if (!(dma_flags & DDI_DMA_SBUS_64BIT)) {
		/*
		 * return burst size for 32-bit mode
		 */
		*burstsizep &= softsp->sbus_burst_sizes;
		return (DDI_FAILURE);
	} else {
		/*
		 * check if SBus supports 64 bit and if caller
		 * is child of SBus. No support through bridges
		 */
		if (softsp->sbus64_burst_sizes &&
		    (ddi_get_parent(rdip) == dip)) {
			struct regspec *rp;

			rp = ddi_rnumber_to_regspec(rdip, 0);
			if (rp == (struct regspec *)0) {
				*burstsizep &=
					softsp->sbus_burst_sizes;
				return (DDI_FAILURE);
			} else {
				/* Check for old-style 64 bit burstsizes */
				if (burstsize64 & SYSIO64_BURST_MASK) {
					/* Scale back burstsizes if Necessary */
					*burstsizep &=
					    (softsp->sbus64_burst_sizes |
					    softsp->sbus_burst_sizes);
				} else {
					/* Get the 64 bit burstsizes. */
					*burstsizep = burstsize64;

					/* Scale back burstsizes if Necessary */
					*burstsizep &=
					    (softsp->sbus64_burst_sizes >>
					    SYSIO64_BURST_SHIFT);
				}

				/*
				 * Set the largest value of the smallest
				 * burstsize that the device or the bus
				 * can manage.
				 */
				*minxferp = max(*minxferp, (1 <<
				    (ddi_ffs(softsp->sbus64_burst_sizes) -1)));

				return (DDI_SUCCESS);
			}
		} else {
			/*
			 * SBus doesn't support it or bridge. Do 32-bit
			 * xfers
			 */
			*burstsizep &= softsp->sbus_burst_sizes;
			return (DDI_FAILURE);
		}
	}
}


int
iommu_dma_allochdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_attr_t *dma_attr, int (*waitfp)(caddr_t), caddr_t arg,
    ddi_dma_handle_t *handlep)
{
	u_long addrlow, addrhigh, segalign;
	ddi_dma_impl_t *mp;
	struct dma_impl_priv *mppriv;
	struct sbus_soft_state *softsp =
		(struct sbus_soft_state *) ddi_get_soft_state(sbusp,
		ddi_get_instance(dip));


	/*
	 * Setup dma burstsizes and min-xfer counts.
	 */
	(void) iommu_dma_lim_setup(dip, rdip, softsp,
	    &dma_attr->dma_attr_burstsizes,
	    (u_int) dma_attr->dma_attr_burstsizes, &dma_attr->dma_attr_minxfer,
	    dma_attr->dma_attr_flags);

	if (dma_attr->dma_attr_burstsizes == 0) {
		return (DDI_DMA_BADATTR);
	}
	addrlow = (u_long)dma_attr->dma_attr_addr_lo;
	addrhigh = (u_long)dma_attr->dma_attr_addr_hi;
	segalign = (u_long)dma_attr->dma_attr_seg;

	/*
	 * Check sanity for hi and lo address limits
	 */
	if ((addrhigh <= addrlow) || (addrhigh
	    < (u_long)softsp->iommu_dvma_base)) {
		return (DDI_DMA_BADATTR);
	}
	if (dma_attr->dma_attr_flags & DDI_DMA_FORCE_PHYSICAL) {
		return (DDI_DMA_BADATTR);
	}

	mppriv = kmem_zalloc(sizeof (*mppriv),
	    (waitfp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);

	if (mppriv == NULL) {
		if (waitfp != DDI_DMA_DONTWAIT) {
		    ddi_set_callback(waitfp, arg, &softsp->dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}
	mp = (ddi_dma_impl_t *) mppriv;

	DPRINTF(IOMMU_DMA_ALLOCHDL_DEBUG, ("dma_allochdl: (%s) handle 0x%x "
	    "hi %x lo 0x%x "
	    "min 0x%x burst 0x%x\n", ddi_get_name(dip), mp, addrhigh, addrlow,
	    dma_attr->dma_attr_minxfer, dma_attr->dma_attr_burstsizes));

	mp->dmai_rdip = rdip;
	mp->dmai_minxfer = (u_int)dma_attr->dma_attr_minxfer;
	mp->dmai_burstsizes = (u_int)dma_attr->dma_attr_burstsizes;
	mp->dmai_attr = *dma_attr;
	/* See if the DMA engine has any limit restrictions. */
	if (segalign == 0xffffffffu && addrhigh == 0xffffffffu &&
	    addrlow == 0) {
		mp->dmai_rflags |= DMP_NOLIMIT;
	}
	mppriv->softsp = softsp;
	mppriv->phys_sync_flag = va_to_pa((caddr_t) &mppriv->sync_flag);

	*handlep = (ddi_dma_handle_t)mp;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
int
iommu_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle)
{
	struct dma_impl_priv *mppriv = (struct dma_impl_priv *)handle;
	struct sbus_soft_state *softsp = mppriv->softsp;
	ASSERT(softsp != NULL);

	kmem_free(mppriv, sizeof (*mppriv));

	if (softsp->dvma_call_list_id != 0) {
		ddi_run_callback(&softsp->dvma_call_list_id);
	}
	return (DDI_SUCCESS);
}


static int
iommu_dma_setup(struct ddi_dma_req *dmareq,
    u_long addrlow, u_long addrhigh, u_int segalign,
    ddi_dma_impl_t *mp)
{
	page_t *pp;
	u_int off;
	u_int size;
/*LINTED warning: constant truncated by assignment */
	u_int align = (u_int) -1;
	u_long ioaddr, offset;
	u_long addr = 0;
	int npages, rval;
	struct sbus_soft_state *softsp;
	struct page **pplist = NULL;
	struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;

	DPRINTF(IOMMU_DMA_SETUP_DEBUG, ("dma_setup: hi %x lo 0x%x\n",
	    addrhigh, addrlow));

	/*
	 * If this is an advisory call, then we're done.
	 */
	if (mp == 0) {
		return (DDI_DMA_MAPOK);
	}

	size = OBJSIZE;
	if ((mp->dmai_rflags & DMP_NOLIMIT) == 0) {
		off = size - 1;

		if (off > segalign) {
			if ((dmareq->dmar_flags & DDI_DMA_PARTIAL) == 0) {
				rval = DDI_DMA_TOOBIG;
				goto bad;
			}
			size = segalign + 1;
		}
		if (addrlow + off > addrhigh || addrlow + off < addrlow) {
			if (!((addrlow + OBJSIZE == 0) &&
			    (addrhigh == (u_long) -1))) {
				if ((dmareq->dmar_flags & DDI_DMA_PARTIAL) ==
				    0) {
					rval = DDI_DMA_TOOBIG;
					goto bad;
				}
				size = min(addrhigh - addrlow + 1, size);
			}
		}
	}

	switch (dmareq->dmar_object.dmao_type) {
	case DMA_OTYP_VADDR:
		addr = (u_long) dmareq->dmar_object.dmao_obj.virt_obj.v_addr;
		offset = addr & IOMMU_PAGEOFFSET;
		addr &= ~IOMMU_PAGEOFFSET;
		pplist = dmareq->dmar_object.dmao_obj.virt_obj.v_priv;

		npages = iommu_btopr(OBJSIZE + offset);

		DPRINTF(IOMMU_DMAMAP_DEBUG, ("dma_map vaddr: # of pages 0x%x "
			    "request addr 0x%x off 0x%x OBJSIZE  0x%x \n",
			    npages, addr, offset, OBJSIZE));

		/* We don't need the addr anymore if we have a shadow list */
		if (pplist)
			addr = NULL;
		pp = NULL;
		break;

	case DMA_OTYP_PAGES:
		pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
		offset = dmareq->dmar_object.dmao_obj.pp_obj.pp_offset;
		npages = iommu_btopr(OBJSIZE + offset);

		DPRINTF(IOMMU_DMA_SETUP_DEBUG, ("dma_setup pages: pg %x"
				"pp = %x , offset = %x OBJSIZE = %x \n",
				npages, pp, offset, OBJSIZE));
		break;

	case DMA_OTYP_PADDR:
	default:
		/*
		 * Not a supported type for this implementation
		 */
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}

	/* Get our soft state once we know we're mapping an object. */
	softsp = mppriv->softsp;
	ASSERT(softsp != NULL);

	if (mp->dmai_rflags & DDI_DMA_PARTIAL) {
		/*
		 * If the size was rewritten above due to device dma
		 * constraints, make sure that it still makes sense
		 * to attempt anything. Also, in this case, the
		 * ability to do a dma mapping at all predominates
		 * over any attempt at optimizing the size of such
		 * a mapping.
		 */

		if (size != OBJSIZE) {
			/*
			 * If the request is for partial mapping arrangement,
			 * the device has to be able to address at least the
			 * size of the window we are establishing.
			 */
			if (size < iommu_ptob(MIN_DVMA_WIN_SIZE)) {
				rval = DDI_DMA_NOMAPPING;
				goto bad;
			}
			npages = iommu_btopr(size + offset);
		}
		/*
		 * If the size requested is less than a moderate amt,
		 * skip the partial mapping stuff- it's not worth the
		 * effort.
		 */
		if (npages > MIN_DVMA_WIN_SIZE) {
			npages = MIN_DVMA_WIN_SIZE + iommu_btopr(offset);
			size = iommu_ptob(MIN_DVMA_WIN_SIZE);
			DPRINTF(IOMMU_DMA_SETUP_DEBUG, ("dma_setup: SZ %x pg "
			    "%x sz %x \n", OBJSIZE, npages, size));
			if (pplist != NULL) {
				mp->dmai_minfo = (void *)pplist;
				mp->dmai_rflags |= DMP_SHADOW;
			}
		} else {
			mp->dmai_rflags ^= DDI_DMA_PARTIAL;
		}
	} else {
		if (npages >= iommu_btop(softsp->iommu_dvma_size)
		    - MIN_DVMA_WIN_SIZE) {
			rval = DDI_DMA_TOOBIG;
			goto bad;
		}
	}

	/*
	 * save dmareq-object, size and npages into mp
	 */
	mp->dmai_object = dmareq->dmar_object;
	mp->dmai_size = size;
	mp->dmai_ndvmapages = npages;

	/*
	 * Okay- we have to do some mapping here.
	 */
	if ((mp->dmai_rflags & DMP_NOLIMIT) != 0) {
		u_long pf;

		if (dmareq->dmar_fp == DDI_DMA_SLEEP) {
			pf = rmalloc_wait(softsp->dvmamap, npages);
		} else {
			mutex_enter(&maplock(softsp->dvmamap));
			pf = rmalloc_locked(softsp->dvmamap, npages);
			mutex_exit(&maplock(softsp->dvmamap));

			/* Fail the request if resources are not available. */
			if (pf == 0) {
				rval = DDI_DMA_NORESOURCES;
				goto bad;
			}
		}

		ioaddr = iommu_ptob(pf);

		/*
		 * If we have a 1 page request and we're working with a page
		 * list, we're going to speed load an IOMMU entry.
		 */
		if ((npages) == 1 && !addr) {
			u_ll_t iotte_flag = IOTTE_VALID | IOTTE_CACHE |
			    IOTTE_WRITE | IOTTE_STREAM;
			volatile u_ll_t *iotte_ptr;
			u_int pfn;
#if	defined(DEBUG) && defined(IO_MEMUSAGE)
			struct io_mem_list *iomemp;
			u_int *pfnp;
#endif /* DEBUG && IO_MEMUSAGE */

			iotte_ptr = IOTTE_NDX(ioaddr,
			    softsp->soft_tsb_base_addr);

			if (mp->dmai_rflags & DDI_DMA_CONSISTENT) {
				mp->dmai_rflags |= DMP_NOSYNC;
				iotte_flag ^= IOTTE_STREAM;
			} else if (!softsp->stream_buf_off)
				iotte_flag ^= IOTTE_STREAM;

			if (pp)
				pfn = page_pptonum(pp);
			else
				pfn = page_pptonum(*pplist);

			iommu_tlb_flush(softsp, ioaddr, 1);

			*iotte_ptr = ((u_longlong_t)pfn << IOMMU_PAGESHIFT) |
			    iotte_flag;

			mp->dmai_mapping = (u_long) (ioaddr + offset);
			mp->dmai_nwin = 0;

#ifdef DEBUG
			{
			u_int hi, lo;
			hi = (u_int)(iotte_flag >> 32);
			lo = (u_int)(iotte_flag & 0xffffffff);
			DPRINTF(IOMMU_TTE, ("speed loading: TTE index 0x%x, "
			    "pfn 0x%x, tte flag hi 0x%x lo 0x%x, addr 0x%x, "
			    "ioaddr 0x%x\n", iotte_ptr, pfn, hi, lo, addr,
			    ioaddr));
			}
#endif /* DEBUG */
#if	defined(DEBUG) && defined(IO_MEMUSAGE)
			iomemp =
			    kmem_alloc(sizeof (struct io_mem_list), KM_SLEEP);
			iomemp->rdip = mp->dmai_rdip;
			iomemp->ioaddr = ioaddr;
			iomemp->addr = addr;
			iomemp->npages = npages;
			pfnp = iomemp->pfn = kmem_zalloc(
			    sizeof (u_int) * (npages + 1), KM_SLEEP);
			*pfnp = pfn;
			mutex_enter(&softsp->iomemlock);
			iomemp->next = softsp->iomem;
			softsp->iomem = iomemp;
			mutex_exit(&softsp->iomemlock);
#endif /* DEBUG && IO_MEMUSAGE */

			return (DDI_DMA_MAPPED);
		}
	} else {
		ioaddr = getdvmapages(npages, addrlow, addrhigh, align,
		    segalign, (dmareq->dmar_fp == DDI_DMA_SLEEP)? 1 : 0,
		    softsp->dvmamap, softsp);
	}

	if (ioaddr == 0) {
		if (dmareq->dmar_fp == DDI_DMA_SLEEP)
			rval = DDI_DMA_NOMAPPING;
		else
			rval = DDI_DMA_NORESOURCES;
		goto bad;
	}

	/*
	 * establish real virtual address for caller
	 * This field is invariant throughout the
	 * life of the mapping.
	 */

	mp->dmai_mapping = (u_long) (ioaddr + offset);

	ASSERT(mp->dmai_mapping >= softsp->iommu_dvma_base);

	/*
	 * At this point we have a range of virtual address allocated
	 * with which we now have to map to the requested object.
	 */
	if (addr) {
		if ((rval = iommu_create_vaddr_mappings(mp, addr))
		    == DDI_DMA_NOMAPPING)
			goto bad;
	} else {
		if ((rval = iommu_create_pp_mappings(mp, pp, pplist))
		    == DDI_DMA_NOMAPPING)
			goto bad;
	}

	DPRINTF(IOMMU_DMA_SETUP_DEBUG, ("dma_setup: handle %x flags %x "
	    "kaddr %x size %x\n", mp, mp->dmai_rflags,
	    mp->dmai_mapping, mp->dmai_size));
	if (mp->dmai_rflags & DDI_DMA_PARTIAL) {
		size = iommu_ptob(
			mp->dmai_ndvmapages - iommu_btopr(offset));
		mp->dmai_nwin =
		    (dmareq->dmar_object.dmao_size + (size - 1)) / size;
		return (DDI_DMA_PARTIAL_MAP);
	} else {
		mp->dmai_nwin = 0;
		return (DDI_DMA_MAPPED);
	}
bad:

	DPRINTF(IOMMU_DMA_SETUP_DEBUG, ("?*** iommu_dma_setup: failure(%d)\n",
	    rval));

	if (rval == DDI_DMA_NORESOURCES &&
	    dmareq->dmar_fp != DDI_DMA_DONTWAIT) {
		ddi_set_callback(dmareq->dmar_fp,
		    dmareq->dmar_arg, &softsp->dvma_call_list_id);
	}
	return (rval);
}

/* ARGSUSED */
int
iommu_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cp, u_int *ccountp)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ddi_dma_attr_t *dma_attr;
	u_long addrlow, addrhigh;
	int rval;


	/*
	 * no mutex for speed
	 */
	if (mp->dmai_inuse) {
		return (DDI_DMA_INUSE);
	}
	mp->dmai_inuse = 1;
	mp->dmai_offset = 0;
	mp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
	dma_attr = &mp->dmai_attr;
	addrlow = (u_long)dma_attr->dma_attr_addr_lo;
	addrhigh = (u_long)dma_attr->dma_attr_addr_hi;


	rval = iommu_dma_setup(dmareq, addrlow, addrhigh,
			(u_int)dma_attr->dma_attr_seg, mp);
	if (rval && (rval != DDI_DMA_PARTIAL_MAP)) {
		mp->dmai_inuse = 0;
		return (rval);
	}
	cp->dmac_notused = 0;
	cp->dmac_address = mp->dmai_mapping;
	cp->dmac_size = mp->dmai_size;
	cp->dmac_type = 0;
	*ccountp = 1;

	DPRINTF(IOMMU_DMA_BINDHDL_DEBUG, ("iommu_dma_bindhdl :"
	    "Handle 0x%x, cookie addr 0x%x, size 0x%x \n", mp,
	    cp->dmac_address, cp->dmac_size));

	return (rval);
}

/* ARGSUSED */
int
iommu_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	register u_long addr;
	register u_int npages;
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *) handle;
	struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;
	struct sbus_soft_state *softsp = mppriv->softsp;
	ASSERT(softsp != NULL);

	addr = mp->dmai_mapping & ~IOMMU_PAGEOFFSET;
	npages = mp->dmai_ndvmapages;

	DPRINTF(IOMMU_DMA_UNBINDHDL_DEBUG, ("iommu_dma_unbindhdl :"
	    "unbinding Virt addr 0x%x, for 0x%x pages.\n", addr,
	    mp->dmai_ndvmapages));

	/* sync the entire object */
	if (!(mp->dmai_rflags & DDI_DMA_CONSISTENT)) {
		/* flush stream write buffers */
		sync_stream_buf(softsp, addr, npages, (int *)&mppriv->sync_flag,
		    mppriv->phys_sync_flag);
	}

#if	defined(DEBUG) && defined(IO_MEMDEBUG)
	/*
	 * 'Free' the dma mappings.
	 */
	iommu_remove_mappings(mp);
#endif /* DEBUG && IO_MEMDEBUG */

	ASSERT(npages > (u_int)0);
	rmfree(softsp->dvmamap, (long)(npages), iommu_btop(addr));

	mp->dmai_ndvmapages = 0;
	mp->dmai_inuse = 0;
	mp->dmai_minfo = NULL;

	if (softsp->dvma_call_list_id != 0) {
		ddi_run_callback(&softsp->dvma_call_list_id);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
iommu_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, u_int len,
    u_int cache_flags)
{
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *) handle;
	struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;

	/* Make sure our mapping structure is valid */
	if (!mp)
		return (DDI_FAILURE);

	if (!(mp->dmai_rflags & DDI_DMA_CONSISTENT)) {
		sync_stream_buf(mppriv->softsp, mp->dmai_mapping,
		    mp->dmai_ndvmapages, (int *)&mppriv->sync_flag,
		    mppriv->phys_sync_flag);
	}
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
iommu_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    uint_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	u_long offset;
	u_long winsize, newoff;
	int rval;


	offset = mp->dmai_mapping & IOMMU_PAGEOFFSET;
	winsize = iommu_ptob(mp->dmai_ndvmapages - iommu_btopr(offset));

	DPRINTF(IOMMU_DMA_WIN_DEBUG, ("getwin win %d winsize %x\n", win,
	    (int)winsize));

	/*
	 * win is in the range [0 .. dmai_nwin-1]
	 */
	if (win >= mp->dmai_nwin) {
		return (DDI_FAILURE);
	}

	newoff = win * winsize;
	if (newoff > mp->dmai_object.dmao_size - mp->dmai_minxfer) {
		return (DDI_FAILURE);
	}

	ASSERT(cookiep);
	cookiep->dmac_notused = 0;
	cookiep->dmac_type = 0;
	cookiep->dmac_address = mp->dmai_mapping;
	cookiep->dmac_size = mp->dmai_size;
	*ccountp = 1;
	*offp = (off_t)newoff;
	*lenp = (u_int)winsize;

	if (newoff == mp->dmai_offset) {
		/*
		 * Nothing to do...
		 */
		return (DDI_SUCCESS);
	}

	if ((rval = iommu_map_window(mp, newoff, winsize)) !=
	    DDI_SUCCESS) {
		return (rval);
	}

	/*
	 * Set this again in case iommu_map_window() has changed it
	 */
	cookiep->dmac_size = mp->dmai_size;

	return (DDI_SUCCESS);
}

static int
iommu_map_window(ddi_dma_impl_t *mp, u_long newoff,
    u_long winsize)
{
	u_long addr = 0;
	page_t *pp;
	u_long flags;
	struct page **pplist = NULL;

#if	defined(DEBUG) && defined(IO_MEMDEBUG)
	/* Free mappings for current window */
	iommu_remove_mappings(mp);
#endif /* DEBUG && IO_MEMDEBUG */

	mp->dmai_offset = newoff;
	mp->dmai_size = mp->dmai_object.dmao_size - newoff;
	mp->dmai_size = min(mp->dmai_size, winsize);

	if (mp->dmai_object.dmao_type == DMA_OTYP_VADDR) {
		if (mp->dmai_rflags & DMP_SHADOW) {
			pplist = (struct page **)mp->dmai_minfo;
			ASSERT(pplist != NULL);
			pplist = pplist + (newoff >> MMU_PAGESHIFT);
		} else {
			addr = (u_long)
			    mp->dmai_object.dmao_obj.virt_obj.v_addr;
			addr = (addr + newoff) & ~IOMMU_PAGEOFFSET;
		}
		pp = NULL;
	} else {
		pp = mp->dmai_object.dmao_obj.pp_obj.pp_pp;
		flags = 0;
		while (flags < newoff) {
			pp = pp->p_next;
			flags += MMU_PAGESIZE;
		}
	}

	/* Set up mappings for next window */
	if (addr) {
		if (iommu_create_vaddr_mappings(mp, addr) < 0)
			return (DDI_FAILURE);
	} else {
		if (iommu_create_pp_mappings(mp, pp, pplist) < 0)
			return (DDI_FAILURE);
	}

	/*
	 * also invalidate read stream buffer
	 */
	if (!(mp->dmai_rflags & DDI_DMA_CONSISTENT)) {
		struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;

		sync_stream_buf(mppriv->softsp, mp->dmai_mapping,
		    mp->dmai_ndvmapages, (int *)&mppriv->sync_flag,
		    mppriv->phys_sync_flag);
	}

	return (DDI_SUCCESS);

}

int
iommu_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	ddi_dma_lim_t *dma_lim = dmareq->dmar_limits;
	ddi_dma_impl_t *mp;
	struct dma_impl_priv *mppriv;
	u_long addrlow, addrhigh;
	u_int segalign;
	int rval;
	struct sbus_soft_state *softsp =
		(struct sbus_soft_state *) ddi_get_soft_state(sbusp,
		ddi_get_instance(dip));


	/*
	 * Setup dma burstsizes and min-xfer counts.
	 */
	(void) iommu_dma_lim_setup(dip, rdip, softsp, &dma_lim->dlim_burstsizes,
	    (u_int) dma_lim->dlim_burstsizes, &dma_lim->dlim_minxfer,
	    dmareq->dmar_flags);


	DPRINTF(IOMMU_DMAMAP_DEBUG, ("dma_map: %s (%s) hi %x lo 0x%x min 0x%x "
	    "burst 0x%x\n", (handlep)? "alloc" : "advisory",
	    ddi_get_name(rdip), AHI, ALO, dma_lim->dlim_minxfer,
	    dma_lim->dlim_burstsizes));

	addrlow = dma_lim->dlim_addr_lo;
	addrhigh = dma_lim->dlim_addr_hi;
	segalign = dma_lim->dlim_cntr_max;

	/*
	 * If not an advisory call, get a dma record.
	 */
	if (handlep) {
		mppriv = kmem_zalloc(sizeof (*mppriv),
		    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
		mp = (ddi_dma_impl_t *) mppriv;

		DPRINTF(IOMMU_DMAMAP_DEBUG, ("iommu_dma_map: handlep != 0, "
		    "mp= 0x%x\n", (u_int)(mp)));

		if (mp == 0) {
			if (dmareq->dmar_fp != DDI_DMA_DONTWAIT)
				ddi_set_callback(dmareq->dmar_fp,
				dmareq->dmar_arg, &softsp->dvma_call_list_id);
			rval = DDI_DMA_NORESOURCES;
			goto bad;
		}

		/*
		 * Save requestor's information
		 */
		mp->dmai_rdip = rdip;
		mp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
		mp->dmai_minxfer = dma_lim->dlim_minxfer;
		mp->dmai_burstsizes = dma_lim->dlim_burstsizes;
		mp->dmai_offset = 0;
		mp->dmai_ndvmapages = 0;
		mp->dmai_minfo = 0;
		/* See if the DMA engine has any limit restrictions. */
		if (segalign == 0xffffffffu && addrhigh == 0xffffffffu &&
		    addrlow == 0) {
			mp->dmai_rflags |= DMP_NOLIMIT;
		}
		mppriv->softsp = softsp;
		mppriv->phys_sync_flag = va_to_pa((caddr_t) &mppriv->sync_flag);

	} else {
		mp = (ddi_dma_impl_t *) 0;
	}

	/*
	 * Validate device burstsizes
	 */
	if (dma_lim->dlim_burstsizes == 0) {
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}

	/*
	 * Check sanity for hi and lo address limits
	 */
	if ((addrhigh <= addrlow) ||
	    (addrhigh < (u_long)softsp->iommu_dvma_base)) {
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}

	rval = iommu_dma_setup(dmareq, addrlow, addrhigh,
		dma_lim->dlim_cntr_max, mp);
bad:
	if (rval && (rval != DDI_DMA_PARTIAL_MAP)) {
		if (mp) {
			kmem_free(mppriv, sizeof (*mppriv));
		}
	} else {
		if (mp) {
			*handlep = (ddi_dma_handle_t)mp;
		}
	}
	return (rval);
}

/*ARGSUSED*/
int
iommu_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, u_int *lenp,
    caddr_t *objp, u_int cache_flags)
{
	register u_long addr, offset;
	register u_int npages;
	register ddi_dma_cookie_t *cp;
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *) handle;

	DPRINTF(IOMMU_DMAMCTL_DEBUG, ("dma_mctl: handle %x ", mp));
	switch (request) {
	case DDI_DMA_FREE:
	{
		struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;
		struct sbus_soft_state *softsp = mppriv->softsp;
		ASSERT(softsp != NULL);

		/*
		 * 'Free' the dma mappings.
		 */
		addr = mp->dmai_mapping & ~IOMMU_PAGEOFFSET;
		npages = mp->dmai_ndvmapages;

		DPRINTF(IOMMU_DMAMCTL_DMA_FREE_DEBUG, ("iommu_dma_mctl dmafree:"
		    "freeing Virt addr 0x%x, for 0x%x pages.\n", addr,
		    mp->dmai_ndvmapages));
		/* sync the entire object */
		if (!(mp->dmai_rflags & DDI_DMA_CONSISTENT)) {
			/* flush stream write buffers */
			sync_stream_buf(softsp, addr, npages,
			    (int *)&mppriv->sync_flag, mppriv->phys_sync_flag);
		}

#if	defined(DEBUG) && defined(IO_MEMDEBUG)
		iommu_remove_mappings(mp);
#endif /* DEBUG && IO_MEMDEBUG */

		ASSERT(npages > (u_int)0);
		rmfree(softsp->dvmamap, (long)(npages),
		    iommu_btop(addr));

		kmem_free(mppriv, sizeof (*mppriv));

		if (softsp->dvma_call_list_id != 0) {
			ddi_run_callback(&softsp->dvma_call_list_id);
		}

		break;
	}

	case DDI_DMA_SYNC:
	{
		struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;

		DPRINTF(IOMMU_DMAMCTL_SYNC_DEBUG, ("sync\n"));

		/* Make sure our mapping structure is valid */
		if (!mp)
			return (DDI_FAILURE);

		if (!(mp->dmai_rflags & DDI_DMA_CONSISTENT)) {
			sync_stream_buf(mppriv->softsp, mp->dmai_mapping,
			    mp->dmai_ndvmapages, (int *)&mppriv->sync_flag,
			    mppriv->phys_sync_flag);
		}

		break;
	}

	case DDI_DMA_SET_SBUS64:
	{
		struct dma_impl_priv *mppriv = (struct dma_impl_priv *) mp;

		return (iommu_dma_lim_setup(dip, rdip, mppriv->softsp,
		    &mp->dmai_burstsizes, (u_int) *lenp, &mp->dmai_minxfer,
		    DDI_DMA_SBUS_64BIT));
	}

	case DDI_DMA_HTOC:
		DPRINTF(IOMMU_DMAMCTL_HTOC_DEBUG, ("htoc off %x mapping %x "
		    "size %x\n", (u_long) *offp, mp->dmai_mapping,
		    mp->dmai_size));

		if ((u_int) *offp >= mp->dmai_size) {
			return (DDI_FAILURE);
		}

		cp = (ddi_dma_cookie_t *) objp;
		cp->dmac_notused = 0;
		cp->dmac_address = (mp->dmai_mapping + (u_long) *offp);
		cp->dmac_size =
		    mp->dmai_mapping + mp->dmai_size - cp->dmac_address;
		cp->dmac_type = 0;

		break;

	case DDI_DMA_KVADDR:

		/*
		 * If a physical address mapping has percolated this high,
		 * that is an error (maybe?).
		 */
		if (mp->dmai_rflags & DMP_PHYSADDR) {
			DPRINTF(IOMMU_DMAMCTL_KVADDR_DEBUG, ("kvaddr of phys "
			    "mapping\n"));
			return (DDI_FAILURE);
		}

		return (DDI_FAILURE);

	case DDI_DMA_NEXTWIN:
	{
		register ddi_dma_win_t *owin, *nwin;
		u_long winsize, newoff;
		int rval;

		DPRINTF(IOMMU_DMAMCTL_NEXTWIN_DEBUG, ("nextwin\n"));

		mp = (ddi_dma_impl_t *) handle;
		owin = (ddi_dma_win_t *) offp;
		nwin = (ddi_dma_win_t *) objp;
		if (mp->dmai_rflags & DDI_DMA_PARTIAL) {
			if (*owin == NULL) {
				DPRINTF(IOMMU_DMAMCTL_NEXTWIN_DEBUG,
				    ("nextwin: win == NULL\n"));
				mp->dmai_offset = 0;
				*nwin = (ddi_dma_win_t) mp;
				return (DDI_SUCCESS);
			}

			offset = mp->dmai_mapping & IOMMU_PAGEOFFSET;
			winsize = iommu_ptob(mp->dmai_ndvmapages -
				iommu_btopr(offset));

			newoff = mp->dmai_offset + winsize;
			if (newoff > mp->dmai_object.dmao_size -
				mp->dmai_minxfer) {
				return (DDI_DMA_DONE);
			}


			if ((rval = iommu_map_window(mp, newoff, winsize))
			    != DDI_SUCCESS) {
				return (rval);
			}
		} else {
			DPRINTF(IOMMU_DMAMCTL_NEXTWIN_DEBUG, ("nextwin: no "
			    "partial mapping\n"));
			if (*owin != NULL) {
				return (DDI_DMA_DONE);
			}
			mp->dmai_offset = 0;
			*nwin = (ddi_dma_win_t) mp;
		}
		break;
	}

	case DDI_DMA_NEXTSEG:
	{
		register ddi_dma_seg_t *oseg, *nseg;

		DPRINTF(IOMMU_DMAMCTL_NEXTSEG_DEBUG, ("nextseg:\n"));

		oseg = (ddi_dma_seg_t *) lenp;
		if (*oseg != NULL) {
			return (DDI_DMA_DONE);
		} else {
			nseg = (ddi_dma_seg_t *) objp;
			*nseg = *((ddi_dma_seg_t *) offp);
		}
		break;
	}

	case DDI_DMA_SEGTOC:
	{
		register ddi_dma_seg_impl_t *seg;

		seg = (ddi_dma_seg_impl_t *) handle;
		cp = (ddi_dma_cookie_t *) objp;
		cp->dmac_notused = 0;
		cp->dmac_address = seg->dmai_mapping;
		cp->dmac_size = *lenp = seg->dmai_size;
		cp->dmac_type = 0;
		*offp = seg->dmai_offset;
		break;
	}

	case DDI_DMA_MOVWIN:
	{
		u_long winsize, newoff;
		int rval;

		offset = mp->dmai_mapping & IOMMU_PAGEOFFSET;
		winsize = iommu_ptob(mp->dmai_ndvmapages - iommu_btopr(offset));

		DPRINTF(IOMMU_DMAMCTL_MOVWIN_DEBUG, ("movwin off %x len %x "
		    "winsize %x\n", *offp, *lenp, winsize));

		if ((mp->dmai_rflags & DDI_DMA_PARTIAL) == 0) {
			return (DDI_FAILURE);
		}

		if (*lenp != (u_int) -1 && *lenp != winsize) {
			DPRINTF(IOMMU_DMAMCTL_MOVWIN_DEBUG, ("bad length\n"));
			return (DDI_FAILURE);
		}
		newoff = (u_long) *offp;
		if (newoff & (winsize - 1)) {
			DPRINTF(IOMMU_DMAMCTL_MOVWIN_DEBUG, ("bad off\n"));
			return (DDI_FAILURE);
		}

		if (newoff == mp->dmai_offset) {
			/*
			 * Nothing to do...
			 */
			break;
		}

		/*
		 * Check out new address...
		 */
		if (newoff > mp->dmai_object.dmao_size - mp->dmai_minxfer) {
			DPRINTF(IOMMU_DMAMCTL_MOVWIN_DEBUG, ("newoff out of "
			    "range\n"));
			return (DDI_FAILURE);
		}

		if ((rval = iommu_map_window(mp, newoff,
			    winsize)) != DDI_SUCCESS) {
			return (rval);
		}

		if ((cp = (ddi_dma_cookie_t *) objp) != 0) {
			cp->dmac_notused = 0;
			cp->dmac_address = mp->dmai_mapping;
			cp->dmac_size = mp->dmai_size;
			cp->dmac_type = 0;
		}
		*offp = (off_t) newoff;
		*lenp = (u_int) winsize;
		break;
	}

	case DDI_DMA_REPWIN:
		if ((mp->dmai_rflags & DDI_DMA_PARTIAL) == 0) {
			DPRINTF(IOMMU_DMAMCTL_REPWIN_DEBUG, ("repwin fail\n"));

			return (DDI_FAILURE);
		}

		*offp = (off_t) mp->dmai_offset;

		addr = mp->dmai_ndvmapages -
		    iommu_btopr(mp->dmai_mapping & IOMMU_PAGEOFFSET);

		*lenp = (u_int) iommu_ptob(addr);

		DPRINTF(IOMMU_DMAMCTL_REPWIN_DEBUG, ("repwin off %x len %x\n",
		    mp->dmai_offset, mp->dmai_size));

		break;

	case DDI_DMA_GETERR:
		DPRINTF(IOMMU_DMAMCTL_GETERR_DEBUG,
		    ("iommu_dma_mctl: geterr\n"));

		break;

	case DDI_DMA_COFF:
		cp = (ddi_dma_cookie_t *) offp;
		addr = cp->dmac_address;

		if (addr < mp->dmai_mapping ||
		    addr >= mp->dmai_mapping + mp->dmai_size)
			return (DDI_FAILURE);

		*objp = (caddr_t) (addr - mp->dmai_mapping);

		DPRINTF(IOMMU_DMAMCTL_COFF_DEBUG, ("coff off %x mapping %x "
		    "size %x\n", (u_long) *objp, mp->dmai_mapping,
		    mp->dmai_size));

		break;

	case DDI_DMA_RESERVE:
	{
		struct ddi_dma_req *dmareq = (struct ddi_dma_req *) offp;
		ddi_dma_lim_t *dma_lim;
		ddi_dma_handle_t *handlep;
		u_int np;
		u_long ioaddr;
		int i;
		struct fast_dvma *iommu_fast_dvma;
		struct sbus_soft_state *softsp =
			(struct sbus_soft_state *) ddi_get_soft_state(sbusp,
			ddi_get_instance(dip));

		/* Some simple sanity checks */
		dma_lim = dmareq->dmar_limits;
		if (dma_lim->dlim_burstsizes == 0) {
			DPRINTF(IOMMU_FASTDMA_RESERVE,
			    ("Reserve: bad burstsizes\n"));
			return (DDI_DMA_BADLIMITS);
		}
		if ((AHI <= ALO) || (AHI < softsp->iommu_dvma_base)) {
			DPRINTF(IOMMU_FASTDMA_RESERVE,
			    ("Reserve: bad limits\n"));
			return (DDI_DMA_BADLIMITS);
		}

		np = dmareq->dmar_object.dmao_size;
		mutex_enter(&softsp->dma_pool_lock);
		if (np > softsp->dma_reserve) {
			mutex_exit(&softsp->dma_pool_lock);
			DPRINTF(IOMMU_FASTDMA_RESERVE,
			    ("Reserve: dma_reserve is exhausted\n"));
			return (DDI_DMA_NORESOURCES);
		}

		softsp->dma_reserve -= np;
		mutex_exit(&softsp->dma_pool_lock);
		mp = kmem_zalloc(sizeof (*mp), KM_SLEEP);
		mp->dmai_rflags = DMP_BYPASSNEXUS;
		mp->dmai_rdip = rdip;
		mp->dmai_minxfer = dma_lim->dlim_minxfer;
		mp->dmai_burstsizes = dma_lim->dlim_burstsizes;

		ioaddr = getdvmapages(np, ALO, AHI, (u_int)-1,
				dma_lim->dlim_cntr_max,
				(dmareq->dmar_fp == DDI_DMA_SLEEP)? 1 : 0,
				softsp->dvmamap, softsp);

		if (ioaddr == 0) {
			mutex_enter(&softsp->dma_pool_lock);
			softsp->dma_reserve += np;
			mutex_exit(&softsp->dma_pool_lock);
			kmem_free(mp, sizeof (*mp));
			DPRINTF(IOMMU_FASTDMA_RESERVE,
			    ("Reserve: No dvma resources available\n"));
			return (DDI_DMA_NOMAPPING);
		}

		/* create a per request structure */
		iommu_fast_dvma = kmem_alloc(sizeof (struct fast_dvma),
		    KM_SLEEP);

		/*
		 * We need to remember the size of the transfer so that
		 * we can figure the virtual pages to sync when the transfer
		 * is complete.
		 */
		iommu_fast_dvma->pagecnt = kmem_zalloc(np *
		    sizeof (u_int), KM_SLEEP);

		/* Allocate a streaming cache sync flag for each index */
		iommu_fast_dvma->sync_flag = kmem_zalloc(np *
		    sizeof (int), KM_SLEEP);

		/* Allocate a physical sync flag for each index */
		iommu_fast_dvma->phys_sync_flag =
		    kmem_zalloc(np * sizeof (unsigned long long), KM_SLEEP);

		for (i = 0; i < np; i++)
			iommu_fast_dvma->phys_sync_flag[i] = va_to_pa((caddr_t)
			    &iommu_fast_dvma->sync_flag[i]);

		mp->dmai_mapping = ioaddr;
		mp->dmai_ndvmapages = np;
		iommu_fast_dvma->ops = &iommu_dvma_ops;
		iommu_fast_dvma->softsp = (caddr_t)softsp;
		mp->dmai_nexus_private = (caddr_t)iommu_fast_dvma;
		handlep = (ddi_dma_handle_t *) objp;
		*handlep = (ddi_dma_handle_t) mp;

		DPRINTF(IOMMU_FASTDMA_RESERVE,
		    ("Reserve: Mapping object 0x%x, Base addr 0x%x, "
		    "size 0x%x\n", mp, mp->dmai_mapping, mp->dmai_ndvmapages));

		break;
	}
	case DDI_DMA_RELEASE:
	{
		ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
		u_int np = npages = mp->dmai_ndvmapages;
		u_long ioaddr = mp->dmai_mapping;
		volatile u_ll_t *iotte_ptr;
		struct fast_dvma *iommu_fast_dvma = (struct fast_dvma *)
		    mp->dmai_nexus_private;
		struct sbus_soft_state *softsp = (struct sbus_soft_state *)
		    iommu_fast_dvma->softsp;
		ASSERT(softsp != NULL);

		/* Unload stale mappings and flush stale tlb's */
		iotte_ptr = IOTTE_NDX(ioaddr, softsp->soft_tsb_base_addr);

		while (npages > (u_int) 0) {
			*iotte_ptr = (u_ll_t)0;	/* unload tte */
			iommu_tlb_flush(softsp, ioaddr, 1);

			npages--;
			iotte_ptr++;
			ioaddr += IOMMU_PAGESIZE;
		}

		ioaddr = mp->dmai_mapping;
		mutex_enter(&softsp->dma_pool_lock);
		softsp->dma_reserve += np;
		mutex_exit(&softsp->dma_pool_lock);
		kmem_free(mp, sizeof (*mp));

		rmfree(softsp->dvmamap, (long)np, iommu_btop(ioaddr));

		kmem_free(iommu_fast_dvma->pagecnt, np * sizeof (u_int));
		kmem_free(iommu_fast_dvma->sync_flag, np * sizeof (int));
		kmem_free(iommu_fast_dvma->phys_sync_flag, np *
		    sizeof (unsigned long long));
		kmem_free(iommu_fast_dvma, sizeof (struct fast_dvma));


		DPRINTF(IOMMU_FASTDMA_RESERVE,
		    ("Release: Base addr 0x%x, size 0x%x\n", ioaddr, np));
		/*
		 * Now that we've freed some resource,
		 * if there is anybody waiting for it
		 * try and get them going.
		 */
		if (softsp->dvma_call_list_id != 0) {
			ddi_run_callback(&softsp->dvma_call_list_id);
		}

		break;
	}

	default:
		DPRINTF(IOMMU_DMAMCTL_DEBUG, ("iommu_dma_mctl: unknown option "
		    "0%x\n", request));

		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
void
iommu_dvma_kaddr_load(ddi_dma_handle_t h, caddr_t a, u_int len, u_int index,
    ddi_dma_cookie_t *cp)
{

	u_long ioaddr, addr, offset;
	u_int pfn;
	int npages;
	volatile u_ll_t *iotte_ptr;
	u_ll_t iotte_flag = 0;
	struct as *as = 0;
	extern struct as kas;
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;
	struct fast_dvma *iommu_fast_dvma =
	    (struct fast_dvma *)mp->dmai_nexus_private;
	struct sbus_soft_state *softsp = (struct sbus_soft_state *)
	    iommu_fast_dvma->softsp;
#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	struct io_mem_list *iomemp;
	u_int *pfnp;
#endif /* DEBUG && IO_MEMUSAGE */

	ASSERT(softsp != NULL);

	addr = (u_long)a;
	ioaddr =  mp->dmai_mapping + iommu_ptob(index);
	offset = addr & IOMMU_PAGEOFFSET;
	iommu_fast_dvma->pagecnt[index] = iommu_btopr(len + offset);
	as = &kas;
	addr &= ~IOMMU_PAGEOFFSET;
	npages = iommu_btopr(len + offset);

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	iomemp = kmem_alloc(sizeof (struct io_mem_list),
	    KM_SLEEP);
	iomemp->rdip = mp->dmai_rdip;
	iomemp->ioaddr = ioaddr;
	iomemp->addr = addr;
	iomemp->npages = npages;
	pfnp = iomemp->pfn = kmem_zalloc(
	    sizeof (u_int) * (npages + 1), KM_SLEEP);
#endif /* DEBUG && IO_MEMUSAGE */

	cp->dmac_address = ioaddr | offset;
	cp->dmac_size = len;

	iotte_ptr = IOTTE_NDX(ioaddr, softsp->soft_tsb_base_addr);
	/* read/write and streaming io on */
	iotte_flag = IOTTE_VALID | IOTTE_WRITE | IOTTE_CACHE;

	if (!softsp->stream_buf_off)
		iotte_flag |= IOTTE_STREAM;

	DPRINTF(IOMMU_FASTDMA_LOAD, ("kaddr_load: ioaddr 0x%x, "
	    "size 0x%x, offset 0x%x, index 0x%x, kernel addr 0x%x\n",
	    ioaddr, len, offset, index, addr));
	ASSERT(npages > 0);
	do {
		pfn = hat_getpfnum(as->a_hat, (caddr_t) addr);
		if (pfn == (u_int) -1) {
#ifdef lint
			h = h;
#endif /* lint */
			DPRINTF(IOMMU_FASTDMA_LOAD, ("kaddr_load: invalid pfn "
			    "from hat_getpfnum()\n"));
		}

		iommu_tlb_flush(softsp, ioaddr, 1);

		*iotte_ptr = ((u_longlong_t)pfn << IOMMU_PAGESHIFT) |
			iotte_flag;	/* load tte */

		npages--;
		iotte_ptr++;

		addr += IOMMU_PAGESIZE;
		ioaddr += IOMMU_PAGESIZE;

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
		*pfnp = pfn;
		pfnp++;
#endif /* DEBUG && IO_MEMUSAGE */

	} while (npages > 0);

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	mutex_enter(&softsp->iomemlock);
	iomemp->next = softsp->iomem;
	softsp->iomem = iomemp;
	mutex_exit(&softsp->iomemlock);
#endif /* DEBUG && IO_MEMUSAGE */

}

/*ARGSUSED*/
void
iommu_dvma_unload(ddi_dma_handle_t h, u_int index, u_int view)
{
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;
	register u_long ioaddr;
	u_int	npages;
	struct fast_dvma *iommu_fast_dvma =
	    (struct fast_dvma *)mp->dmai_nexus_private;
	struct sbus_soft_state *softsp = (struct sbus_soft_state *)
	    iommu_fast_dvma->softsp;
#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	struct io_mem_list **prevp, *walk;
#endif /* DEBUG && IO_MEMUSAGE */

	ASSERT(softsp != NULL);

	ioaddr =  mp->dmai_mapping + iommu_ptob(index);
	npages = iommu_fast_dvma->pagecnt[index];

#if	defined(DEBUG) && defined(IO_MEMUSAGE)
	mutex_enter(&softsp->iomemlock);
	prevp = &softsp->iomem;
	walk = softsp->iomem;

	while (walk) {
		if (walk->ioaddr == ioaddr) {
			*prevp = walk->next;
			break;
		}

		prevp = &walk->next;
		walk = walk->next;
	}
	mutex_exit(&softsp->iomemlock);

	kmem_free(walk->pfn, sizeof (u_int) * (npages + 1));
	kmem_free(walk, sizeof (struct io_mem_list));
#endif /* DEBUG && IO_MEMUSAGE */

	DPRINTF(IOMMU_FASTDMA_SYNC, ("kaddr_unload: handle 0x%x, sync flag "
	    "addr 0x%x, sync flag pfn 0x%x, index 0x%x, page count 0x%x\n", mp,
	    &iommu_fast_dvma->sync_flag[index],
	    iommu_fast_dvma->phys_sync_flag[index],
	    index, npages));

	sync_stream_buf(softsp, ioaddr, npages,
	    (int *)&iommu_fast_dvma->sync_flag[index],
	    iommu_fast_dvma->phys_sync_flag[index]);
}

/*ARGSUSED*/
void
iommu_dvma_sync(ddi_dma_handle_t h, u_int index, u_int view)
{
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;
	register u_long ioaddr;
	u_int   npages;
	struct fast_dvma *iommu_fast_dvma =
	    (struct fast_dvma *)mp->dmai_nexus_private;
	struct sbus_soft_state *softsp = (struct sbus_soft_state *)
	    iommu_fast_dvma->softsp;

	ASSERT(softsp != NULL);
	ioaddr =  mp->dmai_mapping + iommu_ptob(index);
	npages = iommu_fast_dvma->pagecnt[index];
	DPRINTF(IOMMU_FASTDMA_SYNC, ("kaddr_sync: handle 0x%x, "
	    "sync flag addr 0x%x, sync flag pfn 0x%x\n", mp,
	    &iommu_fast_dvma->sync_flag[index],
	    iommu_fast_dvma->phys_sync_flag[index]));

	sync_stream_buf(softsp, ioaddr, npages,
	    (int *)&iommu_fast_dvma->sync_flag[index],
	    iommu_fast_dvma->phys_sync_flag[index]);
}
