/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)startup.c	1.129	96/10/15 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/vm.h>

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
#include <sys/memlist.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/modctl.h>		/* for "procfs" hack */
#include <sys/kvtopdata.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#define	SUNDDI_IMPL		/* so sunddi.h will not redefine splx() et al */
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/dki_lock.h>
#include <sys/autoconf.h>
#include <sys/clock.h>
#include <sys/scb.h>
#include <sys/stack.h>
#include <sys/eeprom.h>
#include <sys/intreg.h>
#include <sys/trap.h>
#include <sys/x_call.h>
#include <sys/privregs.h>
#include <sys/fpu/fpusystm.h>


#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vmparam.h>
#include <vm/hat_sfmmu.h>
#include <sys/iommu.h>
#include <sys/vtrace.h>
#include <sys/instance.h>
#include <sys/kobj.h>
#include <sys/async.h>
#include <sys/spitasi.h>
#include <vm/mach_page.h>
#include <sys/tuneable.h>
#include <sys/platform_module.h>

#include <sys/prom_debug.h>
#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#include <sys/memnode.h>

/*
 * External Data:
 */
extern lksblk_t *lksblks_head;
extern int vac_size;	/* cache size in bytes */
extern u_int vac_mask;	/* VAC alignment consistency mask */

int snooping = 0;
u_int snoop_interval = 50 * 1000000;
extern void deadman();
extern void tickint_init();
extern void tickint_clnt_add();

/*
 * Global Data Definitions:
 */

#ifdef TRAPTRACE
caddr_t ttrace_buf;	/* bop alloced traptrace for all cpus except 0 */
int	ttrace_index;	/* index used in assignments */
#endif /* TRAPTRACE */


/*
 * Declare these as initialized data so we can patch them.
 */
int physmem = 0;	/* memory size in pages, patch if you want less */
int kernprot = 1;	/* write protect kernel text */

#ifdef DEBUG
int forthdebug	= 1;	/* Load the forthdebugger module */
#else
int forthdebug	= 0;	/* Don't load the forthdebugger module */
#endif DEBUG

#define	FDEBUGSIZE (50 * 1024)
#define	FDEBUGFILE "misc/forthdebug"

int use_cache = 1;		/* cache not reliable (605 bugs) with MP */
int vac_copyback = 1;
char	*cache_mode = (char *)0;
int use_mix = 1;
int prom_debug = 0;

struct bootops *bootops = 0;	/* passed in from boot in %o2 */
caddr_t boot_tba;
u_int	tba_taken_over = 0;

/*
 * DEBUGADDR is where we expect the deubbger to be if it's there.
 * We really should be allocating virtual addresses by looking
 * at the virt_avail list.
 */
#define	DEBUGADDR		((caddr_t)0xedd00000)

caddr_t s_text;			/* start of kernel text segment */
caddr_t e_text;			/* end of kernel text segment */
caddr_t s_data;			/* start of kernel data segment */
caddr_t e_data;			/* end of kernel data segment */

caddr_t		econtig;	/* end of first block of contiguous kernel */
caddr_t		ncbase;		/* beginning of non-cached segment */
caddr_t		ncend;		/* end of non-cached segment */
caddr_t		sdata;		/* beginning of data segment */
caddr_t		extra_etva;	/* beginning of end of text - va */
u_longlong_t	extra_etpa;	/* beginning of end of text - pa */
u_int		extra_et;	/* bytes from end of text to 4MB boundary */

u_int	ndata_remain_sz;	/* bytes from end of data to 4MB boundary */
caddr_t	nalloc_base;		/* beginning of nucleus allocation */
caddr_t nalloc_end;		/* end of nucleus allocatable memory */
caddr_t valloc_base;		/* beginning of kvalloc segment	*/

u_int shm_alignment = 0;	/* VAC address consistency modulus */
struct memlist *phys_install;	/* Total installed physical memory */
struct memlist *phys_avail;	/* Available (unreserved) physical memory */
struct memlist *virt_avail;	/* Available (unmapped?) virtual memory */
int memexp_flag;		/* memory expansion card flag */
u_longlong_t ecache_flushaddr;	/* physical address used for flushing E$ */

/*
 * VM data structures
 */
int page_hashsz;		/* Size of page hash table (power of two) */
struct machpage *pp_base;	/* Base of system page struct array */
u_int pp_sz;			/* Size in bytes of page struct array */
struct	page **page_hash;	/* Page hash table */
struct	seg *segkmap;		/* Kernel generic mapping segment */
struct	seg ktextseg;		/* Segment used for kernel executable image */
struct	seg kvalloc;		/* Segment used for "valloc" mapping */
struct	seg *segkp;		/* Segment for pageable kernel virt. memory */
struct	seg *seg_debug;		/* Segment for debugger */
struct	memseg *memseg_base;
u_int	memseg_sz;		/* Used to translate a va to page */
struct	vnode unused_pages_vp;

/*
 * VM data structures allocated early during boot.
 */
/*
 *	Fix for bug 1119063.
 */
#define	KERNELMAP_SZ(frag)	\
	max(MMU_PAGESIZE,	\
	roundup((sizeof (struct map) * (physmem)/2/(frag)), MMU_PAGESIZE))

u_int pagehash_sz;
u_int memlist_sz;
u_int kernelmap_sz;

/*
 * startup_alloc_vaddr is initialized to 1 page + SYSBASE instead of being
 * exactly SYSBASE because rmap uses 0 as a delimeter so we can't have
 * offset 0 (ie. page 0) as part of the resource map. Page 0 is simply never
 * used. The fact that startup_alloc_vaddr starts at page 1 of the resource
 * map is also hardwired in the call to rmget.
 */
caddr_t startup_alloc_vaddr = (caddr_t)SYSBASE + MMU_PAGESIZE;
u_int startup_alloc_size;
u_int	startup_alloc_chunk_size = 20 * MMU_PAGESIZE;

char tbr_wr_addr_inited = 0;

/*
 * Saved beginning page frame for kernel .data and last page
 * frame for up to end[] for the kernel. These are filled in
 * by kvm_init().
 */
u_int kpfn_dataseg, kpfn_endbss;

/*
 * crash dump, libkvm support - see sys/kvtopdata.h for details
 */
struct kvtopdata kvtopdata;

/*
 * Static Routines:
 */
static void memlist_add(u_longlong_t, u_longlong_t, struct memlist **,
	struct memlist **);
static void memseg_list_add(struct memseg *);
static void kphysm_init(machpage_t *, struct memseg *, u_int);
static void kvm_init(void);
static void setup_kvpm(void);

struct cpu *prom_cpu;
kmutex_t prom_mutex;
kcondvar_t prom_cv;
static void kern_preprom(void);
static void kern_postprom(void);

static void startup_init(void);
static void startup_memlist(void);
static void startup_modules(void);
static void startup_bop_gone(void);
static void startup_vm(void);
static void startup_end(void);
static void setup_trap_table(void);
static caddr_t iommu_tsb_alloc(caddr_t);
static void startup_build_mem_nodes(void);

static u_int npages;
static int dbug_mem;
static int debug_start_va;
static struct memlist *memlist;


/*
 * The following variables can be patched to override the auto-selection
 * of dvma space based on the amount of installed physical memory.
 */
int sbus_iommu_tsb_alloc_size = 0;
int pci_iommu_tsb_alloc_size = 0;

/*
 * Enable some debugging messages concerning memory usage...
 */
#ifdef  DEBUGGING_MEM
static int debugging_mem;
static void
printmemlist(char *title, struct memlist *list)
{
	if (!debugging_mem)
		return;

	printf("%s\n", title);

	while (list) {
		prom_printf("\taddr = 0x%x %8x, size = 0x%x %8x\n",
		    (u_int)(list->address >> 32), (u_int)list->address,
		    (u_int)(list->size >> 32), (u_int)(list->size));
		list = list->next;
	}
}

void
printmemseg(struct memseg *memseg)
{
	if (!debugging_mem)
		return;

	printf("memseg\n");

	while (memseg) {
		prom_printf("\tpage = 0x%x, epage = 0x%x, "
			"pfn = 0x%x, epfn = 0x%x\n",
			(u_int)memseg->pages, (u_int)memseg->epages,
			memseg->pages_base, memseg->pages_end);
		memseg = memseg->next;
	}
}

#define	debug_pause(str)	if (prom_getversion() > 0) halt((str))
#define	MPRINTF(str)		if (debugging_mem) prom_printf((str))
#define	MPRINTF1(str, a)	if (debugging_mem) prom_printf((str), (a))
#define	MPRINTF2(str, a, b)	if (debugging_mem) prom_printf((str), (a), (b))
#define	MPRINTF3(str, a, b, c) \
	if (debugging_mem) prom_printf((str), (a), (b), (c))
#else	/* DEBUGGING_MEM */
#define	MPRINTF(str)
#define	MPRINTF1(str, a)
#define	MPRINTF2(str, a, b)
#define	MPRINTF3(str, a, b, c)
#endif	/* DEBUGGING_MEM */

/* Simple message to indicate that the bootops pointer has been zeroed */
#ifdef DEBUG
static int bootops_gone_on = 0;
#define	BOOTOPS_GONE() \
	if (bootops_gone_on) \
		prom_printf("The bootops vec is zeroed now!\n");
#else
#define	BOOTOPS_GONE()
#endif DEBUG

