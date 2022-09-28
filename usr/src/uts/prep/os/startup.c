/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)startup.c	1.75	96/07/28 SMI"

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
#include <sys/msgbuf.h>

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
#include <sys/prom_plat.h>
#include <sys/modctl.h>		/* for "procfs" hack */
#include <sys/kvtopdata.h>

#include <sys/consdev.h>
#include <sys/console.h>

#define	SUNDDI_IMPL		/* so sunddi.h will not redefine splx() et al */
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
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
#include <vm/hat_ppcmmu.h>
#include <sys/instance.h>
#include <sys/smp_impldefs.h>

void	debug_enter(char *);
void	lomem_init(void);

extern	void	param_init();
extern	void	param_calc(int);
extern	void	release_bootstrap();

static void serial_number_init(void);
static void kvm_init(void);
static int establish_console(void);

int console = CONSOLE_IS_FB;

/*
 * Declare these as initialized data so we can patch them.
 */
int physmem = 0;	/* memory size in pages, patch if you want less */
int kernprot = 1;	/* write protect kernel text */

/*
 * Currently, must statically allocate CPU, and thread structs for all CPUs.
 */
struct cpu	cpus[NCPU];			/* CPU data */
struct cpu	*cpu[NCPU] = {&cpus[0]};	/* pointers to all CPUs */

/* Global variables for MP support. Used in mp_startup */
caddr_t	rm_platter_va;
paddr_t	rm_platter_pa;

/*
 * External Data.
 */
extern int maxusers;
extern int kmacctboot;
extern int kmacctflag;

caddr_t econtig;		/* end of first block of contiguous kernel */
caddr_t eecontig;		/* end of segkp, which is after econtig */
caddr_t page_table_virtual = (caddr_t)PAGE_TABLE; /* page table address */

struct bootops *bootops = 0;	/* passed in from boot */

extern lksblk_t *lksblks_head;

/*
 * new memory fragmentations are possible in startup() due to BOP_ALLOCs. this
 * depends on number of BOP_ALLOC calls made and requested size, memory size `
 * combination.
 */
#define	POSS_NEW_FRAGMENTS	10

/*
 * VM data structures
 */
int page_hashsz;		/* Size of page hash table (power of two) */
struct machpage *pp_base;	/* Base of system page struct array */
struct page **page_hash;	/* Page hash table */
struct seg *segkmap;		/* Kernel generic mapping segment */
struct seg ktextseg;		/* Segment used for kernel executable image */
struct seg kvalloc;		/* Segment used for "valloc" mapping */
struct seg *segkp;		/* Segment for pageable kernel virt. memory */

struct memseg *memseg_base;
struct vnode unused_pages_vp;
spte_t Sysmap[SYSPTSIZE]; /* array of sptes mapping SYSBASE to SYSLIMIT */

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
struct  memlist *memlist;
caddr_t startup_alloc_vaddr = (caddr_t)SYSBASE + 3*MMU_PAGESIZE;
u_int startup_alloc_size = sizeof (msgbuf);
u_int	startup_alloc_chunk_size = 20 * MMU_PAGESIZE;	/* patchable */
extern caddr_t  dump_addr, cur_dump_addr; /* for dumping purposes */

caddr_t s_text;		/* start of kernel text segment */
caddr_t e_text;		/* end of kernel text segment */
caddr_t s_data;		/* start of kernel data segment */
caddr_t e_data;		/* end of kernel data segment */

struct memlist *phys_install;	/* Total installed physical memory */
struct memlist *phys_avail;	/* Available (unreserved) physical memory */
struct memlist *virt_avail;	/* Available (unmapped?) virtual memory */
u_int  pmeminstall;		/* total physical memory installed */
struct map *kobjmap;		/* map to manage kobj segment space */

static void kphysm_init();

extern int segkmem_ready;

