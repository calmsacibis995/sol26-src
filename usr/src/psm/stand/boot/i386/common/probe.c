/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 *
 *	i86pc memory/hardware probe routines
 *
 *	This file contains memory management routines to provide
 *	Sparc machine prom functionality
 */

#pragma ident	"@(#)probe.c	1.36	96/05/27 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/bootconf.h>
#include <sys/booti386.h>
#include <sys/bootdef.h>
#include <sys/sysenvmt.h>
#include <sys/bootinfo.h>
#include <sys/bootlink.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>
#include <sys/cmosram.h>
#include <sys/machine.h>
#include <sys/eisarom.h>
#include <sys/salib.h>
#include <sys/promif.h>
#include "cbus.h"

#define	ptalign(p) 	((int *)((uint)(p) & ~(MMU_PAGESIZE-1)))

void dcache_l1l2(void);
void ecache_l1l2(void);
int ibm_l2(void);
void disablecache(void);
void enablecache(void);
void flushcache(void);
paddr_t get_fontptr(void);
void get_eisanvm(void);
void holdit(void);
void read_default_memory(void);
int fastmemchk(ulong memsrt, ulong cnt, ulong step, ushort flag);
int memwrap(ulong memsrt, ulong memoff);
int memchk(ulong memsrt, ulong cnt, ushort flag);
void memtest(void);
paddr_t getfontptr(unsigned short which);
void probe_eisa(void);
void probe(void);

extern caddr_t rm_malloc(u_int, u_int, caddr_t);
extern void rm_free(caddr_t, u_int);
extern void wait100ms(void);
extern struct sysenvmt *sep;
extern struct bootinfo *bip;
extern struct bootenv *btep;
extern struct bootops *bop;
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *s1, void *s2, size_t n);
extern BOOL CbusInitializePlatform(struct bootmem *mrp);
extern void check_hdbios(void);
extern int doint(void);
extern int is_MC(void);
extern int is_EISA(void);
extern int is486(void);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);

#define	SYS_MODEL() *(char *)0xFFFFE
#define	MODEL_AT	(unchar)0xFC
#define	MODEL_MC	(unchar)0xF8

#define	EISAIDLOC ((long *)0xFFFD9)
#define	EISAIDSTR (*(long *)"EISA")

#define	MEM1M	(ulong)0x100000
#define	MEM4M	(ulong)0x400000
#define	MEM32M	(ulong)0x2000000
#define	MEM64M	(ulong)0x4000000
#define	MEM1G	(ulong)0x40000000
#define	MEM2G	(ulong)0x80000000
#define	MEM4G_1	(ulong)0xffffffff	/* 4G minus 1 to fit in ulong */

extern char outline[64];
extern int boot_device;
static int flush_l2_flag;

void
probe(void)
{
	int	i;
	extern struct	int_pb		ic;

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("Begin probe\n");
#endif
	(void) memcpy((caddr_t)physaddr(bootinfo.id), (caddr_t)0xfed00,
	    sizeof (bootinfo.id));

/*
 * Get the ega font pointer locations. This has to be collected
 * after shadow ram has been turned off.
 */
	sep->font_ptr[0] = getfontptr(0x0300);	/* 8 x 8  */
	sep->font_ptr[1] = getfontptr(0x0200);	/* 8 x 14 */
	sep->font_ptr[2] = getfontptr(0x0500);	/* 9 x 14 */
	sep->font_ptr[3] = getfontptr(0x0600);	/* 8 x 16 */
	sep->font_ptr[4] = getfontptr(0x0700);	/* 9 x 16 */

	/* get these again after shadow ram change */

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK) {
		for (i = 0; i < 5; i++)
			printf("egafont[%d] 0x%x\n", i, sep->font_ptr[i]);
	}