/*
 * Monitor pages may not be where this sez they are.
 * and the debugger may not be there either.
 *
 * Also, note that 'pages' here are *physical* pages,
 * which are 8k on sun4u.
 *
 *		  Physical memory layout
 *		(not necessarily contiguous)
 *		(THIS IS SOMEWHAT WRONG)
 *		_________________________
 *		|	monitor pages	|
 *    availmem -|-----------------------|
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
 *		|    interrupt stack	|
 *		|-----------------------|
 *		|    kernel text (RO)	|
 *		|-----------------------|
 *		|    trap table (4k)	|
 *		|-----------------------|
 *	page 1	|	 msgbuf		|
 *		|-----------------------|
 *	page 0	|	reclaimed	|
 *		|_______________________|
 *
 *
 *
 *	      Kernels Virtual Memory Layout.
 *		/-----------------------\
 *		|			|
 *		|	OBP/kadb/...	|
 *		|			|
 * 0xF0000000  -|-----------------------|- SYSEND
 *		|			|
 *		|			|
 *		|  segkmem segment	|	(SYSEND - SYSBASE = 2.5G)
 *		|			|
 *		|			|
 * 0x50000000  -|-----------------------|- SYSBASE
 *		|			|
 *		|  segmap segment	|	SEGMAPSIZE	(256M)
 *		|			|
 * 0x40000000  -|-----------------------|- SEGMAPBASE
 *		|			|
 *		|	segkp		|	SEGKPSIZE	(256M)
 *		|			|
 * 0x30000000  -|-----------------------|- SEGKPBASE
 *              |                       |
 *             -|-----------------------|- MEMSCRUBBASE (SEGKPBASE - 0x400000)
 *		|			|
 *             -|-----------------------|- ARGSBASE (MEMSCRUBBASE - NCARGS)
 *		|			|
 *             -|-----------------------|- PPMAPBASE (ARGSBASE - PPMAPSIZE)
 *		|			|
 *             -|-----------------------|- PPMAP_FAST_BASE
 *		|			|
 *             -|-----------------------|- NARG_BASE
 *		:			:
 *		|			|
 *		|-----------------------|- econtig
 *		|    vm structures	|
 * 0x10800000	|-----------------------|- nalloc_end
 *		|	  tsb		|
 *		|-----------------------|
 *		|    hmeblk pool	|
 *		|-----------------------|
 *		|    hmeblk hashtable	|
 *		|-----------------------|- end/nalloc_base
 *		|  kernel data & bss	|
 * 0x10400000	|-----------------------|
 *		|			|
 *		|-----------------------|- etext
 *		|	kernel text	|
 *		|-----------------------|
 *		|   trap table (48k)	|
 * 0x10000000  -|-----------------------|- KERNELBASE
 *		|			|
 *		|	invalid		|
 *		|			|
 * 0x00000000  _|_______________________|
 *
 *
 *
 *
 *	       Users Virtual Memory Layout.
 *		/-----------------------\
 *		|			|
 *		|        invalid 	|
 *		|			|
 * 0xF0000000  -|-----------------------|- USERLIMIT
 *		|	user stack	|
 *		:			:
 *		:			:
 *		:			:
 *		|	user data	|
 *	       -|-----------------------|-
 *		|	user text	|
 * 0x00002000  -|-----------------------|-
 *		|	invalid		|
 * 0x00000000  _|_______________________|
 *
 */

static kcondvar_t farg_cv;
static kmutex_t farg_mtx;

static
struct arg_slot {
	caddr_t base;
	struct arg_slot *next;
};

struct arg_slot farg_array[N_ARG_SLOT];
struct arg_slot *farg_list;
int arg_wait = 0;

static void
arg_base_init()
{
	int i;
	struct arg_slot *parg;
	caddr_t addr;

	parg = farg_array;
	addr = (caddr_t)NARG_BASE;

	mutex_init(&farg_mtx, "free_arg_mutex", MUTEX_DEFAULT, NULL);
	farg_list = NULL;

	/*
	 * Make sure each slot is aligned with VAC constraints.
	 * For small VAC machines, we just need to make sure
	 * buttom of stack lined up correctly, and the size
	 * of slots are modulos of VAC alignments.
	 *
	 * The current setup will work for upto 0x8000
	 * shm_alignment. We panic for now if things don't
	 * work out so we can fix it. The fix should be
	 * trivial- just adjust #define's above.
	 */
	if (addr_to_vcolor(NARG_BASE) != addr_to_vcolor(USRSTACK)) {
		cmn_err(CE_PANIC, "buttom of stack not aligned");
	}

	if (ARG_SLOT_SIZE % shm_alignment) {
		cmn_err(CE_PANIC, "wrong stack slot size");
	}

	for (i = 0; i < N_ARG_SLOT; i++) {
		parg->base = addr;
		addr += ARG_SLOT_SIZE;

		parg->next = farg_list;
		farg_list = parg;
		parg++;
	}
}

/*
 * Fast path to get some virtual spaces to setup the exec
 * arguments. It has to be self-contained in terms of locking.
 * It should NOT use/depend on any VM locks.
 */
caddr_t
get_arg_base(u_int nsize)
{
	struct arg_slot *parg = NULL;

	if (nsize > ARG_SLOT_SIZE) {
		return (NULL);
	}

	mutex_enter(&farg_mtx);
again:
	if (farg_list) {
		parg = farg_list;
		farg_list = farg_list->next;
	} else {
		arg_wait++;
		cv_wait(&farg_cv, &farg_mtx);
		arg_wait--;
		goto again;

	}
	mutex_exit(&farg_mtx);

	return ((parg->base + ARG_SLOT_SIZE) - nsize);
}

void
free_arg_base(caddr_t base)
{
	struct arg_slot *parg;
	u_int i;

	i = ((u_int) base - NARG_BASE) >> ARG_SLOT_SHIFT;

	parg = &farg_array[i];

	if ((u_int) parg->base != ((u_int)base & ~(ARG_SLOT_SIZE - 1))) {
		panic("wrong parg 0x%x, base 0x%x\n", parg, base);
	}

	mutex_enter(&farg_mtx);
	parg->next = farg_list;
	farg_list = parg;
	if (arg_wait > 0) {
		cv_signal(&farg_cv);
	}
	mutex_exit(&farg_mtx);
}

/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	startup_init();
	startup_memlist();
	startup_modules();
	startup_bop_gone();
	startup_vm();
	startup_end();
}

static void
startup_init(void)
{
	extern void dki_lock_setup(lksblk_t *);
	extern void ppmapinit(void);
	extern void kncinit(void);
	extern void init_vx_handler(void);
	extern int callback_handler(cell_t *arg_array);

	(void) check_boot_version(BOP_GETVERSION(bootops));

	dki_lock_setup(lksblks_head);

	/*
	 * Initialize the turnstile allocation mechanism.
	 */
	tstile_init();

	kncinit();

	/*
	 * Initialize the address map for cache consistent mappings
	 * to random pages, must done after vac_size is set.
	 */
	ppmapinit();

	arg_base_init();

	/*
	 * Initialize the PROM callback handler and install the PROM
	 * callback handler. For this 32 bit client program, we install
	 * "callback_handler" which is the glue that binds the 64 bit
	 * prom callback handler to the 32 bit client program callback
	 * handler: vx_handler.
	 */
	init_vx_handler();
	(void) prom_set_callback((void *)callback_handler);
}

