/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the PowerPC MMU (ppcmmu)
 * specific hat data structures and the ppcmmu specific hat procedures.
 * The machine independent interface is described in <vm/hat.h>.
 *
 * The definitions assume 32bit implementation of PowerPC.
 */

#ifndef	_VM_HAT_PPCMMU
#define	_VM_HAT_PPCMMU

#pragma ident	"@(#)hat_ppcmmu.h	1.13	96/06/07 SMI"

#include <sys/t_lock.h>
#include <vm/hat.h>
#include <sys/types.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <vm/seg.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	HAT_SUPPORTED_LOAD_FLAGS (HAT_LOAD | HAT_LOAD_LOCK | HAT_LOAD_ADV |\
		HAT_LOAD_CONTIG | HAT_LOAD_NOCONSIST | HAT_LOAD_REMAP)

/*
 * PPC mmu specific flags for page_t
 */
#define	P_PNC	0x8		/* non-caching is permanent bit */
#define	P_TNC	0x10		/* non-caching is temporary bit */

#define	PP_GENERIC_ATTR(pp)	((pp)->p_nrm & (P_MOD | P_REF | P_RO))
#define	PP_ISMOD(pp)		((pp)->p_nrm & P_MOD)
#define	PP_ISREF(pp)		((pp)->p_nrm & P_REF)
#define	PP_ISNC(pp)		((pp)->p_nrm & (P_PNC|P_TNC))
#define	PP_ISPNC(pp)		((pp)->p_nrm & P_PNC)
#define	PP_ISTNC(pp)		((pp)->p_nrm & P_TNC)
#define	PP_ISRO(pp)		((pp)->p_nrm & P_RO)

#define	PP_SETMOD(pp)		((pp)->p_nrm |= P_MOD)
#define	PP_SETREF(pp)		((pp)->p_nrm |= P_REF)
#define	PP_SETREFMOD(pp)	((pp)->p_nrm |= (P_REF|P_MOD))
#define	PP_SETPNC(pp)		((pp)->p_nrm |= P_PNC)
#define	PP_SETTNC(pp)		((pp)->p_nrm |= P_TNC)
#define	PP_SETRO(pp) 		((pp)->p_nrm |= P_RO)
#define	PP_SETREFRO(pp)		((pp)->p_nrm |= (P_REF|P_RO))

#define	PP_CLRMOD(pp)		((pp)->p_nrm &= ~P_MOD)
#define	PP_CLRREF(pp)		((pp)->p_nrm &= ~P_REF)
#define	PP_CLRREFMOD(pp)	((pp)->p_nrm &= ~(P_REF|P_MOD))
#define	PP_CLRPNC(pp)		((pp)->p_nrm &= ~P_PNC)
#define	PP_CLRTNC(pp)		((pp)->p_nrm &= ~P_TNC)
#define	PP_CLRRO(pp)		((pp)->p_nrm &= ~P_RO)

#define	HAT_PRIVSIZ 4		/* number of words of private data storage */

struct hat {
	struct	hat	*hat_next;	/* for address space list */
	struct	as	*hat_as;	/* as this hat provides mapping for */
	u_int	hat_data[HAT_PRIVSIZ];	/* private data optimization */
	kmutex_t	hat_mutex;	/* protects hat, hatops */
	int		ppc_rss;	/* approx # of pages used by as */
	short		s_rmstat;	/* turn on/off getting statistics */
};

/*
 * The hment entry, hat mapping entry.
 * The ppcmmu dependent translation on the mapping list for a page
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




#ifndef _ASM
/*
 * MMU specific hat data for ppcmmu is in hat_data[] array of generic
 *  hat structure (see common/vm/hat.h).
 *
 * Use of hat_data[] array for PPC MMU:
 *
 *	hat_data[0] -	vsid-range-id.
 *		Specifies the start of vsid range for this address space.
 *		The value represents the top 20 bits of the (24 bit) VSID field
 *		in segment register. It is -1 if no valid VSID range is
 *		assigned to this hat.
 *
 */

