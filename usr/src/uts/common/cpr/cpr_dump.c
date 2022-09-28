/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_dump.c	1.51	96/05/29 SMI"

/*
 * Fill in and write out the cpr state file
 *	1. Allocate and write headers, ELF and cpr dump header
 *	2. Allocate bitmaps according to phys_install
 *	3. Tag kernel pages into corresponding bitmap
 *	4. Write bitmaps to state file
 *	5. Write actual physical page data to state file
 */

#include <sys/types.h>
#include <sys/param.h>

#include <sys/systm.h>

#include <sys/sunddi.h>
#include <sys/vm.h>
#include <sys/memlist.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/elf.h>
#include <sys/cpr_impl.h>
#include <sys/cpr.h>

static int cpr_write_elf(vnode_t *);
static int cpr_write_header(vnode_t *);
static int cpr_write_bitmap(vnode_t *);
static int cpr_write_statefile(vnode_t *);
int cpr_count_kpages(int);
static int cpr_tag_upages();
int cpr_setbit(u_int);
static int cpr_compress_and_write(vnode_t *, u_int, u_int, u_int);
static int cpr_flush_write(vnode_t *);
static int cpr_write_terminator(vnode_t *);
static int cpr_allocate_bitmaps();

#ifdef sparc
extern struct vnode prom_ppages;
#endif sparc
struct cpr_terminator cpr_term;

/* Local defines and variables */
#define	CPRBUFS		256			/* no. of cpr write buffer */
#define	CPRBUFSZ	(CPRBUFS * DEV_BSIZE)	/* cpr write buffer size */
#define	PAGE_ROUNDUP(val)	(((val) + (MMU_PAGESIZE - 1)) &		\
					~(MMU_PAGESIZE - 1))

static unsigned char	cpr_pagedata[(CPR_MAXCONTIG + 1)*MMU_PAGESIZE];
#ifdef	DEBUG
static unsigned char	cpr_pagecopy[CPR_MAXCONTIG * MMU_PAGESIZE];
#endif
static unsigned char	cpr_buf[CPRBUFSZ];	/* may not be page aligned */
static unsigned char	*cpr_wptr; /* keep track of where to write to next */
static int cpr_file_bn;	/* cpr state-file block offset */
static int totcompress;
static int no_compress;

int	cpr_test_mode;

extern u_int i_cpr_count_special_kpages(int);

/*
 * creates the CPR state file, the following sections are
 * written out in sequence:
 *    - writes the elf header
 *    - writes the cpr dump header
 *    - writes the memory usage bitmaps
 *    - writes the platform dependent info
 *    - writes the remaining user pages
 *    - writes the kernel pages
 */
int
cpr_dump(vnode_t *vp)
{
	int error;

	/*
	 * initialize global variables for used by the write operation
	 */
	cpr_file_bn = 0;
	STAT->cs_real_statefsz = 0;
	STAT->cs_dumped_statefsz = 0;
	cpr_wptr = cpr_buf;    	/* pt to top of internal buffer */

	if ((error = cpr_write_elf(vp)) != 0)
		return (error);

	if ((error = cpr_write_header(vp)) != 0)
		return (error);

	if ((error = i_cpr_write_machdep(vp)) != 0)
		return (error);

	if ((error = cpr_write_bitmap(vp)) != 0)
		return (error);

	if ((error = cpr_write_statefile(vp)) != 0)
		return (error);

	if ((error = cpr_write_terminator(vp)) != 0)
		return (error);

	if ((error = cpr_flush_write(vp)) != 0)
		return (error);

	return (0);
}

/*
 * Create and write ELF header to state file, no memory allocation
 * is allowed at this point.
 */