static void
startup_memlist(void)
{
	u_int real_sz;
	u_int ctrs_sz;
	caddr_t real_base;
	caddr_t alloc_base;
	int memblocks = 0;
	caddr_t memspace;
	u_int memspace_sz;
	struct memlist *cur;
	u_int syslimit = (u_int)Syslimit;	/* See: 1124059 */
	u_int sysbase = (u_int)Sysbase;		/* See: 1124059 */
	caddr_t bop_alloc_base;
	u_int kmapsz;

	extern int ecache_linesize;
	extern void mt_lock_init(void);
	extern caddr_t ndata_alloc_cpus();
	extern caddr_t ndata_alloc_hat();
	extern caddr_t ndata_alloc_page_freelists(caddr_t);
	extern u_int	page_ctrs_sz(void);
	extern caddr_t	page_ctrs_alloc(caddr_t);
	extern caddr_t e_text, e_data;

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

	/*
	 * We're loaded by boot with the following configuration (as
	 * specified in the sun4u/conf/Mapfile):
	 * 	text:		4 MB chunk aligned on a 4MB boundary
	 *	data & bss:	4 MB chunk aligned on a 4MB boundary
	 * These two chunks will eventually be mapped by 2 locked 4MB ttes
	 * and will represent the nucleus of the kernel.
	 * This gives us some free space that is already allocated.
	 * The free space in the text chunk is currently being returned
	 * to the physavail list. Eventually it would be nice to use this
	 * space for other kernel text and thus take more advantage of the
	 * kernel 4MB tte.
	 * The free space in the data-bss chunk is used for nucleus allocatable
	 * data structures and we reserve it using the nalloc_base and
	 * nalloc_end variables.  This space is currently being used for
	 * hat data structures required for tlb miss handling operations.
	 * We align nalloc_base to a l2 cache linesize because this is the
	 * line size the hardware uses to maintain cache coherency
	 */

	nalloc_base = (caddr_t)roundup((u_int)e_data, MMU_PAGESIZE);
	nalloc_end = (caddr_t)roundup((u_int)nalloc_base, MMU_PAGESIZE4M);
	valloc_base = nalloc_base;

	/*
	 * Calculate the start of the data segment.
	 */
	sdata = (caddr_t)((u_int)e_data & MMU_PAGEMASK4M);

	PRM_DEBUG(nalloc_base);
	PRM_DEBUG(nalloc_end);
	PRM_DEBUG(sdata);

	/*
	 * Remember any slop after e_text so we can add it to the
	 * physavail list.
	 */
	PRM_DEBUG(e_text);
	extra_etva = (caddr_t)roundup((u_int)e_text, MMU_PAGESIZE);
	PRM_DEBUG((u_int)extra_etva);
	extra_etpa = va_to_pa(extra_etva);
	PRM_DEBUG(extra_etpa);
	if (extra_etpa != (u_int)-1) {
		extra_et = roundup((u_int)e_text, MMU_PAGESIZE4M) -
			(u_int)extra_etva;
	} else {
		extra_et = 0;
	}
	PRM_DEBUG(extra_et);

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

	/*
	 * Remember what the physically available highest page is
	 * so that dumpsys works properly, and find out how much
	 * memory is installed.
	 */
	installed_top_size(bootops->boot_mem->physinstalled, &physmax,
	    &physinstalled);
	PRM_DEBUG(physinstalled);
	PRM_DEBUG(physmax);

	/* Fill out memory nodes config structure */
	startup_build_mem_nodes();

	/*
	 * Get the list of physically available memory to size
	 * the number of page structures needed.
	 */
	size_physavail(bootops->boot_mem->physavail, &npages, &memblocks);

	/* Account for any pages after e_text and e_data */
	npages += mmu_btop(extra_et);
	npages += mmu_btopr(nalloc_end - nalloc_base);
	PRM_DEBUG(npages);

	/*
	 * npages is the maximum of available physical memory possible.
	 * (ie. it will never be more than this)
	 */

	/*
	 * Allocate cpus structs from the nucleus data area and
	 * update nalloc_base.
	 */
	nalloc_base = (caddr_t)roundup((u_int)ndata_alloc_cpus(nalloc_base),
		ecache_linesize);

	if (nalloc_base > nalloc_end) {
		cmn_err(CE_PANIC, "no more nucleus memory after cpu alloc");
	}

	/*
	 * Allocate page_freelists bin headers from the nucleus data area and
	 * update nalloc_base.
	 */
	nalloc_base = ndata_alloc_page_freelists(nalloc_base);
	nalloc_base = (caddr_t)roundup((u_int)nalloc_base, ecache_linesize);

	if (nalloc_base > nalloc_end) {
		cmn_err(CE_PANIC,
			"no more nucleus memory after page free lists alloc");
	}

	/*
	 * Allocate hat related structs from the nucleus data area and
	 * update nalloc_base.
	 */
	nalloc_base = ndata_alloc_hat(nalloc_base, nalloc_end, npages);
	nalloc_base = (caddr_t)roundup((u_int)nalloc_base, ecache_linesize);
	if (nalloc_base > nalloc_end) {
		cmn_err(CE_PANIC, "no more nucleus memory after hat alloc");
	}

	/*
	 * Given our current estimate of npages we do a premature calculation
	 * on how much memory we are going to need to support this number of
	 * pages.  This allows us to calculate a good start virtual address
	 * for other BOP_ALLOC operations.
	 * We want to do the BOP_ALLOCs before the real allocation of page
	 * structs in order to not have to allocate page structs for this
	 * memory.  We need to calculate a virtual address because we want
	 * the page structs to come before other allocations in virtual address
	 * space.  This is so some (if not all) of page structs can actually
	 * live in the nucleus.
	 */
	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz = 1 << highbit((u_long)page_hashsz);
	pagehash_sz = sizeof (struct page *) * page_hashsz;

	/*
	 * Size up per page size free list counters.
	 */
	ctrs_sz = page_ctrs_sz();

	/*
	 * The memseg list is for the chunks of physical memory that
	 * will be managed by the vm system.  The number calculated is
	 * a guess as boot may fragment it more when memory allocations
	 * are made before kphysm_init().  Currently, there are two
	 * allocations before then, so we assume each causes fragmen-
	 * tation, and add a couple more for good measure.
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + 4);
	pp_sz = sizeof (struct machpage) * npages;

	real_sz = pagehash_sz + memseg_sz + pp_sz + ctrs_sz;
	PRM_DEBUG(real_sz);

	bop_alloc_base = (caddr_t)roundup((uint)(nalloc_end + real_sz),
		MMU_PAGESIZE);
	PRM_DEBUG(bop_alloc_base);

	/*
	 * Add other BOP_ALLOC operations here
	 */
	alloc_base = bop_alloc_base;
	alloc_base = sfmmu_tsb_alloc(alloc_base, npages);
	alloc_base = (caddr_t)roundup((uint)alloc_base, ecache_linesize);
	PRM_DEBUG(alloc_base);
#ifdef	TRAPTRACE
	alloc_base = trap_trace_alloc(alloc_base);
#endif	/* TRAPTRACE */

	/*
	 * Allocate IOMMU TSB array.  We do this here so that the physical
	 * memory gets deducted from the PROM's physical memory list.
	 */
	alloc_base = (caddr_t)roundup((u_int)iommu_tsb_alloc(alloc_base),
		ecache_linesize);
	PRM_DEBUG(alloc_base);

	/*
	 * The only left to allocate for the kvalloc segment should be the
	 * vm data structures.
	 */
	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);
	npages = 0;
	memblocks = 0;
	size_physavail(bootops->boot_mem->physavail, &npages, &memblocks);
	PRM_DEBUG(npages);
	/* account for memory after etext */
	npages += mmu_btop(extra_et);

	/*
	 * Calculate the remaining memory in nucleus data area.
	 * We need to figure out if page structs can fit in there or not.
	 * We also make sure enough page structs get created for any physical
	 * memory we might be returning to the system.
	 */
	ndata_remain_sz = (u_int) (nalloc_end - nalloc_base);
	PRM_DEBUG(ndata_remain_sz);
	pp_sz = sizeof (struct machpage) * npages;
	if (ndata_remain_sz > pp_sz) {
		npages += mmu_btop(ndata_remain_sz - pp_sz);
	}
	/*
	 * If physmem is patched to be non-zero, use it instead of
	 * the monitor value unless physmem is larger than the total
	 * amount of memory on hand.
	 */
	if (physmem == 0 || physmem > npages)
		physmem = npages;

	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz = 1 << highbit((u_long)page_hashsz);
	pagehash_sz = sizeof (struct page *) * page_hashsz;


	/*
	 * The memseg list is for the chunks of physical memory that
	 * will be managed by the vm system.  The number calculated is
	 * a guess as boot may fragment it more when memory allocations
	 * are made before kphysm_init().  Currently, there are two
	 * allocations before then, so we assume each causes fragmen-
	 * tation, and add a couple more for good measure.
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + 4);
	pp_sz = sizeof (struct machpage) * npages;
	real_sz = pagehash_sz + memseg_sz;
	real_sz = roundup(real_sz, ecache_linesize) + pp_sz;
	real_sz = roundup(real_sz, ecache_linesize) + ctrs_sz;
	PRM_DEBUG(real_sz);

	/*
	 * Allocate the page structures from the remaining memory in the
	 * nucleus data area.
	 */
	real_base = nalloc_base;

	if (ndata_remain_sz >= real_sz) {
		/*
		 * Figure out the base and size of the remaining memory.
		 */
		nalloc_base += real_sz;
		ASSERT(nalloc_base <= nalloc_end);
		ndata_remain_sz = nalloc_end - nalloc_base;
	} else if (ndata_remain_sz < real_sz) {
		/*
		 * The page structs need extra memory allocated through
		 * BOP_ALLOC.
		 */
		real_sz = roundup((real_sz - ndata_remain_sz),
			MMU_PAGESIZE);
		memspace = (caddr_t)BOP_ALLOC(bootops, nalloc_end, real_sz,
			MMU_PAGESIZE);
		if (memspace != nalloc_end)
			panic("system page struct alloc failure");

		nalloc_base = nalloc_end;
		ndata_remain_sz = 0;
		if ((nalloc_end + real_sz) > bop_alloc_base) {
			prom_panic("vm structures overwrote other bop alloc!");
		}
	}
	PRM_DEBUG(nalloc_base);
	PRM_DEBUG(ndata_remain_sz);
	PRM_DEBUG(real_base + real_sz);
	nalloc_base = (caddr_t)roundup((uint)nalloc_base, MMU_PAGESIZE);
	ndata_remain_sz = nalloc_end - nalloc_base;

	page_hash = (struct page **)real_base;

	memseg_base = (struct memseg *)roundup(
		(u_int)page_ctrs_alloc((caddr_t)((u_int)page_hash +
			pagehash_sz)),
		ecache_linesize);

	pp_base = (struct machpage *)roundup((u_int)memseg_base + memseg_sz,
		ecache_linesize);

	ASSERT(((u_int)pp_base + pp_sz) <= (u_int)bop_alloc_base);

	PRM_DEBUG(page_hash);
	PRM_DEBUG(memseg_base);
	PRM_DEBUG(pp_base);
	econtig = alloc_base;
	PRM_DEBUG(econtig);

	/*
	 * the memory lists from boot, and early versions of the kernelmap
	 * are allocated from the virtual address region managed by kernelmap
	 * so that later they can be freed and/or reallocated.
	 */
	memlist_sz = bootops->boot_mem->extent;
	/*
	 * Between now and when we finish copying in the memory lists,
	 * allocations happen so the space gets fragmented and the
	 * lists longer.  Leave enough space for lists twice as long
	 * as what boot says it has now; roundup to a pagesize.
	 * Also add space for the final phys-avail copy in the fixup
	 * routine.
	 */
	memlist_sz *= 4;
	memlist_sz = roundup(memlist_sz, MMU_PAGESIZE);
	kernelmap_sz = KERNELMAP_SZ(2);
	memspace_sz = memlist_sz + kernelmap_sz;
	memspace = (caddr_t)BOP_ALLOC(bootops, startup_alloc_vaddr,
	    memspace_sz, BO_NO_ALIGN);
	startup_alloc_vaddr += memspace_sz;
	startup_alloc_size += memspace_sz;
	if (memspace == NULL)
		halt("Boot allocation failed.");

	memlist = (struct memlist *)memspace;
	kernelmap = (struct map *)((u_int)memlist + memlist_sz);

	kmapsz = (u_int)(SYSEND - SYSBASE);
	kmapsz >>= MMU_PAGESHIFT;
	mapinit(kernelmap, (long)(kmapsz - 1), (u_long)1,
		"kernel map", kernelmap_sz / sizeof (struct map));
	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

	/*
	 * Remove the space used by BOP_ALLOC from the kernelmap
	 * plus the area actually used by the OBP (if any)
	 * ignoring virtual addresses in virt_avail, above Syslimit.
	 *
	 * Note that we handle sysbase/syslimit as u_int via a temporary
	 * variable to workaround compiler bug 1124059. sysbase and syslimt
	 * have the same address value as Sysbase, Syslimit (respectively).
	 */

	virt_avail = memlist;
	copy_memlist(bootops->boot_mem->virtavail, &memlist);

	mutex_enter(&maplock(kernelmap));
	for (cur = virt_avail; cur->next; cur = cur->next) {
		u_longlong_t range_base, range_size;

		if ((range_base = cur->address + cur->size) <
		    (u_longlong_t)sysbase)
			continue;
		if (range_base >= (u_longlong_t)syslimit)
			break;
		/*
		 * Limit the range to end at Syslimit.
		 */
		range_size = MIN(cur->next->address,
		    (u_longlong_t)syslimit) - range_base;
		if (rmget(kernelmap, btop(range_size),
		    btop((u_int)range_base - sysbase)) == 0)
			prom_panic("can't remove OBP hole");
	}
	mutex_exit(&maplock(kernelmap));

	phys_avail = memlist;
	(void) copy_physavail(bootops->boot_mem->physavail, &memlist, 0, 0);

	/*
	 * Add any extra mem after e_text to physavail list.
	 */
	if (extra_et) {
		memlist_add(extra_etpa, (u_longlong_t)extra_et, &memlist,
			&phys_avail);
	}
	/*
	 * Add any extra nucleus mem to physavail list.
	 */
	if (ndata_remain_sz) {
		ASSERT(nalloc_end == (nalloc_base + ndata_remain_sz));
		memlist_add(va_to_pa(nalloc_base),
			(u_longlong_t)ndata_remain_sz, &memlist, &phys_avail);
	}

	/*
	 * Initialize the page structures from the memory lists.
	 */
	kphysm_init(pp_base, memseg_base, npages);

	availrmem_initial = availrmem = freemem;
	PRM_DEBUG(availrmem);

	/*
	 * Some of the locks depend on page_hashsz being set!
	 * kmem_init() depends on this; so, keep it here.
	 */
	mt_lock_init();

	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Initialize the kstat framework.
	 */
	kstat_init();
}