/*
 * Per PTE-Group-Pair (PTEGP) structure. Since the mapping/unmapping is done
 * within a PTEG pair this structure contains the information about the ptes
 * within the PTEG pair.
 *
 *	ptegp_mutex
 *		Changing mappings in the PTEG pair requires this lock. And
 *		it protects all the fields in this structure.
 *
 *	ptegp_validmap
 *		Each bit indicates if the corresponding PTE is valid.
 *		Note:	LSB corresponds to the lowest numbered PTE in the PTEG
 *			pair.
 *
 *	ptegp_lockmap
 *		Two bits per PTE are used to indicate lock count and when the
 *		lock count exceeds 2 then the count for that pte is maintained
 *		in a hash table. The values for each lock field is:
 *			0 	lockcnt=0
 *			1	lockcnt=1
 *			2	lockcnt=2
 *			3	lockcnt=get_ptelock(pte)
 *		Note:	The LSB field corresponds to the lowest numbered PTE
 *			in the PTEG pair.
 *
 */

typedef struct ptegp {
	kmutex_t	ptegp_mutex;
	u_short		ptegp_validmap;
	u_short		ptegp_unused;		/* unused */
	u_int		ptegp_lockmap;
} ptegp_t;

/*
 * stats for ppcmmu
 */
struct vmhatstat {
	kstat_named_t	vh_pteoverflow;		/* pte overflow count */
	kstat_named_t	vh_pteload;		/* calls to ppcmmu_pteload */
	kstat_named_t	vh_mlist_enter;		/* calls to mlist_lock_enter */
	kstat_named_t	vh_mlist_enter_wait;	/* had to do a cv_wait */
	kstat_named_t	vh_mlist_exit;		/* calls to mlist_lock_exit */
	kstat_named_t	vh_mlist_exit_broadcast; /* had to do cv_broadcast */
	kstat_named_t	vh_vsid_gc_wakeups;	/* wakeups to ppcmmu_vsid_gc */
	kstat_named_t	vh_hash_ptelock;	/* #of pte lock counts hashed */
};

/*
 * structure for vsid range allocation set.
 */
struct vsid_alloc_set {
	int	vs_nvsid_ranges;		/* no. of free vsid ranges */
	int	vs_vsid_range_id;		/* vsid range id */
	struct vsid_alloc_set	*vs_next;	/* pointer to the next set */
};

/*
 * Software structure definition for BAT information.
 */
struct batinfo {
	u_short	batinfo_type;	/* Bat type, 0 text, 1 data */
	u_short	batinfo_valid;	/* Valid flag 		*/
	caddr_t	batinfo_vaddr;	/* virtual address	*/
	caddr_t	batinfo_paddr;	/* physical address	*/
	u_int	batinfo_size;	/* block size in bytes	*/
};

#endif /* _ASM */

/*
 * Some MMU specific constants.
 */
#define	NPTEPERPTEGP	16	/* number of ptes in PTEG pair */
#define	NPTEPERPTEG	8	/* number of ptes in PTEG */
#define	H_PRIMARY	0	/* H bit value for primary PTEG entry */
#define	H_SECONDARY	1	/* H bit value for secondary PTEG entry */

#define	HASH_VSIDMASK		0x7FFFF
#define	HASH_PAGEINDEX_MASK	0xFFFF
#define	SEGREGSHIFT		28
#define	SEGREGMASK		0xF

#define	PTEGSIZE	0x40		/* PTEG size in bytes */
#define	PTEOFFSET	(PTEGSIZE -1)	/* mask for pte offset within pteg */
#define	PTESHIFT	3		/* pte index within the PTEG */

#define	APISHIFT	22		/* API field in logical address */
#define	APIMASK		0x3F		/* mask for API value */

/*
 * Flags for hme_impl field in hment structure.
 */
#define	HME_BUSY	1	/* hme is being modified elsewhere */
#define	HME_STOLEN	2	/* hme was stolen last time in the group */

/*
 * Flags to pass to ppcmmu_pteunload().
 */
#define	PPCMMU_NOSYNC	0x00
#define	PPCMMU_RMSYNC	0x01
#define	PPCMMU_INVSYNC	0x02
#define	PPCMMU_RMSTAT	0x04
#define	PPCMMU_NOFLUSH	0x08

/*
 * Flags to pass to hat_pagecachectl
 */
#define	HAT_CACHE	0x0
#define	HAT_UNCACHE	0x1
#define	HAT_TMPNC	0x2

/* Lockflag to ppcmmu_ptefind() */
#define	PTEGP_LOCK	1	/* ptegp_mutex needed */
#define	PTEGP_NOLOCK	0	/* ptegp_mutex not needed */

extern int pf_is_memory(u_int pf);