static int
cpr_write_elf(vnode_t *vp)
{
	Elf32_Ehdr elfp;

	elfp.e_ident[EI_MAG0] = ELFMAG0;
	elfp.e_ident[EI_MAG1] = ELFMAG1;
	elfp.e_ident[EI_MAG2] = ELFMAG2;
	elfp.e_ident[EI_MAG3] = ELFMAG3;
	elfp.e_ident[EI_CLASS] = ELFCLASS32;
	elfp.e_ident[EI_VERSION] = EV_CURRENT;
	elfp.e_type = ET_CORE;
	elfp.e_version = EV_CURRENT;
	elfp.e_ehsize = sizeof (Elf32_Ehdr);

	elfheadset(&elfp.e_ident[EI_DATA], &elfp.e_machine, &elfp.e_flags);

	return (cpr_write(vp, (caddr_t)&elfp, sizeof (Elf32_Ehdr)));
}

/*
 * CPR dump header contains the following information:
 *	1. header magic -- unique to cpr state file
 *	2. total no. of bitmaps allocated according to phys_install,
 *	   so cpr_allocate_bitmaps() will be called.
 *	3. total no. of kernel physical page frames tagged,
 *	   so cpr_count_kpages() will be called.
 */
static int
cpr_write_header(vnode_t *vp)
{
	struct cpr_dump_desc cdump;
	u_int tot_physpgs = 0;
	int bitmap_pages;
	struct memlist *pmem;
	extern struct memlist *phys_install;

	/*
	 * Allocate bitmaps, tag pages and fill in cpr dump descriptor.
	 */
	cdump.cdd_magic = (u_int)CPR_DUMP_MAGIC;

	cdump.cdd_rtnpc = (caddr_t)i_cpr_restore_hwregs;
	cdump.cdd_rtnpc_pfn = i_cpr_va_to_pfn((caddr_t)i_cpr_restore_hwregs);

	cdump.cdd_curthread = (caddr_t)curthread;	/* g7 info */
	cdump.cdd_qsavp = (caddr_t)&ttolwp(curthread)->lwp_qsav;
	cdump.cdd_debug = cpr_debug;
	cdump.cdd_test_mode = cpr_test_mode;

	cdump.cdd_bitmaprec = cpr_allocate_bitmaps();
	if (cdump.cdd_bitmaprec == (u_int)-1) {
		errp("Can't allocate statefile bitmaps, ABORT !\n");
		return (ENOMEM);
	}
	cdump.cdd_dumppgsize = cpr_count_kpages(CPR_TAG);
	cdump.cdd_dumppgsize += cpr_tag_upages();

	/*
	 * Find out how many pages of bitmap are needed to represent
	 * the physical memory.
	 */
	for (pmem = phys_install; pmem; pmem = pmem->next) {
		tot_physpgs += (pmem->size / MMU_PAGESIZE);
	}
	bitmap_pages = PAGE_ROUNDUP(tot_physpgs / NBBY) / MMU_PAGESIZE;

	/*
	 * Export accurate statefile size for statefile allocation
	 * retry.
	 * statefile_size = all the headers + total pages +
	 * number of pages used by the bitmaps.
	 * Roundup will be done in the file allocation code.
	 */
	STAT->cs_grs_statefsz = sizeof (Elf32_Ehdr) + sizeof (cdd_t) +
		(sizeof (cbd_t) * cdump.cdd_bitmaprec) +
		(sizeof (cpd_t) * cdump.cdd_dumppgsize) +
		sizeof (cmd_t) +
		((cdump.cdd_dumppgsize + bitmap_pages) * MMU_PAGESIZE);

	DEBUG1(errp("Accurate statefile size before compression: %d\n",
		STAT->cs_grs_statefsz));
	DEBUG9(printf("Accurate statefile size before compression: %d\n",
		STAT->cs_grs_statefsz));

	/*
	 * If the estimated statefile is not big enough,
	 * go retry now to save un-necessary operations.
	 */
	if (!(CPR->c_flags & C_COMPRESSING) &&
		(STAT->cs_grs_statefsz > STAT->cs_est_statefsz))
		return (ENOSPC);
	else
		/*
		 * Write cpr dump descriptor.
		 */
		return (cpr_write(vp, (caddr_t)&cdump, sizeof (cdd_t)));
}

