/*
 * Copyright (c) 1992-1996 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)ppc_memlist.c	1.7	96/08/06 SMI"

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/openprom.h>	/* only for struct prom_memlist */
#include <sys/bootconf.h>
#include <sys/promimpl.h>	/* XXX: prom_decode_int is a macro */
#include <sys/salib.h>

/*
 * This file defines the interface from the prom and platform-dependent
 * form of the memory lists, to boot's more generic form of the memory
 * list.  For PowerPC, the memory list properties are address, size
 * which is similar to boot's format, except boot's format is a linked
 * list, and the prom's is an array of these structures. Note that the
 * these values are encoded as encode_phys and encode_int format, which
 * will need decoding when OpenFirmware is running in little endian.
 *
 */

/*
 * boot properties exported to the kernel for the msgbuf
 */
u_int	msgbuf_paddr;

extern int	pagesize;

struct ppc_prom_memlist {
	u_int	addr;
	u_int	size;
};

struct ppc_prom_memlist scratch_memlist[200];

#define	NIL	((u_int)0)

struct memlist *fill_memlists(char *name, char *prop, struct memlist *old);
extern struct memlist *pfreelistp, *vfreelistp, *pinstalledp;

extern caddr_t getlink(u_int n);
static struct memlist *reg_to_list(struct ppc_prom_memlist *a, int size,
			struct memlist *old);
static void sort_reglist(struct ppc_prom_memlist *ar, int size);
extern void kmem_init(void);
static void decode_reg_properties(struct ppc_prom_memlist *, int);
static int claim_low_phys_mem(u_int size, u_int *addr);
extern void add_to_freelist(struct memlist *old_list);
extern struct memlist *get_memlist_struct();

void
init_memlists()
{
	/* this list is a map of pmem actually installed */
	pinstalledp = fill_memlists("memory", "reg", pinstalledp);
	vfreelistp = fill_memlists("virtual-memory", "available", vfreelistp);
	pfreelistp = fill_memlists("memory", "available", pfreelistp);

	/*
	 * the kernel depends on boot to dynamically claim memory
	 * for the msgbuf
	 * which is not guaranteed to be already wired down by the
	 * prom.  We allocate them in boot early on hoping to obtain the
	 * same page from boot to boot (so msgbuf lives on from boot to
	 * boot).
	 */
	if (claim_low_phys_mem(2 * pagesize, &msgbuf_paddr) < 0) {
		prom_panic("boot: Unable to claim memory for msgbuf\n");
	}
	kmem_init();
}

struct memlist *
fill_memlists(char *name, char *prop, struct memlist *old)
{
	static dnode_t pmem = 0;
	static dnode_t pmmu = 0;
	dnode_t node;
	int links;
	struct memlist *al;
	struct ppc_prom_memlist *pm = scratch_memlist;

	if (pmem == (dnode_t)0)  {

		/*
		 * Figure out the interesting phandles, one time
		 * only.
		 */

		ihandle_t ih;

		if ((ih = prom_mmu_ihandle()) == (ihandle_t)-1)
			prom_panic("Can't get mmu ihandle");
		pmmu = prom_getphandle(ih);

		if ((ih = prom_memory_ihandle()) == (ihandle_t)-1)
			prom_panic("Can't get memory ihandle");
		pmem = prom_getphandle(ih);
	}

	if (strcmp(name, "memory") == 0)
		node = pmem;
	else
		node = pmmu;

	/*
	 * Read memory node and calculate the number of entries
	 */
	if ((links = prom_getproplen(node, prop)) == -1)
		prom_panic("Cannot get list.\n");
	if (links > sizeof (scratch_memlist)) {
		prom_printf("%s list <%s> exceeds boot capabilities\n",
			name, prop);
		prom_panic("fill_memlists - memlist size");
	}
	links = links / sizeof (struct ppc_prom_memlist);


	(void) prom_getprop(node, prop, (caddr_t)pm);
	decode_reg_properties(pm, links);
	sort_reglist(pm, links);
	al = reg_to_list(pm, links, old);
	return (al);
}

