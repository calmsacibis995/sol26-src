/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)startup.c	1.67	96/10/29 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>
#include <sys/dki_lock.h>

#include <sys/disp.h>
#include <sys/class.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/kstat.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/callo.h>

#include <sys/procfs.h>
#include <sys/acct.h>

#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/modctl.h>		/* for "procfs" hack */
#include <sys/kvtopdata.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#define	SUNDDI_IMPL		/* so sunddi.h will not redefine splx() et al */
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/tss.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
#include <sys/fp.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/hat_i86.h>
#include <sys/instance.h>
#include <sys/smp_impldefs.h>
#include <sys/x86_archext.h>

void	debug_enter(char *);
void	lomem_init(void);

/*
 * XXX make declaration below "static" when drivers no longer use this
 * interface.
 */
caddr_t i86devmap(int, int, u_int);
extern	void param_init();
extern	void	param_calc(int);
extern	void	kadb_uses_kernel();
extern	caddr_t	p0_va;	/* Virtual address for accessing physical page 0 */
extern	int do_forcefault;  /* don't do forcefaults until after startup() */
extern	void get_font_ptrs(void);

static void kvm_init(void);
static int establish_console(void);

int console = CONSOLE_IS_FB;

/*
 * Declare these as initialized data so we can patch them.
 */
int physmem = 0;	/* memory size in pages, patch if you want less */
int kernprot = 1;	/* write protect kernel text */

/* Global variables for MP support. Used in mp_startup */
caddr_t	rm_platter_va;
paddr_t	rm_platter_pa;


/*
 * Configuration parameters set at boot time.
 */

int do_pg_coloring = 0;		/* will be set for non-VAC Sun-4/110 only */

extern int maxusers;

caddr_t econtig;		/* end of first block of contiguous kernel */
caddr_t eecontig;		/* end of segkp, which is after econtig */
caddr_t	four_kecontig;

struct bootops *bootops = 0;	/* passed in from boot */

char bootblock_fstype[16];

extern lksblk_t *lksblks_head;

/*
* new memory fragmentations are possible in startup() due to BOP_ALLOCs. this
* depends on number of BOP_ALLOC calls made and requested size, memory size `
*  combination.
*/
#define	POSS_NEW_FRAGMENTS	10

/*
 * VM data structures
 */
int page_hashsz;		/* Size of page hash table (power of two) */
struct machpage *pp_base;	/* Base of system page struct array */
struct	page **page_hash;	/* Page hash table */
struct	seg *segkmap;		/* Kernel generic mapping segment */
struct	seg ktextseg;		/* Segment used for kernel executable image */
struct	seg kvalloc;		/* Segment used for "valloc" mapping */
struct seg *segkp;		/* Segment for pageable kernel virt. memory */
struct memseg *memseg_base;
struct	vnode unused_pages_vp;
pte_t *KERNELmap;		/* pointer to ptes mapping kernel */
pte_t *Sysmap1;			/* pointer to ptes mapping from SYSBASE */
pte_t *Sysmap2;			/* pointer to ptes mapping from Syslimit */

extern char Sysbase[];
extern char Syslimit[];

extern page_t *page_numtopp_alloc();

/*
 * VM data structures allocated early during boot.
 */
#define	KERNELMAP_SZ(frag)	\
	max(MMU_PAGESIZE,	\
	roundup((sizeof (struct map) * SYSPTSIZE/2/(frag)), MMU_PAGESIZE))

caddr_t valloc_base;
struct memlist *memlist;
caddr_t startup_alloc_vaddr = (caddr_t)SYSBASE + MMU_PAGESIZE;
u_int startup_alloc_size;
u_int	startup_alloc_chunk_size = 20 * MMU_PAGESIZE;	/* patchable */

extern caddr_t  dump_addr, cur_dump_addr; /* for dumping purposes */

caddr_t s_text;		/* start of kernel text segment */
caddr_t e_text;		/* end of kernel text segment */
caddr_t s_data;		/* start of kernel data segment */
caddr_t e_data;		/* end of kernel data segment */

struct memlist *phys_install;	/* Total installed physical memory */
struct memlist *phys_avail;	/* Available (unreserved) physical memory */
struct memlist *virt_avail;	/* Available (unmapped?) virtual memory */
struct memlist *shadow_ram;	/* Non-dma-able memory XXX needs work */
u_int pmeminstall;		/* total physical memory installed */

static void kphysm_init();

extern int segkmem_ready;

/* Value to be loaded into cr3 for kernel-only page directory */
u_int	kernel_only_cr3;
caddr_t kernel_only_pagedir;

/*
 * Monitor pages may not be where this says they are.
 * and the debugger may not be there either.
 *
 *		  Physical memory layout
 *		(not necessarily contiguous)
 *
 *    availmem -+-----------------------+
 *		|			|
 *		|	page pool	|
 *		|			|
 *		|-----------------------|
 *		|   configured tables	|
 *		|	buffers		|
 *   firstaddr -|-----------------------|
 *		|   hat data structures |
 *		|-----------------------|
 *		|    kernel data, bss	|
 *		|-----------------------|
 *		|    interrupt stack	| ?????
 *		|-----------------------|
 *		|	kernel text	|
 *		+-----------------------+
 *
 *
 *		  Virtual memory layout.
 *		+-----------------------+
 *		|	psm 1-1 map	|
 *		|	exec args area	|
 * 0xFFC00000  -|-----------------------|- ARGSBASE
 *		|	debugger	|
 * 0xFF800000  -|-----------------------|- DEBUGSTART
 *		|	Hole		|
 * 0xFF000000  -|-----------------------|- Syslimit
 *		|	allocatable	|- phys_syslimit
 *		|    thru kernelmap	|
 * 0xF5400000  -|-----------------------|- Sysbase
 *		|	Ramdisk		|
 *		| (debugging only)	|	On a machine with largepage
 *		|-----------------------|			|
 *		|	segkmap		|			V
 *		|-----------------------|- eecontig	|-------| eecontig
 *		|			|		|	|
 *		|			| 4Mb		|segkp  |
 *		|	segkp		|  aligned ->	|-------| econtig
 *		|			|		| Hole	|
 *		|-----------------------|- econtig	|-------| four_kecontig
 *		|   page structures	|
 *		|	page hash	|
 *		|   memseg structures	|
 *		|-----------------------|- end
 *		|	kernel		|
 * 0xE0001000  -|-----------------------|- start
 *		|  user copy red zone	|
 *		|	(invalid)	|
 * 0xE0000000  -|-----------------------|- KERNELBASE
 *		|	user stack	|
 *		:			:
 *		:			:
 *		|	user data	|
 *		|-----------------------|
 *		|	user text	|
 * 0x00010000  -|-----------------------|
 *		|	invalid		|
 * 0x00000000	+-----------------------+
 */

extern char t0stack;
extern struct _kthread t0;
struct _klwp lwp0;
struct proc p0;

void init_intr_threads(struct cpu *);
void init_clock_thread();
void init_panic_thread();

/* real-time-clock initialization parameters */
long gmt_lag;		/* offset in seconds of gmt to local time */
extern long process_rtc_config_file(void);

/*
 * On a pentium machine we relocate the kernel text+data+stack to a 4Mb page
 * and free the 4K pages that boot allocated. Now, to describe the new
 * available range of address we use the 'glbpmem' array. For each range of
 * address an element from the 'glbpmem' gets used. We do not expect to have
 * more than 16 fragments. GLBPMEM_OVERFLOW  helps us ensure we dont crash
 * when there are more than 16 fragments due to 4Mb pages for kernel text+data
 */
struct  memlist glbpmem[16];
#define	GLBPMEM_OVERFLOW(x)	((x) > &glbpmem[16])
static void	fourmb_support_warning();


/*
 * The default size of segmap segment is 16Mb  this variable can be used
 * to change the size of segmap segment.
 */
int	segmaplen = SEGMAPSIZE;
u_int	phys_syslimit;
int	insert_into_pmemlist(struct memlist **, struct memlist *);
u_int	kernelbase = (u_int)_start;


#ifdef  DEBUGGING_MEM
/*
 * Enable some debugging messages concerning memory usage...
 */
static int debugging_mem;
static void
printmemlist(char *title, struct memlist *mp)
{
	if (debugging_mem) {
		prom_printf("%s:\n", title);
		while (mp != 0)  {
			prom_printf("\tAddress 0x%x, size 0x%x\n",
			    mp->address, mp->size);
			mp = mp->next;
		}
	}
}
#endif	DEBUGGING_MEM