/*
 * CPR dump tail record contains the following information:
 *	1. header magic -- unique to cpr state file
 *	2. all misc info that needs to be passed to cprboot or resumed kernel
 */
static int
cpr_write_terminator(vnode_t *vp)
{
	cpr_term.magic = (u_int)CPR_TERM_MAGIC;
	cpr_term.va = (caddr_t)&cpr_term;
	cpr_term.pfn = i_cpr_va_to_pfn((caddr_t)&cpr_term);

	/* count the last one (flush) */
	cpr_term.real_statef_size = STAT->cs_real_statefsz +
		btod(cpr_wptr - cpr_buf) * DEV_BSIZE;

	DEBUG9(errp("cpr_dump: Real Statefile Size: %d\n",
		STAT->cs_real_statefsz));

	cpr_term.tm_shutdown = i_cpr_todget();

	return (cpr_write(vp, (caddr_t)&cpr_term,
		sizeof (struct cpr_terminator)));
}

/*
 * Traverse c_bitmaps_chain which contain all the bitmap information, and
 * write it out to the state file.
 */
static int
cpr_write_bitmap(vnode_t *vp)
{
	int error = 0;
	struct cpr_bitmap_desc *bmd;

	for (bmd = CPR->c_bitmaps_chain; bmd; bmd = bmd->cbd_next) {
		if (error = cpr_write(vp, (caddr_t)bmd, sizeof (cbd_t)))
			return (error);
		if (error = cpr_write(vp, (caddr_t)bmd->cbd_bitmap,
			bmd->cbd_size))
			return (error);
	}
	return (0);
}

static int
cpr_write_statefile(vnode_t *vp)
{
	cbd_t *tmp;
	u_int kas_cnt = 0;
	u_int spin_cnt = 0;
	u_int spfn, totbit;
	int i, error = 0;
	int j, totpg = 0, dumppgs = 0;
	void flush_windows();

	/* Internal use only */
	totcompress = 0;
	no_compress = 0;


	flush_windows();

	for (tmp = CPR->c_bitmaps_chain; tmp; tmp = tmp->cbd_next) {
		spfn = tmp->cbd_spfn;
		totbit = tmp->cbd_size * NBBY;
		i = 0; /* Beginning of bitmap */
		j = 0;
		while (i < totbit) {
			while ((j < CPR_MAXCONTIG) && ((j + i) < totbit)) {
				if (isset(tmp->cbd_bitmap, j+i)) {
					totpg++;
					j++;
					dumppgs = 1;
				} else { /* not contiguous anymore */
					break;
				}
			}
			/* Dump the contiguous pages */
			if (dumppgs) {
				error = cpr_compress_and_write(vp, 0,
					spfn + i, totpg);
				if (error) {
		DEBUG3(errp("cpr_write_statefile: Bad kas page %x Error %d\n",
			spfn + i, error));

					return (error);
				}
				dumppgs = 0;
			}
			if (j == CPR_MAXCONTIG)
				i += j;
			else
				/* Stopped on a non-tagged page */
				i = i + j + 1;

				kas_cnt += totpg;
				spin_cnt++;
				if ((spin_cnt & 0x5F) == 1)
					cpr_spinning_bar();
				j = 0;
				totpg = 0;

		}
	}
	DEBUG1(errp("cpr_write_statefile: total=%d compress=%d no_comprs=%d\n",
		kas_cnt, totcompress, no_compress));
	DEBUG9(printf("cpr_write_statefile: total=%d comprs=%d no_comprs=%d\n",
		kas_cnt, totcompress, no_compress));

	return (0);
}

/*
 * Allocate bitmaps according to phys_install memlist in the kernel.
 */
