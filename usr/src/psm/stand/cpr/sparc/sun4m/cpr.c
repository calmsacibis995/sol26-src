/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr.c	1.60	96/09/19 SMI"

/*
 * This module contains the boot portion of checkpoint-resume
 * All code in this module are platform independent.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fcntl.h>
#include <sys/pte.h>
#include <sys/bootconf.h>
#include <sys/clock.h>		  /* for COUNTER_ADDR */
#include <sys/eeprom.h>		 /* for EEPROM_ADDR */
#include <sys/memerr.h>		 /* for MEMERR_ADDR */
#include <sys/intreg.h>		 /* for INTREG_ADDR */
#include <sys/comvec.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/prom_plat.h>

/* XXX: Move to cpr_impl.c later */
#include <sys/mmu.h>

#define	MAXVALUE 0xFFFFFFFF
						/* From cpr_compress.c */
extern uint_t cpr_decompress(uchar_t *, uint_t, uchar_t *);
extern pfn_is_valid(u_int pfn);
extern struct memlist *fill_memlists(char *name, char *prop);

void cpr_relocate_promem(void);
void cpr_mem_update(void);
void cpr_set_bootmap(void);
int cpr_relocate_page(u_int, u_int, u_int);
int cpr_kpage_is_free(int);
int cpr_bpage_is_free(int);
int cpr_boot_setbit(int);
int cpr_read_bitmap(int);
void early_startup(union sunromvec *, int, struct bootops *);
int open(char *, char *);
extern int cpr_reset_properties();
extern int cpr_locate_statefile(char *, char *);

static void cpr_create_nonvirtavail(struct memlist **);
static int cpr_is_prom_mem(u_int pa);
static void cpr_sort_memlist(struct memlist *, struct memlist **);
static int cpr_relocate_boot(int);

uint_t cpr_compress(uchar_t *, uint_t, uchar_t *);
extern void turn_cache_on(int);
extern void cpr_relocate_page_tables();
extern void prom_unmap(caddr_t, u_int);
extern int read(int, caddr_t, int);
extern u_int va_to_pa(u_int);
extern struct pte *cpr_srmmu_ptefind(caddr_t);
extern u_int cpr_va_to_pfn(u_int);
extern int move_page(u_int, u_int);
extern int ldphys(int);
extern void stphys(int, int);
extern void srmmu_mmu_flushpage(caddr_t);
extern int cpr_getprop(struct bootops *, char *, void *);


#define	CPR_MAX_MEMLIST		8
struct memlist mlbuf[CPR_MAX_MEMLIST];

/* Global variables */
char	cpr_statefile[OBP_MAXPATHLEN];
char	cpr_filesystem[OBP_MAXPATHLEN];
int totbitmaps, totpages;
u_int  mapva, ptva, free_va;	/* temp virtual address used for mapping */
struct cpr_bitmap_desc *bitmap_desc;
struct bootops *bootops;
int compressed, no_compress, good_pg;
int cur_bitmap_inx;	/* Current bitmap to search for a free page */
u_int  cur_pfn;		/* Current pfn to start searching */
int cpr_debug;		/* cpr debug level */
static struct sun4m_machdep mach_info;

#define	RELOCATE_BOOT		0
#define	RELOCATE_CPRBOOT	1


/*
 * ufsboot may grow its memory usage in the middle of read. Reserve extra
 * pages for it.
 */
#define	BOOT_SCRATCHMEM_GROW	(8 * MMU_PAGESIZE)

char target_bootname[80];
char target_bootargs[80];

#if defined(lint)
char start[1];
char end[1];
#endif /* lint */

/*
 * main routine for resuming from a checkpoint state file
 * performs the following sequentially:
 *
 *	- finds and opens the state file
 *	- reads in elf header
 *	- reads in memory usage bitmap
 *	- relocate boot, cprboot and prom out of the way
 *	- call machine dependent routine (MMU setup)
 *	- read in physical memory image
 *	- jump back into kernel
 *
 * to resume from checkpoint: boot cpr /CPR
 */


union sunromvec *romp;