static void
startup_modules(void)
{
	extern void param_calc(int);
	extern void param_init(void);
	extern void create_va_to_tte(void);

	extern int maxusers;

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
#ifdef TRACE
	cmn_err(CE_CONT, "TRACE enabled\n");
#endif
#ifdef GPROF
	cmn_err(CE_CONT, "GPROF enabled\n");
#endif

	/*
	 * Let the platforms have a chance to change default
	 * values before reading system file.
	 */
	set_platform_defaults();

	/*
	 * Read system file, (to set maxusers, physmem.......)
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
	 * Initialize loadable module system and apply the 'set' commands
	 * gleaned from the /etc/system file.
	 */
	mod_setup();

	/*
	 * Initialize system parameters
	 */
	param_init();

	/*
	 * If debugger is in memory, note the pages it stole from physmem.
	 * XXX: Should this happen with V2 Proms?  I guess it would be
	 * set to zero in this case?
	 */
	if (boothowto & RB_DEBUG)
		dbug_mem = *dvec->dv_pages;
	else
		dbug_mem = 0;

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	/* Set segkp limits. */
	ncbase = DEBUGADDR;
	ncend = DEBUGADDR;

	/*
	 * Initialize the hat layer.
	 */
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	seg_init();

	/*
	 * Create the va>tte handler, so the prom can understand
	 * kernel translations.  The handler is installed later, just
	 * as we are about to take over the trap table from the prom.
	 */
	create_va_to_tte();

	/*
	 * If obpdebug or forthdebug is set, load the obpsym kernel
	 * symbol support module, now.
	 */
	if ((obpdebug) || (forthdebug)) {
		obpdebug = 1;
		(void) modload("misc", "obpsym");
	}

	/*
	 * Load the forthdebugger if forthdebug is set.
	 */
	if (forthdebug) {
		extern void forthdebug_init(void);
		forthdebug_init();
	}

	if (modloadonly("fs", "specfs") == -1)
		halt("Can't load specfs");

	if (modloadonly("misc", "swapgeneric") == -1)
		halt("Can't load swapgeneric");

	dispinit(NULL);

	setup_ddi();

	/*
	 * Lets take this opportunity to load the the root device.
	 */
	if (loadrootmodules() != 0)
		debug_enter("Can't load the root filesystem");
}

static void
startup_bop_gone(void)
{
	struct memlist *pmem;
	extern int ecache_size;

	/*
	 * Call back into boot and release boots resources.
	 */
	BOP_QUIESCE_IO(bootops);

	/*
	 * Copy physinstalled list into kernel space.
	 */
	phys_install = memlist;
	copy_memlist(bootops->boot_mem->physinstalled, &memlist);

	/*
	 * setup physically contiguous area twice as large as the ecache.
	 * this is used while doing displacement flush of ecaches
	 */
	for (pmem = phys_install; pmem; pmem = pmem->next) {
		if (pmem->size >= (u_longlong_t)ecache_size * 2) {
			ecache_flushaddr = pmem->address;
			break;
		}
	}
	if (pmem == NULL) {
		cmn_err(CE_PANIC, "startup: no memory to set ecache_flushaddr");
	}

	/*
	 * Virtual available next.
	 */
	virt_avail = memlist;
	copy_memlist(bootops->boot_mem->virtavail, &memlist);

	/*
	 * Last chance to ask our booter questions ..
	 */

	/*
	 * For checkpoint-resume:
	 * Get kadb start address from prom "debugger-start" property,
	 * which is the same as segkp_limit at this point.
	 */
	debug_start_va = 0;
	(void) BOP_GETPROP(bootops, "debugger-start", (caddr_t)&debug_start_va);
}


/*
 * startup_fixup_physavail - called from mach_sfmmu.c after the final
 * allocations have been performed.  We can't call it in startup_bop_gone
 * since later operations can cause obp to allocate more memory.
 */
void
startup_fixup_physavail(void)
{
	struct memlist *cur;

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

	/*
	 * Copy phys_avail list, again.
	 * Both the kernel/boot and the prom have been allocating
	 * from the original list we copied earlier.
	 */
	cur = memlist;
	(void) copy_physavail(bootops->boot_mem->physavail, &memlist, 0, 0);

	/*
	 * Make sure we add any memory we added back to the old list.
	 */
	if (extra_et) {
		memlist_add(extra_etpa, (u_longlong_t)extra_et, &memlist,
		    &cur);
	}
	if (ndata_remain_sz) {
		memlist_add(va_to_pa(nalloc_base),
		    (u_longlong_t)ndata_remain_sz, &memlist, &cur);
	}

	/*
	 * There isn't any bounds checking on the memlist area
	 * so ensure it hasn't grown into kernelmap.
	 */
	if ((caddr_t)memlist > (caddr_t)kernelmap)
		cmn_err(CE_PANIC, "startup: memlist size exceeded");

	/*
	 * The kernel removes the pages that were allocated for it from
	 * the freelist, but we now have to find any -extra- pages that
	 * the prom has allocated for it's own book-keeping, and remove
	 * them from the freelist too. sigh.
	 */
	fix_prom_pages(phys_avail, cur);

	phys_avail = cur;

	/*
	 * We're done with boot.  Just after this point in time, boot
	 * gets unmapped, so we can no longer rely on its services.
	 * Zero the bootops to indicate this fact.
	 */
	bootops = (struct bootops *)NULL;
	BOOTOPS_GONE();
}