#define	VSIDRANGE_INVALID	0		/* invalid VSID range */
#define	MAX_VSIDRANGES		0x00100000	/* max vsid ranges available */
#define	VSIDRANGE_SHIFT		4
/* default number of vsid  ranges used */
#ifdef DEBUG
#define	DEFAULT_VSIDRANGES	0x400
#else
#define	DEFAULT_VSIDRANGES	0x00080000
#endif

/* definitions for lock count field(s) in ptegp_lockmap word */
#ifdef HAT_DEBUG
#define	LKCNT_HASHED		1	/* pte lock count is in hash table */
#else
#define	LKCNT_HASHED		3	/* pte lock count is in hash table */
#endif
#define	LKCNT_UPPER_SHIFT	16	/* for upper PTEG in ptegp_lockmap */
#define	LKCNT_LOWER_SHIFT	0	/* for lower PTEG in ptegp_lockmap */
#define	LKCNT_MASK		0x3	/* lock count mask */
#define	LKCNT_SHIFT		2	/* shift for lock count bits */
#define	LKCNT_NBITS		2	/* no. of bits for lock count field */

#ifndef _ASM

/*
 * Macros
 */
#define	vsid_valid(range) ((range) != VSIDRANGE_INVALID)
#define	hme_to_hwpte(p)	(&ptes[(struct hment *)(p) - hments])
#define	hwpte_to_hme(p)	(&hments[(hwpte_t *)(p) - ptes])
/*
 * hash_get_primary(h)
 *	Given the primary hash function value compute the address of the
 *	primary PTEG.
 */
#define	hash_get_primary(h) \
	(hwpte_t *)((((u_int)(h) & hash_pteg_mask) << 6) + (u_int)ptes)
/*
 * hash_get_secondary(h)
 *	Given the primary hash function value compute the address of the
 *	secondary PTEG.
 */
#define	hash_get_secondary(pri_hash) \
	(hwpte_t *)((hash_pteg_mask << 6) ^ (u_int)pri_hash)

/*
 * These routines are all MMU-SPECIFIC interfaces to the ppcmmu routines.
 * These routines are called from machine specific places to do the
 * dirty work for things like boot initialization, mapin/mapout and
 * first level fault handling.
 */

void ppcmmu_memload(struct hat *, struct as *, caddr_t, struct page *,
		u_int, int);
void ppcmmu_devload(struct hat *, struct as *, caddr_t, devpage_t *,
		u_int, u_int, int);
void ppcmmu_mempte(struct hat *, struct page *, u_int, struct pte *, caddr_t);
u_int ppcmmu_vtop_prot(caddr_t, u_int);
u_int ppcmmu_ptov_prot(hwpte_t *);
hwpte_t *ppcmmu_ptefind(struct as *, caddr_t, int);
ptegp_t * hwpte_to_ptegp(hwpte_t *p);
ptegp_t * hme_to_ptegp(struct hment *p);
u_int ppcmmu_getpfnum(struct as *, caddr_t);
u_int ppcmmu_getkpfnum(caddr_t);
void ptelock(struct ptegp *, hwpte_t *);
void pteunlock(struct ptegp *, hwpte_t *);


/*
 * This data is used in MACHINE-SPECIFIC places.
 */
extern	struct hment *hments, *ehments;
extern	hwpte_t *ptes;		/* ptes	Page Table start */
extern	hwpte_t *eptes;		/* Page Table end */
extern	hwpte_t *mptes;		/* upper half of Page Table */
extern	struct ptegp *ptegps;	/* Per-PTEG-pair structures */
extern	struct ptegp *eptegps;	/* end of Per-PTEG-pair structures */
extern	u_int nptegp;		/* number of PTEG-pairs */
extern	u_int nptegmask;	/* mask for the PTEG number */
extern	u_int hash_pteg_mask;	/* mask used for computing PTEG offset */
extern	u_int mmu601;		/* flag to indicate if it is 601 MMU */
extern	int dcache_blocksize;
extern	int cache_blockmask;
extern	struct batinfo bats[];	/* Bat information table */

/* low level functions defined in ppcmmu_asm.s */
extern u_int mmu_delete_pte(hwpte_t *, caddr_t);
extern u_int mmu_pte_reset_rmbits(hwpte_t *, caddr_t);
extern void mmu_update_pte_prot(hwpte_t *, int, caddr_t);
extern void mmu_update_pte_wimg(hwpte_t *, int, caddr_t);
extern void mmu_update_pte(hwpte_t *, pte_t *, int, caddr_t);
extern void mmu_segload(u_int);
extern void mmu_cache_flushpage(caddr_t);
#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_HAT_PPCMMU */