/* ARGSUSED */
int
main(union sunromvec *iromp)
{
	cpr_func_t cpr_resume;
	caddr_t	cpr_thread;
	struct cpr_terminator ct_tmp; /* cprboot version of terminator */
	u_int cpr_pfn;
	caddr_t	qsavp;
	int z, n_read = 0, pg_read = 0;
	int error = 0;
	int fd;
	size_t machdep_len;

#if defined(lint)
	/* Just to appease lint */
	errp("op2_bootargs=%s\n", iromp->obp.op2_bootargs);
	early_startup(0, 0, 0);
	(void) cpr_compress(0, 0, 0);
#endif /* lint */

	ct_tmp.tm_cprboot_start.tv_sec = prom_gettime() / 1000;

	prom_printf("\033[p");
	prom_printf("\014");
	prom_printf("\033[1P");
	prom_printf("\033[18;21H");
	prom_printf("Restoring the System. Please Wait... ");

	/*
	 * Restore the original value of the nvram properties saved
	 * during suspend.  If we cannot read this info, the state
	 * file itself may be obsolete or bad, so we must abort.
	 * However, we do continue the resume even if we can't set
	 * one of the properties.
	 */
	if (cpr_reset_properties() == -1) {
		errp("cprboot: Cannot read "
		    "saved nvram info, please do a normal boot\n");
			return (-1);
	}

	/*
	 * Get pathname of statefile and full device tree path
	 * of the filesystem it resides on.
	 */
	if (cpr_locate_statefile(cpr_statefile, cpr_filesystem) == -1) {
		errp("cprboot: Cannot find statefile; "
			"please do a normal boot.\n");
		return (-1);
	}

	/*
	 * For Sunergy only since Sunergy prom doesn't turn on cache;
	 * We do it here to speed things up. Other platforms are no-op.
	 */
	(void) turn_cache_on(0);

	if ((fd = open(cpr_statefile, cpr_filesystem)) == -1) {
		errp("can't open %s on %s, please do a normal reboot\n",
		    cpr_statefile, cpr_filesystem);
		return (-1);
	}

	/*
	 * NOTE:
	 * Discard cpr_read_headers() and make 2 calls instead
	 */
	if ((error = cpr_read_elf(fd)) != 0)
		return (-1);

	if ((error = cpr_read_cdump(fd, &cpr_resume, &cpr_thread,
		&cpr_pfn, &qsavp, &totbitmaps, &totpages)) != 0)
		return (-1);

	if ((machdep_len = (u_int)
	    cpr_get_machdep_len(fd)) != sizeof (struct sun4m_machdep)) {
		errp("Bad size for machdep field\n");
		return (-1);
	}

	if (cpr_read_machdep(fd, (caddr_t) &mach_info, machdep_len) != 0)
		return (-1);

	if ((error = cpr_read_bitmap(fd)) != 0)
		return (-1);

	/*
	 * map in the first page of kernel resume code, need to allocate
	 * mapping early since prom_map may allocate memory and all memory
	 * allocation needs to be done in the very begining so we will have
	 * an easier time relocate them.
	 */
	DEBUG4(errp("Mapping kernel page: va=%x  pfn=%x\n",
		cpr_resume, cpr_pfn));
	if (prom_map(MMU_L3_BASE(cpr_resume), 0, PN_TO_ADDR(cpr_pfn),
		MMU_PAGESIZE) == 0)
		errp("PROM_MAP resume kernel page failed\n");

	/*
	 * reserve mapping for two page aligned virtual addresses,
	 * this will be used later for buffering.
	 */
	if ((mapva = (u_int)prom_map(0, 0, 0, MMU_PAGESIZE)) == 0)
		errp("PROM_MAP mapva page failed\n");

	if ((free_va = (u_int)prom_map(0, 0, 0,
		(CPR_MAXCONTIG * MMU_PAGESIZE))) == 0)
		errp("PROM_MAP free_va failed\n");

	if ((ptva = (u_int)prom_map((caddr_t)0, 0, 0, MMU_PAGESIZE)) == 0)
		errp("PROM_MAP ptva page failed\n");

	cpr_set_bootmap();

	(void) cpr_relocate_page_tables();

	/*
	 * Relocate boot and cprboot.
	 */
	if ((int) cpr_relocate_boot(RELOCATE_BOOT) != 0)
		return (-1);

	DEBUG4(errp("*** Boot relocated\n"));

	if (cpr_relocate_boot(RELOCATE_CPRBOOT) != 0)
		return (-1);

	DEBUG4(errp("*** cproot relocated\n"));

	/*
	 * THERE CAN BE NO MORE PROM MEMORY ALLOCATION AFTER THIS POINT!!!
	 * Violating this rule will result in severe bodily harm to
	 * the coder!
	 */
	cpr_relocate_promem();

	/*
	 * read in pages
	 *
	 * XXX: this is inline because I suspect that we
	 *	  have exceeded the max reg stack win allowed
	 *	+ Yes, the trap stuff was not setup so we fail
	 */
	while (good_pg < totpages) {

		if ((pg_read = cpr_read_phys_page(fd, free_va, &z)) > 0) {
			n_read++;
			good_pg	+= pg_read; /* NOTE: changed */
			if (z)
				compressed += pg_read;
			else
				no_compress += pg_read;
		} else {
			errp("phys page read err: read=%d good=%d total=%d\n",
				n_read, good_pg, totpages);
			prom_enter_mon();
		}
	}
	DEBUG4(errp("Read=%d totpages=%d no_compress=%d compress=%d\n",
		good_pg, totpages, no_compress, compressed));

	if ((error = cpr_read_terminator(fd, &ct_tmp, mapva)) != 0)
		return (-1);
	DEBUG4(errp("Ready to Resume: error=%d\n", error));
	/* NO MORE time exhaustive activities after this */

	/*
	 * remap the kernel entry page again, the mapping may be chgd
	 */
	(void) prom_unmap(MMU_L3_BASE(cpr_resume), MMU_PAGESIZE);
	if (prom_map(MMU_L3_BASE(cpr_resume), 0, PN_TO_ADDR(cpr_pfn),
		MMU_PAGESIZE) == 0)
		errp("remapping resume kernel page failed\n");

	(*cpr_resume)(mach_info.mmu_ctp, mach_info.mmu_ctx,
		cpr_thread, qsavp, mach_info.mmu_ctl);

	/* NOTREACHED */
	return (0);
}