static	u_longlong_t	fourmb_base, fourmb_size;
static	int		num4mb_pages;
#define	MAX_KERNEL_4MBPAGES	18
#define	BOOT_END	(3*1024*1024)	/* default to 3mb */

/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	register int unixsize;
	register unsigned int i;
	u_int npages;
	u_int pfn;
	struct segmap_crargs a;
	int memblocks;
	struct memlist *bootlistp, *previous, *current, *best_bootlistp;
	u_int pp_giveback;
	u_int addr, highest_addr;
	caddr_t va;
	caddr_t memspace;
	u_int memspace_sz, segmapbase = 0;
	u_int nppstr;
	u_int memseg_sz;
	u_int pagehash_sz;
	u_int memlist_sz;
	u_int kernelmap_sz;
	u_int pp_sz;			/* Size in bytes of page struct array */
	major_t major;
	page_t *lowpages_pp;
	int dbug_mem;
	u_int avmem, total_memory;
	int	max_virt_segkp;
	int	max_phys_segkp;
	int	segkp_len;
	int	b_ext;
	static char b_ext_prop[] = "bootops-extensions";
	caddr_t	boot_end;
	u_int	first_free_page;
	static char boot_end_prop[] = "boot-end";

	static void setup_kvpm();
	extern void mt_lock_init();
	extern caddr_t mm_map;
	extern u_int va_to_pfn(u_int);
	extern int ddi_load_driver(char *);
	extern void hat_kern_setup(void);
	extern void _db_install(void);
	extern void setup_mtrr();
	extern void prom_setup(void);

	extern void setx86isalist(void);

	kernelbase = MMU_L1_VA(MMU_L1_INDEX(kernelbase));
	(void) check_boot_version(BOP_GETVERSION(bootops));

	/*
	 * This kernel needs bootops extensions to be at least 1
	 * (for the 1275-ish functions).
	 *
	 * jhd: cachefs boot updates boot extensions to rev 2
	 */
	if ((BOP_GETPROPLEN(bootops, b_ext_prop) != sizeof (int)) ||
	    (BOP_GETPROP(bootops, b_ext_prop, &b_ext) < 0) ||
	    (b_ext < 2)) {
		prom_printf("Booting system too old for this kernel.\n");
		prom_panic("halting");
		/*NOTREACHED*/
	}

	/*
	 * BOOT PROTECT. Ask boot for its '_end' symbol - the
	 * first available address above boot. We use this info
	 * to protect boot until it is no longer needed.
	 */
	if ((BOP_GETPROPLEN(bootops, boot_end_prop) != sizeof (caddr_t)) ||
	    (BOP_GETPROP(bootops, boot_end_prop, &boot_end) < 0))
		first_free_page = mmu_btopr(BOOT_END);
	else
		first_free_page = mmu_btopr((u_int)boot_end);

	dki_lock_setup(lksblks_head);

	/*
	 * Initialize the turnstile allocation mechanism.
	 */
	tstile_init();

	setcputype();	/* mach/io/autoconf.c - cputype needs definition */

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

#if 0
	/*
	 * Add up how much physical memory /boot has passed us.
	 */
	phys_install = (struct memlist *)bootops->boot_mem->physinstalled;

	phys_avail = (struct memlist *)bootops->boot_mem->physavail;

	virt_avail = (struct memlist *)bootops->boot_mem->virtavail;

	/* XXX - what about shadow ram ???????????????? */
#endif

	/*
	 * Initialize enough of the system to allow kmem_alloc
	 * to work by calling boot to allocate its memory until
	 * the time that kvm_init is completed.  The page structs
	 * are allocated after rounding up end to the nearest page
	 * boundary; kernelmap, and the memsegs are intialized and
	 * the space they use comes from the area managed by kernelmap.
	 * With appropriate initialization, they can be reallocated
	 * later to a size appropriate for the machine's configuration.
	 *
	 * At this point, memory is allocated for things that will never
	 * need to be freed, this used to be "valloced".  This allows a
	 * savings as the pages don't need page structures to describe
	 * them because them will not be managed by the vm system.
	 */
	valloc_base = (caddr_t)roundup((u_int)e_data, MMU_PAGESIZE);

	/*
	 * Remember what the physically available highest page is
	 * so that dumpsys works properly, and find out how much
	 * memory is installed.
	 */
	installed_top_size(bootops->boot_mem->physinstalled, &physmax,
	    &physinstalled);
	pmeminstall = ptob(physinstalled);

	/*
	 * Get the list of physically available memory to size
	 * the number of page structures needed.
	 * This is something of an overestimate, as it include
	 * the boot and any DMA (low continguous) reserved space.
	 */
	size_physavail(bootops->boot_mem->physavail, &npages, &memblocks);

	/*
	 * If physmem is patched to be non-zero, use it instead of
	 * the monitor value unless physmem is larger than the total
	 * amount of memory on hand.
	 */
	if (physmem == 0 || physmem > npages)
		physmem = npages;
	else {
		npages = physmem;
	}


	/*
	 * The kernel address space has two contiguous regions of
	 * virtual space, KERNELBASE to eecontig and SYSBASE to 4G
	 * The resource map "kernelmap" starts from SYSBASE and goes
	 * up to Syslimit. Since we can not allocate more than physmem
	 * of memory, we limit Syslimit to SYSBASE + (physmem * 2)
	 * So on machines with less than 64Mb of memory we have hole
	 * between phys_syslimit and Syslimit
	 */


	/* total memory rounded to multiple of 4MB */

	total_memory = ((physmem * MMU_PAGESIZE) + PTSIZE) & ~(PTSIZE - 1);

	if (total_memory < 64 * 1024 * 1024) {
		phys_syslimit = SYSBASE + (total_memory * 2);
		if (phys_syslimit > (u_int)Syslimit)
			phys_syslimit = (u_int)Syslimit;
	} else phys_syslimit = (u_int)Syslimit;

	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz = 1 << highbit(page_hashsz);
	pagehash_sz = sizeof (struct page *) * page_hashsz;

	/*
	 * Some of the locks depend on page_hashsz being set!
	 */
	mt_lock_init();

	/*
	 * The memseg list is for the chunks of physical memory that
	 * will be managed by the vm system.  The number calculated is
	 * a guess as boot may fragment it more when memory allocations
	 * are made before kvm_init(); twice as many are allocated
	 * as are currently needed.
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + POSS_NEW_FRAGMENTS);
	pp_sz = sizeof (struct machpage) * npages;
	memspace_sz = roundup(pagehash_sz + memseg_sz + pp_sz, MMU_PAGESIZE);

	/*
	 * We don't need page structs for the memory we are allocating
	 * so we subtract an appropriate amount.
	 */
	nppstr = btop(memspace_sz -
	    (btop(memspace_sz) * sizeof (struct machpage)));
	pp_giveback = nppstr * sizeof (struct machpage);

	memspace_sz -= pp_giveback;
	npages -= btopr(memspace_sz);
	pp_sz -= pp_giveback;

	memspace = (caddr_t)BOP_ALLOC(bootops, valloc_base, memspace_sz,
	    BO_NO_ALIGN);
	if (memspace != valloc_base)
		panic("system page struct alloc failure");
	bzero(memspace, memspace_sz);

	page_hash = (struct page **)memspace;
	memseg_base = (struct memseg *)((u_int)page_hash + pagehash_sz);
	pp_base = (struct machpage *)((u_int)memseg_base + memseg_sz);
	econtig = (caddr_t)roundup((u_int)pp_base + pp_sz, MMU_PAGESIZE);
	four_kecontig = econtig;
	if (x86_feature & X86_LARGEPAGE) {
		econtig = (caddr_t)(((u_int)econtig +
		(FOURMB_PAGESIZE - 1)) & ~(FOURMB_PAGESIZE - 1));
	}


	/*
	 * the memory lists from boot, and early versions of the kernelmap
	 * is allocated from the virtual address region managed by kernelmap
	 * so that later they can be freed and/or reallocated.
	 */
	memlist_sz = bootops->boot_mem->extent;
	/*
	 * Between now and when we finish copying in the memory lists,
	 * allocations happen so the space gets fragmented and the
	 * lists longer.  Leave enough space for lists twice as long
	 * as what boot says it has now; roundup to a pagesize.
	 */
	memlist_sz *= 2;
	memlist_sz = roundup(memlist_sz, MMU_PAGESIZE);
	kernelmap_sz = KERNELMAP_SZ(4);
	memspace_sz =  memlist_sz + kernelmap_sz;
	memspace = (caddr_t)BOP_ALLOC(bootops, startup_alloc_vaddr,
	    memspace_sz, BO_NO_ALIGN);
	startup_alloc_vaddr += memspace_sz;
	startup_alloc_size += memspace_sz;
	if (memspace == NULL)
		halt("Boot allocation failed.");
	bzero(memspace, memspace_sz);

	memlist = (struct memlist *)((u_int)memspace);
	kernelmap = (struct map *)((u_int)memlist + memlist_sz);

	mapinit(kernelmap, (long)btoc(phys_syslimit - (u_int)Sysbase) - 1,
		(u_long)1, "kernel map", kernelmap_sz / sizeof (struct map));

	mutex_enter(&maplock(kernelmap));
	if (rmget(kernelmap, btop(startup_alloc_size), 1) == 0)
		panic("can't make initial kernelmap allocation");
	mutex_exit(&maplock(kernelmap));

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

	phys_avail = current = previous = memlist;

	/*
	 * This block is used to copy the memory lists from boot's
	 * address space into the kernel's address space.  The lists
	 * represent the actual state of memory including boot and its
	 * resources.  kvm_init will use these lists to initialize the
	 * vm system.
	 */

	/*
	 * If the chip supports large pagesize, then we relocate kernel
	 * text and data to a 4MB page. Adjust the incoming memlist
	 * to reflect this relocation.
	 */
	if (!(x86_feature & X86_LARGEPAGE))
		goto no4mb_pages;
	if (!(IN_SAME_4MB_PAGE((u_int)four_kecontig, (u_int)_start))) {
		fourmb_base = (u_int)_start & ~(FOURMB_PAGESIZE - 1);
		num4mb_pages = ((u_int)roundup(((u_int)four_kecontig -
			fourmb_base), FOURMB_PAGESIZE))/FOURMB_PAGESIZE;
		/*
		 * If the size of kernel is bigger than 72Mb
		 * we dont map kernel text+data by 4Mb pages.
		 *
		 */
		if (num4mb_pages > MAX_KERNEL_4MBPAGES) {
			fourmb_base = 0;
			goto no4mb_pages;
		}
	} else num4mb_pages = 1;

	highest_addr = 0x1000000;
	best_bootlistp = NULL;
	bootlistp = bootops->boot_mem->physavail;
	for (; bootlistp; bootlistp = bootlistp->next) {

		if (bootlistp->address + bootlistp->size > highest_addr) {
			highest_addr = bootlistp->address + bootlistp->size;
			best_bootlistp = bootlistp;
		}
	}
	fourmb_size = (num4mb_pages + 2) * FOURMB_PAGESIZE;
	if (best_bootlistp && best_bootlistp->size  > fourmb_size) {
		best_bootlistp->size -= fourmb_size;
		fourmb_base = best_bootlistp->address + best_bootlistp->size;
	} else {
		fourmb_base = 0;
	}