#endif

	(void) memset(outline, 0, sizeof (outline));
	/* Gather memory size info from CMOS and BIOS ram */
	sep->CMOSmembase = CMOSreadwd(BMLOW);
	(void) bsetprop(bop, "cmos-membase",
		sprintf(outline, "%d", sep->CMOSmembase), 0, 0);
	sep->CMOSmemext  = CMOSreadwd(EMLOW);
	(void) bsetprop(bop, "cmos-memext",
		sprintf(outline, "%d", sep->CMOSmemext), 0, 0);
	sep->CMOSmem_hi  = CMOSreadwd(EMLOW2);
	(void) bsetprop(bop, "cmos-memhi",
		sprintf(outline, "%d", sep->CMOSmem_hi), 0, 0);

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("sep->CMOSmem_hi 0x%x\n", sep->CMOSmem_hi);
#endif

	ic.intval = 0x15;
	ic.ax = 0x8800;
	(void) doint();
	sep->sys_mem = ic.ax;

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("Begin machine identification.\n");
#endif

	sep->machine = MPC_UNKNOWN;
	(void) bsetprop(bop, "machine-mfg", "unknown", 0, 0);

	if (is_MC()) {	/* do Micro Channel initialization */
#ifdef BOOT_DEBUG
		if (btep->db_flag & BOOTTALK) {
			printf("micro channel bus\n");
		}
#endif
		sep->machflags |= MC_BUS;
		(void) bsetprop(bop, "bus-type", "mc", 0, 0);
		read_default_memory();
		sep->machine = MPC_MC386;
		(void) bsetprop(bop, "machine-mfg", "generic MC", 0, 0);
		/* bip->machenv.machine = M_PS2; */
	}
	/* insert other bus types here when needed...... */
	else {	/* if all else fails, must be AT type ISA or EISA */
		/* do generic ISA/EISA initialization */
		sep->machine = MPC_AT386;
		(void) bsetprop(bop, "machine-mfg", "generic AT", 0, 0);
		read_default_memory();
		/* this must come before Eisa default mem setup */

		if (is_EISA()) {
			sep->machflags |= EISA_BUS;
			(void) bsetprop(bop, "bus-type", "eisa", 0, 0);
#ifdef BOOT_DEBUG
			if (btep->db_flag & BOOTTALK) {
				printf("EISA bus\n");
			}
#endif
		} else {	/* at this point, must be ISA */
			(void) bsetprop(bop, "bus-type", "isa", 0, 0);
#ifdef BOOT_DEBUG
			if (btep->db_flag & BOOTTALK) {
				printf("ISA bus\n");
			}
#endif
		}
	}

	if (is486()) {
#ifdef BOOT_DEBUG
	    if (btep->db_flag & BOOTTALK) {
		printf("486 machine\n");
	    }
#endif
	    sep->machflags |= IS_486;
	    (void) bsetprop(bop, "cpu-type", "486", 0, 0);

	} else {		/* we don't have 586's....yet! */
	    sep->machflags = 0;
	    (void) bsetprop(bop, "cpu-type", "386", 0, 0);
	}

/*
 *	look for memory above 16 MB - remove the 16MB barrier
 *	for all machines.
 */
	for (i = 0; btep->memrng[i].extent != 0; i++)
		;

/*
 *	set the last entry to 16Mb - 4Gb
 */
	btep->memrng[i].base = 0x1000000;
	btep->memrng[i].extent = (MEM4G_1 - MEM16M) + 1;
	btep->memrng[i++].flags = B_MEM_EXPANS;
	btep->memrngcnt = i;

	if (sep->machine == MPC_AT386 || sep->machine == MPC_MC386) {
/*		btep->db_flag |= (MEMDEBUG | BOOTTALK); turn off debug mode */
		memtest();
	}

	btep->db_flag &= ~BOOTTALK;
	check_hdbios();
}


/*
 * Separate this from probe() so that the property
 * is stored in kmem.
 */
void
probe_eisa(void)
{
	if (is_EISA())
		get_eisanvm();
}

paddr_t
getfontptr(unsigned short which)
{
	extern struct	int_pb		ic;

	ic.intval = 0x10;
	ic.ax = 0x1130;
	ic.bx = which;
	ic.cx = ic.dx = ic.es = ic.bp = 0;
	if (doint()) {
		printf("doint error getting font pointers, may be trouble\n");
		return (0);
	}
	else
		return ((uint)((ic.es) << 4) + ic.bp);
}