/*
 * Property intercept routines for kadb, so that it can
 * tell unix it's real name, and it's real bootargs. We
 * also let it figure out our virtual start and end addresses
 * rather than hardcoding them somewhere nasty.
 */
int
cpr_getprop(struct bootops *bop, char *name, void *buf)
{
	extern	char	start[];
	extern	char	end[];
	u_int start_addr = (u_int)start;
	u_int end_addr = (u_int)end;

	if (strcmp("whoami", name) == 0) {
		(void) strcpy(buf, target_bootname);
	} else if (strcmp("boot-args", name) == 0) {
		(void) strcpy(buf, target_bootargs);
	} else if (strcmp("cprboot-start", name) == 0) {
		bcopy((char *)&start_addr, buf, sizeof (caddr_t));
	} else if (strcmp("cprboot-end", name) == 0) {
		bcopy((char *)&end_addr, buf, sizeof (caddr_t));
	} else
	return (BOP_GETPROP(bop->bsys_super, name, buf));
	return (0);
}

/* ARGSUSED0 */
void
early_startup(union sunromvec *rump, int shim, struct bootops *buutops)
{
	extern struct bootops *bootops;
	static struct bootops cpr_bootops;

	/*
	* Save parameters from boot in obvious globals, and set
	* up the bootops to intercept property look-ups.
	*/
	romp = rump;
	bootops = buutops;

	prom_init("cpr", rump);

	if (BOP_GETVERSION(bootops) != BO_VERSION) {
	prom_printf("WARNING: %d != %d => %s\n",
	    BOP_GETVERSION(bootops), BO_VERSION,
	    "mismatched version of /boot interface.");
	}

	bcopy((caddr_t)bootops, (caddr_t)&cpr_bootops,
	    sizeof (struct bootops));

	cpr_bootops.bsys_super = bootops;
	cpr_bootops.bsys_getprop = cpr_getprop;

	/*
	cpr_bootops.bsys_getproplen = cpr_getproplen;
	*/

	bootops = &cpr_bootops;

	/*
	(void) fiximp();
	*/
}

/*
 * Read in the memory usage bitmap so we know which
 * physical pages are free for us to use
 */