no4mb_pages:

	/*
	 * Now copy the memlist into kernel space.
	 */
	bootlistp = bootops->boot_mem->physavail;
	for (; bootlistp; bootlistp = bootlistp->next) {
		/*
		 * Reserve page zero - see use of 'p0_va'
		 */
		if (bootlistp->address == 0) {
			if (bootlistp->size > PAGESIZE) {
				bootlistp->address += PAGESIZE;
				bootlistp->size -= PAGESIZE;
			} else
				continue;
		}

		if ((previous != current) && (bootlistp->address ==
		    previous->address + previous->size)) {
			/* coalesce */
			previous->size += bootlistp->size;
			continue;
		}
		current->address = bootlistp->address;
		current->size = bootlistp->size;
		current->next = (struct memlist *)0;
		if (previous == current) {
			current->prev = (struct memlist *)0;
			current++;
		} else {
			current->prev = previous;
			previous->next = current;
			current++;
			previous++;
		}
	}

	/*
	 * Initialize the page structures from the memory lists.
	 */
	kphysm_init(pp_base, memseg_base, npages, memblocks+POSS_NEW_FRAGMENTS);

	availrmem_initial = availrmem = freemem;

	/*
	 * BOOT PROTECT. All of boots pages are now in the available
	 * page list, but we still have a few boot chores remaining.
	 * Acquire locks on boot pages before they get can get consumed
	 * by kmem_allocs after kvm_init() is called. Unlock the
	 * pages once boot is really gone.
	 */
	lowpages_pp = (page_t *)NULL;
	for (pfn = 0; pfn < first_free_page; pfn++) {
		page_t *pp;
		if ((pp = page_numtopp_alloc(pfn)) != NULL) {
			pp->p_next = lowpages_pp;
			lowpages_pp = pp;
		}
	}

	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Initialize the kstat framework.
	 */
	kstat_init();

#if XXX_NEEDED
	/*
	 * we scale panic timeout by allowing 30 seconds for every
	 * 64 of memory.
	 */
	default_panic_timeout = 3000 * (physmax/0x4000 + 1);
#endif

	/*
	 * Lets display the banner early so the user has some idea that
	 * UNIX is taking over the system.
	 */
#ifdef OLD_BANNER
	cmn_err(CE_CONT,
	    " \nSolaris 2.1 for x86 FCS Release\n");
#else
	/*
	 * Lets display the banner early so the user has some idea that
	 * UNIX is taking over the system.
	 */
	cmn_err(CE_CONT,
	    "\rSunOS Release %s Version %s [UNIX(R) System V Release 4.0]\n",
	    utsname.release, utsname.version);
#endif
	cmn_err(CE_CONT, "Copyright (c) 1983-1996, Sun Microsystems, Inc.\n");
#ifdef DEBUG
	cmn_err(CE_CONT, "DEBUG enabled\n");
#endif
#ifdef TRACE
	cmn_err(CE_CONT, "TRACE enabled\n");
#endif
#ifdef GPROF
	cmn_err(CE_CONT, "GPROF enabled\n");
#endif

	/*
	 * Initialize ten-micro second timer so that drivers will
	 * not get short changed in their init phase. This was
	 * not getting called until clkinit which, on fast cpu's
	 * caused the drv_usecwait to be way too short.
	 */
	microfind();

	/*
	 * Read system file, (to set maxusers, lomempages.......)
	 *
	 * Variables that can be set by the system file shouldn't be
	 * used until after the following initialization!
	 */
	mod_read_system_file(boothowto & RB_ASKNAME);

	/*
	 * Read the GMT lag from /etc/rtc_config.
	 */
	gmt_lag = process_rtc_config_file();

	/*
	 * Calculate default settings of system parameters based upon
	 * maxusers, yet allow to be overridden via the /etc/system file.
	 */
	param_calc(maxusers);


	/*
	 * Initialize loadable module system, and apply the `set' commands
	 * gleaned from /etc/system.
	 */
	mod_setup();


	/*
	 * Setup MTRR registers in P6
	 */
	setup_mtrr();

	/*
	 * Initialize system parameters.
	 */
	param_init();

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	/* Allocate space for page directories */
	if ((kernel_only_pagedir =
		(caddr_t)kmem_zalloc(ptob(2), KM_NOSLEEP)) == NULL)
		prom_panic("Cannot allocate memory for page directories");

	kernel_only_cr3 = (u_int)(va_to_pfn((u_int)kernel_only_pagedir)
						<<MMU_STD_PAGESHIFT);
	CPU->cpu_pagedir = (pte_t *)(kernel_only_pagedir + MMU_STD_PAGESIZE);

	ASSERT((((u_int)CPU->cpu_pagedir) & PAGEOFFSET) == 0);

	/*
	 * For the purpose of setting up the kernel's page tables, which
	 * is done in hat_kern_setup(), we have to determine the size of
	 * the segkp segment, even though it hasn't been created yet.
	 * Later on seg_alloc() will creat the segkp segment starting at
	 * econtig.
	 */
	max_virt_segkp = btop(Sysbase - econtig);
	max_phys_segkp = (physmem * 2);
	/*
	 * We need to allocate segmap just after segkp
	 * segment. segmap has to be MAXBSIZE aligned. We also
	 * leave a hole between eecontig and Sysbase.
	 */
	max_virt_segkp -= (btop(segmaplen) + 4);

	segkp_len = ptob(min(max_virt_segkp, max_phys_segkp));
	eecontig = (caddr_t)((u_int)econtig + segkp_len);

	/* align accross MAXBSIZE boundary */
	segmapbase = ((u_int)eecontig & ~(MAXBSIZE - 1)) + MAXBSIZE;
	eecontig = (caddr_t)(segmapbase + segmaplen);


	/*
	 * Initialize the hat layer.
	 */
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	seg_init();

