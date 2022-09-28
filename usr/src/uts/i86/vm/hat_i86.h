/*
 * Copyright (c) 1987-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_VM_HAT_I86MMU_H
#define	_VM_HAT_I86MMU_H

#pragma ident	"@(#)hat_i86.h	1.31	96/07/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the Intel 80x86 MMU
 * specific hat data structures and its specific hat procedures.
 * The machine independent interface is described in <common/vm/hat.h>.
 */
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/debug/debug.h>
#include <sys/x_call.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/mach_page.h>

#define	HAT_SUPPORTED_LOAD_FLAGS (HAT_LOAD | HAT_LOAD_LOCK | HAT_LOAD_ADV |\
		HAT_LOAD_CONTIG | HAT_LOAD_NOCONSIST |\
		HAT_LOAD_SHARE | HAT_LOAD_REMAP)

#define	HAT_INVLDPFNUM		0xffffffff

struct hat {
	kmutex_t		hat_mutex;	/* protects hat, hatops */
	struct	hat		*hat_next;
	struct	hat		*hat_prev;
	struct	as		*hat_as;
	struct	hwptepage	*hat_hwpp;
	short			hat_numhwpp;
	short			hat_stat;
	u_int			hat_cpusrunning;
	struct	hat		*hat_dup;
	u_int			*hat_pagedir;
	u_int			hat_pdepfn;
	u_short			hat_flags;
	kcondvar_t 		hat_cv;
};

/*
 * The hment entry, hat mapping entry.
 * The mmu independent translation on the mapping list for a page
 */
struct hment {
	struct	page *hme_page;		/* what page this maps */
	struct	hment *hme_next;	/* next hment */
	u_int	hme_hat : 16;		/* index into hats */
	u_int	hme_impl : 8;		/* implementation hint */
	u_int	hme_notused : 2;	/* extra bits */
	u_int	hme_prot : 2;		/* protection */
	u_int	hme_noconsist : 1;	/* mapping can't be kept consistent */
	u_int	hme_ncpref: 1;		/* consistency resolution preference */
	u_int	hme_nosync : 1;		/* ghost unload flag */
	u_int	hme_valid : 1;		/* hme loaded flag */
	struct	hment *hme_prev;	/* prev hment */
};

#ifdef	_KERNEL

int	nhats;
struct hat *hats;
struct hat *hatsNHATS;

#define	P_NC	0x08

#define	PP_ISMOD(pp)		(((machpage_t *)(pp))->p_nrm & P_MOD)
#define	PP_ISREF(pp)		(((machpage_t *)(pp))->p_nrm & P_REF)
#define	PP_ISRO(pp)		(((machpage_t *)(pp))->p_nrm & P_RO)
#define	PP_ISNC(pp)		(((machpage_t *)(pp))->p_nrm & P_NC)

#define	PP_SETMOD(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_MOD)
#define	PP_SETREF(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_REF)
#define	PP_SETRO(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_RO)
#define	PP_SETREFRO(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), R_REF|P_RO)
#define	PP_SETPNC(pp)	atomic_orb(&(((machpage_t *)(pp))->p_nrm), P_NC)

#define	PP_CLRMOD(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_MOD)
#define	PP_CLRREF(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_REF)
#define	PP_CLRRO(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_RO)
#define	PP_CLRPNC(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), ~P_NC)
#define	PP_CLRALL(pp)	atomic_andb(&(((machpage_t *)(pp))->p_nrm), \
				~P_MOD|~P_REF|~P_RO|~P_NC)
#endif /* _KERNEL */
/*
 * Right now the basic unit of hme allocation is 2 hme's. NUMHME_ALLOCTE
 * defines this. If this changes to something different, then hmetopte() and
 * ptetohme() macros will also have to change
 */
#define	NUMHME_ALLOCATE	2

/*
 * Pagetable can hold i86hme pointers when we do not have a valid pte
 * indexed above I86MMU_MAXPTES in that pagetable
 */
#define	I86MMU_MAXPTES	(((NPTEPERPT)/(NUMHME_ALLOCATE + 1)) * NUMHME_ALLOCATE)

/*
 * space needed to hold hme pointers for all ptes in a pagetable
 */