#define	MEMTEST0	(ulong)0x00000000
#define	MEMTEST1	(ulong)0xA5A5A5A5
#define	MEMTEST2	(ulong)0x5A5A5A5A
#define	MEM16M		(ulong)0x1000000

extern  void flcache(), savcr0(), rcache(), flush_l2(ulong *);
extern  int dcache();
extern	caddr_t top_realmem;
extern	caddr_t top_bootmem;
extern	caddr_t max_bootaddr;

void
memtest(void)
{

	struct  bootmem	*map, *mrp;
	ulong		memsrt;
	ulong		base_sum, ext_sum;
	int		extmem_clip = 0;
	int		i;
	paddr_t		base_avail = 0;		/* XXX is this right? */


/*	set page boundary for base memory				*/
	base_sum = (ulong)ptalign(sep->base_mem * 1024);

/*	set page boundary for extended memory				*/
	ext_sum = (ulong)ptalign(sep->CMOSmemext * 1024);

	if (btep->db_flag & (MEMDEBUG | BOOTTALK)) {
		printf("Memory test begin\n");
		printf("Given %d memory areas as follows:\n", btep->memrngcnt);
		for (mrp = &btep->memrng[0]; mrp->extent > 0; mrp++)
			printf("Base 0x%x size 0x%x flag 0x%x\n",
				mrp->base, mrp->extent, mrp->flags);
		printf("memsize: CMOS base %dKB, expansion %dKB,"
			" sys >1MB(max in PS2) %dKB\n",
			sep->CMOSmembase, sep->CMOSmemext,
			sep->CMOSmem_hi);
		printf("         BIOS base %dKB, system %dKB\n",
			sep->base_mem, sep->sys_mem);
		printf("sizer base_sum = %x, ext_sum = %x \n",
				base_sum, ext_sum);
		(void) goany();
	}

/*
 *	set aside the first & second avail slots for use by boot
 */
	bip->memavailcnt = 0;
	map = &bip->memavail[0];

	map->base = 0;
	map->extent = (long)ptalign(top_realmem);
	map->flags |= B_MEM_BOOTSTRAP;
	map++;
	bip->memavailcnt++;
	map->base = (paddr_t)(1 * 1024 * 1024);			/* 1M */
	map->extent = (long)top_bootmem - (long)map->base;	/* 3M */
	map->flags |= B_MEM_BOOTSTRAP;
	map++;
	bip->memavailcnt++;

	if (sep->machflags & IS_486) {
		savcr0();	/* save previous value of cr0 reg */
		(void) dcache(); /* disable and flush on-chip i486 cache, */
				/* and flush any external caches */

		if ((sep->machflags & MC_BUS) && (ibm_l2() == 1)) {
			flush_l2_flag = 1;
		}
	}

	mrp = &btep->memrng[0];
	mrp[btep->memrngcnt].extent = 0;
#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("Testing for Corollary\n");
#endif
	(void) CbusInitializePlatform(&btep->memrng[0]);
	mrp = &btep->memrng[0];

	for (; mrp->extent != 0; mrp++) {

		(void) memcpy(map, mrp, sizeof (struct bootmem));

		if (map->flags & B_MEM_BASE) {	/* check for base memory */
			map->base = base_avail;
			map->extent = base_sum - map->base;
		} else {	/* check for extended and shadow memory */
			memsrt = map->base;
			if (map->flags & B_MEM_TREV)
				memsrt -= NBPC;

			if (map->flags & B_MEM_SHADOW) {
				goto below;	/* sigh... for lint */
			/*
			 * For memory above 16M:-
			 * no memory scan will be performed unless
			 * there must be at least 3 pages of non-wrap memory
			 * and no clipped extended memory below
			 */
			} else if (memsrt >= MEM16M) {
				for (i = 0; i < 3; i++) {
				    if (memwrap(MEM16M+(ulong)(i*NBPC), MEM16M))
						break;
				}
				if (i < 3)
					map->extent = 0;
			} else if (map->extent > ext_sum) {
				/*
				 * For memory below 16M:-
				 * clip extended memory to CMOS recorded limit
				 */
				map->extent = ext_sum;
				extmem_clip++;
			}
below:

			/* skip if running out of extended memory */
			if (!map->extent)
				continue;

			if (memsrt >= MEM1M) {
				if (map->extent = fastmemchk(memsrt,
				    map->extent, MEM1M, map->flags)) {
					if (map->flags & B_MEM_TREV)
						map->base -= map->extent;
					/*
					 * decrement CMOS ext sum for non-shadow
					 * memory below 16M
					 */
					if ((memsrt < MEM16M) &&
					    !(map->flags & B_MEM_SHADOW))
						ext_sum -= map->extent;
				}
			} else if (map->extent = memchk(memsrt, map->extent,
			    map->flags)) {
				if (map->flags & B_MEM_TREV)
					map->base -= map->extent;
				/*
				 * decrement CMOS ext sum for non-shadow memory
				 * below 16M
				 */
				if ((memsrt < MEM16M) &&
				    !(map->flags & B_MEM_SHADOW))
					ext_sum -= map->extent;
			}
		}
		if (map->extent) {
			map++;
			bip->memavailcnt++;
		}
	}

	map->extent = 0;

	if (sep->machflags & IS_486) {
#ifdef notdef
	    if ((sep->machflags & MC_BUS) && (ibm_l2() == 1)) {
		ecache_l1l2();
	    }
#endif
	    rcache();
	}

	if (btep->db_flag & BOOTTALK) {
		printf("Memory test complete\n");
		printf("Found %d memory avail areas as follows:\n",
			bip->memavailcnt);
		for (map = &bip->memavail[0]; map->extent > 0; map++)
			printf("Base %x  size %x flgs 0x%x\n",
				map->base, map->extent, map->flags);
		printf("Bootsize 0x%x\n", btep->bootsize);
		(void) goany();
	}
}