#if	NM_DEBUG
	add_nmintr(100, nmfunc1, "debug nmi 1", 99);
	add_nmintr(200, nmfunc1, "debug nmi 2", 199);
	add_nmintr(150, nmfunc1, "debug nmi 3", 149);
	add_nmintr(50, nmfunc1, "debug nmi 4", 49);
#endif

	if (modload("fs", "specfs") == -1)
		halt("Can't load specfs");

	if (modloadonly("misc", "swapgeneric") == -1)
		halt("Can't load swapgeneric");

	dispinit(NULL);

#if 0
	/*
	 * Initialize the instance number data base--this must be done
	 * after mod_setup and before the bootops are given up
	 */

	e_ddi_instance_init();
#endif
	setup_ddi();
	/*
	 * Make the in core copy of the prom tree - used for
	 * emulation of ieee 1275 boot environment
	 */
	prom_setup();

	get_font_ptrs();

	console = establish_console();
	if (ddi_load_driver("kd") == DDI_FAILURE)
		halt("Can't load KD");
	if (console == CONSOLE_IS_ASY)
		if (ddi_load_driver("asy") == DDI_FAILURE)
			halt("Can't load ASY");
	/*
	 * Lets take this opportunity to load the the root device.
	 */
	if (loadrootmodules() != 0)
		halt("Can't load the root filesystem");

	/*
	 * Load all platform specific modules
	 */
	psm_modload();

	/*
	 * Call back into boot and release boots resources.
	 */
	BOP_QUIESCE_IO(bootops);

	/*
	 * Copy physinstalled list into kernel space.
	 */
	phys_install = current;
	copy_memlist(bootops->boot_mem->physinstalled, &current);

	/*
	 * Virtual available next.
	 */
	virt_avail = current;
	copy_memlist(bootops->boot_mem->virtavail, &current);

	/*
	 * Copy in boot's page tables,
	 * set up extra page tables for the kernel,
	 * and switch to the kernel's context.
	 */
	hat_kern_setup();
	bcopy((caddr_t)CPU->cpu_pagedir, kernel_only_pagedir, MMU_STD_PAGESIZE);

	/*
	 * Initialize VM system, and map kernel address space.
	 */
	kvm_init();

	/*
	 * Map page 0 for drivers, such as kd, that need to pick up
	 * parameters left there by controllers/BIOS.
	 */
	p0_va = i86devmap(btop(0x0), 1, (PROT_READ));  /* 4K */

	/*
	 * Set up a map for translating kernel vitual addresses;
	 * used by dump and libkvm.
	 */
	setup_kvpm();

	/*
	 * Allocate a vm slot for the dev mem driver.
	 */
	i = rmalloc(kernelmap, (long)CLSIZE);
	mm_map = (caddr_t)kmxtob(i);

	i = rmalloc(kernelmap, DUMPPAGES);
	cur_dump_addr = dump_addr = kmxtob(i);

#ifdef XXX_NEEDED
	if ((ncache = (struct ncache *)kmem_zalloc(sizeof (struct ncache) *
	    ncsize, KM_NOSLEEP)) == NULL)
		prom_panic("Cannot allocate memory for ncache structs");
#endif
	/*
	 * If the following is true, someone has patched
	 * phsymem to be less than the number of pages that
	 * the system actually has.  Remove pages until system
	 * memory is limited to the requested amount.  Since we
	 * have allocated page structures for all pages, we
	 * correct the amount of memory we want to remove
	 * by the size of the memory used to hold page structures
	 * for the non-used pages.
	 */
	if (physmem < npages) {
		u_int diff;
		offset_t off;
		struct page *pp;
		caddr_t rand_vaddr;

		cmn_err(CE_WARN, "limiting physmem to %d pages", physmem);

		off = 0;
		diff = npages - physmem;
		diff -= mmu_btopr(diff * sizeof (struct machpage));
		while (diff--) {
			rand_vaddr = (caddr_t)(((u_int)&unused_pages_vp >> 7) ^
						((u_offset_t)off >> PAGESHIFT));
			pp = page_create_va(&unused_pages_vp, off, MMU_PAGESIZE,
				PG_WAIT | PG_EXCL, &kas, rand_vaddr);
			if (pp == NULL)
				cmn_err(CE_PANIC, "limited physmem too much!");
			page_io_unlock(pp);
			page_downgrade(pp);
			availrmem--;
			off += MMU_PAGESIZE;
		}
	}

	/*
	 * When printing memory, show the total as physmem less
	 * that stolen by a debugger.
	 * XXX - do we know how much memory kadb uses?
	 */
	dbug_mem = 0;	/* XXX */
	cmn_err(CE_CONT, "?mem = %dK (0x%x)\n",
	    (physinstalled - dbug_mem) << (PAGESHIFT - 10),
	    ptob(physinstalled - dbug_mem));

	/*
	 * unixsize doesn't include any modules, kmem_alloc'd memory,
	 * it is everything the kernel knows about that is not backed
	 * by page structures.
	 */
	unixsize = btoc((u_int)(four_kecontig - kernelbase));


	/*
	 * Clear allocated space, and make r/w entries
	 * for the space in the kernel map.
	 */
	if (unixsize >= physmem - 8 * (btoc(USIZE))) {
		printf("unixsize = %x   physmem = %x\n", unixsize, physmem);
		halt("no memory");
	}

	/*
	 * cmn_err doesn't do long long's and %u is treated
	 * just like %d, so we do this hack to get decimals
	 * > 2G printed.
	 */
	avmem = ctob((u_int)freemem);
	if (avmem >= (u_int)0x80000000)
		cmn_err(CE_CONT, "?avail mem = %d%d\n", avmem /
		    (1000 * 1000 * 1000), avmem % (1000 * 1000 * 1000));
	else
		cmn_err(CE_CONT, "?avail mem = %d\n", avmem);

	/*
	 * Initialize the segkp segment type.  We position it
	 * after the configured tables and buffers (whose end
	 * is given by econtig) and before Sysbase.
	 */

	va = econtig;

	rw_enter(&kas.a_lock, RW_WRITER);
	segkp = seg_alloc(&kas, va, segkp_len);
	if (segkp == NULL)
		cmn_err(CE_PANIC, "startup: cannot allocate segkp");
	if (segkp_create(segkp) != 0)
		cmn_err(CE_PANIC, "startup: segkp_create failed");
	rw_exit(&kas.a_lock);

	/*
	 * Now create generic mapping segment.  This mapping
	 * goes NCARGS beyond Syslimit up to DEBUGSTART.
	 * But if the total virtual address is greater than the
	 * amount of free memory that is available, then we trim
	 * back the segment size to that amount.
	 */
	va = (caddr_t)segmapbase;
	i = segmaplen;
	/*
	 * 1201049: segkmap base address must be MAXBSIZE aligned
	 */
	ASSERT(((u_int)va & MAXBOFFSET) == 0);

#ifdef XXX
	/*
	 * If there's a debugging ramdisk, we want to replace DEBUGSTART to
	 * the start of the ramdisk.
	 */
