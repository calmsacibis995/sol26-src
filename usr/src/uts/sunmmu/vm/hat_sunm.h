/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#ifndef _VM_HAT_SUNM_H
#define	_VM_HAT_SUNM_H

#pragma ident	"@(#)hat_sunm.h	1.29	96/06/11 SMI" /* from S5R4 1.4 */

#include <sys/t_lock.h>
#include <vm/page.h>
#include <vm/hat.h>
#include <sys/mmu.h>
#include <sys/pte.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the machine specific
 * hat data structures and the machine specific hat procedures.
 * The machine independent interface is described in <vm/hat.h>.
 */

#define	HAT_SUPPORTED_LOAD_FLAGS (HAT_LOAD | HAT_LOAD_LOCK | HAT_LOAD_ADV |\
		HAT_LOAD_CONTIG | HAT_LOAD_NOCONSIST | HAT_LOAD_REMAP)
/*
 * sun mmu specific flags for page_t
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
#define	PP_SETRO(pp)		((pp)->p_nrm |= P_RO)
#define	PP_SETREFRO(pp)		((pp)->p_nrm |= (P_REF|P_RO))

#define	PP_CLRMOD(pp)		((pp)->p_nrm &= ~P_MOD)
#define	PP_CLRREF(pp)		((pp)->p_nrm &= ~P_REF)
#define	PP_CLRREFMOD(pp)	((pp)->p_nrm &= ~(P_REF|P_MOD))
#define	PP_CLRPNC(pp)		((pp)->p_nrm &= ~P_PNC)
#define	PP_CLRTNC(pp)		((pp)->p_nrm &= ~P_TNC)
#define	PP_CLRRO(pp)		((pp)->p_nrm &= ~P_RO)




#define	HAT_PRIVSIZ	4	/* number of words of private data storage */

struct hat {
	struct	hat	*hat_next;	/* for address space list */
	struct	as	*hat_as;	/* as this hat provides mapping for */
	u_int   hat_data[HAT_PRIVSIZ];	/* private data optimization */
	kmutex_t	hat_mutex;	/* protects hat, hatops */
	int		sunm_rss;	/* approx # od pages used by as */
	short		s_rmstat;	/* turn on/off getting statistics */
};

/*
 * The hment entry, hat mapping entry.
 * The sunm mmu dependent translation on the mapping list for a page
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

/* private data for the sun mmu hat */
struct sunm {
	struct	ctx	*sunm_ctx;	/* hardware ctx info (if any) */
	struct	pmgrp	*sunm_pmgrps;	/* pmgrps belonging to this hat */
	struct	smgrp	*sunm_smgrps;	/* smgrps belonging to this hat */
	u_int		sunm_pmgldcnt;	/* number of pmgrps loaded in HW */
};

struct ctx {
	u_char	c_lock;			/* this ctx is locked */
	u_char	c_clean;		/* this ctx has no lines in cache */
	u_char	c_num;			/* hardware info (context number) */
	u_short	c_time;			/* pseudo-time for ctx lru */
	struct	as *c_as;		/* back pointer to address space */
};

struct sment {
	struct	pmgrp *sme_pmg;		/* what pmeg this maps */
	int	sme_valid;
};

struct smgrp {
	u_int	smg_lock: 1;
	u_int	smg_num: 15;		/* hardware info (smgrp #) */
	u_short	smg_keepcnt;		/* `keep' count */
	struct	as *smg_as;		/* address space using this smgrp */
	caddr_t	smg_base;		/* base address within address space */
	struct	smgrp *smg_next;	/* next smgrp for this as */
	struct	smgrp *smg_prev;	/* previous smgrp for this as */
	struct	sment *smg_sme;		/* sment array for this smgrp */
};