/*
 *		  Physical memory layout.
 *
 * availmem 	+-----------------------+
 *		:			:
 *		:	page pool	:
 *		:			:
 *		|-----------------------|
 *		|	Page Table	|
 *		| (aligned to its size)	|
 * PAGETABLE 	|-----------------------|
 *		|			|
 *		|	page pool	|
 *		|			|
 * 		|-----------------------|
 *		:			:
 *		|   kadb, unix, etc. 	|
 *		:			:
 * 		|-----------------------|
 *		|    Ramdisk (temp)	|	(8-16 M)
 * 0x00100000	|-----------------------|
 *		|    boot code/data/etc.|	(~1M)
 * 0x00009000	|-----------------------|
 *		|    boot stack		|	(8 K)
 * 0x00007000	|-----------------------|
 *		|    msgbuf		|	(8 K)
 * 0x00005000	|-----------------------|
 *		|    rtt, fast syscall	|	(4 K)
 * 0x00004000	|-----------------------|
 *		|	Level-0		|	(16 K)
 *		| Interrupt handlers	|
 * 0x00000000	+-----------------------+
 *
 *
 *		  Virtual memory layout.
 *
 *		/-----------------------\
 *		|	INVALID		|
 * 0xFFF00000	|-----------------------|
 *		|	debugger	|			(1 M)
 * 0xFFE00000	|-----------------------|- DEBUGSTART
 *		:			:
 *		|	Ramdisk		|			(15 M)
 *		|  (debugging only)	|
 *		:			:
 * 0xFEF00000  -|-----------------------|- RAMDISK
 *		:			:
 *		|	unused		|
 *		:			:
 * 		|-----------------------|
 *		|    exec args area	|  NCARGS		(1 M)
 * 0xF3800000  -|-----------------------|- ARGSBASE
 *		:			:
 *		|  PCI/ISA I/O SPACE	|			(8 M)
 *		:   (non cacheable)	:
 * 0xF3000000  -|-----------------------|- PCIISA_VBASE
 *		|	segkmap		|  SEGMAPSIZE		(16 M)
 * 0xF2000000  -|-----------------------|
 *		|			|
 *		:	Page Table	:  MAXPTSIZE		(32 M)
 *		| (may be mapped by BAT)|
 * 0xF0000000  -|-----------------------|- PAGE_TABLE
 *		:			:
 *		|	unused		|
 *		:			:
 * 0xEF000000  -|-----------------------|- SYSLIMIT
 *		:			:
 *		|	Sysmap		| SYSPTSIZE		(128 M)
 *		:			:
 *		|			|
 * 0xE7003000  -|-----------------------|
 *		|	 msgbuf		|			(8 K)
 * 0xE7001000  -|-----------------------|- V_MSGBUF_ADDR
 *		|     not used (4k)	|
 * 0xE7000000	|-----------------------|- SYSBASE
 *		:			:
 *		|	unused		|
 *		:			:
 * 		|-----------------------|- eecontig
 *		|	 segkp		|
 *		|-----------------------|- econtig
 *		|    vm structures	|
 *		| (page structures	|
 *		|  page hash		|
 *		|  memseg structures)	|
 *		|-----------------------|- e_data
 *		|	kernel data	|
 *		| (data sections for	|
 *		|  genunix, unix, rtld)	|
 * 0xE2000000  -|-----------------------|- s_data = (KERNELBASE + 32MB)
 *		| text sections of	|
 *		| kernel dynamic	|
 *		| modules (managed by	|
 *		| lokmem_alloc())	|
 *		|-----------------------|- e_text
 *		|	kernel text	|
 *		| (text sections for	|
 *		|  genunix, unix, rtld)	|
 * 0xE0001000  -|-----------------------|- s_text
 *		|  user copy red zone	|			(4K)
 *		|	(invalid)	|
 * 0xE0000000  -|-----------------------|- KERNELBASE
 *		|	user stack	|
 *		:			:
 *		|   dynamic segments	|
 *		:			:
 *		|	user data	|
 *		|-----------------------|- (user data aligned for 64K)
 *		|	user text	|
 * 0x02000000	|-----------------------|
 *		:			:
 *		|   dynamic segments	|
 *		:			:
 * 0x00010000  -|-----------------------|
 *		|	not used	|
 * 0x00000000  _|_______________________|
 */

extern char t0stack;
extern struct _kthread t0;
struct _klwp lwp0;
struct proc p0;