#endif XXX
	if (i > mmu_ptob(freemem))
		i = mmu_ptob(freemem);
	i &= MAXBMASK;	/* 1201049: segkmap size must be MAXBSIZE aligned */

	rw_enter(&kas.a_lock, RW_WRITER);
	segkmap = seg_alloc(&kas, va, i);
	if (segkmap == NULL)
		cmn_err(CE_PANIC, "cannot allocate segkmap");

	a.prot = PROT_READ | PROT_WRITE;
	a.shmsize = 0;
	a.nfreelist = 2;

	if (segmap_create(segkmap, (caddr_t)&a) != 0)
		panic("segmap_create segkmap");
	rw_exit(&kas.a_lock);

	if ((addr = rmalloc(kernelmap, (long)(2 * CLSIZE))) == NULL) {
		cmn_err(CE_PANIC,
		    "Couldn't rmalloc pages for cpu_caddr");
	}
	cpu[0]->cpu_caddr1 = Sysbase + mmu_ptob(addr);
	cpu[0]->cpu_caddr2 = cpu[0]->cpu_caddr1 + PAGESIZE;
	cpu[0]->cpu_caddr1pte =
		(u_int *)&Sysmap1[mmu_btop(cpu[0]->cpu_caddr1 - Sysbase)];
	cpu[0]->cpu_caddr2pte =
		(u_int *)&Sysmap1[mmu_btop(cpu[0]->cpu_caddr2 - Sysbase)];

	mutex_init(&cpu[0]->cpu_ppaddr_mutex, "ppcopy addr lock",
	    MUTEX_DEFAULT, DEFAULT_WT);


	/*
	 * DO NOT MOVE THIS BEFORE mod_setup() is called since
	 * _db_install() is in a loadable module that will be
	 * loaded on demand.
	 */
	{ extern int gdbon; if (gdbon) _db_install(); }

	/*
	 * Perform tasks that get done after most of the VM
	 * initialization has been done but before the clock
	 * and other devices get started.
	 */
	kern_setup1();

	/*
	 * Garbage collect any kmem-freed memory that really came from
	 * boot but was allocated before kvseg was initialized, and send
	 * it back into segkmem.
	 * First, set hme_valid bits to account for boot-allocated pages.
	 */
	{	register pte_t *ptep = kptes;
		register struct hment *hmep = kptetohme(ptep);
		while (ptep < keptes) {
			hmep++->hme_valid = pte_valid(ptep++);
		}
	}

	kmem_gc();

	/*
	 * Configure the system.
	 */
	configure();		/* set up devices */

	/*
	 * Set the isa_list string to the defined instruction sets we
	 * support. Default to i386
	 */

	setx86isalist();

	init_intr_threads(CPU);

	{

		psm_install();

		init_clock_thread();

		if (console == CONSOLE_IS_ASY) {
			if (((major = ddi_name_to_major("asy")) == -1) ||
			    (ddi_hold_installed_driver(major) == NULL))
				halt("Can't hold installed driver for ASY");
		} else {
			if (((major = ddi_name_to_major("kd")) == -1) ||
			    (ddi_hold_installed_driver(major) == NULL))
				halt("Can't hold installed driver for KD");
		}

		kadb_uses_kernel();

		(*picinitf)();
		sti();

		(void) add_avsoftintr((void *)NULL, 1, softlevel1,
			"softlevel1", NULL, NULL); /* XXX to be moved later */

		/*
		 * For AT386 machines we need an interrupt handler for
		 * processing FP exceptions. But, on 486 we set NE bit
		 * in cr0 to generate 'int 16' traps for FP exceptions
		 * (see fpu_probe()).
		 *
		 * Note: Interrupt level is hard coded to 7 here,
		 *	 change it if it should be something else.
		 */

		if (fpu_exists && ((cputype & CPU_ARCH) == I86_386_ARCH)) {
			(void) add_avintr((void *)NULL, 1, fpintr, "fpintr",
					13, NULL, NULL);
		}
	}
	do_forcefault = 1;

	/*
	 * BOOT PROTECT. We are really done with boot
	 * now - unlock its pages.
	 */
	while (lowpages_pp) {
		page_t *pp;
		pp = lowpages_pp;
		lowpages_pp = pp->p_next;
		pp->p_next = (struct page *)0;
		page_free(pp, 1);
	}

	/* Boot pages available - allocate any needed low phys pages */

	/*
	 * Get 1 page below 1 MB so that other processors can boot up.
	 */
	for (pfn = 1; pfn < btop(1*1024*1024); pfn++) {
		if (page_numtopp_alloc(pfn) != NULL) {
			rm_platter_va = i86devmap(pfn, 1, PROT_READ|PROT_WRITE);
			rm_platter_pa = ptob(pfn);
			break;
		}
	}
	if (pfn == btop(1*1024*1024)) {
		cmn_err(CE_WARN,
		    "No page available for starting up auxiliary processors\n");
	}

	/*
	 * Allocate contiguous, memory below 16 mb
	 * with corresponding data structures to control its use.
	 */
	lomem_init();
}


#define	TBUF	1024

void
setx86isalist(void)
{
	char *tp;
	char *rp;
	size_t len;
	extern char *isa_list;

	tp = kmem_alloc(TBUF, KM_SLEEP);

	*tp = '\0';

	switch (cputype & CPU_ARCH) {
	/* The order of these case statements is very important! */

	case I86_P5_ARCH:
		if (x86_feature & X86_CMOV) {
			/* PentiumPro */
			(void) strcat(tp, "pentium_pro");
			(void) strcat(tp, (x86_feature & X86_MMX) ?
				"+mmx pentium_pro " : " ");
		}
		/* fall through to plain Pentium */
		(void) strcat(tp, "pentium");
		(void) strcat(tp, (x86_feature & X86_MMX) ?
			"+mmx pentium " : " ");
		/* FALLTHROUGH */

	case I86_486_ARCH:
		(void) strcat(tp, "i486 ");

		/* FALLTHROUGH */
	case I86_386_ARCH:
	default:
		(void) strcat(tp, "i386");
	}

	/*
	 * Allocate right-sized buffer, copy temporary buf to it,
	 * and free temporary buffer.
	 */
	len = strlen(tp) + 1;   /* account for NULL at end of string */
	rp = kmem_alloc(len, KM_SLEEP);
	if (rp == NULL)
		return;
	isa_list = strcpy(rp, tp);
	kmem_free(tp, TBUF);

}

extern char hw_serial[];
char *_hs1107 = hw_serial;
ulong_t  _bdhs34;

void
post_startup(void)
{
	int sysinitid;

	if ((sysinitid = modload("misc", "sysinit")) != -1)
		(void) modunload(sysinitid);
	else
		cmn_err(CE_CONT, "sysinit load failed");

	/*
	 * Perform forceloading tasks for /etc/system.
	 */
	(void) mod_sysctl(SYS_FORCELOAD, NULL);
	/*
	 * ON4.0: Force /proc module in until clock interrupt handle fixed
	 * ON4.0: This must be fixed or restated in /etc/systems.
	 */
	(void) modload("fs", "procfs");

	/*
	 * Load the floating point emulator if necessary.
	 */
	if (fp_kind == FP_NO) {
		if (modload("misc", "emul_80387") == -1)
			cmn_err(CE_CONT, "No FP emulator found\n");
		cmn_err(CE_CONT, "FP hardware will %sbe emulated by software\n",
			fp_kind == FP_SW ? "" : "not ");
	}

	maxmem = freemem;

	(void) spl0();		/* allow interrupts */
}

/*
 * kphysm_init() initializes physical memory.
 * The old startup made some assumptions about the kernel living in
 * physically contiguous space which is no longer valid.
 */