int
cpr_read_bitmap(int fd)
{
	int i, bitmapsize;
	char *bitmap, *auxbitmap;
	cbd_t *bp;

	/*
	 * dynamically allocate the bitmap desc array
	 * Read in the bitmap descriptor follow by the actual bitmap
	 */
	bitmap_desc = (cbd_t *)prom_alloc((caddr_t)0,
		totbitmaps * sizeof (cbd_t), 0);

	for (i = 0; i < totbitmaps; i++) {
		bp = &bitmap_desc[i];
		if (read(fd, (caddr_t)bp, sizeof (cbd_t))
			< sizeof (cbd_t)) {
			errp("cpr_read_bitmap: Err reading bitmap des %d\n", i);
			return (-1);
		}

		bitmapsize = bp->cbd_size;

		/*
		 * dynamically allocate the bitmap for both the map we are
		 * about to read and the map we will use for relocation.
		 */
		bitmap = prom_alloc((caddr_t)0, bitmapsize, 0);
		auxbitmap = prom_alloc((caddr_t)0, bitmapsize, 0);
		bp->cbd_bitmap = bitmap;
		bp->cbd_auxmap = auxbitmap; /* boot mem actg */

		if (read(fd, bitmap, bitmapsize) < bitmapsize) {
			errp("cpr_read_bitmap: err reading bitmap %d\n", i);
			return (-1);
		}

		if (bp->cbd_magic != CPR_BITMAP_MAGIC) {
			errp("cpr_read_bitmap: Bitmap %d BAD MAGIC %x\n", i,
				bp->cbd_magic);
			return (-1);
		}
	}
	return (0);
}


/*
 * rellocate and remap any prom page that will be used by the incoming kernel.
 */
void
cpr_relocate_promem()
{
	struct memlist *vmem, *nvmavail = NULL;
	u_int va, pfn, opfn;
	struct pte *ptep;
	int cnt = 0;

	cpr_create_nonvirtavail(&nvmavail);

	/*
	 * move and remap any conflict prom page used by coming kernel
	 */
	for (vmem = nvmavail; vmem; vmem = vmem->next) {
		/*
		 * address + size can be MAXVALUE so check u_int overflow
		 */
		for (va = vmem->address; va >= vmem->address &&
			va < (vmem->address + vmem->size); va += MMU_PAGESIZE) {
			/*
			 * check if it belongs to prom mem
			 */
			if (!cpr_is_prom_mem(va_to_pa(va)))
				continue;

			ptep = (struct pte *)cpr_srmmu_ptefind((caddr_t)va);
			if (ptep == (struct pte *)-1)
				continue;

			opfn = cpr_va_to_pfn(va);

			if ((pfn = cpr_relocate_page(opfn, (u_int)ptep, va))
				== opfn)
				continue;

			if (pfn == (int)-1) {
			errp("cpr_relocate_promem:fail to reloc va=%x pfn=%x\n",
				va, cpr_va_to_pfn(va));
				continue;
			}

			DEBUG4(errp("reloc'ing promem: va=%x opfn=%x npfn=%x\n",
				va, opfn, pfn));
			cnt++;
		}
	}
	DEBUG4(errp("\n**** %d prom pages were relocated\n", cnt));
}


static void
cpr_create_nonvirtavail(struct memlist **nvmavail)
{
	struct memlist *vmem, *sortvmem = NULL;
	u_int newsize;
	int i = 0;

	/*
	 * sort the virtavail mem list
	 */
	for (vmem = bootops->boot_mem->virtavail; vmem; vmem = vmem->next) {
		cpr_sort_memlist(vmem, &sortvmem);
	}
	/*
	 * XXX hardcode max virtual MAXVALUE
	 */
	for (vmem = sortvmem; vmem; vmem = vmem->next) {
		u_int size, addr, naddr;
		if (i >= CPR_MAX_MEMLIST) {
			errp("cpr_create_nonvirtavail: not enough mlbuf\n");
			return;
		}
		addr = vmem->address;
		size = vmem->size;
		/*
		 * last one
		 */
		if (vmem->next == NULL) {
			if ((newsize = MAXVALUE - (addr + size)) == 0)
				continue;
			mlbuf[i].size = newsize;
			mlbuf[i].address = addr + size;
			mlbuf[i].next = *nvmavail;
			*nvmavail = &mlbuf[i];

			i++;
			continue;
		}

		naddr = vmem->next->address;

		/*
		 * special processing for the 1st one
		 */
		if (vmem == sortvmem && addr != 0) {
			mlbuf[i].size = addr;
			mlbuf[i].address = 0;
			mlbuf[i].next = *nvmavail;
			*nvmavail = &mlbuf[i];

			i++;
			/* fall through to make up # of loops needed */
		}
		/*
		 * the middle ones
		 */
		if ((newsize = naddr - (addr + size)) == 0)
			continue;
		mlbuf[i].size = newsize;
		mlbuf[i].address = addr + size;
		mlbuf[i].next = *nvmavail;
		*nvmavail = &mlbuf[i];
		i++;
	}
	DEBUG4(errp("cpr_create_nonvirtavail: # of memlists = %d\n", i));
}