struct pmgrp {
	u_int	pmg_lock: 1;
	u_int	pmg_mapped: 1;		/* pmgrp is pointed to from seg map */
	u_int	pmg_num: 14;		/* hardware info (pmgrp #) */
	u_short	pmg_keepcnt;		/* `keep' count */
	struct	as *pmg_as;		/* address space using this pmgrp */
	caddr_t	pmg_base;		/* base address within address space */
	struct	pmgrp *pmg_next;	/* next pmgrp for this as */
	struct	pmgrp *pmg_prev;	/* previous pmgrp for this as */
	struct	sment *pmg_sme;		/* ptr to the sment for this pmgrp */
	struct	hment pmg_hme[NPMENTPERPMGRP];	/* hment array for this pmgrp */
	struct  pte   pmg_pte[NPMENTPERPMGRP];	/* SW copies of PTE's */
};

struct hwpmg {
	struct	pmgrp	*hwp_pmgrp;	/* pointer to SW pmgrp */
	struct	hwpmg	*hwp_next;	/* used in HW free list */
};

struct pmgseg {
	struct pmgrp	*pms_base;
	struct pmgseg	*pms_next;
	int		pms_size;
};

#define	PMGNUM_SW	0x3fff		/* magic pmgnum for software pmegs */

/*
 * Flags to pass to sunm_pteunload() and sunm_ptesync().
 */
#define	SUNM_RMSYNC	0x01
#define	SUNM_NCSYNC	0x02
#define	SUNM_INVSYNC	0x40
#define	SUNM_VADDR	0x08
#define	SUNM_RMSTAT	0x10

/*
 * Flags to pass to hat_pteload() and segkmem_mapin().
 * These are part of HAT_FLAGS_RESV
 */
#define	PTELD_IOCACHE	0x1000000
#define	PTELD_INTREP	0x2000000	/* it will be removed in generic */
					/* segkmem */

/*
 * mode for sunm_chgattr
 */
#define	SUNM_SETATTR	0x0
#define	SUNM_CLRATTR	0x1
#define	SUNM_CHGATTR	0x2

#ifdef _KERNEL

extern struct ctx *ctxs;
extern int mmu_3level;		/* indicates 3 level MMU can exist */

/*
 * The sun mmu requires a reserved pmgrp (and smgrp for three level mmus)
 * that it uses for invalid portions of a context.  Startup code should
 * verify that they are invalid as changes in the bootup environment have
 * tended to use these mappings.
 */
extern	struct pmgrp *pmgrp_invalid;

#ifdef MMU_3LEVEL
extern  struct smgrp *smgrp_invalid;
#endif /* MMU_3LEVEL */

/*
 * These routines are all MACHINE-SPECIFIC interfaces to the hat routines.
 * These routines are called from machine specific places to do the
 * dirty work for things like boot initialization, mapin/mapout and
 * first level fault handling.
 */

/*
 * Boot time initialization routines.
 */
void	sunm_pmginit(void);
void	sunm_pmgreserve(struct as *, caddr_t, int);
void	sunm_hwpmgshuffle(void);

/*
 * Reserve hat resources for the address range. In this implementation, this
 * means allocating and locking the pmegs. This currently only works with
 * kernel addresses.
 */
void	sunm_reserve(struct as *, caddr_t, u_int);

/*
 * Called by UNIX during pagefault to insure a context is allocated
 * for this address space and that the ctx time is updated.  Also
 * used internally before all operations involving the translation
 * hardware if you need to be in the correct context for the operation.
 * Done inline for performance reasons.
 */
struct	as *sunm_setup(struct as *, int);

void	sunm_pteload(struct as *, caddr_t, page_t *, struct pte, u_int, int);
u_int	sunm_vtop_prot(u_int);
void	sunm_mempte(page_t *, u_int, struct pte *);
void	sunm_unload(struct as *, caddr_t, u_int, int);


extern kmutex_t	sunm_mutex;
extern int pf_is_memory(u_int pf);

void	hat_pagecachectl(struct page *, int);
void	hat_page_badecc(u_long);

/* flags for hat_pagecachectl */
#define	HAT_CACHE	0x0
#define	HAT_UNCACHE	0x1
#define	HAT_TMPNC	0x2

/*
 * Machine-dependent stuff for dealing with hw and sw pmgs
 */