static void
kphysm_init(machpage_t *pp, struct memseg *memsegp, u_int npages, u_int blks)
{
	struct memlist *pmem, *npmem;
	int	i, start_pfn, pfn, fourmb_pfn;
	int	end_ktextdata_4mbpfn;
	u_int	*pagedir, start_addr, four_mb_base;
	u_int	start_offset, newstart_addr;
	u_int	*kernelbase_pgtbl[MAX_KERNEL_4MBPAGES], *kernelbasepgtbl;
	extern	void swtch_to_4mbpage(), _start();
	void	(*fptr)();
	int	j, last_index;
	extern	int	x86_feature;
	int	glbpge = (x86_feature & X86_PGE) ? 1 : 0;
	extern	bcopy_nodebug();
	struct memseg *cur_memseg;
	struct memseg *tmp_memseg;
	struct memseg **prev_memsegp;
	u_long pnum;
	extern void page_coloring_init(void);

#ifdef lint
	blks = blks;
#endif /* lint */

	if (fourmb_base == 0)
		goto no4mb_pages;
	npmem = &glbpmem[0];
	if (fourmb_base & FOURMB_PAGEOFFSET) {
		npmem->address = fourmb_base;
		npmem->size = FOURMB_PAGESIZE -
			(fourmb_base & FOURMB_PAGEOFFSET);
		fourmb_base = npmem->address + npmem->size;
		fourmb_size -= npmem->size;
		if (insert_into_pmemlist(&phys_avail, npmem))
			npmem++;
	}
	npmem->address = fourmb_base + num4mb_pages * FOURMB_PAGESIZE;
	npmem->size = fourmb_size - (num4mb_pages * FOURMB_PAGESIZE);
	if (insert_into_pmemlist(&phys_avail, npmem))
		npmem++;
	fourmb_pfn = MMU_L1_INDEX(fourmb_base);

	/*
	 * copy Kernel text+data to a 4Mb page and put the currently used
	 * 4k pages on to the memlist.
	 */
	pagedir = (u_int *)cr3();
	start_addr = (u_int)_start;
	start_offset = (u_int)start_addr & (FOURMB_PAGESIZE -1);
	setcr3(cr3());
	/*
	 * setup virtual address mapping to the 4Mb page that would contain
	 * Kernel text+data
	 */
	for (i = 0; i < num4mb_pages; i++) {
		pagedir[MMU_L1_INDEX(KERNELSHADOW_VADDR) + i] =
		FOURMB_PDE((fourmb_pfn + i) * NPTEPERPT, 0, MMU_STD_SRWX, 1);
		kernelbase_pgtbl[i] = (u_int *)
			(pagedir[MMU_L1_INDEX(kernelbase) + i] &
				~(MMU_PAGESIZE - 1));
	}
	setcr3(cr3());

	/* Copy Kernel text+data to the new 4Mb page */
	for (newstart_addr = (u_int)((u_int)KERNELSHADOW_VADDR+start_offset);
		start_addr < (u_int)four_kecontig;
		start_addr += PAGESIZE, newstart_addr += PAGESIZE) {
		/*
		 * we need to make sure that the source address from which we
		 * are copying has a valid translation, since we could
		 * have 4K holes.
		 */
		kernelbasepgtbl = (u_int *)
			(pagedir[MMU_L1_INDEX(start_addr)]
				& ~(MMU_PAGESIZE - 1));
		if (kernelbasepgtbl[PAGETABLE_INDEX(start_addr)])
			bcopy_nodebug((caddr_t)start_addr,
				(caddr_t)newstart_addr,
				PAGESIZE);
	}

	/*
	 * swtch_to_4mbpage() is a function that is executed of
	 * KERNELSHADOW_VADDR. The function would copy the stack to the new
	 * 4mb page and change the page directory entry indexed at kernelbase
	 * to point to the new 4Mb page. When we return from the function
	 * we would be executing of the new 4Mb page
	 * swtch_to_4mbpage() should sit in the first 4MB starting at
	 * kernelbase
	 */
	fptr = (void (*)())(((u_int)swtch_to_4mbpage & (FOURMB_PAGESIZE - 1))
		+ KERNELSHADOW_VADDR);

	/* switch to the new physical page */
	((void (*)())fptr)(
		FOURMB_PDE(fourmb_pfn * NPTEPERPT, glbpge, MMU_STD_SRWX, 1),
		MMU_L1_INDEX(kernelbase), num4mb_pages);


	/* clear the pagedirectory entries we created above */
	for (i = 0; i < num4mb_pages; i++)
		pagedir[MMU_L1_INDEX(KERNELSHADOW_VADDR) + i] = 0;


	setcr3(cr3());
	start_addr = (u_int)_start;
	/*
	 * we can now free the 4K pages that was allocated by boot for  kernel
	 * text+data+stack.
	 */
	four_mb_base = kernelbase;
	start_addr = (u_int)_start;
	for (j = 0; j < num4mb_pages; j++) {

		/* For each 4Mb page */

		kernelbasepgtbl = kernelbase_pgtbl[j];

		/* pick the last entry in the pagetable that we need to scan */
		if (IN_SAME_4MB_PAGE(start_addr, (u_int)four_kecontig))
			last_index = PAGETABLE_INDEX((u_int)four_kecontig);
		else last_index = NPTEPERPT;


		start_pfn = pfn = PTE_PFN(kernelbasepgtbl[
					PAGETABLE_INDEX(start_addr)]);
		for (i = PAGETABLE_INDEX(start_addr); i < last_index - 1; ) {

			if (kernelbasepgtbl[i] == 0) {
				/*
				 * we have a hole here. We need to do two
				 * things. One we need to skip this and we
				 * need to free the equivalent 4K page in the
				 * 4MB page to the freelist
				 */
				npmem->address = (i * MMU_PAGESIZE) +
					((fourmb_pfn + j) * FOURMB_PAGESIZE);
				npmem->size = 0;
				while (i < last_index &&
					(kernelbasepgtbl[i] == 0)) {
					i++;
					npmem->size += MMU_PAGESIZE;
				}
				if (!GLBPMEM_OVERFLOW(npmem)) {
					if (insert_into_pmemlist(&phys_avail,
						npmem))
						npmem++;
				} else fourmb_support_warning(npmem);
			} else if (PTE_PFN(kernelbasepgtbl[++i]) == pfn + 1) {
				pfn++;
				continue;
			} else {
				/*
				 * we found a chunk, insert this in
				 * to phys_avail list
				 */
				npmem->address = start_pfn * PAGESIZE;
				npmem->size = (pfn - start_pfn + 1) * PAGESIZE;
				if (!GLBPMEM_OVERFLOW(npmem)) {
					if (insert_into_pmemlist(&phys_avail,
						npmem))
						npmem++;
				} else fourmb_support_warning(npmem);
			}
			if (i == last_index)
				break;
			start_pfn = pfn = PTE_PFN(kernelbasepgtbl[i]);
		}
		npmem->address = start_pfn * PAGESIZE;
		npmem->size = (pfn - start_pfn + 1)  * PAGESIZE;
		if (!GLBPMEM_OVERFLOW(npmem)) {
			if (insert_into_pmemlist(&phys_avail, npmem))
				npmem++;
		} else fourmb_support_warning(npmem);
		four_mb_base += FOURMB_PAGESIZE;
		start_addr = (u_int)four_mb_base;
	}

	end_ktextdata_4mbpfn = (fourmb_pfn + num4mb_pages - 1);

	/*
	 * The unused portion in the 4Mb page(holding kernel text+data)
	 * can be inserted in to phys_avail list
	 */
	npmem->address = (PAGETABLE_INDEX((u_int)four_kecontig) +
				end_ktextdata_4mbpfn * 1024 + 1) * PAGESIZE;
	npmem->size = (NPTEPERPT -  PAGETABLE_INDEX((u_int)four_kecontig) - 1) *
				PAGESIZE;
	if (!GLBPMEM_OVERFLOW(npmem)) {
		if (insert_into_pmemlist(&phys_avail, npmem))
			npmem++;
	} else fourmb_support_warning(npmem);

	setcr3(cr3());



	/*
	 * Hopefully things look fine now, We should have switched to a
	 * 4Mb page for kernel text+data and filled in page directory entries
	 * for segkmem.
	 */
no4mb_pages:
	ASSERT(page_hash != NULL && page_hashsz != 0);

	page_coloring_init();

	cur_memseg = memsegp;
	for (pmem = phys_avail; pmem && npages;
	    pmem = pmem->next, cur_memseg++) {
		u_long base;
		u_int num;

		/*
		 * Build the memsegs entry
		 */
		num = btop(pmem->size);
		if (num > npages)
			num = npages;
		npages -= num;
		base = btop(pmem->address);

		cur_memseg->pages = pp;
		cur_memseg->epages = pp + num;
		cur_memseg->pages_base = base;
		cur_memseg->pages_end = base + num;

		/* insert in memseg list, decreasing number of pages order */
		for (prev_memsegp = &memsegs, tmp_memseg = memsegs;
		    tmp_memseg;
		    prev_memsegp = &(tmp_memseg->next),
		    tmp_memseg = tmp_memseg->next) {
			if (num > tmp_memseg->pages_end -
			    tmp_memseg->pages_base)
				break;
		}
		cur_memseg->next = *prev_memsegp;
		*prev_memsegp = cur_memseg;

		/*
		 * Initialize the PSM part of the page struct
		 */
		pnum = cur_memseg->pages_base;
		for (pp = cur_memseg->pages; pp < cur_memseg->epages; pp++) {
			pp->p_pagenum = pnum;
			cv_init(&(pp->p_mlistcv), "page mlist",
			    CV_DEFAULT, NULL);
			pnum++;
		}

		/*
		 * have the PIM initialize things for this
		 * chunk of physical memory.
		 */
		add_physmem((page_t *)cur_memseg->pages, num);
	}

	build_pfn_hash();
}

/*
 * Kernel VM initialization.
 */