int
memchk(ulong memsrt, ulong cnt, ushort flag)
{
	int	bytecnt = 0;
	ulong	*ta;
	ulong	memsave1;
	ulong	memsave2;

	ta = (ulong *)memsrt;
	if (btep->db_flag & BOOTTALK)
		printf("Memory test starting %x %s cnt %x", ta,
			(flag & B_MEM_TREV)?"down":"up", cnt);

	for (; cnt; cnt -= NBPC) {
		memsave1 = *ta;
		memsave2 = *(ta+1);
		*ta++ = MEMTEST1;
		*ta-- = MEMTEST0;
		if (sep->machflags & IS_486)
		    flcache();		/* flush i486 on-chip cache, and */
		if (flush_l2_flag)
		    flush_l2(ta);	/* write-back any external cache */
		if (*ta == MEMTEST1) {
			*ta++ = MEMTEST2;
			*ta-- = MEMTEST0;
			if (sep->machflags & IS_486)
			    flcache();
			if (flush_l2_flag)
			    flush_l2(ta);

			if (*ta == MEMTEST2) {
				bytecnt += NBPC;
				*ta = memsave1;
				*(ta+1) = memsave2;
			} else {
				if (btep->db_flag & MEMDEBUG)
					printf(" abort 2");
				break;
			}
		} else {
			if (btep->db_flag & MEMDEBUG)
				printf(" abort 1");
			break;
		}
		if (flag & B_MEM_TREV)
			ta -= (NBPC / sizeof (long));
		else
			ta += (NBPC / sizeof (long));
	}

	if (btep->db_flag & (BOOTTALK|MEMDEBUG))
		printf(" ended at %x, area size %x\n", ta, bytecnt);

	return (bytecnt);
}