int
cpr_allocate_bitmaps()
{
	cbd_t *cur;
	struct memlist *pmem;
	int tot_bitmaps = 0, pages;
	extern struct memlist *phys_install;

	if (CPR->c_bitmaps_chain) {
		for (cur = CPR->c_bitmaps_chain; cur; cur = cur->cbd_next) {
			bzero(cur->cbd_bitmap, (u_int)cur->cbd_size);
			cur->cbd_pagedump = 0;
			tot_bitmaps++;
		}
		return (tot_bitmaps);
	}

	/* Create bitmaps according to phys_install */
	for (tot_bitmaps = 0, pmem = phys_install; pmem;
		pmem = pmem->next, tot_bitmaps++) {
		if ((cur = (cbd_t *)kmem_zalloc(sizeof (cbd_t),
			KM_NOSLEEP)) == NULL) {
		cmn_err(CE_WARN, "No space for statefile bitmap descriptor\n");
			return (-1);
		}

		cur->cbd_magic = (u_int)CPR_BITMAP_MAGIC;
		cur->cbd_spfn = mmu_btop(pmem->address);

		/* Calculate the size of the bitmap and allocate space for it */
		cur->cbd_epfn = mmu_btop(pmem->size + pmem->address);
		pages = cur->cbd_epfn - cur->cbd_spfn;
		cur->cbd_size = pages / NBBY;

		cur->cbd_bitmap = kmem_zalloc((u_int)cur->cbd_size, KM_NOSLEEP);
		if (cur->cbd_bitmap == NULL) {
			cmn_err(CE_WARN, "No space for statefile bitmap\n");
			return (-1);
		}

		cur->cbd_next = CPR->c_bitmaps_chain;
		CPR->c_bitmaps_chain = cur;
	}

#ifdef CPR_DUMP_DEBUG
	for (cur = CPR->c_bitmaps_chain; cur; cur = cur->cbd_next) {
		DEBUG2(errp("spfn = %x epfn = %x mapsize = %d next = %x\n",
		cur->cbd_spfn, cur->cbd_epfn, cur->cbd_size, cur->cbd_next));
	}
#endif CPR_DUMP_DEBUG

	return (tot_bitmaps);
}

/*
 * Go through kas to select valid pages and mark it in the appropriate bitmap.
 * This includes pages in segkmem, segkp, segkmap and kadb.
 */
int
cpr_count_kpages(int flag)
{
	struct seg *segp;
	u_int sva, eva, va, pfn;
	u_int kas_cnt = 0;


	/*
	 * Some pages need to be taken care of differently. For example: msgbuf
	 * pages of Sun4m are not in kas but they need to be saved. In Sun4u
	 * the physical pages of msgbuf are allocated via prom_retain().
	 * Msgbuf pages are not in kas.
	 */
	kas_cnt += i_cpr_count_special_kpages(flag);

	segp = AS_SEGP(&kas, kas.a_segs);
	do {
		sva = (u_int)segp->s_base;
		eva = sva + segp->s_size;
		for (va = sva; va < eva; va += MMU_PAGESIZE) {
			pfn = i_cpr_va_to_pfn((caddr_t)va);
			if (pfn != (u_int)-1 && pf_is_memory(pfn)) {
				if (flag == CPR_TAG) {
					if (!cpr_setbit(pfn))
						kas_cnt++;
				} else
					kas_cnt++;
			}
		}
		segp = AS_SEGP(&kas, segp->s_next);
	} while (segp != NULL && segp != AS_SEGP(&kas, kas.a_segs));

	DEBUG9(printf("cpr_count_kpages: kas_cnt=%d\n", kas_cnt));
	return (kas_cnt);
}


/*
 * Go thru all pages and pick up any page not caught during the invalidation
 * stage. This is also used to save pages with cow lock or phys page lock held
 * (none zero p_lckcnt or p_cowcnt)
 */