static void
kvm_init(void)
{
	register caddr_t va;
	u_int range_size, range_base, range_end;
	struct memlist *cur, *prev;
	extern void _start();

	/*
	 * Put the kernel segments in kernel address space.  Make it a
	 * "kernel memory" segment objects.
	 */
	rw_enter(&kas.a_lock, RW_WRITER);

	(void) seg_attach(&kas, (caddr_t)kernelbase,
	    (u_int)(e_data - kernelbase), &ktextseg);
	(void) segkmem_create(&ktextseg, (caddr_t)NULL);

	(void) seg_attach(&kas, (caddr_t)valloc_base, (u_int)four_kecontig -
		(u_int)valloc_base, &kvalloc);
	(void) segkmem_create(&kvalloc, (caddr_t)NULL);

	/*
	 * We're about to map out /boot.  This is the beginning of the
	 * system resource management transition. We can no longer
	 * call into /boot for I/O or memory allocations.
	 */
	(void) seg_attach(&kas, (caddr_t)SYSBASE,
	    (u_int)(phys_syslimit - SYSBASE), &kvseg);
	(void) segkmem_create(&kvseg, (caddr_t)NULL);

	rw_exit(&kas.a_lock);

	rw_enter(&kas.a_lock, RW_READER);

	/*
	 * Now we can ask segkmem for memory instead of boot.
	 */
	segkmem_ready = 1;

	/*
	 * All level 1 entries other than the kernel were set invalid
	 * when our prototype level 1 table was created.  Thus, we only need
	 * to deal with addresses above kernelbase here.  Also, all ptes
	 * for this region have been allocated and locked, or they are not
	 * used.  Thus, all we need to do is set protections.  Invalid until
	 * start.
	 */
	ASSERT((((u_int)_start) & PAGEOFFSET) == 0);
	for (va = (caddr_t)kernelbase; va < (caddr_t)_start; va += PAGESIZE) {
		/* user copy red zone */
		(void) as_setprot(&kas, va, PAGESIZE, 0);
	}

	/*
	 * (Normally) Read-only until end of text.
	 */
	(void) as_setprot(&kas, va, (u_int)(e_text - va), (u_int)
	    (PROT_READ | PROT_EXEC | ((kernprot == 0) ? PROT_WRITE : 0)));
	va = (caddr_t)roundup((u_int)e_text, PAGESIZE);

	/*
	 * Invalidate pages between end of text and the start of data.
	 */
	(void) as_setprot(&kas, va, s_data - va, 0);

	va = s_data;
	/*
	 * Writable until end.
	 */
	(void) as_setprot(&kas, va, (u_int)(e_data - va),
	    PROT_READ | PROT_WRITE | PROT_EXEC);
	va = (caddr_t)roundup((u_int)e_data, PAGESIZE);

	/*
	 * Validate the valloc'ed structures.
	 */
	(void) as_setprot(&kas, va, (u_int)(four_kecontig - va),
	    PROT_READ | PROT_WRITE | PROT_EXEC);

	/*
	 * Validate to Syslimit.  Memory allocations done early on
	 * by boot are in this region.
	 */
	for (cur = virt_avail; cur->address < (u_int)startup_alloc_vaddr;
		cur = cur->next)
		prev = cur;

	range_base = prev->address + prev->size;
	range_size = cur->address - range_base;

	(void) as_setprot(&kas, (caddr_t)range_base, range_size,
	    PROT_READ | PROT_WRITE | PROT_EXEC);

	range_end = range_base + range_size;
	va = (caddr_t)roundup(range_end, PAGESIZE);

	/*
	 * Invalidate unused portion of the region managed by kernelmap.
	 */
	(void) as_setprot(&kas, va, phys_syslimit - (u_int)va, 0);

	rw_exit(&kas.a_lock);

	/*
	 * Flush the PDC of any old mappings.
	 */
	mmu_tlbflush_all();

}

/*
 * crash dump, libkvm support - see sys/kvtopdata.h for details
 */
struct kvtopdata kvtopdata;

static void
setup_kvpm(void)
{
	u_int va, pfn;
	int i = 0;
	u_int pages = 0;
	int lastpfnum = -1;

	for (va = (u_int)kernelbase; va < (u_int)four_kecontig;
		va += PAGESIZE) {
		pfn = hat_getpfnum(kas.a_hat, (caddr_t)va);
		if (pfn != (u_int)HAT_INVLDPFNUM) {
			if (lastpfnum == -1) {
				lastpfnum = pfn;
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
				pages = 1;
			} else if (lastpfnum+1 == pfn) {
				lastpfnum = pfn;
				pages++;
			} else {
				kvtopdata.kvtopmap[i].kvpm_len = pages;
				lastpfnum = pfn;
				pages = 1;
				if (++i > NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					break;
				}
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
			}
		} else if (lastpfnum != -1) {
			lastpfnum = -1;
			kvtopdata.kvtopmap[i].kvpm_len = pages;
			pages = 0;
			if (++i > NKVTOPENTS) {
				cmn_err(CE_WARN, "out of kvtopents");
				break;
			}
		}
	}
	/*
	 * Pages allocated early from sysmap region that don't
	 * have page structures and need to be entered in to the
	 * kvtop array for libkvm.  The rule for memory pages is:
	 * it is either covered by a page structure or included in
	 * kvtopdata.
	 */
	for (va = (u_int)Sysbase; va < (u_int)startup_alloc_vaddr;
	    va += PAGESIZE) {
		pfn = hat_getpfnum(kas.a_hat, (caddr_t)va);
		if (pfn != (u_int)HAT_INVLDPFNUM) {
			if (lastpfnum == -1) {
				lastpfnum = pfn;
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
				pages = 1;
			} else if (lastpfnum+1 == pfn) {
				lastpfnum = pfn;
				pages++;
			} else {
				kvtopdata.kvtopmap[i].kvpm_len = pages;
				lastpfnum = pfn;
				pages = 1;
				if (++i > NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					break;
				}
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
			}
		} else if (lastpfnum != -1) {
			lastpfnum = -1;
			kvtopdata.kvtopmap[i].kvpm_len = pages;
			pages = 0;
			if (++i > NKVTOPENTS) {
				cmn_err(CE_WARN, "out of kvtopents");
				break;
			}
		}
	}

	if (pages)
		kvtopdata.kvtopmap[i].kvpm_len = pages;
	else
		i--;

	kvtopdata.hdr.version = KVTOPD_VER;
	kvtopdata.hdr.nentries = i + 1;
	kvtopdata.hdr.pagesize = MMU_PAGESIZE;


#ifdef XXX
	pfn = hat_getpfnum(kas.a_hat, (caddr_t)&kvtopdata);
/* msg_map?  msgbuf has been redefined. */
	msgbuf.msg_map = (tpte->PhysicalPageNumber << MMU_PAGESHIFT) |
		((u_int)(&kvtopdata) & MMU_PAGEOFFSET);
#endif XXX
}

static int
establish_console(void)
{
	/*
	 * XXXX HACK
	 * This must return CONSOLE_IS_FB or CONSOLE_IS_ASY
	 */
	return (console);
}

int
insert_into_pmemlist(head, cur)
struct 	memlist **head;
struct	memlist	*cur;
{
	struct memlist *tmemlist, *ptmemlist;

	if (*head == (struct memlist *)0) {
		*head = cur;
		cur->next = (struct memlist *)0;
	} else {
		tmemlist = *head;
		ptmemlist = (struct memlist *)0;
		while (tmemlist) {
			if (cur->address < tmemlist->address) {
				if (((u_int)cur->address + cur->size) ==
				    tmemlist->address) {
					tmemlist->address = cur->address;
					tmemlist->size += cur->size;
					return (0);
				} else break;
			} else if (cur->address == ((u_int)tmemlist->address +
				tmemlist->size)) {
				tmemlist->size += cur->size;
				return (0);
			}
			ptmemlist = tmemlist;
			tmemlist = tmemlist->next;
		}
		if (tmemlist == (struct memlist *)0) {
			/* get to the tail of the list */
			ptmemlist->next = cur;
			cur->next = (struct memlist *)0;
		} else if (ptmemlist == (struct memlist *)0) {
			/* get to the head of the list */
			cur->next = *head;
			*head = cur;
		} else {
			/* insert in between */
			cur->next = ptmemlist->next;
			ptmemlist->next = cur;
		}
	}
	return (1);
}

static void
fourmb_support_warning(npmem)
struct memlist *npmem;
{
	cmn_err(CE_WARN, "kphysm_init: dropping memory size %X, starting %X\n",
		npmem->size, npmem->address);
}


/*
 * These are MTTR registers supported by P6
 */
struct	mtrrvar	mtrrphys_arr[MAX_MTRRVAR];
u_int	mtrr64k[2], mtrr16k1[2], mtrr16k2[2];
u_int	mtrr4k1[2], mtrr4k2[2], mtrr4k3[2];
u_int	mtrr4k4[2], mtrr4k5[2], mtrr4k6[2];
u_int	mtrr4k7[2], mtrr4k8[2], mtrrcap[2];
u_int	mtrrdef[2];

u_int	mtrr_size1, mtrr_base1, mtrr_type1;
u_int	mtrr_size2, mtrr_base2, mtrr_type2;