#define	HMESPACE_SIZE	((NPTEPERPT/NUMHME_ALLOCATE) * sizeof (int))
#define	HMEPOS_BITMASK	1
#define	HME_FIRST	0
#define	HME_SECOND	1
#define	HMEPOSITION(a)	((a)->hme_impl & HMEPOS_BITMASK)
#define	HMETOI86HME(a)	((struct i86hme *)((a) - HMEPOSITION(a)))
#define	hmetopte(a)	(((a)->hme_impl & HME_KERNEL) ? khmetopte(a) :\
			((HMETOI86HME(a))->i86hme_pte + HMEPOSITION(a)))
#define	kptetohme(a)	(khments + ((a) - kptes))
#define	khmetopte(a)	(kptes + ((a) - khments))
#define	FIRSTHME(a)	(((a)->hme_impl & HMEPOS_BITMASK) == HME_FIRST)
#define	FIRSTHME_TOPTE(a)	(((struct i86hme *)(a))->i86hme_pte)
#define	SECONDHME_TOPTE(hme)	(((struct i86hme *)((hme) -1))->i86hme_pte + 1)
#define	ODD_PFNUM(addr)	((((u_int)(addr)) >> MMU_STD_PAGESHIFT) & 0x01)
#define	I86HME_TO_HME(a, addr)	(&(a->i86hme_hme[ODD_PFNUM(addr)]))
#define	I86HME_TO_FIRSTHME(a)	(&((a)->i86hme_hme[0]))
#define	I86HME_TO_SECONDHME(a)	(&((a)->i86hme_hme[1]))
#define	HME_SHIFT		(MMU_STD_PAGESHIFT + NUMHME_ALLOCATE/2)
#define	HME_MASK		(NPTEPERPT/NUMHME_ALLOCATE - 1)
#define	HMETABLE_INDEX(a)	(((u_int)a >> HME_SHIFT) & HME_MASK)
#define	PTEI86HME_MASK	((NUMHME_ALLOCATE * sizeof (int)) - 1)
#define	PTE_TO_I86HMEPTE(a)	((struct pte *)(((u_int)(a)) & ~PTEI86HME_MASK))

struct	i86hme {
	struct	hment	i86hme_hme[NUMHME_ALLOCATE];
	struct	i86hme	*i86hme_next;
	union {
		struct	pte	*pte;
		struct {
			u_short		pteindex;
			u_short		hwppbase;
		}pteinfo;
	} i86hme_un;
};

#define	i86hme_pte 	i86hme_un.pte
#define	i86hme_pteindex	i86hme_un.pteinfo.pteindex
#define	i86hme_hwppbase	i86hme_un.pteinfo.hwppbase

/*
 * hme->hme_impl should be looked at in the following fashion
 * hme_impl_position:1
 * hme_impl_prot:2
 * hme_impl_notused:1
 * hme_impl_flags:4
 *
 * This define is here, since this just extracts the protection bits as
 * they are, so that they could be used in hme->hme_impl byte. The lowest bit
 * of hme->hme_impl byte is used to encode the position of the hme in
 * 'struct i86hme'
 */
#define	I86MMU_PTEHMEPROT(pte, a) ((a)->hme_impl |= ((*(u_int *)pte) & 0x06))
#define	I86MMU_PROTFROMHME(a)	(((u_int)((a)->hme_impl & 0x06)) >> 1)
#define	I86MMU_HMEPROT(a, prot)	((a)->hme_impl |= ((prot & 0x03) << 1))
#define	I86MMU_ZEROHMEPROT(a)	((a)->hme_impl &= ~0x06)

/*
 * hme_impl_flags
 */
#define	HME_PTEUNLOADED	0x10	/* pte for this hme has been stolen */
#define	HME_PAGEUNLOADED 0x20	/* The hme has been marked so that it would */
				/* dropped by hat_mlist_exit() */
#define	HME_KERNEL	0x40	/* this hme belongs to kernel */

/*
 * hat->hat_flags
 */
#define	I86MMU_FREEING			0x01
#define	I86MMU_DUP_PARENT		0x02
#define	I86MMU_DUP_CHILD		0x04
#define	I86MMU_PTEPAGE_STOLEN		0x08

/*
 * This guy has either more than two pagetables or has them mapped at
 * a place different from the usual ones
 */
#define	I86MMU_UNUSUAL			0x010
#define	I86MMU_SWAPOUT			0x020
#define	I86MMU_HMESTEAL			0x040
#define	I86MMU_SPTAS			0x080

#define	I86MMU_DUP		(I86MMU_DUP_PARENT|I86MMU_DUP_CHILD)
#define	I86MMU_HWPPDONTSTEAL	\
	(I86MMU_FREEING | I86MMU_PTEPAGE_STOLEN | I86MMU_DUP_CHILD)