int
memwrap(ulong memsrt, ulong memoff)
{
	ulong	*ta;
	ulong	*ta_wrap;
	ulong	save_val;
	int	mystatus = 1;	/* assume memory is wrap around		*/

	ta = (ulong *)memsrt;
	ta_wrap = (ulong *)(memsrt-memoff);
	save_val = *ta;
	*ta = MEMTEST1;
	if (sep->machflags & IS_486)
	    flcache();		/* flush i486 on-chip cache, and */
	if (flush_l2_flag)
	    flush_l2(ta);	/* write-back any external cache */
	if ((*ta == MEMTEST1) && (*ta != *ta_wrap)) {
	    *ta = MEMTEST2;
	    if (sep->machflags & IS_486)
		flcache();
	    if (flush_l2_flag)
		flush_l2(ta);
	    if ((*ta == MEMTEST2) && (*ta != *ta_wrap))
		mystatus = 0;
	}
/*	restore original value whether wrap or no wrap			*/
	*ta = save_val;

#ifdef BOOT_DEBUG
	printf("memwrap: ta= 0x%x ta_wrap= 0x%x memory %s\n", ta,
		ta_wrap, (mystatus? "WRAP": "NO WRAP"));
#endif

	return (mystatus);
}

int
fastmemchk(ulong memsrt, ulong cnt, ulong step, ushort flag)
{
	int	bytecnt = 0;
	ulong   *ta;
	ulong   memsave1;
	ulong   memsave2;

	ta = (ulong *)memsrt;
	if (btep->db_flag & BOOTTALK)
		printf("Fast Memory test starting %x %s cnt %x step %x\n", ta,
			(flag & B_MEM_TREV)?"down":"up", cnt, step);

	for (; cnt != 0; cnt -= step) {
		memsave1 = *ta;
		memsave2 = *(ta+1);
		*ta++ = MEMTEST1;
		*ta-- = MEMTEST0;
		if (sep->machflags & IS_486)
			flcache();	/* flush i486 on-chip cache, and */
		if (flush_l2_flag)
			flush_l2(ta);	/* write-back any external cache */
		if (*ta == MEMTEST1) {
			*ta++ = MEMTEST2;
			*ta-- = MEMTEST0;
			if (sep->machflags & IS_486)
				flcache();
			if (flush_l2_flag)
				flush_l2(ta);

			if (*ta == MEMTEST2) {
				bytecnt += step;
				*ta = memsave1;
				*(ta+1) = memsave2;
			} else {
				if (btep->db_flag & MEMDEBUG)
					printf(" abort 2");
				break;
			}
		} else {
			if (btep->db_flag & MEMDEBUG)
				printf(" abort 1");
			break;
		}
		if (flag & B_MEM_TREV)
			ta -= (step / sizeof (long));
		else
			ta += (step / sizeof (long));
	}
/*
 *      make sure the last 4 Mb are full
 */
	if (bytecnt) {
		if (flag & B_MEM_TREV)
			ta += (step / sizeof (long));
		else
			ta -= (step /sizeof (long));
		bytecnt -= step;
		bytecnt += memchk((ulong)ta, step, flag);
	}

	if (btep->db_flag & (BOOTTALK|MEMDEBUG))
		printf("Return area size %x\n", bytecnt);

	return (bytecnt);
}

void
read_default_memory(void)
{
	btep->memrngcnt = 1;

/*
	btep->memrng[0].base = 0;
	btep->memrng[0].extent = 640 * 1024;
	btep->memrng[0].flags = B_MEM_BASE;
*/

	btep->memrng[0].base = 0x400000;
	btep->memrng[0].extent = (12 * 1024 * 1024);
	btep->memrng[0].flags = B_MEM_EXPANS;

	btep->memrng[1].base = 0;
	btep->memrng[1].extent = 0;
	btep->memrng[1].flags = 0;

	btep->memrng[2].base = 0;
	btep->memrng[2].extent = 0;
	btep->memrng[2].flags = 0;

	btep->memrng[3].base = 0;
	btep->memrng[3].extent = 0;
	btep->memrng[3].flags = 0;

	btep->bootsize = btop((u_int)max_bootaddr);

	/* ===================================== */

	bip->memavailcnt = 0;

	bip->memavail[0].base = 0;
	bip->memavail[0].extent = 0;
	bip->memavail[0].flags = B_MEM_BASE;

	bip->memavail[1].base = 0;
	bip->memavail[1].extent = 0;
	bip->memavail[1].flags = B_MEM_EXPANS;

	bip->memavail[2].base = 0;
	bip->memavail[2].extent = 0;
	bip->memavail[2].flags = 0;

	bip->memavail[3].base = 0;
	bip->memavail[3].extent = 0;
	bip->memavail[3].flags = 0;

	bip->memavail[4].base = 0;
	bip->memavail[4].extent = 0;
	bip->memavail[4].flags = 0;
}