void
setup_mtrr()
{
	int i, ecx;
	int empty_slot, vcnt;
	struct	mtrrvar	*mtrrphys;
	u_int	*addr;
	int	mtrr_fix();

	if (!(x86_feature & X86_MTRR))
		return;

	rdmsr(REG_MTRRCAP, mtrrcap);
	rdmsr(REG_MTRRDEF, mtrrdef);
	if (mtrrcap[0] & MTRRCAP_FIX) {
		rdmsr(REG_MTRR64K, mtrr64k);
		rdmsr(REG_MTRR16K1, mtrr16k1);
		rdmsr(REG_MTRR16K2, mtrr16k2);
		rdmsr(REG_MTRR4K1, mtrr4k1);
		rdmsr(REG_MTRR4K2, mtrr4k2);
		rdmsr(REG_MTRR4K3, mtrr4k3);
		rdmsr(REG_MTRR4K4, mtrr4k4);
		rdmsr(REG_MTRR4K5, mtrr4k5);
		rdmsr(REG_MTRR4K6, mtrr4k6);
		rdmsr(REG_MTRR4K7, mtrr4k7);
		rdmsr(REG_MTRR4K8, mtrr4k8);
	}
	if ((vcnt = (mtrrcap[0] & MTRRCAP_VCNTMASK)) > MAX_MTRRVAR)
		vcnt = MAX_MTRRVAR;

	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		rdmsr(ecx, &mtrrphys->mtrrphys_base);
		rdmsr(ecx + 1, &mtrrphys->mtrrphys_mask);

	}

	if (!mtrr_size1 && !mtrr_size2)
		return;
	else if (!mtrr_size1) {
		mtrr_size1 = mtrr_size2;
		mtrr_base1 = mtrr_base2;
		mtrr_type1 = mtrr_type2;
		mtrr_size2 = 0;
	}

	empty_slot = 0;
	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		if (!(mtrrphys->mtrrphys_mask & MTRRPHYSMASK_V)) {
			empty_slot = 1;
			break;
		}
	}
	if (!empty_slot) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, no free slot\n");
		return;
	}
	if ((mtrr_base1 & PAGEOFFSET) || (mtrr_size1 & PAGEOFFSET)) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, Unaligned address");
		return;
	}
	addr = &mtrrphys->mtrrphys_base;
	MTRR_SETVBASE(addr, mtrr_base1, mtrr_type1);
	addr = &mtrrphys->mtrrphys_mask;
	MTRR_SETVMASK(addr, mtrr_size1, 1);

	while (mtrr_fix(vcnt));

	if (!mtrr_size2)
		goto load_mtrr;

	empty_slot = 0;
	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		if (!(mtrrphys->mtrrphys_mask & MTRRPHYSMASK_V)) {
			empty_slot = 1;
			break;
		}
	}
	if (!empty_slot) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, no free slot\n");
		return;
	}
	if ((mtrr_base2 & PAGEOFFSET) || (mtrr_size2 & PAGEOFFSET)) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, Unaligned address");
		return;
	}
	addr = &mtrrphys->mtrrphys_base;
	MTRR_SETVBASE(addr, mtrr_base2, mtrr_type2);
	addr = &mtrrphys->mtrrphys_mask;
	MTRR_SETVMASK(addr, mtrr_size2, 1);

	while (mtrr_fix(vcnt));
load_mtrr:

	mtrr_sync();
}


void
mtrr_warning(struct mtrrvar *mtrrphys, int otype)
{
	u_int	base, size;
	int	ntype;

	base = MTRR_GETVBASE(mtrrphys->mtrrphys_base);
	ntype = MTRR_GETVTYPE(mtrrphys->mtrrphys_base);
	size = MTRR_GETVSIZE(mtrrphys->mtrrphys_mask);


	cmn_err(CE_WARN, "MTRR for range %X to %X changing from %d to %d\n",
		base, base + size, otype, ntype);
}
int
mtrr_fix(int vcnt)
{
	int i, j,  ecx, type, ttype;
	int size, tsize, tecx;
	struct	mtrrvar	*mt, *tmt;
	u_int	base, tbase, *addr;


	for (i = 0, ecx = REG_MTRRPHYSBASE0, mt = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mt++) {
		if (!(mt->mtrrphys_mask & MTRRPHYSMASK_V))
			continue;
		base = MTRR_GETVBASE(mt->mtrrphys_base);
		size = MTRR_GETVSIZE(mt->mtrrphys_mask);
		type = MTRR_GETVTYPE(mt->mtrrphys_base);
		for (j = 0, tecx = REG_MTRRPHYSBASE0, tmt = mtrrphys_arr;
			j <  vcnt - 1; j++, tecx += 2, tmt++) {

			if (mt == tmt)
				continue;
			else if (!(tmt->mtrrphys_mask & MTRRPHYSMASK_V))
				continue;
			tbase = MTRR_GETVBASE(tmt->mtrrphys_base);
			tsize = MTRR_GETVSIZE(tmt->mtrrphys_mask);
			ttype = MTRR_GETVTYPE(tmt->mtrrphys_base);

			if ((base < tbase && base + size < tbase) ||
				base > tbase + tsize)
				continue;
			if (base > tbase) {
				if (base + size < tbase + tsize) {
					if (type < ttype) {
						MTRR_SETTYPE(
						tmt->mtrrphys_base, type);
						mtrr_warning(tmt, ttype);
					}
					MTRR_SETVINVALID(mt->mtrrphys_mask);
					return (1);
				}
				tsize -= (base - tbase);
				addr = &tmt->mtrrphys_mask;
				MTRR_SETVMASK(addr, tsize, 1);
				if (ttype < type) {
					MTRR_SETTYPE(mt->mtrrphys_base, ttype);
					mtrr_warning(tmt, type);
				}
				return (1);
			} else if (base + size > tbase + tsize) {
				if (ttype < type) {
					MTRR_SETTYPE(mt->mtrrphys_base, ttype);
					mtrr_warning(mt, type);
				}
				MTRR_SETVINVALID(tmt->mtrrphys_mask);
				return (1);
			} else {
				tsize -= (base + size - tbase);
				tbase = base + size;
				addr = &tmt->mtrrphys_base;
				MTRR_SETVBASE(addr, tbase, ttype);
				addr = &tmt->mtrrphys_mask;
				MTRR_SETVMASK(addr, tsize, 1);
				if (ttype < type) {
					MTRR_SETTYPE(mt->mtrrphys_base, ttype);
					mtrr_warning(mt, type);
				}
				return (1);
			}
		}
	}
	return (0);
}
/*
 * Sync current cpu mtrr with the incore copy of mtrr.
 * This function hat to be invoked with interrupts disabled
 * Currently we do not capture other cpu's. This is invoked on cpu0
 * just after reading /etc/system.
 * On other cpu's its invoked from mp_startup().
 */
void
mtrr_sync()
{
	u_int	my_mtrrdef[2];
	u_int	crvalue, cr0_orig;
	extern	invalidate_cache();
	int	vcnt, i, ecx;
	struct	mtrrvar	*mtrrphys;

	cr0_orig = crvalue = cr0();
	crvalue |= CR0_CD;
	crvalue &= ~CR0_NW;
	setcr0(crvalue);
	invalidate_cache();
	setcr3(cr3());

	rdmsr(REG_MTRRDEF, my_mtrrdef);
	my_mtrrdef[0] &= ~MTRRDEF_E;
	wrmsr(REG_MTRRDEF, my_mtrrdef);
	if (mtrrcap[0] & MTRRCAP_FIX) {
		wrmsr(REG_MTRR64K, mtrr64k);
		wrmsr(REG_MTRR16K1, mtrr16k1);
		wrmsr(REG_MTRR16K2, mtrr16k2);
		wrmsr(REG_MTRR4K1, mtrr4k1);
		wrmsr(REG_MTRR4K2, mtrr4k2);
		wrmsr(REG_MTRR4K3, mtrr4k3);
		wrmsr(REG_MTRR4K4, mtrr4k4);
		wrmsr(REG_MTRR4K5, mtrr4k5);
		wrmsr(REG_MTRR4K6, mtrr4k6);
		wrmsr(REG_MTRR4K7, mtrr4k7);
		wrmsr(REG_MTRR4K8, mtrr4k8);
	}
	if ((vcnt = (mtrrcap[0] & MTRRCAP_VCNTMASK)) > MAX_MTRRVAR)
		vcnt = MAX_MTRRVAR;
	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		wrmsr(ecx, &mtrrphys->mtrrphys_base);
		wrmsr(ecx + 1, &mtrrphys->mtrrphys_mask);
	}
	wrmsr(REG_MTRRDEF, mtrrdef);
	setcr3(cr3());
	invalidate_cache();
	setcr0(cr0_orig);
}