#define	I86MMU4MBLOCKMAP	1
#define	I86MMU4KLOCKMAP		2

/*
 * The hwptepage structure contains one entry for each potential page of
 * real (hardware) page table entries.
 *
 */
typedef struct hwptepage {
	/*
	 * When free, either on list of structs
	 * with attached page or without.
	 */
	struct hwptepage *hwpp_next;
	struct	i86hme   *hwpp_firsthme;
	struct	i86hme   *hwpp_lasthme;
	u_int	*hwpp_hme;	/* Pointer to the area holding hme pointers */
	pte_t	*hwpp_pte;	/* Pointer to page of ptes */
	u_int	hwpp_pfn;	/* Page frame number */
	struct 	hat *hwpp_hat;
	u_int	hwpp_pde;	/* page directory entry */
	u_short	hwpp_base;	/* Base address of 4 meg section of as */
	u_short	hwpp_lockcnt;	/* Number of locks on  ptes in this hwpp */
	u_short	hwpp_numptes;	/* number of valid ptes in this hwpp */
	u_short	hwpp_mapping; 	/* This hwpp maps a 4Mb page */
} hwptepage_t;

/*
 * user text+data+stack by default falls in the 4MB slot indexed at offset
 * 0x20 in page directory.
 * user shared library starts at the 4MB slot indexed at 0x37f in page
 * directory
 */
#define	I86MMU_USER_TEXT		0x20
#define	I86MMU_USER_SHLIB		0x37f
#define	I86MMUNUSUAL(a) 	(((a) != I86MMU_USER_TEXT) && \
				((a) != I86MMU_USER_SHLIB))

/*
 * ctx, ptbl, mlistlock and other stats for i86mmu
 * XXX - NEEDS WORK.
 */
struct vmhatstat {
	kstat_named_t	vh_mlist_enter;		/* calls to mlist_lock_enter */
	kstat_named_t	vh_mlist_enter_wait;	/* had to do a cv_wait */
	kstat_named_t	vh_mlist_exit;		/* calls to mlist_lock_exit */
	kstat_named_t	vh_mlist_exit_broadcast; /* had to do cv_broadcast */
};

#define	MIN_HWPTEPAGES	32	/* min number of hwptepages */
#define	MAX_HWPTEPAGES	1024	/* max number of hwptepages */

/*
 * Convert an address to its base hwpp portion
 */
#define	addrtohwppbase(addr) ((u_int)(addr) >> MMU_STD_SEGSHIFT)

/*
 * Convert an hwpp base to a virtual address
 */
#define	hwppbasetoaddr(base) ((caddr_t)((base) << MMU_STD_SEGSHIFT))

/*
 * Flags to pass to i86mmu_pteunload().
 */
#define	HAT_RMSYNC	0x01
#define	HAT_NCSYNC	0x02
#define	HAT_INVSYNC	0x04
#define	HAT_VADDR	0x08
#define	HAT_RMSTAT	0x10

/*
 * Flags to pass to hat_pagecachectl().
 */
#define	HAT_CACHE	0x1
#define	HAT_UNCACHE	0x2

#define	PAGETABLE_INDEX(a)	MMU_L2_INDEX(a)
#define	PAGEDIR_INDEX(a)	MMU_L1_INDEX(a)

#define	ptetohwpp(arg) (hwpp_array[((arg) - ptes) >> NPTESHIFT])

/*
 * This macro synchronizes the I/O Mapper to a kernel pte mapping if
 * appropriate.
 */
#ifdef IOC
#define	IOMAP_SYNC(addr, ptr)	{ if (addr >= (caddr_t)SYSBASE) \
		((pte_t *)(V_IOMAP_ADDR))[ \
		mmu_btop((u_int)addr - SYSBASE)] = *ptr; }
#else
#define	IOMAP_SYNC(addr, ptr)
#endif

extern	int	xc_broadcast_tlbflush();
extern	void	xc_waitfor_tlbflush();

/*
 * Macros for implementing MP critical sections in the hat layer.
 */
#ifdef  MP
extern int flushes_require_xcalls;

/*
 * OPTIMAL_CACHE can be defined to cause flushing to occur only on those
 * CPUs that have accessed the address space.  During the debugging process,
 * this code can be selectively enabled or disabled.
 */
#define	OPTIMAL_CACHE