static void
startup_vm(void)
{
	register unsigned i;
	struct segmap_crargs a;
	u_longlong_t avmem;
	caddr_t va;
	int	max_virt_segkp;
	int	max_phys_segkp;
	int	mnode;
	extern caddr_t mm_map, cur_dump_addr, dump_addr;
	extern void hat_kern_setup(void);
	extern void install_va_to_tte(void);
	extern kmutex_t atomic_nc_mutex;
	extern void sfmmu_hblk_init();
	extern void page_freelist_coalesce(int);

	/*
	 * get prom's mappings, create hments for them and switch
	 * to the kernel context.
	 */
	hat_kern_setup();

	/*
	 * Take over trap table
	 */
	mutex_init(&atomic_nc_mutex, "non-$ atomic lock", MUTEX_DEFAULT, NULL);
	setup_trap_table();

	/*
	 * Install the va>tte handler, so that the prom can handle
	 * misses and understand the kernel table layout in case
	 * we need call into the prom.
	 */
	install_va_to_tte();

	/*
	 * Set a flag to indicate that the tba has been taken over.
	 */
	tba_taken_over = 1;

	/*
	 * The boot cpu can now take interrupts, x-calls, x-traps
	 */
	CPUSET_ADD(cpu_ready_set, CPU->cpu_id);
	CPU->cpu_flags |= (CPU_READY | CPU_ENABLE | CPU_EXISTS);

	/*
	 * Set a flag to tell write_scb_int() that it can access V_TBR_WR_ADDR.
	 */
	tbr_wr_addr_inited = 1;

	/*
	 * Initialize VM system, and map kernel address space.
	 */
	kvm_init();

	/*
	 * Setup a map for translating kernel virtual addresses;
	 * used by dump and libkvm.
	 */
	setup_kvpm();

	/*
	 * XXX4U: previously, we initialized and turned on
	 * the caches at this point. But of course we have
	 * nothing to do, as the prom has already done this
	 * for us -- main memory must be E$able at all times.
	 */

	/*
	 * Allocate a vm slot for the dev mem driver, and 2 slots for dump.
	 * XXX - this should be done differently, see ppcopy.
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

		cmn_err(CE_WARN, "limiting physmem to %d pages", physmem);

		off = 0;
		diff = npages - physmem;
		diff -= mmu_btopr(diff * sizeof (struct machpage));
		while (diff--) {
			pp = page_create_va(&unused_pages_vp, (offset_t)off,
				MMU_PAGESIZE, PG_WAIT | PG_EXCL,
				&kas, (caddr_t)off);
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
	 * that stolen by a debugger. printf currently can't
	 * print >4G, so do the trailing zero hack because we
	 * know the number is a multiple of 0x1000.
	 */
	cmn_err(CE_CONT, "?mem = %dK (0x%x000)\n",
	    (physinstalled - dbug_mem) << (PAGESHIFT - 10),
	    (physinstalled - dbug_mem) << (PAGESHIFT - 12));

	/*
	 * cmn_err doesn't do long long's and %u is treated
	 * just like %d, so we do this hack to get decimals
	 * > 2G printed.
	 */
	avmem = (u_longlong_t)freemem << PAGESHIFT;
	if (avmem >= 0x80000000ull)
		cmn_err(CE_CONT, "?avail mem = %d%d\n",
		    (u_int)(avmem / (1000 * 1000 * 1000)),
		    (u_int)(avmem % (1000 * 1000 * 1000)));
	else
		cmn_err(CE_CONT, "?avail mem = %d\n", (u_int)avmem);

	/*
	 * Initialize the segkp segment type.  We position it
	 * after the configured tables and buffers (whose end
	 * is given by econtig) and before V_WKBASE_ADDR.
	 * Also in this area are the debugger (if present)
	 * and segkmap (size SEGMAPSIZE).
	 */

	/* XXX - cache alignment? */
	va = (caddr_t)SEGKPBASE;
	ASSERT(((u_int)va & PAGEOFFSET) == 0);

	max_virt_segkp = btop(SEGKPSIZE);
	max_phys_segkp = (physmem * 2);
	i = ptob(min(max_virt_segkp, max_phys_segkp));

	/*
	 * 1201049: segkmap assumes that its segment base and size are
	 * at least MAXBSIZE aligned.  We can guarantee this without
	 * introducing a hole in the kernel address space by ensuring
	 * that the previous segment -- segkp -- *ends* on a MAXBSIZE
	 * boundary.  (Avoiding a hole between segkp and segkmap is just
	 * paranoia in case anyone assumes that they're contiguous.)
	 *
	 * The following statement ensures that (va + i) is at least
	 * MAXBSIZE aligned.  Note that it also results in correct page
	 * alignment regardless of page size (exercise for the reader).
	 */
	i -= (u_int)va & MAXBOFFSET;

	rw_enter(&kas.a_lock, RW_WRITER);
	segkp = seg_alloc(&kas, va, i);
	if (segkp == NULL)
		cmn_err(CE_PANIC, "startup: cannot allocate segkp");
	if (segkp_create(segkp) != 0)
		cmn_err(CE_PANIC, "startup: segkp_create failed");
	rw_exit(&kas.a_lock);

	/*
	 * Now create generic mapping segment.  This mapping
	 * goes SEGMAPSIZE beyond SEGMAPBASE.  But if the total
	 * virtual address is greater than the amount of free
	 * memory that is available, then we trim back the
	 * segment size to that amount
	 */
	va = (caddr_t)SEGMAPBASE;

	/*
	 * 1201049: segkmap base address must be MAXBSIZE aligned
	 */
	ASSERT(((u_int)va & MAXBOFFSET) == 0);

	i = SEGMAPSIZE;
	if (btopr(i) > freemem)
		i = mmu_ptob(freemem);
	i &= MAXBMASK;	/* 1201049: segkmap size must be MAXBSIZE aligned */

	rw_enter(&kas.a_lock, RW_WRITER);
	segkmap = seg_alloc(&kas, va, i);
	if (segkmap == NULL)
		cmn_err(CE_PANIC, "cannot allocate segkmap");

	a.prot = PROT_READ | PROT_WRITE;
	a.shmsize = shm_alignment;
	a.nfreelist = 4;

	if (segmap_create(segkmap, (caddr_t)&a) != 0)
		panic("segmap_create segkmap");
	rw_exit(&kas.a_lock);

	/*
	 * Create a segment for kadb for checkpoint-resume.
	 */
	if (debug_start_va != 0) {
		rw_enter(&kas.a_lock, RW_WRITER);
		seg_debug = seg_alloc(&kas, (caddr_t)debug_start_va,
			DEBUGSIZE);
		if (seg_debug == NULL)
			cmn_err(CE_PANIC, "cannot allocate seg_debug");
		(void) segkmem_create(seg_debug, (caddr_t)NULL);
		rw_exit(&kas.a_lock);
	}

	/*
	 * Allocate initial hmeblks for the sfmmu hat layer, could not do
	 * it in sfmmu_init(). XXX is there a better place for this ?
	 */
	sfmmu_hblk_init();

	/*
	 * Coalesce the freelist.
	 */
	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++)
		if (mem_node_config[mnode].exists)
			page_freelist_coalesce(mnode);
}

static void
startup_end()
{
	extern kmutex_t panic_lock;
	extern int memscrub_init(void);

	/*
	 * Initialize interrupt related stuff
	 */
	init_intr_threads(CPU);
	tickint_init();			/* Tick_Compare register interrupts */

	(void) splzs();			/* allow hi clock ints but not zs */

	/*
	 * DO NOT MOVE THIS BEFORE mod_setup() is called since
	 * _db_install() is in a loadable module that will be
	 * loaded on demand.
	 */
	{
		/* XXX Is this stuff needed anymore? */
		extern int gdbon;
		extern void _db_install(void);

		if (gdbon)
			_db_install();
	}

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
	 * Initialize errors.
	 */
	error_init();

	/*
	 * Startup memory scrubber.
	 */
	if (memscrub_init()) {
		cmn_err(CE_WARN, "Memory scrubber failed to initialize");
	}

	/*
	 * initializize the mutex used by panic code.
	 */
	mutex_init(&panic_lock, "panic lock", MUTEX_DEFAULT, NULL);

	/*
	 * Install the "real" pre-emption guards before DDI services
	 * are available.
	 */
	mutex_init(&prom_mutex, "prom mutex", MUTEX_DEFAULT, NULL);
	cv_init(&prom_cv, "prom cv", CV_DEFAULT, NULL);
	(void) prom_set_preprom(kern_preprom);
	(void) prom_set_postprom(kern_postprom);
	CPU->cpu_m.mutex_ready = 1;
	start_mon_clock();

	/*
	 * Configure the root devinfo node.
	 */
	configure();		/* set up devices */
}

static void
setup_trap_table()
{
	extern struct scb trap_table;
	extern setwstate();

	intr_init(CPU);			/* init interrupt request free list */
	setwstate(WSTATE_KERN);
	prom_set_traptable((void *)&trap_table);
}

void
post_startup(void)
{
#ifdef	PTL1_PANIC_DEBUG
	extern void init_tl1_panic_thread(void);
#endif	/* PTL1_PANIC_DEBUG */

	/*
	 * Configure the rest of the system.
	 * Perform forceloading tasks for /etc/system.
	 */
	(void) mod_sysctl(SYS_FORCELOAD, NULL);
	/*
	 * ON4.0: Force /proc module in until clock interrupt handle fixed
	 * ON4.0: This must be fixed or restated in /etc/systems.
	 */
	(void) modload("fs", "procfs");

	load_platform_drivers();

	/* load vis simulation module, if we are running w/fpu off */
	if (!fpu_exists) {
		if (modload("misc", "vis") == -1)
			halt("Can't load vis");
	}

	maxmem = freemem;

	(void) spl0();		/* allow interrupts */
	if (snooping)
		tickint_clnt_add(deadman, snoop_interval);
#ifdef	PTL1_PANIC_DEBUG
	init_tl1_panic_thread();
#endif	/* PTL1_PANIC_DEBUG */
}

#ifdef	PTL1_PANIC_DEBUG
int		test_tl1_panic = 0;
kthread_id_t	tl1_panic_thread = NULL;
kcondvar_t	tl1_panic_cv;
kmutex_t	tl1_panic_mutex;

void
tl1_panic_recurse(int n)
{
	if (n != 0)
		tl1_panic_recurse(n - 1);
	else
		asm("ta	0x7C");
}

void
tl1_panic_wakeup(void)
{
	mutex_enter(&tl1_panic_mutex);
	cv_signal(&tl1_panic_cv);
	mutex_exit(&tl1_panic_mutex);
}

void
ptl1_panic_thread(void)
{
	int n = 8;

	mutex_enter(&tl1_panic_mutex);
	while (tl1_panic_thread) {
		if (test_tl1_panic) {
			test_tl1_panic = 0;
			tl1_panic_recurse(n);
		}
		(void) timeout(tl1_panic_wakeup, NULL, 60);
		(void) cv_wait(&tl1_panic_cv, &tl1_panic_mutex);
	}
	mutex_exit(&tl1_panic_mutex);
}

void
init_tl1_panic_thread(void)
{
	kthread_id_t tp;

	mutex_init(&tl1_panic_mutex, "tl1 panic test Mutex",
		MUTEX_DEFAULT, DEFAULT_WT);
	cv_init(&tl1_panic_cv, "tl1 panic test CV", CV_DEFAULT, NULL);
	tp = thread_create(NULL, PAGESIZE, ptl1_panic_thread,
		NULL, 0, &p0, TS_RUN, 0);
	if (tp == NULL) {
		cmn_err(CE_WARN,
			"init_tl1_panic_thread: cannot start tl1 panic thread");
		cv_destroy(&tl1_panic_cv);
		return;
	}
	tl1_panic_thread = tp;
}
#endif	/* PTL1_PANIC_DEBUG */

/*
 * These routines are called immediately before and
 * immediately after calling into the firmware.  The
 * firmware is significantly confused by preemption -
 * particularly on MP machines - but also on UP's too.
 *
 * Cheese alert.
 *
 * We have to handle the fact that when slave cpus start, they
 * aren't yet read for mutex's (i.e. they are still running on
 * the prom's tlb handlers, so they will fault if they touch
 * curthread).
 *
 * To handle this, the cas on prom_cpu is the actual lock, the
 * mutex is so "adult" cpus can cv_wait/cv_signal themselves.
 * This routine degenerates to a spin lock anytime a "juvenile"
 * cpu has the lock.
 */
static void
kern_preprom(void)
{
	struct cpu *cp, *prcp;
	extern int cas(int *, int, int);
	extern void membar_consumer();
#ifdef DEBUG
	extern greg_t getpil();
#endif /* DEBUG */

	for (;;) {
		cp = cpu[getprocessorid()];
		if (cp->cpu_m.mutex_ready) {
			/*
			 * Disable premption, and re-validate cp.  We can't
			 * move from a mutex_ready cpu to a non mutex_ready
			 * cpu, so just getting the current cpu is ok.
			 *
			 * Try the lock.  If we dont't get the lock,
			 * re-enable preemption and see if we should
			 * sleep.
			 */
			kpreempt_disable();
			cp = CPU;
			if (cas((int *)&prom_cpu, 0, (u_int)cp) == 0)
				break;
			kpreempt_enable();
			/*
			 * We have to be very careful here since both
			 * prom_cpu and prcp->cpu_m.mutex_ready can
			 * be changed at any time by a non mutex_ready
			 * cpu.
			 *
			 * If prom_cpu is mutex_ready, prom_mutex
			 * protects prom_cpu being cleared on us.
			 * If prom_cpu isn't mutex_ready, we only know
			 * it will change prom_cpu before changing
			 * cpu_m.mutex_ready, so we invert the check
			 * order with a membar in between to make sure
			 * the lock holder really will wake us.
			 */
			mutex_enter(&prom_mutex);
			prcp = prom_cpu;
			if (prcp != NULL && prcp->cpu_m.mutex_ready != 0) {
				membar_consumer();
				if (prcp == prom_cpu)
					cv_wait(&prom_cv, &prom_mutex);
			}
			mutex_exit(&prom_mutex);
			/*
			 * Check for panic'ing.
			 */
			if (panicstr) {
				panic_hook();
				return;
			}
		} else {
			/*
			 * Non mutex_ready cpus just grab the lock
			 * and run with it.
			 */
			ASSERT(getpil() == PIL_MAX);
			if (cas((int *)&prom_cpu, 0, (u_int)cp) == 0)
				break;
		}
	}
}