#if 0					/* vla fornow..... */
shomem(used, idm, cnt, bmp)
char *idm;
int  cnt, used;
struct bootmem *bmp;
{
	int i;

	printf("%s %d\n", idm, cnt);
	for (i = 0; i < cnt; i++, bmp++) {
		printf("%d %x %x %x", i, bmp->base, bmp->extent, bmp->flags);
		if (used)
			printf(" %x\n", bootenv.sectaddr[i]);
		else
			printf("\n");
	}
	(void) goany();
}
#endif					/* vla fornow..... */

#if defined(lint)
/*
 * inline functions defined here for lint's sake.
 */

/*ARGSUSED*/
u_char	inb(int port) { return (*(u_char *)port); }
void	outb(int port, u_char v) { *(u_char *)port = v; }

#endif /* defined(lint) */

void
get_eisanvm(void)
{
	extern struct	int_pb		ic;
	int	i, j;
	int	status;
	struct	es_slot		slotbuf;
	struct	es_slot		*es_slotp;
	struct	es_func		*es_funcp;
	int	number_of_functions;
	int	memory_needed;
	char	*nvm_data;

	sep->machflags |= EISA_NVM_DEF;
	/*
	 * First see how much memory we need.
	 */
	memory_needed = 0;
	for (i = 0; i < EISA_MAXSLOT; i++) {
		ic.intval = 0x15;
		ic.ax = 0xd800;
		ic.cx = (ushort)(i & 0xffff);
		ic.bx = ic.dx = ic.si = ic.di = ic.bp = ic.es = 0;
		status = doint();
		slotbuf.es_slotinfo.eax.word.ax = ic.ax;
		slotbuf.es_slotinfo.edx.word.dx = ic.dx;
		memory_needed += sizeof (struct es_slot);
		if (slotbuf.es_slotinfo.eax.byte.ah != 0)	/* error */
			continue;
		number_of_functions = slotbuf.es_slotinfo.edx.byte.dh;
		memory_needed += number_of_functions * sizeof (struct es_func);
	}
	/*
	 * make sure we have enough kmem space
	 */
	if (!(nvm_data = rm_malloc(memory_needed, 0, 0))) {
		printf("No memory for EISA NVRAM property\n");
		return;
	}

	es_slotp = (struct es_slot *)nvm_data;
	if (btep->db_flag & BOOTTALK) {
		printf("get_eisanvm: es_slotp= 0x%x\n", es_slotp);
	}

	for (i = 0; i < EISA_MAXSLOT; i++, es_slotp++) {
		ic.intval = 0x15;
		ic.ax = 0xd800;
		ic.cx = (ushort)(i & 0xffff);
		ic.bx = ic.dx = ic.si = ic.di = ic.bp = ic.es = 0;
		status = doint();
		es_slotp->es_slotinfo.eax.word.ax = ic.ax;
		es_slotp->es_slotinfo.ebx.word.bx = ic.bx;
		es_slotp->es_slotinfo.ecx.word.cx = ic.cx;
		es_slotp->es_slotinfo.edx.word.dx = ic.dx;
		es_slotp->es_slotinfo.esi.word.si = ic.si;
		es_slotp->es_slotinfo.edi.word.di = ic.di;
		es_slotp->es_funcoffset = 0;

		if (btep->db_flag & BOOTTALK) {
			if (!status)
				if (es_slotp->es_slotinfo.eax.byte.ah)
				    printf("get_eisanvm: cflg = 0 ah = 0x%x\n",
					es_slotp->es_slotinfo.eax.byte.ah);
			if (status) {
				if (!es_slotp->es_slotinfo.eax.byte.ah)
				    printf("get_eisanvm: cflg != 0 ah == 0\n");
				else
				    printf("get_eisanvm:slot[%d] ah= 0x%x\n",
					i, es_slotp->es_slotinfo.eax.byte.ah);
			}
		}
	}

	es_funcp = (struct es_func *)es_slotp;
	es_slotp = (struct es_slot *)nvm_data;
	for (i = 0; i < EISA_MAXSLOT; i++, es_slotp++) {
		if (!es_slotp->es_slotinfo.eax.byte.ah) {
			es_slotp->es_funcoffset = (int)((char *)es_funcp -
								nvm_data);
			if (btep->db_flag & BOOTTALK) {
				printf("eisanvm: slot[%d] func addr= 0x%x"
					" cnt= %d\n",
				i, es_funcp, es_slotp->es_slotinfo.edx.byte.dh);
			}
			for (j = 0; j < es_slotp->es_slotinfo.edx.byte.dh;
			    j++) {
				ic.intval = 0x15;
				ic.bx = ic.dx = ic.di = ic.bp = ic.es = 0;
				ic.ax = 0xd801;
				ic.cx = (ushort)(((j&0xff)<<8)|(i & 0xff));
				ic.ds = ((paddr_t)(es_funcp->ef_buf)&0xffff0) >>
					4;
				ic.si = (paddr_t)(es_funcp->ef_buf) & 0xf;
				status = doint();
				es_funcp->eax.word.ax = ic.ax;
				es_funcp++;
			}
		}
	}
	(void) bsetprop(bop, "eisa-nvram", nvm_data, memory_needed, 0);
	rm_free(nvm_data, memory_needed);
}