#ifndef	OPTIMAL_CACHE
/*
 * The CAPTURE_CPUS and RELEASE_CPUS macros can be used to implement
 * critical code sections where a set of CPUs are held while ptes are
 * updated and the TLB and VAC caches are flushed.  This prevents races
 * where a pte is being updated while another CPU is accessing this
 * pte or past instances of this pte in its TLB.  The current feeling is
 * that it may be possible to avoid these races without this explicit
 * capture-release protocol.  However, keep these macros around just in
 * case they are needed.
 * flushes_require_xcalls is set during the start-up sequence once MP
 * start-up is about to begin, and once the x-call mechanisms have been
 * initialized.
 * Note that some I86MMU-based machines may not need critical section
 * support, and they will never set flushes_require_xcalls.
 */
#define	CAPTURE_CPUS    if (flushes_require_xcalls) \
				xc_capture_cpus(-1);

#define	CAPTURE_SELECTED_CPUS(mask)    if (flushes_require_xcalls) \
					xc_capture_cpus(-1);

#define	RELEASE_CPUS    if (flushes_require_xcalls) \
				xc_release_cpus();

/*
 * The XCALL_PROLOG and XCALL_EPILOG macros are used before and after
 * code which performs x-calls.  XCALL_PROLOG will obtain exclusive access
 * to the x-call mailbox (for the particular x-call level) and will specify
 * the set of CPUs involved in the x-call.  XCALL_EPILOG will release
 * exclusive access to the x-call mailbox.
 * Note that some I86MMU machines may not need to perform x-calls for
 * cache flush operations, so they will not set flushes_require_xcalls.
 */
#define	XCALL_PROLOG    if (flushes_require_xcalls) \
				xc_prolog(-1);

#define	XCALL_EPILOG    if (flushes_require_xcalls) \
				xc_epilog();
#else	OPTIMAL_CACHE
#define	CAPTURE_CPUS(arg)						\
	if (flushes_require_xcalls) {					\
		if ((arg)) {					\
			xc_capture_cpus((arg)->hat_cpusrunning);	\
		} else							\
			xc_capture_cpus(-1);				\
	}

#define	CAPTURE_SELECTED_CPUS(mask)					\
	if (flushes_require_xcalls) {					\
		xc_capture_cpus(mask);					\
	}

#define	RELEASE_CPUS	if (flushes_require_xcalls) { \
				xc_release_cpus(); \
			}

#define	TLBFLUSH_BRDCST(arg, addr, gen)					\
	if (flushes_require_xcalls) {					\
		if ((arg)) {					\
			gen = xc_broadcast_tlbflush((arg)->hat_cpusrunning, \
			addr);\
		} else							\
			gen = xc_broadcast_tlbflush(-1, addr);		\
	}

#define	TLBFLUSH_WAIT(gen)	xc_waitfor_tlbflush(gen)


#endif	OPTIMAL_CACHE

#else   /* MP */
#define	CAPTURE_CPUS
#define	RELEASE_CPUS
#define	CAPTURE_SELECTED_CPUS(mask)
#define	XCALL_PROLOG
#define	XCALL_EPILOG
#endif  /* MP */

#if defined(_KERNEL)

/*
 * functions to atomically manipulate hat_cpusrunning mask
 * (defined in i86/ml/i86.il)
 */
extern void atomic_orl(unsigned long *addr, unsigned long val);
extern void atomic_andl(unsigned long *addr, unsigned long val);
extern void atomic_orb(unsigned char *addr, unsigned char val);
extern void atomic_andb(unsigned char *addr, unsigned char val);
extern u_int intr_clear();
extern void intr_restore(u_int);

/*
 * These routines are all MMU-SPECIFIC interfaces to the i86mmu routines.
 * These routines are called from machine specific places to do the
 * dirty work for things like boot initialization, mapin/mapout and
 * first level fault handling.
 */

/*
 * These flags are defined by mmu, but they get passed down by hat layer
 * Theses flags would be or'ed with HAT_MAP or HAT_UNMAP and are used
 * in i86mmu_map() and i86mmu_unmap().
 */
#define	HAT4KPAGESIZE	0x200
#define	HAT4MBPAGESIZE	0x400


/*
 * This data is used in MACHINE-SPECIFIC places.
 */
extern	struct i86mmu	*i86mmus, *ei86mmus;
extern	struct pte *kptes, *keptes;
extern	struct hment *khments;

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_HAT_I86MMU_H */