static void
kern_postprom(void)
{
	struct cpu *cp;
	extern void membar_producer();

	cp = cpu[getprocessorid()];
	ASSERT(prom_cpu == cp || panicstr);
	if (cp->cpu_m.mutex_ready) {
		kpreempt_enable();
		mutex_enter(&prom_mutex);
		prom_cpu = NULL;
		cv_signal(&prom_cv);
		mutex_exit(&prom_mutex);
	} else {
		prom_cpu = NULL;
		membar_producer();
	}
}

/*
 * Add to a memory list.
 * start = start of new memory segment
 * len = length of new memory segment in bytes
 * memlistp = pointer to array of available memory segment structures
 * curmemlistp = memory list to which to add segment.
 */
static void
memlist_add(u_longlong_t start, u_longlong_t len, struct memlist **memlistp,
	struct memlist **curmemlistp)
{
	struct memlist *cur, *new, *last;
	u_longlong_t end = start + len;

	new = *memlistp;
	new->address = start;
	new->size = len;
	*memlistp = new + 1;
	for (cur = *curmemlistp; cur; cur = cur->next) {
		last = cur;
		if (cur->address >= end) {
			new->next = cur;
			new->prev = cur->prev;
			cur->prev = new;
			if (cur == *curmemlistp)
				*curmemlistp = new;
			else
				new->prev->next = new;
			return;
		}
		if (cur->address + cur->size > start)
			panic("munged memory list = 0x%x\n", curmemlistp);
	}
	new->next = NULL;
	new->prev = last;
	last->next = new;
}

/*
 * In the case of architectures that support dynamic addition of
 * memory at run-time there are two cases where memsegs need to
 * be initialized and added to the memseg list.
 * 1) memsegs that are contructed at startup.
 * 2) memsegs that are constructed at run-time on
 *    hot-plug capable architectures.
 * This code was originally part of the function kphysm_init().
 */

static void
memseg_list_add(struct memseg *memsegp)
{
	struct memseg *tmp_memseg;
	struct memseg **prev_memsegp;

	/* insert in memseg list, decreasing number of pages order */

	if (memsegs == (struct memseg *)NULL) {
		memsegs = memsegp;
	} else {
		for (prev_memsegp = &memsegs, tmp_memseg = memsegs;
		    tmp_memseg; prev_memsegp = &(tmp_memseg->next),
		    tmp_memseg = tmp_memseg->next) {
			if (npages > tmp_memseg->pages_end -
			    tmp_memseg->pages_base)
				break;
		}

		memsegp->next = *prev_memsegp;
		*prev_memsegp = memsegp;
	}
}

/*
 * kphysm_init() tackles the problem of initializing physical memory.
 * The old startup made some assumptions about the kernel living in
 * physically contiguous space which is no longer valid.
 */