void	sunm_pmgswapptes(caddr_t, struct pte *, struct pte *);
void	sunm_pmgloadptes(caddr_t, struct pte *);
void	sunm_pmgunloadptes(caddr_t, struct pte *);
void	sunm_pmgloadswptes(caddr_t, struct pte *);

faultcode_t	sunm_fault(struct hat *, caddr_t);

/*
 * sunmmu hat locking functions
 */
void    hat_page_enter(struct page *);
void    hat_page_exit(struct page *);
void    hat_mlist_enter(struct page *);
void    hat_mlist_exit(struct page *);
/*
 * ECC handling routine
 */
int hat_kill_procs(page_t *, caddr_t);


#endif /* _KERNEL */


/*
 * hat layer statistics.
 */

/*
 * Context, smeg, and pmeg statistics.
 */
struct vmhatstat {

	/* Context allocation statistics */
	kstat_named_t vh_ctxfree;	/* ctx allocs without a ctx steal */
	kstat_named_t vh_ctxstealclean;	/* ctx allocs requiring a ctx steal */
	kstat_named_t vh_ctxstealflush;	/* ctx allocs requiring a ctx steal */
	kstat_named_t vh_ctxmappmgs;	/* pmgs mapped at ctx allocation */

	/* SW pmg statistics */
	kstat_named_t vh_pmgallocfree;	/* pmg allocs without a pmg steal */
	kstat_named_t vh_pmgallocsteal;	/* pmg allocs requiring a pmg steal */

	/* HW pmg map and load/unload statistics */
	kstat_named_t vh_pmgmap;	/* mappings of loaded pmgs */
	kstat_named_t vh_pmgldfree;	/* allocs of free HW pmg */
	kstat_named_t vh_pmgldnoctx;	/* HW pmg allocs with no ctx */
	kstat_named_t vh_pmgldcleanctx;	/* HW pmg allocs with clean ctx */
	kstat_named_t vh_pmgldflush;	/* HW pmg allocs needing flush */
	kstat_named_t vh_pmgldnomap;    /* HW pmg allocs taking unmapped pmg */

	/* sunm_fault statitistics */
	kstat_named_t vh_faultmap;	/* sunm_fault mapped HW pmg */
	kstat_named_t vh_faultload;	/* sunm_fault loaded SW pmg in HW */
	kstat_named_t vh_faultinhw;	/* sunm_fault failed to resolve fault */
	kstat_named_t vh_faultnopmg;	/* sunm_fault failed to resolve fault */
	kstat_named_t vh_faultctx;	/* sunm_fault setup context */

	/* HW smg allocation statistics */
	kstat_named_t vh_smgfree;
	kstat_named_t vh_smgnoctx;
	kstat_named_t vh_smgcleanctx;
	kstat_named_t vh_smgflush;

	/* added later to the end not to break pmegstat */
	kstat_named_t vh_pmgallochas;	/* has already a pmeg */

	/* pmgseg statistics */
	kstat_named_t vh_pmsalloc;
	kstat_named_t vh_pmsfree;
	kstat_named_t vh_pmsallocfail;
};

/*
 * Page cacheability statistics.
 */
struct cache_stats {
	int cs_ionc;		/* non-cached IO translations */
	int cs_ioc;		/* cached IO translations */
	int cs_knc;		/* non-cached kernel translations */
	int cs_kc;		/* cached kernel translations */
	int cs_unc;		/* non-cached user translations */
	int cs_uc;		/* cached user translations */
	int cs_other;		/* other non-type 0 pages */
	int cs_iowantchg;	/* # of IO cached to NC changes skipped */
	int cs_kchange;		/* # of kernel cached to non-cached changes */
	int cs_uchange;		/* # of user cached to non-cached changes */
	int cs_unloadfix;	/* # of unload's that made pages cachable */
	int cs_unloadnofix;	/* # of  " that didn't made pages cachable */
	int cs_skip;		/* XXX should be after cs_other */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_HAT_SUNM_H */