static int
cpr_is_prom_mem(u_int pa)
{
	struct memlist *pp;

	/*
	 * if the page doesn't have a mapped virtual addr, assume it's
	 * ok to use.
	 */
	if (pa == (u_int)MAXVALUE)
		return (0);

	for (pp = bootops->boot_mem->physavail; pp; pp = pp->next) {
		if ((pa >= pp->address) && pa < (pp->address + pp->size))
			return (1);
	}
	return (0);
}

static void
cpr_sort_memlist(struct memlist *seg, struct memlist **smem)
{
	struct memlist *mp;

	/*
	 * add seg to the head of the smem list
	 */
	if (*smem == NULL || seg->address < (*smem)->address) {
		seg->next = *smem;
		*smem = seg;
		return;
	}
	for (mp = *smem; mp; mp = mp->next) {
		/*
		 * add seg to the tail of the smem list
		 */
		if (seg->address >= mp->address && mp->next == NULL) {
			seg->next = mp->next;
			mp->next = seg;
			return;
		}
		/*
		 * insert seg into middle of the list
		 */
		if (seg->address >= mp->address &&
		    seg->address < mp->next->address) {
			seg->next = mp->next;
			mp->next = seg;
			return;
		}
	}
	/* NOTREACHED */
	DEBUG4(errp("cpr_sort_memlist: cannot sort entry %x\n", seg));
}

int
cpr_find_free_page()
{
	int i, j;
	register u_int npfn;
	cbd_t *bp;

	if (cur_bitmap_inx >= totbitmaps) {
		errp("cpr_find_free_page: ran out of free pages\n");
		return (-1);
	}

	for (i = cur_bitmap_inx; i < totbitmaps; i++) {
		bp = &bitmap_desc[i];
		for (j = cur_pfn; j < bp->cbd_size * NBBY; j++) {

			npfn = bp->cbd_spfn + j;
			if (cpr_kpage_is_free(npfn) &&
			    cpr_bpage_is_free(npfn)) {

				(void) cpr_boot_setbit(npfn);
				cur_pfn = j + 1;

				if (cur_pfn < bp->cbd_size * NBBY)
					cur_bitmap_inx = i;
				else {  /* roll over to next bitmap */
					cur_bitmap_inx = i + 1;
					cur_pfn = 0;
				}
	DEBUG4(errp("cpr_find_free_page: npfn %x cur_pfn %x cur_bitmap %d\n",
			npfn, cur_pfn, cur_bitmap_inx));

				return (npfn);
			}
		}
	}
	errp("cpr_find_free_page: no free page found (cbi=%x)\n",
		cur_bitmap_inx);
	return (-1);
}

static int
cpr_setbit(int pfn, int kernel)
{
	cbd_t *bm;
	char *mapp;
	int i;

	for (i = 0; i < totbitmaps; i++) {
		bm = &bitmap_desc[i];
		if ((pfn >= bm->cbd_spfn) && (pfn <= bm->cbd_epfn)) {
			if (kernel)
				mapp = bm->cbd_bitmap;
			else
				mapp = bm->cbd_auxmap;
			if (isclr(mapp, (pfn - bm->cbd_spfn))) {
				setbit(mapp, (pfn - bm->cbd_spfn));
				return (0);
			} else
				return (-1);
		}
	}
	return (-1);
}

#ifdef	NOTNOW