/*
 * Register property is stored as property encoded array. number of
 * (address, size) pairs. address is encoded as encode_phys, size is
 * encoded as encode_int. This routine decodes these pairs. We are safely
 * using the reg property as array since there is no alignment problem
 * with these two elements...
 */

static void
decode_reg_properties(struct ppc_prom_memlist *ptr, int no)
{
	register int i;
	extern u_int prom_decode_phys(u_int);

	for (i = 0; i < no; i++, ptr++) {
		ptr->addr = prom_decode_phys(ptr->addr);
		ptr->size = prom_decode_int(ptr->size);
	}
}

/*
 *  Simple selection sort routine.
 *  Sorts platform dependent memory lists into ascending order
 */

static void
sort_reglist(struct ppc_prom_memlist *ar, int n)
{
	int i, j, min;
	struct ppc_prom_memlist temp;

	for (i = 0; i < n; i++) {
		min = i;

		for (j = i+1; j < n; j++)  {
			if (ar[j].addr < ar[min].addr)
				min = j;
		}

		if (i != min)  {
			/* Swap ar[i] and ar[min] */
			temp = ar[min];
			ar[min] = ar[i];
			ar[i] = temp;
		}
	}
}

/*
 *  This routine will convert our platform dependent memory list into
 *  struct memlists's.  And it will also coalesce adjacent  nodes if
 *  possible.
 */
static struct memlist *
reg_to_list(struct ppc_prom_memlist *ar, int n, struct memlist *old)
{
	struct memlist *ptr, *head, *last;
	int i;
	u_int size = 0;
	u_int addr = 0;
	u_int start1, start2;
	int flag = 0;

	if (n == 0)
		return ((struct memlist *)0);

	/*
	 * if there was a memory list allocated before, free it first.
	 */
	if (old)
		(void) add_to_freelist(old);

	head = NULL;
	last = NULL;

	for (i = 0; i < n; i++) {
		start1 = ar[i].addr;
		start2 = ar[i+1].addr;
		if (i < n-1 && (start1 + ar[i].size == start2)) {
			size += ar[i].size;
			if (!flag) {
				addr = start1;
				flag++;
			}
			continue;
		} else if (flag) {
			/*
			 * catch the last one on the way out of
			 * this iteration
			 */
			size += ar[i].size;
		}

		ptr = (struct memlist *)get_memlist_struct();
		if (!head)
			head = ptr;
		if (last)
			last->next = ptr;
		ptr->address = flag ? addr : start1;
		ptr->size = size ? size : ar[i].size;
		ptr->prev = last;
		last = ptr;

		size = 0;
		flag = 0;
		addr = 0;
	}

	last->next = NULL;
	return (head);
}

/*
 * returns 0 on success; -1 on failure.  Allocate size physical
 * pages at low memory.  size must be page aligned.
 */
static int
claim_low_phys_mem(u_int size, u_int *phys_addr)
{
	static dnode_t pmem = (dnode_t)0;
	ihandle_t ih;
	int len;
	int i;
	int ret;
	int memlist_len;
	struct ppc_prom_memlist *pm, *ptr;

	if (pmem == (dnode_t)0) {
		if ((ih = prom_memory_ihandle()) == (ihandle_t)-1) {
			return (-1);
		}
		pmem = prom_getphandle(ih);
	}

	if ((len = prom_getproplen(pmem, "available")) == -1) {
		return (-1);
	}

	pm = (struct ppc_prom_memlist *)prom_alloc(0, len, pagesize);
	if (pm == (struct ppc_prom_memlist *)-1) {
		return (-1);
	}

	if (prom_bounded_getprop(pmem, "available", (caddr_t)pm, len) < 0) {
		prom_free((caddr_t)pm, len);
		return (-1);
	}
	memlist_len = len / sizeof (struct ppc_prom_memlist);

	decode_reg_properties(pm, memlist_len);
	sort_reglist(pm, memlist_len);

	ptr = pm;
	for (i = 0; i < memlist_len; i++, ptr++) {
		if (ptr->size >= size) {
			break;
		}
	}
	if (prom_claim_phys(size, ptr->addr) != 0) {
		ret = -1;
	} else {
		ret = size;
	}
	*phys_addr = ptr->addr;
	prom_free((caddr_t)pm, len);
	return (ret);
}