static void
kphysm_init(machpage_t *pp, struct memseg *memsegp, u_int npages)
{
	struct memlist *pmem;
	struct memseg *cur_memseg;
	u_long pnum;
	extern void page_coloring_init(void);

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

		memseg_list_add(cur_memseg);

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
 * Kernel VM initialization.
 * Assumptions about kernel address space ordering:
 *	(1) gap (user space)
 *	(2) kernel text
 *	(3) kernel data/bss
 *	(4) gap
 *	(5) kernel data structures
 *	(6) gap
 *	(7) debugger (optional)
 *	(8) monitor
 *	(9) gap (possibly null)
 *	(10) dvma
 *	(11) devices
 */
static void
kvm_init(void)
{
	u_int pfnum;
	struct memlist *cur;
	u_int syslimit = (u_int)Syslimit;	/* See: 1124059 */
	u_int sysbase = (u_int)Sysbase;		/* See: 1124059 */
	extern caddr_t e_text;
	extern caddr_t e_data;
	extern int segkmem_ready;

#ifndef KVM_DEBUG
#define	KVM_DEBUG 0	/* 0 = no debugging, 1 = debugging */
#endif

#if KVM_DEBUG > 0
#define	KVM_HERE \
	printf("kvm_init: checkpoint %d line %d\n", ++kvm_here, __LINE__);
#define	KVM_DONE	{ printf("kvm_init: all done\n"); kvm_here = 0; }
	int kvm_here = 0;
#else
#define	KVM_HERE
#define	KVM_DONE
#endif

KVM_HERE
	/*
	 * Put the kernel segments in kernel address space.  Make it a
	 * "kernel memory" segment objects.
	 */
	rw_enter(&kas.a_lock, RW_WRITER);

	(void) seg_attach(&kas, (caddr_t)KERNELBASE,
	    (u_int)(e_data - KERNELBASE), &ktextseg);
	(void) segkmem_create(&ktextseg, (caddr_t)NULL);

KVM_HERE
	(void) seg_attach(&kas, (caddr_t)valloc_base, (u_int)econtig -
		(u_int)valloc_base, &kvalloc);
	(void) segkmem_create(&kvalloc, (caddr_t)NULL);

KVM_HERE
	/*
	 * We're about to map out /boot.  This is the beginning of the
	 * system resource management transition. We can no longer
	 * call into /boot for I/O or memory allocations.
	 */
	(void) seg_attach(&kas, (caddr_t)SYSBASE,
	    (u_int)(Syslimit - SYSBASE), &kvseg);
	(void) segkmem_create(&kvseg, (caddr_t)NULL);

	rw_exit(&kas.a_lock);

KVM_HERE
	rw_enter(&kas.a_lock, RW_READER);

	/*
	 * Now we can ask segkmem for memory instead of boot.
	 */
	segkmem_ready = 1;

	/*
	 * Validate to Syslimit.  There may be several fragments of
	 * 'used' virtual memory in this range, so we hunt 'em all down.
	 *
	 * Note that we handle sysbase/syslimit as u_ints via a temporary
	 * variable to workaround compiler bug 1124059. sysbase and syslimt
	 * have the same address value as Sysbase, Syslimit (respectively).
	 */
	for (cur = virt_avail; cur->next; cur = cur->next) {
		u_longlong_t range_base, range_size;

		if ((range_base = cur->address + cur->size) <
		    (u_longlong_t)sysbase)
			continue;
		if (range_base >= (u_longlong_t)syslimit)
			break;
		/*
		 * Limit the range to end at Syslimit.
		 */
		range_size = MIN(cur->next->address, (u_longlong_t)syslimit) -
		    range_base;
		(void) as_setprot(&kas, (caddr_t)range_base, (u_int)range_size,
		    PROT_READ | PROT_WRITE | PROT_EXEC);
	}

	/*
	 * Invalidate unused portion of the region managed by kernelmap.
	 * (We know that the PROM never allocates any mappings here by
	 * itself without updating the 'virt-avail' list, so that we can
	 * simply render anything that is on the 'virt-avail' list invalid)
	 * (Making sure to ignore virtual addresses above 2**32.)
	 *
	 * Note that we handle sysbase/syslimit as u_int via a temporary
	 * variable to workaround compiler bug 1124059. sysbase and syslimt
	 * have the same address value as Sysbase, Syslimit (respectively).
	 */
	for (cur = virt_avail; cur && cur->address < (u_longlong_t)syslimit;
	    cur = cur->next) {
		u_longlong_t range_base, range_end;

		range_base = MAX(cur->address, (u_longlong_t)sysbase);
		range_end  = MIN(cur->address + cur->size,
		    (u_longlong_t)syslimit);
		if (range_end > range_base)
			as_setprot(&kas, (caddr_t)range_base,
			    (u_int)(range_end - range_base), 0);
	}
	rw_exit(&kas.a_lock);

	/*
	 * Find the begining page frames of the kernel data
	 * segment and the ending page frame (-1) for bss.
	 */
	/*
	 * FIXME - nobody seems to use them but we could later on.
	 */
	pfnum = va_to_pfn((caddr_t)roundup((u_int)e_text, DATA_ALIGN));
	if (pfnum != (u_int)-1)
		kpfn_dataseg = pfnum;
	if ((pfnum = va_to_pfn(e_data)) != -1)
		kpfn_endbss = pfnum;

KVM_DONE
}

static void
setup_kvpm(void)
{
	u_int va;
	int i = 0;
	u_int pages = 0;
	u_int pfnum, nextpfnum;
	u_int lastpfnum;
	u_int npgs, pfn;

	lastpfnum = (u_int)-1;
	npgs = btop(MMU_PAGESIZE);

	rw_enter(&kas.a_lock, RW_READER);
	for (va = (u_int)KERNELBASE; va < (u_int)econtig; va += MMU_PAGESIZE) {
		pfnum = hat_getpfnum(kas.a_hat, (caddr_t)va);
		if (pfnum != (u_int)-1) {
			if (lastpfnum == (u_int)-1) {
				/* first pfnum on this entry */
				lastpfnum = pfnum;
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
				pages = npgs;
				nextpfnum = lastpfnum + npgs;
			} else if (nextpfnum == pfnum) {
				/* contiguous pfn so update current entry */
				nextpfnum = pfnum + npgs;
				pages += npgs;
			} else {
				/* not contiguous so end current entry and */
				/* start new one */
				kvtopdata.kvtopmap[i].kvpm_len = pages;
				lastpfnum = pfnum;
				pages = npgs;
				nextpfnum = lastpfnum + npgs;
				i++;
				if (i >= NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					pages = 0;
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
		pfn = va_to_pfn((caddr_t)va);
		if (pfn != (u_int) -1) {
			if (lastpfnum == (u_int) -1) {
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
				i++;
				if (i >= NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					break;
				}
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
			}
		} else if (lastpfnum != (u_int) -1) {
			lastpfnum = (u_int) -1;
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

	rw_exit(&kas.a_lock);

	msgbuf.msg_map = va_to_pa((caddr_t)&kvtopdata);
}

/*
 * Use boot to allocate the physical memory needed for the IOMMU's TSB arrays.
 * Memory allocated for IOMMU TSBs is accessed via virtual addresses.
 * We can relinquish the virtual address mappings to this space if IOMMU
 * code uses physical addresses with MMU bypass mode, but virtual addresses
 * are easier to deal with.
 *
 * WARNING - since this routine uses boot to allocate memory, it MUST
 * be called before the kernel takes over memory allocation from boot.
 */
caddr_t iommu_tsb_vaddr[MAX_UPA];
int iommu_tsb_alloc_size[MAX_UPA];
dnode_t iommu_nodes[MAX_UPA];

caddr_t
iommu_tsb_alloc(caddr_t alloc_base)
{
	char name[128];
	char compatible[128];
	caddr_t vaddr;
	int i, total_size, size;
	caddr_t iommu_alloc_base = (caddr_t)roundup((u_int)alloc_base,
	    MMU_PAGESIZE);

	/*
	 * determine the amount of physical memory required for the TSB arrays
	 *
	 * assumes iommu_tsb_alloc_size[] has already been initialized, i.e.
	 * map_wellknown_devices()
	 */
	for (i = total_size = 0; i < MAX_UPA; i++) {

		if (iommu_nodes[i] == NULL)
			continue;

		/*
		 * If the system has 32 mb or less of physical memory,
		 * allocate enough space for 64 mb of dvma space.
		 * Otherwise allocate enough space for 256 mb of dvma
		 * space.
		 */
		(void) prom_getprop(iommu_nodes[i], "name", name);
		if (strcmp(name, "sbus") == 0) {

			if (sbus_iommu_tsb_alloc_size != 0)
				size = sbus_iommu_tsb_alloc_size;
			else
				size = (physinstalled <= 0x1000 ?
						0x10000 : 0x40000);
		} else if (strcmp(name, "pci") == 0) {

			if (pci_iommu_tsb_alloc_size != 0)
				size = pci_iommu_tsb_alloc_size;
			else
				size = (physinstalled <= 0x1000 ?
						0x10000 : 0x40000);

			/*
			 * PsychoNG (schizo) has two iommu's so we must
			 * allocate twice the amount of space.
			 */
			(void) prom_getprop(iommu_nodes[i], "compatible",
						compatible);
			if (strcmp(compatible, "pci108e,8001") == 0)
				size = size * 2;
		} else
			continue;	/* unknown i/o bus bridge */

		total_size += size;
		iommu_tsb_alloc_size[i] = size;
	}

	if (total_size == 0)
		return (alloc_base);

	/*
	 * allocate the physical memory for the TSB arrays
	 */
	if ((vaddr = (caddr_t)BOP_ALLOC(bootops, iommu_alloc_base,
	    total_size, MMU_PAGESIZE)) == NULL)
		cmn_err(CE_PANIC, "Cannot allocate IOMMU TSB arrays");

	/*
	 * assign the virtual addresses for each TSB
	 */
	for (i = 0; i < MAX_UPA; i++) {
		if ((size = iommu_tsb_alloc_size[i]) != 0) {
			iommu_tsb_vaddr[i] = vaddr;
			vaddr += size;
		}
	}

	return (iommu_alloc_base + total_size);
}

char obp_tte_str[] =
	"h# %x constant MMU_PAGESHIFT "
	"h# %x constant TTE8K "
	"h# %x constant SFHME_SIZE "
	"h# %x constant SFHME_TTE "
	"h# %x constant HMEBLK_TAG "
	"h# %x constant HMEBLK_NEXT "
	"h# %x constant HMEBLK_MISC "
	"h# %x constant HMEBLK_HME1 "
	"h# %x constant NHMENTS "
	"h# %x constant HBLK_SZMASK "
	"h# %x constant HBLK_RANGE_SHIFT "
	"h# %x constant HMEBP_HBLK "
	"h# %x constant HMEBUCKET_SIZE "
	"h# %x constant HTAG_SFMMUPSZ "
	"h# %x constant HTAG_REHASHSZ "
	"h# %x constant MAX_HASHCNT "
	"h# %x constant uhme_hash "
	"h# %x constant khme_hash "
	"h# %x constant UHMEHASH_SZ "
	"h# %x constant KHMEHASH_SZ "
	"h# %x constant KHATID "
	"h# %x constant CTX_SIZE "
	"h# %x constant CTX_SFMMU "
	"h# %x constant ctxs "
	"h# %x constant ASI_MEM "

	": PHYS-X@ ( phys -- data ) "
	"   ASI_MEM spacex@ "
	"; "

	": PHYS-W@ ( phys -- data ) "
	"   ASI_MEM spacew@ "
	"; "

	": PHYS-L@ ( phys -- data ) "
	"   ASI_MEM spacel@ "
	"; "

	": TTE_PAGE_SHIFT ( ttesz -- hmeshift ) "
	"   3 * MMU_PAGESHIFT + "
	"; "

	": TTE_IS_VALID ( ttep -- flag ) "
	"   PHYS-X@ 0< "
	"; "

	": HME_HASH_SHIFT ( ttesz -- hmeshift ) "
	"   dup TTE8K =  if "
	"      drop HBLK_RANGE_SHIFT "
	"   else "
	"      TTE_PAGE_SHIFT "
	"   then "
	"; "

	": HME_HASH_BSPAGE ( addr hmeshift -- bspage ) "
	"   tuck >> swap MMU_PAGESHIFT - << "
	"; "

	": HME_HASH_FUNCTION ( sfmmup addr hmeshift -- hmebp ) "
	"   >> over xor swap                    ( hash sfmmup ) "
	"   KHATID <>  if                       ( hash ) "
	"      UHMEHASH_SZ and                  ( bucket ) "
	"      HMEBUCKET_SIZE * uhme_hash +     ( hmebp ) "
	"   else                                ( hash ) "
	"      KHMEHASH_SZ and                  ( bucket ) "
	"      HMEBUCKET_SIZE * khme_hash +     ( hmebp ) "
	"   then                                ( hmebp ) "
	"; "

	": HME_HASH_TABLE_SEARCH ( hmebp hblktag -- null | hmeblkp ) "
	"   >r HMEBP_HBLK + x@			( hmeblkp ) ( r: hblktag ) "
	"   begin                               ( hmeblkp ) ( r: hblktag ) "
	"      dup  if                          ( hmeblkp ) ( r: hblktag ) "
	"         dup HMEBLK_TAG + PHYS-X@ r@ =  if ( hmeblkp ) ( r: hblktag ) "
	"            true                       ( hmeblkp true ) "
						"( r: hblktag ) "
	"         else                          ( hmeblkp ) ( r: hblktag ) "
	"            HMEBLK_NEXT + PHYS-X@ false     ( hmeblkp' false ) "
						"( r: hblktag ) "
	"         then                          ( hmeblkp flag ) "
						"( r: hblktag ) "
	"      else                             ( null ) ( r: hblktag ) "
	"         true                          ( null true ) ( r: hblktag ) "
	"      then                             ( hmeblkp flag ) "
						"( r: hblktag ) "
	"   until                               ( null | hmeblkp ) "
						"( r: hblktag ) "
	"   r> drop                             ( null | hmeblkp ) "
	"; "

	": CNUM_TO_SFMMUP ( cnum -- sfmmup ) "
	"   CTX_SIZE * ctxs + CTX_SFMMU + l@ "
	"; "

	": HME_HASH_TAG ( sfmmup rehash addr -- hblktag ) "
	"   over HME_HASH_SHIFT HME_HASH_BSPAGE      ( sfmmup rehash bspage ) "
	"   HTAG_REHASHSZ << or HTAG_SFMMUPSZ << or  ( hblktag ) "
	"; "

	": HBLK_TO_TTEP ( hmeblkp addr -- ttep ) "
	"   over HMEBLK_MISC + PHYS-L@ HBLK_SZMASK and  ( hmeblkp addr ttesz ) "
	"   TTE8K =  if                            ( hmeblkp addr ) "
	"      MMU_PAGESHIFT >> NHMENTS 1- and     ( hmeblkp hme-index ) "
	"   else                                   ( hmeblkp addr ) "
	"      drop 0                              ( hmeblkp 0 ) "
	"   then                                   ( hmeblkp hme-index ) "
	"   SFHME_SIZE * + HMEBLK_HME1 +           ( hmep ) "
	"   SFHME_TTE +                            ( ttep ) "
	"; "

	": unix-tte ( addr cnum -- false | tte-data true ) "
	"   over h# 20 >> 0<>  if             ( addr cnum ) "
	"      2drop false                    ( false ) "
	"   else                              ( addr cnum ) "
	"      CNUM_TO_SFMMUP                 ( addr sfmmup ) "
	"      MAX_HASHCNT 1+ 1  do           ( addr sfmmup ) "
	"         2dup swap i HME_HASH_SHIFT  "
					"( addr sfmmup sfmmup addr hmeshift ) "
	"         HME_HASH_FUNCTION           ( addr sfmmup hmebp ) "
	"         over i 4 pick               "
				"( addr sfmmup hmebp sfmmup rehash addr ) "
	"         HME_HASH_TAG                ( addr sfmmup hmebp hblktag ) "
	"         HME_HASH_TABLE_SEARCH       "
					"( addr sfmmup { null | hmeblkp } ) "
	"         ?dup  if                    ( addr sfmmup hmeblkp ) "
	"            nip swap HBLK_TO_TTEP    ( ttep ) "
	"            dup TTE_IS_VALID  if     ( valid-ttep ) "
	"               PHYS-X@ true          ( tte-data true ) "
	"            else                     ( invalid-tte ) "
	"               drop false            ( false ) "
	"            then                     ( false | tte-data true ) "
	"            unloop exit              ( false | tte-data true ) "
	"         then                        ( addr sfmmup ) "
	"      loop                           ( addr sfmmup ) "
	"      2drop false                    ( false ) "
	"   then                              ( false ) "
	"; "
;

void
create_va_to_tte(void)
{
	char *bp;
	extern int khmehash_num, uhmehash_num;
	extern struct hmehash_bucket *khme_hash, *uhme_hash;

#define	OFFSET(type, field)	((int)(&((type *)0)->field))

	bp = (char *)kobj_zalloc(MMU_PAGESIZE, KM_SLEEP);

	/*
	 * Teach obp how to parse our sw ttes.
	 */
	sprintf(bp, obp_tte_str,
		MMU_PAGESHIFT,
		TTE8K,
		sizeof (struct sf_hment),
		OFFSET(struct sf_hment, hme_tte),
		OFFSET(struct hme_blk, hblk_tag),
		OFFSET(struct hme_blk, hblk_nextpa),
		OFFSET(struct hme_blk, hblk_misc),
		OFFSET(struct hme_blk, hblk_hme),
		NHMENTS,
		HBLK_SZMASK,
		HBLK_RANGE_SHIFT,
		OFFSET(struct hmehash_bucket, hmeh_nextpa),
		sizeof (struct hmehash_bucket),
		HTAG_SFMMUPSZ,
		HTAG_REHASHSZ,
		MAX_HASHCNT,
		uhme_hash,
		khme_hash,
		UHMEHASH_SZ,
		KHMEHASH_SZ,
		KHATID,
		sizeof (struct ctx),
		OFFSET(struct ctx, c_sfmmu),
		ctxs,
		ASI_MEM);
	prom_interpret(bp, 0, 0, 0, 0, 0);

	kobj_free(bp, MMU_PAGESIZE);
}

void
install_va_to_tte(void)
{
	/*
	 * advise prom that he can use unix-tte
	 */
	prom_interpret("' unix-tte is va>tte-data", 0, 0, 0, 0, 0);
}


void
forthdebug_init(void)
{
	char *bp = NULL;
	struct _buf *file = NULL;
	int read_size, ch;
	int buf_size = 0;

	file = kobj_open_path(FDEBUGFILE, 1);
	if (file == (struct _buf *)-1) {
		cmn_err(CE_CONT, "Can't open %s\n", FDEBUGFILE);
		goto bad;
	}

	/*
	 * the first line should be \ <size>
	 * XXX it would have been nice if we could use lex() here
	 * instead of doing the parsing here
	 */
	while (((ch = kobj_getc(file)) != -1) && (ch != '\n')) {
		if ((ch) >= '0' && (ch) <= '9') {
			buf_size = buf_size * 10 + ch - '0';
		} else if (buf_size) {
			break;
		}
	}

	if (buf_size == 0) {
		cmn_err(CE_CONT, "can't determine size of %s\n", FDEBUGFILE);
		goto bad;
	}

	/*
	 * skip to next line
	 */
	while ((ch != '\n') && (ch != -1)) {
		ch = kobj_getc(file);
	}

	/*
	 * Download the debug file.
	 */
	bp = (char *)kobj_zalloc(buf_size, KM_SLEEP);
	read_size = kobj_read_file(file, bp, buf_size, 0);
	if (read_size < 0) {
		cmn_err(CE_CONT, "Failed to read in %s\n", FDEBUGFILE);
		goto bad;
	}
	if (read_size == buf_size && kobj_getc(file) != -1) {
		cmn_err(CE_CONT, "%s is larger than %d\n",
			FDEBUGFILE, buf_size);
		goto bad;
	}
	bp[read_size] = 0;
	cmn_err(CE_CONT, "Read %d bytes from %s\n", read_size, FDEBUGFILE);
	prom_interpret(bp, 0, 0, 0, 0, 0);

bad:
	if (file != (struct _buf *)-1) {
		kobj_close_file(file);
	}

	/*
	 * Make sure the bp is valid before calling kobj_free.
	 */
	if (bp != NULL) {
		kobj_free(bp, buf_size);
	}
}

struct mem_node_conf	mem_node_config[MAX_MEM_NODES];
int			mem_node_pfn_shift;

/*
 * This routine will differ on NUMA platforms.
 */

static void
startup_build_mem_nodes()
{
	/* LINTED */
	ASSERT(MAX_MEM_NODES == 1);

	mem_node_pfn_shift = 0;

	mem_node_config[0].exists = 1;
	mem_node_config[0].physmax = physmax;
}

/*
 * This routine will evolve on NUMA platforms.
 */
static void
mem_node_config_adjust(int mnode, int pnum)
{
	mem_node_config[mnode].physmax = pnum;
}

/*
 * Add a chunk of memory to the system.  page_t's for this memory
 * are allocated in the first few pages of the chunk.
 * base: starting PAGESIZE page of new memory.
 * npgs: length in PAGESIZE pages.
 *
 * Adding mem this way doesn't increase the size of the hash tables;
 * growing them would be too hard.  This should be OK, but adding memory
 * dynamically most likely means more hash misses, since the tables will
 * be smaller than they otherwise would be.
 */
int
kphysm_add_memory_dynamic(uint base, uint npgs)
{
	machpage_t	*pp;
	struct memseg	*seg;
	u_longlong_t	avmem;
	ulong		pfn;
	int		pp_pages;
	int		pp_giveback;
	int		pnum, mnode;

	extern uint	page_ctrs_adjust(int);

	cmn_err(CE_CONT,
	    "?kphysm_add_memory_dynamic: adding %dK at 0x%x000\n",
	    npgs << (PAGESHIFT - 10), base);

	/* Get pages needed for page_t's ... */

	pp_pages = btopr(npgs * sizeof (machpage_t));
	pp_giveback = btop(pp_pages * sizeof (machpage_t));

	while (pp_giveback > 1) {
		pp_pages += pp_giveback -
		    btopr(pp_giveback * sizeof (machpage_t));
		pp_giveback -= pp_giveback -
		    btopr(pp_giveback * sizeof (machpage_t));
	}

	/* Get an address in the kernel address map */

	pp = (machpage_t *)kmxtob(rmalloc(kernelmap, pp_pages));
	if (pp == (machpage_t *)NULL) {
		cmn_err(CE_WARN, "kphysm_add_memory_dynamic:"
		    "Can't allocate VA for page_ts");
		return (ENOMEM);
	}

	/*
	 * Map the page_t pages ...
	 * This should be changed to use hat_memload_array()
	 */

	{
		uint len = ptob(pp_pages);
		uint pf = base;
		caddr_t addr = (caddr_t)pp;

		while (len) {
			hat_devload(kas.a_hat, addr, len, pf,
			    PROT_ALL & ~PROT_USER,
			    HAT_LOAD|HAT_LOAD_LOCK|HAT_LOAD_NOCONSIST);
			len -= MMU_PAGESIZE;
			addr += MMU_PAGESIZE;
			pf++;
		}
	}

	/* See if we can touch the new memory ... */

	if (ddi_peek32((dev_info_t *)NULL,
	    (int32_t *)pp, (int32_t *)0) == DDI_FAILURE) {

		cmn_err(CE_WARN, "kphysm_add_memory_dynamic:"
		    "Can't access pp array at 0x%x [phys 0x%x]", pp, base);

		hat_unload(kas.a_hat, (caddr_t) pp, ptob(pp_pages),
		    HAT_UNLOAD_UNLOCK|HAT_LOAD_NOCONSIST);

		rmfree(kernelmap, pp_pages, (ulong_t)btokmx(pp));

		return (EFAULT);
	}

	bzero((caddr_t) pp, ptob(pp_pages));

	/*
	 * If physmax changes then we have to adjust it as well as
	 * resize and move the page_counters to accommidate
	 * an increase in memory pages. The nucleus does not have enough
	 * room to grow this data structure dynamically. We could
	 * consider saving excess nucleus pages on a special free list
	 * and do later allocations from there for data structures such
	 * as this one. It's something to think about.
	 */

	pnum = base + npgs;
	mnode = PFN_2_MEM_NODE(pnum);
	if (pnum > mem_node_config[mnode].physmax) {
		mem_node_config_adjust(mnode, pnum);
		if (page_ctrs_adjust(mnode) != 0)
			return (ENOMEM);
	}
	physmax = MAX(physmax, pnum);

	/*
	 * Initialize the mem_seg structure representing this memory
	 * and add it to the existing list of memsegs. Do some basic
	 * initialization and add the memory to the system. add_physmem()
	 * puts the pages on the appropriate free lists and updates the
	 * relevant page counters.
	 */

	npgs -= pp_pages;
	base += pp_pages;

	seg = (struct memseg *)kmem_zalloc(sizeof (struct memseg), KM_SLEEP);

	seg->pages = pp;
	seg->epages = pp + npgs;
	seg->pages_base = base;
	seg->pages_end = base + npgs;

	memseg_list_add(seg);

	pfn = seg->pages_base;
	for (pp = seg->pages; pp < seg->epages; pp++) {
		pp->p_pagenum = pfn;
		pfn++;
	}

	add_physmem((page_t *)seg->pages, npgs);
	build_pfn_hash();

	/* Need to reset a number of globals, since we increased memory */

	mutex_enter(&freemem_lock);

	maxmem += npgs;
	physinstalled += npgs;
	availrmem += npgs;

	if (pages_pp_maximum <= MAX(tune.t_minarmem+100, availrmem/10))
		pages_pp_maximum = MAX(tune.t_minarmem+100, availrmem/10);

	mutex_exit(&freemem_lock);

	setupclock(1);	/* Let pageout know about the new memory */

	cmn_err(CE_CONT, "?kphysm_add_memory_dynamic: mem = %dK (0x%x000)\n",
	    physinstalled << (PAGESHIFT - 10), physinstalled);

	avmem = (u_longlong_t)freemem << PAGESHIFT;
	cmn_err(CE_CONT, "?kphysm_add_memory_dynamic:"
	    "avail mem = %lld\n", avmem);

	return (0);		/* Successfully added system memory */

}