cpr_tag_upages()
{
	page_t *pp, *page0;
	int dcnt = 0, tcnt = 0, pfn;

	page0 = pp = page_first();

	do {
#ifdef sparc
		if (pp->p_vnode == NULL || pp->p_vnode == &kvp ||
		    pp->p_vnode == &prom_ppages ||
			PP_ISFREE(pp) && PP_ISAGED(pp))
#else
		if (pp->p_vnode == NULL || pp->p_vnode == &kvp ||
		    PP_ISFREE(pp) && PP_ISAGED(pp))
#endif sparc
			continue;

		pfn = page_pptonum(pp);
		if (pf_is_memory(pfn)) {
			tcnt++;
			if (!cpr_setbit(pfn))
				dcnt++; /* dirty count */
			else
				DEBUG2(errp("uptag: already tagged pfn=%x\n",
					pfn));
		}
	} while ((pp = page_next(pp)) != page0);

	STAT->cs_upage2statef = dcnt;
	DEBUG9(printf("cpr_tag_upages: dirty=%d total=%d\n", dcnt, tcnt));
	return (dcnt);
}

/*
 * Deposit the tagged page to the right bitmap.
 */
int
cpr_setbit(u_int i)
{
	cbd_t *tmp;

	for (tmp = CPR->c_bitmaps_chain; tmp; tmp = tmp->cbd_next) {
		if ((i >= tmp->cbd_spfn) && (i <= tmp->cbd_epfn)) {
			if (isclr(tmp->cbd_bitmap, (i - tmp->cbd_spfn))) {
				setbit(tmp->cbd_bitmap, (i - tmp->cbd_spfn));
				tmp->cbd_pagedump++;
				return (0);
			} else {
				return (1);	/* double mapped case */
			}
		}
	}
	return (1);
}

#if !defined(lint)
/*
 * Remove the above "#if" directive, when this function is used.
 *
 * Used for Debug.
 */
cpr_isset(u_int i)
{
	cbd_t *tmp;

	for (tmp = CPR->c_bitmaps_chain; tmp; tmp = tmp->cbd_next) {
		if ((i >= tmp->cbd_spfn) && (i <= tmp->cbd_epfn)) {
			if (isset(tmp->cbd_bitmap, (i - tmp->cbd_spfn)))
				return (1);
			else
				return (0);
		}
	}
	errp("cpr_isset: cannot find the bitmap for page # %d\n", i);
	return (0);
}
#endif  /* lint */

/*
 * XXX: we are using the panicstr to get around the problem of the
 * esp chg that uses intrpt mode (talk to frits) instead of polling
 * mode while dumping, eventually there will be a "single thread"
 * mode in the kernel that we can use to force polling to happen
 * (talk to jre), at that time replace all panicstr references with
 * the new mode variable, but for now we will kludge it by faking
 * panic mode while dumping the data.
 */
extern char *panicstr;

/*
 * 1. Prepare cpr page descriptor and write it to file
 * 2. Compress page data and write it out
 */