paddr_t
get_fontptr(void)
{
	return ((paddr_t)sep->font_ptr);
}

#define	IBM_MCAR	0xe0	/* IBM Memory Controller Address reg port */
#define	IBM_CCR		0xe2	/* IBM Cache Controller Register */
#define	IBM_CSR		0xe3	/* IBM Cache Status Register */
#define	IBM_MCDR	0xe4	/* IBM Memory Controller Data register port */


#ifdef notdef
/* disable and flush L1 and L2 caches */
void
dcache_l1l2(void)
{
	register unchar regtmp;

	outb(IBM_MCAR, 0xa2);	/* point to the Cache/Timer control reg */
	regtmp = inb(IBM_MCDR);	/* read Cache/Timer control reg */
	outb(IBM_MCAR, 0xa2);	/* point to the cache/Timer control reg */
	outb(IBM_MCDR, regtmp | 0x01);	/* Disable L1 and L2 caches */

	regtmp = inb(IBM_CCR);		/* read cache control register */
	outb(IBM_CCR, regtmp | 0x80);   /* enable L1 cache */
	regtmp = inb(IBM_CCR);		/* read cache control register */
	outb(IBM_CCR, regtmp & 0x3f);   /* disable and flush L1 and L2 caches */
}


/* Enable L1 and L2 caches */
void
ecache_l1l2(void)
{
	register unchar regtmp;

	outb(IBM_MCAR, 0xa2);	/* point to the Cache/Timer control reg */
	regtmp = inb(IBM_MCDR);	/* read Cache/Timer control reg */
	outb(IBM_MCAR, 0xa2);	/* point to the cache/Timer control reg */
	outb(IBM_MCDR, regtmp & 0xfe);	/* Enable L1 and L2 caches */

	regtmp = (inb(IBM_CCR) | 0xc0); /* read cache control register and */
	regtmp &= 0xdf;			/* set/reset desired bits */
	outb(IBM_CCR, regtmp);		/* enable L1 & L2 caches */
}
#endif /* notdef */


/* return 1 if L2 cache exist, return zero if not */
int
ibm_l2(void)
{
	unchar regtmp;

	regtmp = inb(IBM_CSR);  /* read cache status register */

	if ((regtmp & 0x30) != 0)
		return (0);
	else
		return (1);
}