int
cpr_kernel_setbit(pfn)
{
	return (cpr_setbit(pfn, 1));
}
#endif	NOTNOW

int
cpr_boot_setbit(pfn)
{
	return (cpr_setbit(pfn, 0));
}

static
page_is_free(int pfn, int kernel)
{
	cbd_t *bm;
	int bit, i;

	for (i = 0; i < totbitmaps; i++) {
		bm = &bitmap_desc[i];
		if ((pfn >= bm->cbd_spfn) && (pfn <= bm->cbd_epfn)) {
		if (kernel)
			bit = isset(bm->cbd_bitmap, (pfn - bm->cbd_spfn));
		else
			bit = isset(bm->cbd_auxmap, (pfn - bm->cbd_spfn));

		if (bit)
			return (0);
		else
			return (1);
		}
	}
	return (1);
}

int
cpr_kpage_is_free(int pfn)
{
	return (page_is_free(pfn, 1));
}

int
cpr_bpage_is_free(int pfn)
{
	return (page_is_free(pfn, 0));
}

/*
 * page relocation routine
 *
 * if this physical page is being used by the incoming kernel then
 * we move it to the next free page that is not being used by kernel
 * or boot.
 */
int
cpr_relocate_page(u_int pfn, u_int pte_pa, u_int va)
{
	register u_int pte;
	register u_int npfn;
	u_int pa, npa;

	if (cpr_kpage_is_free(pfn))
		return (pfn);

	if ((npfn = cpr_find_free_page()) == -1) {
		errp("cpr_relocate_page: cannot relocate pfn %x\n", pfn);
		return (-1);
	}
	/*
	 * we need to watch for aliasing problem here
	 * on platforms with virtual caches
	 */
	pa = PN_TO_ADDR(pfn);
	npa = PN_TO_ADDR(npfn);

	DEBUG4(errp("reloc pa=%x to npa=%x: va=%x pfn=%x npfn=%x pte_pa=%x\n",
		pa, npa, va, pfn, npfn, pte_pa));

	prom_unmap((caddr_t)mapva, MMU_PAGESIZE);
	if ((u_int) prom_map((caddr_t) mapva, 0, npa, MMU_PAGESIZE) != mapva) {
		errp("cpr_relocate_page: mapva failed\n");
		return (-1);
	}
	(void) move_page(va, mapva);

	/*
	 * need to convert a pfn to a ptp and store
	 * it back to the page table.  Have to make sure
	 * that all important variables are in registers
	 *
	 * be careful not to change the code sequence here!
	 * if you don't know what you are doing you may cause
	 * same pte be allocated twice.
	 */
	pte = ldphys(pte_pa);
	pte = (pte & 0xff) | (npfn << 8);
	stphys(pte_pa, pte);

	srmmu_mmu_flushpage((caddr_t) va);

	return (npfn);
}

/*
 * tag pages in use from the boot side in the boot memory usage map,
 * this includes all pages in use by prom, /boot and /cprboot,
 * the algorithm for find prom pages is physinstalled - physavail.
 */