static int
cpr_compress_and_write(vnode_t *vp, u_int va, u_int pfn, u_int npg)
{
	int error = 0;
	uchar_t *datap;
	cpd_t cpd;	/* cpr page descriptor */
	void i_cpr_mapin(caddr_t, u_int, u_int);
	void i_cpr_mapout(caddr_t, u_int);
#ifdef	DEBUG
	extern u_int cpr_sum(u_char *, int);
#endif

	i_cpr_mapin(CPR->c_mapping_area, npg, pfn);

	DEBUG3(errp("cpr map in %d page from mapping_area %x to pfn %x\n",
		npg, CPR->c_mapping_area, pfn));

	/*
	 * Fill cpr page descriptor.
	 */
	cpd.cpd_magic = (u_int)CPR_PAGE_MAGIC;
	cpd.cpd_pfn = pfn;
	cpd.cpd_va = va;
	cpd.cpd_flag = 0;  /* must init to zero */
	cpd.cpd_page = npg;

	STAT->cs_dumped_statefsz += npg * MMU_PAGESIZE;

	/*
	 * flush the data to mem before compression
	 */
	i_cpr_vac_ctxflush();

#ifdef	DEBUG
	/*
	 * Make a copy of the uncompressed data so we can checksum it.
	 * Compress that copy  so  the checksum works at the other end
	 */
	bcopy((caddr_t)CPR->c_mapping_area, (caddr_t)cpr_pagecopy,
	    npg * MMU_PAGESIZE);
	cpd.cpd_usum = cpr_sum(cpr_pagecopy, npg * MMU_PAGESIZE);
	cpd.cpd_flag |= CPD_USUM;

	datap = cpr_pagecopy;
#else
	datap = (uchar_t *)CPR->c_mapping_area;
#endif
	if (CPR->c_flags & C_COMPRESSING)
		cpd.cpd_length = cpr_compress(datap, npg * MMU_PAGESIZE,
		    cpr_pagedata);
	else
		cpd.cpd_length = npg * MMU_PAGESIZE;

	if (cpd.cpd_length >= (npg * MMU_PAGESIZE)) {
		cpd.cpd_length = npg * MMU_PAGESIZE;
		cpd.cpd_flag &= ~CPD_COMPRESS;
		no_compress += npg;
	} else {
		cpd.cpd_flag |= CPD_COMPRESS;
		datap = cpr_pagedata;
		totcompress += npg;
#ifdef	DEBUG
		cpd.cpd_csum = cpr_sum(datap, cpd.cpd_length);
		cpd.cpd_flag |= CPD_CSUM;
#endif
	}

	/* Write cpr page descriptor */
	error = cpr_write(vp, (caddr_t)&cpd, sizeof (cpd_t));

	/* Write compressed page data */
	error = cpr_write(vp, (caddr_t)datap, cpd.cpd_length);

	/*
	 * Unmap the pages for tlb and vac flushing
	 */
	i_cpr_mapout(CPR->c_mapping_area, npg);

	if (error) {
		DEBUG1(errp("cpr_compress_and_write: vp %x va %x ", vp, va));
		DEBUG1(errp("pfn %x blk %d err %d\n", pfn, cpr_file_bn, error));
	}
	return (error);
}


int
cpr_write(vnode_t *vp, caddr_t buffer, int size)
{
	int	error, count;
	caddr_t	fromp = buffer;

	/*
	 * break the write into multiple part if request is large,
	 * calculate count up to buf page boundary, then write it out.
	 * repeat until done.
	 */
	while (size > 0) {
		count = MIN(size, cpr_buf + CPRBUFSZ - cpr_wptr);

		bcopy(fromp, (caddr_t)cpr_wptr, count);

		cpr_wptr += count;
		fromp += count;
		size -= count;

		if (cpr_wptr < cpr_buf + CPRBUFSZ)
			return (0);	/* buffer not full yet */
		ASSERT(cpr_wptr == cpr_buf + CPRBUFSZ);

		if (dbtob(cpr_file_bn+CPRBUFS) > VTOI(vp)->i_size)
			return (ENOSPC);

		panicstr = "fake";
		DEBUG3(errp("cpr_write: frmp=%x wptr=%x cnt=%x...",
			fromp, cpr_wptr, count));
		error = VOP_DUMP(vp, (caddr_t)cpr_buf, cpr_file_bn, CPRBUFS);
		DEBUG3(errp("done\n"));
		panicstr = NULL;

		STAT->cs_real_statefsz += CPRBUFSZ;

		if (error) {
			errp("Statefile dump error (%d)\n", error);
			return (error);
		}
		cpr_file_bn += CPRBUFS;	/* Increment block count */
		cpr_wptr = cpr_buf;	/* back to top of buffer */
	}
	return (0);
}

int
cpr_flush_write(vnode_t *vp)
{
	int	nblk;
	int	error;

	/*
	 * Calculate remaining blocks in buffer, rounded up to nearest
	 * disk block
	 */
	nblk = btod(cpr_wptr - cpr_buf);

	panicstr = "fake";
	error = VOP_DUMP(vp, (caddr_t)cpr_buf, cpr_file_bn, nblk);
	panicstr = NULL;

	cpr_file_bn += nblk;
	if (error) {
		DEBUG2(errp("cpr_flush_write: error (%d)\n", error));
		return (error);
	}
	return (error);
}