void init_intr_threads(struct cpu *);
void init_clock_thread();
void init_panic_thread();
extern u_int softlevel1();

void ppcmmu_param_init();

/*
 * NEEDSWORK:  This should really be handled internally to the "kd"
 * driver, but the keyboard device node is not currently visible to the
 * kernel so we do it here where we can call the prom directly.
 */
int PReP_kb_layout;

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

/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	register unsigned int i;
	u_int npages;
	struct segmap_crargs a;
	int memblocks;
	struct memlist *bootlistp, *previous, *current;
	u_int pp_giveback;
	caddr_t va;
	caddr_t memspace;
	u_int memspace_sz;
	u_int nppstr;
	u_int memseg_sz;
	u_int msgbuf_sz;
	u_int msgbuf_paddr;
	u_int pagehash_sz;
	u_int memlist_sz;
	u_int kernelmap_sz;
	u_int pp_sz;			/* Size in bytes of page struct array */
	int dbug_mem;
	u_int avmem;
	int	max_virt_segkp;
	int	max_phys_segkp;
	int	segkp_len;

	static void setup_kvpm();
	extern void mt_lock_init();
	extern caddr_t mm_map;
	extern void hat_kern_setup(void);
	extern void _db_install(void);
	static int PReP_get_kb_layout(void);
	extern void ppcmmu_takeover();
	extern void copy_handlers();

	dki_lock_setup(lksblks_head);

	/*
	 * Initialize the turnstile allocation mechanism.
	 */
	tstile_init();

	/*
	 * Map in msgbuf
	 */
	msgbuf_sz = 2 * MMU_PAGESIZE;
	if (BOP_GETPROP(bootops, "msgbuf-paddr", &msgbuf_paddr) < 0) {
		panic("Unable to retrieve msgbuf-paddr boot property");
	}
	memspace = prom_map((caddr_t)&msgbuf, 0, msgbuf_paddr, msgbuf_sz);
	if (memspace != (caddr_t)&msgbuf)
		panic("msgbuf alloc failure");

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

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
	 * them because they will not be managed by the vm system.
	 */
	/* XXXPPC Do we want to align it for BAT use? */
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
	kernelmap_sz = KERNELMAP_SZ(8);
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

	mapinit(kernelmap, (long)(SYSPTSIZE - 1), (u_long)1,
		"kernel map", kernelmap_sz/sizeof (struct map));

	/*
	 * Remove the space used by BOP_ALLOC from the kernelmap.
	 */
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
	 * represent the actual state of memory with boot and its
	 * resources deducted.  kvm_init will use these lists to
	 * initialize the vm system.
	 */
	bootlistp = bootops->boot_mem->physavail;
	for (; bootlistp; bootlistp = (struct memlist *)bootlistp->next) {
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
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Initialize low kernel memory allocator to manage the portion of
	 * kernel text segment (i.e e_text to s_data).
	 */
	lokmem_init(e_text, (int)(s_data - e_text));

	/*
	 * Initialize the kstat framework.
	 */
	kstat_init();

	/*
	 * Lets display the banner early so the user has some idea that
	 * UNIX is taking over the system.
	 */
	cmn_err(CE_CONT,
	    "\rSunOS Release %s Version %s [UNIX(R) System V Release 4.0]\n",
	    utsname.release, utsname.version);
	cmn_err(CE_CONT, "Copyright (c) 1983-1996, Sun Microsystems, Inc.\n");
#ifdef DEBUG
	cmn_err(CE_CONT, "DEBUG enabled\n");
#endif
#ifdef	TRACE
	cmn_err(CE_CONT, "TRACE enabled\n");
#endif
#ifdef	GPROF
	cmn_err(CE_CONT, "GPROF enabled\n");
#endif

	/*
	 * Attempt to retrieve hardware serial number from Open Firmware.
	 */
	serial_number_init();

	/*
	 * Read system file, (e.g., to set maxusers)
	 *
	 * Variables that can be set by the system file shouldn't be
	 * used until after the following initialization!
	 */
	mod_read_system_file(boothowto & RB_ASKNAME);

	/*
	 * Calculate default settings of system parameters based upon
	 * maxusers, yet allow to be overridden via the /etc/system file.
	 */
	param_calc(maxusers);

	/*
	 * Initialize system parameters.
	 */
	param_init();

	/*
	 * Initialize loadable module system, and apply the `set' commands
	 * gleaned from /etc/system.
	 */
	mod_setup();

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	/*
	 * Initialize the hat layer.
	 */
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	seg_init();

	if (modload("fs", "specfs") == -1)
		halt("Can't load specfs");

	if (modloadonly("misc", "swapgeneric") == -1)
		halt("Can't load swapgeneric");

	dispinit(NULL);

	setup_ddi();

	console = establish_console();
	PReP_kb_layout = PReP_get_kb_layout();

	/*
	 * Lets take this opportunity to load the root device.
	 */
	if (loadrootmodules() != 0)
		halt("Can't load the root filesystem");

	(void) prom_map((caddr_t)PCIISA_VBASE, 0, (u_int)PCIIOBASE,
				65536);
	(void) prom_map((caddr_t)PCI_CONFIG_VBASE, 0, (u_int)PCI_CONFIG_PBASE,
				PCI_CONFIG_SIZE);

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

	ppcmmu_takeover();
	/*
	 * handlers need to be copied after mmu takeover - OF uses
	 * trap handlers to load entries in its pages tables to handle
	 * some address faults.
	 */
	copy_handlers();

	/*
	 * This may not be the right place to do this.
	 * Later on, we try (and expect to fail) to load some
	 * modules.  Something in the BOP_OPEN call enables
	 * interrupts, and we die horribly from a clock tick
	 * before we're ready for one.  Zapping bootops like
	 * this makes that call cleanly fail.  Probably the
	 * BOP_OPEN shouldn't enable interrupts.
	 */
	bootops = NULL;

	/*
	 * Update kernel hat structures and page table.
	 */
	hat_kern_setup();

	/*
	 * Initialize VM system, and map kernel address space.
	 */
	kvm_init();

	/*
	 * Allocate contiguous, memory below 16 mb
	 * with corresponding data structures to control its use.
	 */
	lomem_init();

	/*
	 * Set up a map for translating kernel vitual addresses;
	 * used by dump and libkvm.
	 */
	setup_kvpm();

	/*
	 * Allocate a vm slot for the dev mem driver, and 2 slots for dump.
	 */
	i = rmalloc(kernelmap, 1);
	mm_map = (caddr_t)kmxtob(i);
	i = rmalloc(kernelmap, DUMPPAGES);
	cur_dump_addr = dump_addr = kmxtob(i);

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
		u_int diff, off;
		struct page *pp;
		caddr_t rand_vaddr;

		cmn_err(CE_WARN, "limiting physmem to %d pages", physmem);

		off = 0;
		diff = npages - physmem;
		diff -= mmu_btopr(diff * sizeof (struct machpage));
		while (diff--) {
			rand_vaddr = (caddr_t)(((u_int)&unused_pages_vp >> 7) ^
						((u_offset_t)off >> PAGESHIFT));
			pp = page_create_va(&unused_pages_vp, (u_offset_t)off,
				MMU_PAGESIZE, PG_WAIT | PG_EXCL, &kas,
				rand_vaddr);
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
	dbug_mem = 0;
	cmn_err(CE_CONT, "?mem = %dK (0x%x)\n",
	    (physinstalled - dbug_mem) << (PAGESHIFT - 10),
	    ptob(physinstalled - dbug_mem));

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

	max_virt_segkp = btop(SYSBASE - (u_int)econtig);
	max_phys_segkp = (physmem * 2);
	segkp_len = ptob(min(max_virt_segkp, max_phys_segkp));
	eecontig = (caddr_t)((u_int)econtig + segkp_len);

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
	 * size is  SEGMAPSIZE . But if the total virtual address
	 * is greater than the amount of free memory that is available,
	 * then we trim back the segment size to that amount.
	 */
	va = (caddr_t)(PCIISA_VBASE - SEGMAPSIZE);
	/*
	 * 1201049: segkmap base address must be MAXBSIZE aligned
	 */
	ASSERT(((u_int)va & MAXBOFFSET) == 0);

	i = SEGMAPSIZE;
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
	 */
	kmem_gc();

	/*
	 * Garbage collect any lokmem-freed memory that was mapped by boot
	 * before segkmem_ready.
	 */
	lokmem_gc();

	/*
	 * Configure the system.
	 */
	configure();		/* set up devices */

	init_intr_threads(CPU);

	psm_install();

	init_clock_thread();

	release_bootstrap();

	(*picinitf)();

	(void) add_avsoftintr((void *)NULL, 1, softlevel1, "softlevel1",
		NULL, NULL);	/* XXX to be moved later */
}

/*
 *	hw_serial calculation.
 *
 *	If the 1275 system-id property is available, use it as input
 *	data to a crc calculation.
 *
 *	If it is not available, use the x86 method.
 */

/* global data */
extern char hw_serial[];
char *_hs1107 = hw_serial;
ulong_t  _bdhs34;

/*
 *	CRC calculation based on firmware algorithm.
 */

/* The macros below define 16-bit rotations (left and right) */
#define	rol(x, y)	(unsigned short) (((x) << (y)) | ((x) >> (16 - (y))))
#define	ror(x, y)	(unsigned short) (((x) >> (y)) | ((x) << (16 - (y))))

static
unsigned short
crc_gen(unsigned short oldcrc, unsigned char data)
{
	unsigned int pd, crc;

	pd = ((oldcrc >> 8) ^ data) << 8;

	crc = 0xff00 & (oldcrc << 8);
	crc |= pd >> 8;
	crc ^= rol(pd, 4) & 0xf00f;
	crc ^= ror(pd, 3) & 0x1fe0;
	crc ^= pd & 0xf000;
	crc ^= ror(pd, 7) & 0x01e0;

	return (crc);
}

/*
 *	Compute a 32-bit hostid from the serial number.
 *	Glue together 2 16-bit crc calculations, one that moves
 *	forward through the string of characters, and on that
 *	moves backwards through the array.
 */
static
unsigned long
crc_calc(unsigned char *sp, int len)
{
	unsigned short crc;	/* CRC temporary */
	unsigned char *tmp;	/* loop variable */
	unsigned long retval;

	/* initial value for CRC */
	crc = 0xffff;
	for (tmp = sp; tmp < sp + len; tmp++)
		crc = crc_gen(crc, *tmp);
	retval = crc << 16;

	crc = 0xffff;
	for (tmp = sp + len; --tmp >= sp; )
		crc = crc_gen(crc, *tmp);
	retval |= crc;

	return (retval);
}

void
serial_number_init(void)
{
	static const char SERIAL_NUM[] = "system-id";
	long hostid;
	unsigned char *serial_number;
	int serial_number_len;

	serial_number_len = prom_getproplen(prom_rootnode(),
	    (caddr_t)SERIAL_NUM);
	if (serial_number_len == -1) {	/* property not found */
		cmn_err(CE_CONT, "\"system-id\" root node property "
		    "not supported by Open Firmware.\n");
		return;
	}

	serial_number = (unsigned char *) kmem_zalloc(serial_number_len + 1,
	    KM_SLEEP);

	if (prom_getprop(prom_rootnode(), (caddr_t)SERIAL_NUM,
	    (caddr_t)serial_number) == -1) {
		kmem_free(serial_number, serial_number_len + 1);
		return;
	}

	if (strcmp((char *)serial_number, "ffffffffffffffff") == 0) {
		cmn_err(CE_NOTE,
		    "The serial number of this system must be set.");
		drv_usecwait(2000000);	/* delay 2 seconds */
		kmem_free(serial_number, serial_number_len + 1);
		return;
	}

	hostid = crc_calc(serial_number, serial_number_len);
	sprintf(hw_serial, "%u", hostid);
	kmem_free(serial_number, serial_number_len + 1);
}

/*	end of hw_serial calculation.	*/

void
post_startup(void)
{
	int sysinitid;

	if (strcmp(hw_serial, "0") == 0) { /* if not yet set */
		if ((sysinitid = modload("misc", "sysinit")) != -1)
			(void) modunload(sysinitid);
		else
			cmn_err(CE_CONT, "sysinit load failed");
	}

	/*
	 * Perform forceloading tasks for /etc/system.
	 */
	(void) mod_sysctl(SYS_FORCELOAD, NULL);

	/*
	 * ON4.0: Force /proc module in until clock interrupt handle fixed
	 * ON4.0: This must be fixed or restated in /etc/systems.
	 */
	(void) modload("fs", "procfs");

	maxmem = freemem;

	(void) spl0();		/* allow interrupts */
}

#if 0	/* This is useful in adding pages used by OF/VOF */
/*
 * Add to a memory list.
 */
static void
physlist_add(
	u_longlong_t	start,
	u_longlong_t	len,
	struct memlist	**memlistp)
{
	struct memlist *cur, *new, *last;
	u_longlong_t end = start + len;

	new = *memlistp;
	new->address = start;
	new->size = len;
	*memlistp = new + 1;
	for (cur = phys_avail; cur; cur = cur->next) {
		last = cur;
		if (cur->address >= end) {
			new->next = cur;
			new->prev = cur->prev;
			cur->prev = new;
			if (cur == phys_avail)
				phys_avail = new;
			else
				new->prev->next = new;
			return;
		}
		if (cur->address + cur->size > start)
			panic("munged phys_avail list");
	}
	new->next = NULL;
	new->prev = last;
	last->next = new;
}
#endif

/*
 * kphysm_init() initializes physical memory.
 * The old startup made some assumptions about the kernel living in
 * physically contiguous space which is no longer valid.
 */
static void
kphysm_init(machpage_t *pp, struct memseg *memsegp, u_int npages, u_int blks)
{
	struct memlist *pmem;
	struct memseg *cur_memseg;
	struct memseg *tmp_memseg;
	struct memseg **prev_memsegp;
	u_long pnum;
	extern void page_coloring_init(void);
#ifdef lint
	blks = blks;
#endif /* lint */

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
 * Kernel VM initialization. It is assumed that the virtual addresses below
 * KERNELBASE is not allocated from the virt_avail memlist from boot.
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

	(void) seg_attach(&kas, (caddr_t)KERNELBASE,
	    (u_int)(e_data - KERNELBASE), &ktextseg);
	(void) segkmem_create(&ktextseg, (caddr_t)NULL);

	(void) seg_attach(&kas, (caddr_t)valloc_base, (u_int)econtig -
		(u_int)valloc_base, &kvalloc);
	(void) segkmem_create(&kvalloc, (caddr_t)NULL);

	/*
	 * We're about to map out /boot.  This is the beginning of the
	 * system resource management transition. We can no longer
	 * call into /boot for I/O or memory allocations.
	 */
	(void) seg_attach(&kas, (caddr_t)SYSBASE,
	    (u_int)(Syslimit - SYSBASE), &kvseg);
	(void) segkmem_create(&kvseg, (caddr_t)Sysmap);

	rw_exit(&kas.a_lock);

	rw_enter(&kas.a_lock, RW_READER);

	/*
	 * Now we can ask segkmem for memory instead of boot.
	 */
	segkmem_ready = 1;

	/*
	 * Now we just need to fix the protections for the mappings above
	 * KERNELBASE.
	 */

	/*
	 * (Normally) Read-only until end of text.
	 */
	va = (caddr_t)_start;
	(void) as_setprot(&kas, va, (u_int)(e_text - va), (u_int)
	    (PROT_READ | PROT_EXEC | ((kernprot == 0) ? PROT_WRITE : 0)));
	va = (caddr_t)roundup((u_int)e_text, PAGESIZE);

	/*
	 * Writable until end.
	 */
	(void) as_setprot(&kas, va, (u_int)(e_data - va),
	    PROT_READ | PROT_WRITE | PROT_EXEC);
	va = (caddr_t)roundup((u_int)e_data, PAGESIZE);

	/*
	 * Validate the valloc'ed structures.
	 */
	(void) as_setprot(&kas, va, (u_int)(econtig - va),
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
	(void) as_setprot(&kas, va, Syslimit - va, 0);

	rw_exit(&kas.a_lock);
}

/*
 * crash dump, libkvm support - see sys/kvtopdata.h for details
 */
struct kvtopdata kvtopdata;

static void
setup_kvpm(void)
{
	u_int va;
	int i = 0;
	u_int pages = 0;
	register u_int lastpfnum = 0xFFFFFFFFU;
	register u_int pfnum;

	for (va = (u_int)KERNELBASE; va < (u_int)econtig; va += PAGESIZE) {
		pfnum = ppcmmu_getkpfnum((caddr_t)va);
		if (pfnum != (u_int)-1) {
			if (lastpfnum == 0xFFFFFFFFU) {
				lastpfnum = pfnum;
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
				pages = 1;
			} else if (lastpfnum+1 == pfnum) {
				lastpfnum = pfnum;
				pages++;
			} else {
				kvtopdata.kvtopmap[i].kvpm_len = pages;
				lastpfnum = pfnum;
				pages = 1;
				if (++i > NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					break;
				}
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
			}
		} else if (lastpfnum != 0xFFFFFFFFU) {
			lastpfnum = 0xFFFFFFFFU;
			kvtopdata.kvtopmap[i].kvpm_len = pages;
			pages = 0;
			if (++i > NKVTOPENTS) {
				cmn_err(CE_WARN, "out of kvtopents");
				break;
			}
		}
	}
	if (pages) {
		kvtopdata.kvtopmap[i].kvpm_len =
			pages  - btop((u_int)econtig - va);
		i++;
		pages = 0;
	}
	lastpfnum = (u_int)-1;
	/*
	 * Pages allocated early from sysmap region that don't
	 * have page structures and need to be entered in to the
	 * kvtop array for libkvm.  The rule for memory pages is:
	 * it is either covered by a page structure or included in
	 * kvtopdata.
	 */
	for (va = (u_int)Sysbase; va < (u_int)startup_alloc_vaddr;
	    va += PAGESIZE) {
		pfnum = ppcmmu_getkpfnum((caddr_t)va);
		if (pfnum != (u_int)-1) {
			if (lastpfnum == (u_int)-1) {
				lastpfnum = pfnum;
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
				pages = 1;
			} else if (lastpfnum+1 == pfnum) {
				lastpfnum = pfnum;
				pages++;
			} else {
				kvtopdata.kvtopmap[i].kvpm_len = pages;
				lastpfnum = pfnum;
				pages = 1;
				i++;
				if (i >= NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					break;
				}
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
			}
		} else if (lastpfnum != (u_int)-1) {
			lastpfnum = (u_int)-1;
			kvtopdata.kvtopmap[i].kvpm_len = pages;
			pages = 0;
			i++;
			if (i >= NKVTOPENTS) {
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

	msgbuf.msg_map = va_to_pa((u_int)&kvtopdata);
}

#define	NELEM(a)	(sizeof (a) / sizeof ((a)[0]))
static int
PReP_get_kb_layout(void)
{
	int i;
	char buf[100];
	static struct {
		char name[3];
		int code;
	} ISO_to_solaris[] = {
		"DA",	36,
		"NL",	39,
		"EN",	1,
		"FR",	35,
		"DE",	37,
		"IT",	38,
		"NO",	40,
		"PT",	41,
		"ES",	42,
		"SV",	43,
#if	NO_ASSIGNED_SOLARIS_VALUE
		"SK",	???,
		"PL",	???,
		"HU",	???,
		"CS",	???,
		"FI",	???,
#endif
	};

	if (prom_getprop(prom_stdin_node(), "language", buf) > 0) {
		for (i = 0; i < NELEM(ISO_to_solaris); i++) {
			if (strcmp(buf, ISO_to_solaris[i].name) == 0)
				return (ISO_to_solaris[i].code);
		}
		cmn_err(CE_WARN,
	"Keyboard language \"%s\" not supported, using US English instead",
			buf);
	}
	return (0);
}
#undef	NELEM

static int
establish_console()
{
	return (stdout_is_framebuffer() ? CONSOLE_IS_FB : CONSOLE_IS_ASY);
}