void
cpr_set_bootmap()
{
	register u_int va;
	caddr_t start, end;
	struct memlist *pavail;
	u_int spa, epa, pa, pfn, cnt = 0, i, j;
	cbd_t *bm;
	char *bits;

	/*
	 * Get the latest snapshot of memory layout from the
	 * prom before figuring out pages used by the prom.
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

	for (pavail = (struct memlist *)bootops->boot_mem->physavail;
		pavail; pavail = pavail->next) {
		spa = pavail->address;
		epa = pavail->address + pavail->size;

		DEBUG4(errp("Phys avail addr %x  epa %x\n", spa, epa));

		for (pa = spa; pa < epa; pa += MMU_PAGESIZE) {
			pfn = ADDR_TO_PN(pa);
			if ((pfn != (int)-1) && ((pfn & 0x70000) == 0)) {
				(void) cpr_boot_setbit(pfn);
				cnt++;
			}
		}
	}
	DEBUG4(errp("*** %d pages not mark for prom pages ***\n", cnt));

	for (i = 0; i < totbitmaps; i++) {
		bm = &bitmap_desc[i];
		for (j = 0, bits = bm->cbd_auxmap; j < bm->cbd_size; j++)
			bits[j] = ~bits[j];
	}

	/*
	 * tag all pages used by /boot
	 */
	/*
	 * Kludge for in case ufsboot does not export the properties.
	 */
	if ((BOP_GETPROP(bootops, "boot-start", &start) != 0) ||
	    (BOP_GETPROP(bootops, "boot-end", &end) != 0)) {
		start = (caddr_t) 0x100000;
		end = (caddr_t) 0x250000;
	}
	end += BOOT_SCRATCHMEM_GROW;
	DEBUG4(errp("Tag auxmap for boot start %x end %x\n", start, end));

	for (va = (u_int)start; va <= (u_int)end; va += MMU_PAGESIZE) {
		if ((pfn = cpr_va_to_pfn(va)) != (u_int) -1) {
			DEBUG4(errp("Tagging pfn=%x\n", pfn));
			if (cpr_boot_setbit(pfn))
				errp("failed to tag auxmap for pfn=%x", pfn);
		}
	}

	/*
	 * tag all pages used by /cprboot
	 */
	(void) cpr_getprop(bootops, "cprboot-start", &start);
	(void) cpr_getprop(bootops, "cprboot-end", &end);

	DEBUG4(errp("Tag auxmap for cprboot start %x end %x\n", start, end));
	for (va = (u_int)start; va <= (u_int)end; va += MMU_PAGESIZE) {
		if ((pfn = cpr_va_to_pfn(va)) != (u_int) -1)
			(void) cpr_boot_setbit(pfn);
	}
}

static int
cpr_relocate_boot(int flag)
{
	register u_int va, opfn, npfn, npa;
	caddr_t start, end;
	struct pte *ptep;
	union ptes tmppte;

	DEBUG4(errp("Enter cpr relocate boot %d\n", flag));

	/*
	 * Kludge for in case ufsboot does not export the properties.
	 */
	if (flag == RELOCATE_BOOT) {
		if ((BOP_GETPROP(bootops, "boot-start", &start) != 0) ||
		    (BOP_GETPROP(bootops, "boot-end", &end) != 0)) {
		errp("cannot read boot-start/end; set to default values\n");
			start = (caddr_t) 0x100000;
			end = (caddr_t) 0x250000;
		}
		DEBUG4(errp("BOOT START %x  END %x\n", start, end));
		end += BOOT_SCRATCHMEM_GROW;
	} else if (flag == RELOCATE_CPRBOOT) {
		(void) cpr_getprop(bootops, "cprboot-start", &start);
		(void) cpr_getprop(bootops, "cprboot-end", &end);
		DEBUG4(errp("CPRBOOT START %x  END %x\n", start, end));
	}

	for (va = (u_int)start; va <= (u_int)end; va += MMU_PAGESIZE) {

		if ((opfn = cpr_va_to_pfn(va)) == (u_int) -1)
		    continue;

		/*
		 * make these pages cacheable, so we run faster
		 * XXX: may speed this loop up later?
		 */
		ptep = (struct pte *)cpr_srmmu_ptefind((caddr_t)va);
		if (ptep == (struct pte *)-1)
			continue;

		tmppte.pte_int = ldphys((int)ptep);
		tmppte.pte.Cacheable = 1;

		if (cpr_kpage_is_free(opfn)) {
			stphys((int)ptep, tmppte.pte_int);
			continue;
		}

		if ((npfn = cpr_find_free_page()) == -1) {
			errp(" cpr_relocate_boot: NO FREE PAGE\n");
			return (-1);
		}
		npa = PN_TO_ADDR(npfn);

		DEBUG4(errp("cpr_relocate_boot: va %x  opfn %x  npfn %x\n",
			va, opfn, npfn));

		prom_unmap((caddr_t)free_va, MMU_PAGESIZE);
		if ((u_int)prom_map((caddr_t)free_va, 0, npa,
			MMU_PAGESIZE) != free_va) {
			errp("cpr_relocate_boot: Prom_map failed\n");
			return (-1);
		}
		(void) move_page(va, free_va);

		/*
		 * stores the new physical pfn into PTE
		 */
		tmppte.pte.PhysicalPageNumber = npfn;
		stphys((int)ptep, tmppte.pte_int);

		srmmu_mmu_flushpage((caddr_t)va);
	}
	return (0);
}
