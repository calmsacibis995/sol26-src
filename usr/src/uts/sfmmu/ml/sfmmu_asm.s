/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sfmmu_asm.s	1.95	96/10/09 SMI"

/*
 * SFMMU primitives.  These primitives should only be used by sfmmu
 * routines.
 */

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/machtrap.h>
#include <sys/spitasi.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <vm/hat_sfmmu.h>
#include <sys/machparam.h>
#include <sys/privregs.h>
#include <sys/scb.h>
#include <sys/machthread.h>

#include <sys/spitregs.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#ifndef	lint

/*
 * Only the cpu who owns a tsb can modify it so we don't need
 * locking to protect it.
 * Assumes TSBE_TAG is 0
 */

#if TSBE_TAG != 0
ERROR- TSB_UPDATE assumes TSBE_TAG = 0
#endif

#define	TSB_UPDATE(tsb8k, tte, tagtarget)				\
	stxa	tagtarget, [tsb8k] ASI_MEM	/* write tte tag */	;\
	add	tsb8k, TSBE_TTE, tsb8k					;\
	stxa	tte, [tsb8k] ASI_MEM		/* write tte data */


#if (1 << TSBINFO_SZSHIFT) != TSBINFO_SIZE
ERROR- TSBBASESZ_SHIFT does not correspond to TSBINFO_SIZE
#endif

/*
 * macro to flush all tsb entries with ctx = ctxnum
 * This macro uses physical addresses to access the tsb.  It is only
 * meant to be used at tl>0.  A routine that executes at tl0
 * is in sfmmu_unload_tsbctx().
 *
 * ctxnum = 32 bit reg containing ctxnum to flush
 * tmp1, tmp2, tmp5 = 64 bit tmp registers
 * tmp3, tmp4 = 32 bit tmp registers
 */
#define	FLUSH_TSBCTX(ctxnum, tmp1, tmp2, tmp3, tmp4, tmp5)		\
	set	MMU_TSB, tmp1						;\
	ldxa	[tmp1]ASI_DMMU, tmp1	/* inline sfmmu_get_dtsb() */	;\
	and	tmp1, TSB_SZ_MASK, tmp2					;\
	srlx	tmp1, TSBBASE_SHIFT, tmp1				;\
	add	tmp2, TSB_START_SIZE + TSB_ENTRY_SHIFT, tmp2		;\
	set	1, tmp3							;\
	sllx	tmp1, TSBBASE_SHIFT, tmp1	/* tmp1 = tsb base */	;\
	sllx	tmp3, tmp2, tmp2		/* tmp2 = tsb size */	;\
	add	tmp1, tmp2, tmp2		/* tmp2 = end of tsb */	;\
	sethi	%hi(TSBTAG_INVALID), tmp3	/* tmp3 = inv entry */	;\
	add	tmp1, TSBE_TAG + TSBTAG_INTHI, tmp1			;\
	lduha	[tmp1] ASI_MEM, tmp4		/* read tag */		;\
0:									;\
	/* preload next entry to make cache hot for next time around */ ;\
	add	tmp1, TSB_ENTRY_SIZE, tmp5				;\
	lda	[tmp5] ASI_MEM, %g0					;\
									;\
	cmp	tmp4, ctxnum		/* compare the tags */		;\
	be,a,pn %icc, 1f						;\
	sta	tmp3, [tmp1] ASI_MEM	/* invalidate the entry */	;\
1:									;\
	add	tmp1, TSB_ENTRY_SIZE, tmp1	/* the next entry */	;\
	cmp	tmp1, tmp2		/* if not end of TSB go back */ ;\
	bl,a,pt	%icc, 0b						;\
	lduha	[tmp1] ASI_MEM, tmp4		/* read tag */

#endif (lint)


#if defined (lint)

/*
 * sfmmu related subroutines
 */

/* ARGSUSED */
u_int
sfmmu_ctx_steal_tl1(int sctx, int rctx)
{ return(0); }

/* ARGSUSED */
void
sfmmu_itlb_ld(caddr_t vaddr, int ctxnum, tte_t *tte)
{}

/* ARGSUSED */
void
sfmmu_dtlb_ld(caddr_t vaddr, int ctxnum, tte_t *tte)
{}

/*
 * Use cas, if tte has changed underneath us then reread and try again.
 * In the case of a retry, it will update sttep with the new original.
 */
/* ARGSUSED */
int
sfmmu_modifytte(tte_t *sttep, tte_t *stmodttep, tte_t *dttep)
{ return(0); }

/*
 * Use cas, if tte has changed underneath us then return 1, else return 0
 */
/* ARGSUSED */
int
sfmmu_modifytte_try(tte_t *sttep, tte_t *stmodttep, tte_t *dttep)
{ return(0); }

/* ARGSUSED */
void
sfmmu_copytte(tte_t *sttep, tte_t *dttep)
{}

int
sfmmu_getctx_pri()
{ return(0); }

int
sfmmu_getctx_sec()
{ return(0); }

/* ARGSUSED */
void
sfmmu_setctx_pri(int ctx)
{}

/* ARGSUSED */
void
sfmmu_setctx_sec(int ctx)
{}

/*
 * Supports only 32 bit virtual addresses
 */
uint
sfmmu_get_dsfar()
{ return(0); }

uint
sfmmu_get_isfsr()
{ return(0); }

uint
sfmmu_get_dsfsr()
{ return(0); }

uint
sfmmu_get_itsb()
{ return(0); }

uint
sfmmu_get_dtsb()
{ return(0); }

#else	/* lint */

	.seg	".data"
	.global	sfmmu_panic1
sfmmu_panic1:
	.ascii	"sfmmu_asm: interupts already disabled"
	.byte	0

	.align	4
	.seg	".text"


/*
 * 1. Flush TSB of all entrieds whose ctx is being stolen.
 * 2. Flush all TLB entries whose ctx is ctx-being-stolen.
 * 3. If processor is running in the ctx-being-stolen, set the
 *    context to the resv context. That is 
 *    If processor in User-mode - pri/sec-ctx both set to ctx-being-stolen,
 *		change both pri/sec-ctx registers to resv ctx.
 *    If processor in Kernel-mode - pri-ctx is 0, sec-ctx is ctx-being-stolen,
 *		just change sec-ctx register to resv ctx. When it returns to
 *		kenel-mode, user_rtt will change pri-ctx.
 */
	ENTRY(sfmmu_ctx_steal_tl1)
	/*
	 * %g1 = ctx being stolen
	 * %g2 = new resv ctx
	 */
	FLUSH_TSBCTX(%g1, %g3, %g4, %g5, %g6, %g7)
	set	DEMAP_CTX_TYPE | DEMAP_SECOND, %g4
	set	MMU_SCONTEXT, %g3
	ldxa	[%g3]ASI_DMMU, %g5		/* rd sec ctxnum */
	cmp	%g5, %g1
	be,a,pn %icc, 0f
	nop
	stxa	%g1, [%g3]ASI_DMMU		/* wr ctx being stolen */
0:
	stxa	%g0, [%g4]ASI_DTLB_DEMAP
	stxa	%g0, [%g4]ASI_ITLB_DEMAP	/* flush TLB */
	sethi	%hi(FLUSH_ADDR), %g4
	flush	%g4

	!
	! if (old sec-ctxnum == ctx being stolen) {
	!	write resv ctx to sec ctx-reg
	!	if (pri-ctx == ctx being stolen)
	!		write resv ctx to pri ctx-reg
	! } else
	!	restore old ctxnum
	!
	be,a,pn %icc, 1f
	nop
	stxa	%g5, [%g3]ASI_DMMU		/* restore old ctxnum */
	retry					/* and return */
	membar #Sync
1:
	stxa	%g2, [%g3]ASI_DMMU		/* wr resv ctxnum to sec-reg */
	set	MMU_PCONTEXT, %g3
	ldxa	[%g3]ASI_DMMU, %g4		/* rd pri ctxnum */
	cmp	%g1, %g4
	bne,a 	%icc, 2f
	nop
	stxa	%g2, [%g3]ASI_DMMU		/* wr resv ctxnum to pri-reg */
2:
	retry
	membar #Sync
	SET_SIZE(sfmmu_ctx_steal_tl1)

	ENTRY_NP(sfmmu_itlb_ld)
	rdpr	%pstate, %o3
#ifdef DEBUG
	andcc	%o3, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 1f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#endif /* DEBUG */
	wrpr	%o3, PSTATE_IE, %pstate		/* disable interrupts */

	srl	%o0, MMU_PAGESHIFT, %o0
	sll	%o0, MMU_PAGESHIFT, %o0		/* clear page offset */
	or	%o0, %o1, %o0
	ldx	[%o2], %g1
	set	MMU_TAG_ACCESS, %o5
	stxa	%o0,[%o5]ASI_IMMU
	stxa	%g1,[%g0]ASI_ITLB_IN
	sethi	%hi(FLUSH_ADDR), %o1		/* flush addr doesn't matter */
	flush	%o1				/* flush required for immu */
	retl
	  wrpr	%g0, %o3, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_itlb_ld)

	ENTRY_NP(sfmmu_dtlb_ld)
	rdpr	%pstate, %o3
#ifdef DEBUG
	andcc	%o3, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 1f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#endif /* DEBUG */
	wrpr	%o3, PSTATE_IE, %pstate		/* disable interrupts */

	srl	%o0, MMU_PAGESHIFT, %o0
	sll	%o0, MMU_PAGESHIFT, %o0		/* clear page offset */
	or	%o0, %o1, %o0
	ldx	[%o2], %g1
	set	MMU_TAG_ACCESS, %o5
	stxa	%o0,[%o5]ASI_DMMU
	stxa	%g1,[%g0]ASI_DTLB_IN
	membar	#Sync
	retl
	  wrpr	%g0, %o3, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_dtlb_ld)

	ENTRY_NP(sfmmu_modifytte)
	ldx	[%o2], %g3			/* current */
	ldx	[%o0], %g1			/* original */
2:
	ldx	[%o1], %g2			/* modified */
	cmp	%g2, %g3			/* is modified = current? */
	be,a,pt	%xcc,1f				/* yes, don't write */
	  stx	%g3, [%o0]			/* update new original */
#ifdef SF_ERRATA_12 /* atomics cause hang */
	membar #Sync
#endif
	casx	[%o2], %g1, %g2
	cmp	%g1, %g2
	be,a,pt	%xcc, 1f			/* cas succeeded - return */
	  nop
	ldx	[%o2], %g3			/* new current */
	stx	%g3, [%o0]			/* save as new original */
	ba,pt	%xcc, 2b
	  mov	%g3, %g1
1:	retl
	membar	#StoreLoad
	SET_SIZE(sfmmu_modifytte)

	ENTRY_NP(sfmmu_modifytte_try)
	ldx	[%o1], %g2			/* modified */
	ldx	[%o2], %g3			/* current */
	ldx	[%o0], %g1			/* original */
	cmp	%g3, %g2			/* is modified = current? */
	be,a,pn %xcc,1f				/* yes, don't write */
	  mov	0, %o1				/* as if cas failed. */
		
#ifdef SF_ERRATA_12 /* atomics cause hang */
	membar #Sync
#endif
	casx	[%o2], %g1, %g2
	membar	#StoreLoad
	cmp	%g1, %g2
	movne	%xcc, -1, %o1			/* cas failed. */
	move	%xcc, 1, %o1			/* cas succeeded. */
1:
	stx	%g2, [%o0]			/* report "current" value */
	retl
	mov	%o1, %o0
	SET_SIZE(sfmmu_modifytte_try)

	ENTRY_NP(sfmmu_copytte)
	ldx	[%o0], %g1
	retl
	stx	%g1, [%o1]
	SET_SIZE(sfmmu_copytte)

	ENTRY_NP(sfmmu_getctx_pri)
	set	MMU_PCONTEXT, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_getctx_pri)

	ENTRY_NP(sfmmu_getctx_sec)
	set	MMU_SCONTEXT, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_getctx_sec)

	ENTRY_NP(sfmmu_setctx_pri)
	set	MMU_PCONTEXT, %o1
	stxa	%o0, [%o1]ASI_DMMU
	sethi	%hi(FLUSH_ADDR), %o1		/* flush addr doesn't matter */
	retl
	  flush	%o1				/* flush required for immu */
	SET_SIZE(sfmmu_setctx_pri)

	ENTRY_NP(sfmmu_setctx_sec)
	set	MMU_SCONTEXT, %o1
	ldxa	[%o1]ASI_DMMU, %o2
	cmp	%o2, %o0
	be,pt	%icc, 1f
	  sethi	%hi(FLUSH_ADDR), %o3		/* flush addr doesn't matter */
	stxa	%o0, [%o1]ASI_DMMU
	flush	%o3				/* flush required for immu */
1:
	retl
	nop
	SET_SIZE(sfmmu_setctx_sec)

	ENTRY_NP(sfmmu_get_dsfar)
	set	MMU_SFAR, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_get_dsfar)

	ENTRY_NP(sfmmu_get_isfsr)
	set	MMU_SFSR, %o0
	retl
	ldxa	[%o0]ASI_IMMU, %o0
	SET_SIZE(sfmmu_get_isfsr)

	ENTRY_NP(sfmmu_get_dsfsr)
	set	MMU_SFSR, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_get_dsfsr)

	ENTRY_NP(sfmmu_get_itsb)
	set	MMU_TSB, %o0
	retl
	ldxa	[%o0]ASI_IMMU, %o0
	SET_SIZE(sfmmu_get_itsb)

	ENTRY_NP(sfmmu_get_dtsb)
	set	MMU_TSB, %o0
	retl
	ldxa	[%o0]ASI_DMMU, %o0
	SET_SIZE(sfmmu_get_dtsb)

#endif /* lint */

/*
 * Other sfmmu primitives
 */


#if defined (lint)
/* ARGSUSED */
void
sfmmu_set_itsb(int tsb_bspg, uint split, uint size)
{
}

/* ARGSUSED */
void
sfmmu_set_dtsb(int tsb_bspg, uint split, uint size)
{
}

/* ARGSUSED */
void
sfmmu_load_tsb(caddr_t addr, int ctxnum, tte_t *ttep)
{
}

/* ARGSUSED */
void
sfmmu_unload_tsb(caddr_t addr, int ctxnum, int size)
{
}

/* ARGSUSED */
void
sfmmu_unload_tsbctx(uint ctx)
{
}

/* ARGSUSED */
u_int
sfmmu_unload_tsball()
{
return (0);
}

#else /* lint */

	ENTRY_NP(sfmmu_set_itsb)
	sllx	%o0, TSBBASE_SHIFT, %o0	
	sll	%o1, TSBSPLIT_SHIFT, %o1
	or	%o0, %o1, %g1
	or	%g1, %o2, %g1
	set	MMU_TSB, %o3
	stxa    %g1, [%o3]ASI_IMMU
	sethi	%hi(FLUSH_ADDR), %o1		/* flush addr doesn't matter */
	retl
	  flush	%o1				/* flush required by immu */
	SET_SIZE(sfmmu_set_itsb)

	ENTRY_NP(sfmmu_set_dtsb)
	sllx	%o0, TSBBASE_SHIFT, %o0	
	sll	%o1, TSBSPLIT_SHIFT, %o1
	or	%o0, %o1, %g1
	or	%g1, %o2, %g1
	set	MMU_TSB, %o3
	stxa    %g1, [%o3]ASI_DMMU
	retl
	membar	#Sync
	SET_SIZE(sfmmu_set_dtsb)

	.seg	".data"
load_tsb_panic2:
	.ascii	"sfmmu_load_tsb: ctxnum is INVALID_CONTEXT"
	.byte	0

	.align	4
	.seg	".text"

/*
 * routine that loads an entry into per cpu tsb using physical addresses
 * no locking is required since only the cpu who own the tsg is allowed
 * to update it.
 */
	ENTRY_NP(sfmmu_load_tsb)
	/*
	 * %o0 = addr
	 * %o1 = ctxnum
	 * %o2 = ttep
	 */
	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 3f			/* disabled, panic	 */
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
	ret
	restore
3:
	cmp	%o1, INVALID_CONTEXT
	bne,pt	%icc, 3f
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(load_tsb_panic2), %o0
	call	panic
	 or	%o0, %lo(load_tsb_panic2), %o0
	ret
	restore
3:
#endif /* DEBUG */
	/*
	 * disable ints and clear address mask to access 64 bit physaddr
	 */
	wrpr	%o5, PSTATE_IE | PSTATE_AM, %pstate
	ldx	[%o2], %g5			/* g5 = tte */
	or	%o0, %o1, %o4			/* build tagaccess reg */
	GET_TSB_POINTER(%o4, %g1, %g2, %g3, %g4) /* g1 = tsbp */
	BUILD_TSB_TAG(%o0, %o1, %g2, %o2)	/* g2 = tag target */
	TSB_UPDATE(%g1, %g5, %g2)
	retl
	  wrpr	%g0, %o5, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_load_tsb)


/*
 * Flush this cpus's TSB so that all entries with ctx = ctx-passed are flushed.
 * Assumes it is called with preemption disabled
 */ 
	.seg	".data"
unload_tsbctx_panic:
	.ascii	"sfmmu_unload_tsbctx: preemption enabled"
	.byte	0

	.align	4
	.seg	".text"


	ENTRY(sfmmu_unload_tsbctx)
	/*
	 * %o0 = ctx to be flushed
	 */
#ifdef DEBUG
	ldsh	[THREAD_REG + T_PREEMPT], %o1
	brnz,pt	%o1, 1f
	 nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(unload_tsbctx_panic), %o0
	call	panic
	 or	%o0, %lo(unload_tsbctx_panic), %o0
	ret
	restore
1:
#endif /* DEBUG */

	rdpr	%pstate, %o5
	/*
	 * clear address mask to access 64 bit physaddr
	 * trap handler saves out/globals and tstate so it is ok
	 * to have interrupts enabled.
	 */
	wrpr	%o5, PSTATE_AM, %pstate
	FLUSH_TSBCTX(%o0, %g1, %g2, %o1, %o2, %g3)
	membar	#StoreStore
	retl
	 wrpr	%g0, %o5, %pstate		/* restore pstate */
	SET_SIZE(sfmmu_unload_tsbctx)

/*
 * sfmmu_unload_tsball: xcall that unloads all user entries from the tsb
 */ 
	ENTRY(sfmmu_unload_tsball)
	rdpr	%pstate, %o5
	/*
	 * clear address mask to access 64 bit physaddr
	 * trap handler saves out/globals and tstate so it is ok
	 * to have interrupts enabled.
	 */
	wrpr	%o5, PSTATE_AM, %pstate

	set	MMU_TSB, %o0	
	ldxa	[%o0]ASI_DMMU, %o0
	and	%o0, TSB_SZ_MASK, %o1
	srlx	%o0, TSBBASE_SHIFT, %o0
	add	%o1, TSB_START_SIZE + TSB_ENTRY_SHIFT, %o1
	set	1, %o2
	sllx	%o0, TSBBASE_SHIFT, %o0		/* %o0 = tsb base */
	sllx	%o2, %o1, %o1			/* %o1 = tsb size */
	add	%o0, %o1, %o1			/* %o1 = end of tsb */
	sethi	%hi(TSBTAG_INVALID), %o2	/* %o2 = inv entry */
	add	%o0, TSBE_TAG + TSBTAG_INTHI, %o0
	lda	[%o0] ASI_MEM, %o3		/* read tag */	
0:
	/* preload next entry to make cache hot for next time around */
	add	%o0, TSB_ENTRY_SIZE, %o4
	lda	[%o4] ASI_MEM, %g0
	andcc	%o3, %o2, %g0		/* check if already invalid */
	bnz,pt	%icc, 1f
	 srl	%o3, TSBTAG_CTXSHIFT, %o3	/* get ctx number */
	brnz,a,pt %o3, 1f		/* if KCONTEXT skip store */
	sta	%o2, [%o0] ASI_MEM	/* invalidate the entry */
1:
	add	%o0, TSB_ENTRY_SIZE, %o0	/* the next entry */
	cmp	%o0, %o1		/* if not end of TSB go back */
	bl,a,pt	%icc, 0b
	lda	[%o0] ASI_MEM, %o3
	membar	#StoreStore
	retl
	 wrpr	%g0, %o5, %pstate		/* restore pstate */
	SET_SIZE(sfmmu_unload_tsball)

#endif /* lint */

#if defined (lint)
/* ARGSUSED */
uint sfmmu_ttetopfn(tte_t *tte, caddr_t vaddr)
{ return(0); }

#else /* lint */
#define	TTETOPFN(tte, vaddr, label)					\
	srlx	tte, TTE_SZ_SHFT, %g2					;\
	sllx	tte, TTE_PA_LSHIFT, tte					;\
	andcc	%g2, TTE_SZ_BITS, %g2		/* g2 = ttesz */	;\
	sllx	%g2, 1, %g3						;\
	add	%g3, %g2, %g3			/* mulx 3 */		;\
	add	%g3, MMU_PAGESHIFT + TTE_PA_LSHIFT, %g4			;\
	srlx	tte, %g4, tte						;\
	sllx	tte, %g3, tte						;\
	bz,a,pt	%xcc, label/**/1					;\
	  nop								;\
	set	1, %g2							;\
	add	%g3, MMU_PAGESHIFT, %g4					;\
	sllx	%g2, %g4, %g2						;\
	sub	%g2, 1, %g2		/* g2=TTE_PAGE_OFFSET(ttesz) */	;\
	and	vaddr, %g2, %g3						;\
	srl	%g3, MMU_PAGESHIFT, %g3					;\
	or	tte, %g3, tte						;\
label/**/1:


	ENTRY_NP(sfmmu_ttetopfn)
	ldx	[%o0], %g1			/* read tte */
	TTETOPFN(%g1, %o1, sfmmu_ttetopfn_l1)
	/*
	 * g1 = pfn
	 */
	retl
	mov	%g1, %o0
	SET_SIZE(sfmmu_ttetopfn)

#endif /* !lint */


#if defined (lint)
/*
 * The sfmmu_hblk_hash_add is the assembly primitive for adding hmeblks to the
 * the hash list.
 */
/* ARGSUSED */
void
sfmmu_hblk_hash_add(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	u_longlong_t hblkpa)
{
}

/*
 * The sfmmu_hblk_hash_rm is the assembly primitive to remove hmeblks from the
 * hash list.
 */
/* ARGSUSED */
void
sfmmu_hblk_hash_rm(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	u_longlong_t hblkpa, struct hme_blk *prev_hblkp)
{
}
#else /* lint */

/*
 * Functions to grab/release hme bucket list lock.  I only use a byte
 * instead of the whole int because eventually we might want to
 * put some counters on the other bytes (of course, these routines would
 * have to change).  The code that grab this lock should execute
 * with interrupts disabled and hold the lock for the least amount of time
 * possible.
 */
#define	HMELOCK_ENTER(hmebp, tmp1, tmp2, label1)		\
	mov	0xFF, tmp2					;\
	add	hmebp, HMEBUCK_LOCK, tmp1			;\
label1:								;\
	casa	[tmp1] ASI_N, %g0, tmp2				;\
	brnz,pn	tmp2, label1					;\
	 mov	0xFF, tmp2					;\
	membar	#StoreLoad|#StoreStore

#define	HMELOCK_EXIT(hmebp)					\
	membar	#StoreLoad|#StoreStore				;\
	st	%g0, [hmebp + HMEBUCK_LOCK]

	.seg	".data"
hblk_add_panic1:
	.ascii	"sfmmu_hblk_hash_add: interrupts disabled"
	.byte	0
hblk_add_panic2:
	.ascii	"sfmmu_hblk_hash_add: va hmeblkp is NULL but pa is not"
	.byte	0
	.align	4
	.seg	".text"

	ENTRY_NP(sfmmu_hblk_hash_add)
	/*
	 * %o0 = hmebp
	 * %o1 = hmeblkp
	 * %o2 & %o3  = hblkpa
	 */
	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 3f			/* disabled, panic	 */
	  nop
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(hblk_add_panic1), %o0
	call	panic
	 or	%o0, %lo(hblk_add_panic1), %o0
	ret
	restore

3:
#endif /* DEBUG */
	wrpr	%o5, PSTATE_IE, %pstate		/* disable interrupts */
	sllx	%o2, 32, %g1
	or	%g1, %o3, %g1			/* g1 = hblkpa */
	ld	[%o0 + HMEBUCK_HBLK], %o4	/* next hmeblk */
	ldx	[%o0 + HMEBUCK_NEXTPA], %g2	/* g2 = next hblkpa */
#ifdef	DEBUG
	cmp	%o4, %g0
	bne,a,pt %icc, 1f
	 nop
	brz,a,pt %g2, 1f
	 nop
	wrpr	%g0, %o5, %pstate		/* enable interrupts */
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(hblk_add_panic2), %o0
	call	panic
	  or	%o0, %lo(hblk_add_panic2), %o0
	ret
	restore
1:
#endif /* DEBUG */
	/*
	 * We update hmeblks entries before grabbing lock because the stores
	 * could take a tlb miss and require the hash lock.  The buckets
	 * are part of the nucleus so we are cool with those stores.
	 */
	st	%o4, [%o1 + HMEBLK_NEXT]	/* update hmeblk's next */
	stx	%g2, [%o1 + HMEBLK_NEXTPA]	/* update hmeblk's next pa */
	HMELOCK_ENTER(%o0, %o2 ,%o3, hashadd1)
	st	%o1, [%o0 + HMEBUCK_HBLK]	/* update bucket hblk next */
	stx	%g1, [%o0 + HMEBUCK_NEXTPA]	/* add hmeblk to list */
	HMELOCK_EXIT(%o0)
	retl
	  wrpr	%g0, %o5, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_hblk_hash_add)

	ENTRY_NP(sfmmu_hblk_hash_rm)
	/*
	 * This function removes an hmeblk from the hash chain. 
	 * It is written to guarantee we don't take a tlb miss
	 * by using physical addresses to update the list.
	 * 
	 * %o0 = hmebp
	 * %o1 = hmeblkp
	 * %o2 & %o3 = hmeblkp previous pa
	 * %o4 = hmeblkp previous
	 */
	srl	%o0, 0, %o0			/* clear upper 32 bits */
	srl	%o1, 0, %o1
	srl	%o2, 0, %o2
	srl	%o3, 0, %o3
	srl	%o4, 0, %o4

	rdpr	%pstate, %o5
#ifdef DEBUG
	andcc	%o5, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 3f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
3:
#endif /* DEBUG */
	/*
	 * disable interrupts, clear Address Mask to access 64 bit physaddr
	 */
	wrpr	%o5, PSTATE_IE | PSTATE_AM, %pstate

	HMELOCK_ENTER(%o0, %g1, %g3 , hashrm1)
	ld	[%o0 + HMEBUCK_HBLK], %g2	/* first hmeblk in list */
	cmp	%g2, %o1
	bne,pt	%icc,1f
	 mov	ASI_MEM, %asi
	/*
	 * hmeblk is first on list
	 */
	ldx	[%o0 + HMEBUCK_NEXTPA], %g2	/* g2 = hmeblk pa */
	lda	[%g2 + HMEBLK_NEXT] %asi, %o3	/* read next hmeblk va */
	ldxa	[%g2 + HMEBLK_NEXTPA] %asi, %g1	/* read next hmeblk pa */
	st	%o3, [%o0 + HMEBUCK_HBLK]	/* write va */
	ba,pt	%xcc, 2f
	stx	%g1, [%o0 + HMEBUCK_NEXTPA]	/* write pa */
1:
	/* hmeblk is not first on list */
	sethi   %hi(dcache_line_mask), %g4
	ld	[%g4 + %lo(dcache_line_mask)], %g4
	sllx	%o2, 32, %g3
	or	%g3, %o3, %g3			/* g3 = prev hblk pa */
	and	%o4, %g4, %g2
	stxa	%g0, [%g2] ASI_DC_TAG		/* flush prev pa from dcache */
	add	%o4, HMEBLK_NEXT, %o4
	and	%o4, %g4, %g2
	stxa	%g0, [%g2] ASI_DC_TAG		/* flush prev va from dcache */
	membar	#Sync
	ldxa	[%g3 + HMEBLK_NEXTPA] %asi, %g2	/* g2 = hmeblk pa */ 
	lda	[%g2 + HMEBLK_NEXT] %asi, %o3	/* read next hmeblk va */
	ldxa	[%g2 + HMEBLK_NEXTPA] %asi, %g1	/* read next hmeblk pa */
	sta	%o3, [%g3 + HMEBLK_NEXT] %asi	/* write va */
	stxa	%g1, [%g3 + HMEBLK_NEXTPA] %asi	/* write pa */
2:
	HMELOCK_EXIT(%o0)
	retl
	  wrpr	%g0, %o5, %pstate		/* enable interrupts */
	SET_SIZE(sfmmu_hblk_hash_rm)

#endif /* lint */

/*
 * This macro is used to update any of the global sfmmu kstats
 * in perf critical paths.
 * It is only enabled in debug kernels or if SFMMU_STAT_GATHER is defined
 */
#if defined(DEBUG) || defined(SFMMU_STAT_GATHER)
#define	HAT_GLOBAL_DBSTAT(statname, tmp1, tmp2)				\
	sethi	%hi(sfmmu_global_stat), tmp1				;\
	add	tmp1, statname, tmp1					;\
	ld	[tmp1 + %lo(sfmmu_global_stat)], tmp2			;\
	inc	tmp2							;\
	st	tmp2, [tmp1 + %lo(sfmmu_global_stat)]

#else /* DEBUG || SFMMU_STAT_GATHER */

#define	HAT_GLOBAL_DBSTAT(statname, tmp1, tmp2)

#endif  /* DEBUG || SFMMU_STAT_GATHER */

/*
 * This macro is used to update global sfmmu kstas in non
 * perf critical areas so they are enabled all the time
 */
#define	HAT_GLOBAL_STAT(statname, tmp1, tmp2)				\
	sethi	%hi(sfmmu_global_stat), tmp1				;\
	add	tmp1, statname, tmp1					;\
	ld	[tmp1 + %lo(sfmmu_global_stat)], tmp2			;\
	inc	tmp2							;\
	st	tmp2, [tmp1 + %lo(sfmmu_global_stat)]

/*
 * This macro is used to update per cpu stats in non perf
 * critical areas so they are enabled all the time
 */
#define	HAT_PERCPU_STAT(tsbarea, stat, tmp1)				\
	ld	[tsbarea + stat], tmp1					;\
	inc	tmp1							;\
	st	tmp1, [tsbarea + stat]

#if defined (lint)
/*
 * The following routines are jumped to from the mmu trap handlers to do
 * the setting up to call systrap.  They are separate routines instead of 
 * being part of the handlers because the handlers would exceed 32
 * instructions and since this is part of the slow path the jump
 * cost is irrelevant.
 */
void
sfmmu_pagefault()
{
}

void
sfmmu_mmu_trap()
{
}

void
sfmmu_window_trap()
{
}

#else /* lint */

#ifdef	PTL1_PANIC_DEBUG
	.seg	".data"
	.global	test_ptl1_panic
test_ptl1_panic:
	.word	0
	.align	8

	.seg	".text"
	.align	4
#endif	/* PTL1_PANIC_DEBUG */

#define	USE_ALTERNATE_GLOBALS						\
	rdpr	%pstate, %g5						;\
	wrpr	%g5, PSTATE_MG | PSTATE_AG, %pstate

	ENTRY_NP(sfmmu_pagefault_inval)
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g4
	ldxa	[%g4] ASI_DMMU, %g2
	mov	T_DATA_MMU_MISS, %g3		/* arg2 = traptype */
	ba,a,pt	%icc, .sfmmu_pagefault_common
	  nop

	ENTRY_NP(sfmmu_pagefault)
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g6
	ldxa	[%g4] ASI_DMMU, %g5
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g6, FAST_IMMU_MISS_TT
	be,a,pn	%icc, 1f
	  mov	T_INSTR_MMU_MISS, %g3
	mov	%g5, %g2
	cmp	%g6, FAST_DMMU_MISS_TT
	move	%icc, T_DATA_MMU_MISS, %g3	/* arg2 = traptype */
	movne	%icc, T_DATA_PROT, %g3		/* arg2 = traptype */

.sfmmu_pagefault_common:
	/*
	 * If ctx indicates large ttes then let sfmmu_tsb_miss 
	 * perform hash search before faulting.
	 * g2 = tag access
	 * g3 = trap type
	 */
#ifdef  PTL1_PANIC_DEBUG
	/* check if we want to test the tl1 panic */
	sethi	%hi(test_ptl1_panic), %g4
	ld	[%g4 + %lo(test_ptl1_panic)], %g1
	st	%g0, [%g4 + %lo(test_ptl1_panic)]
	cmp	%g1, %g0
	bne,a,pn %icc, ptl1_panic
	or	%g0, PTL1_BAD_WTRAP, %g1
1:
#endif	/* PTL1_PANIC_DEBUG */
	sll	%g2, TAGACC_CTX_SHIFT, %g4
	srl	%g4, TAGACC_CTX_SHIFT, %g4
	sll	%g4, CTX_SZ_SHIFT, %g4
	sethi	%hi(ctxs), %g6
	ld	[%g6 + %lo(ctxs)], %g1
	add	%g4, %g1, %g1
	lduh	[%g1 + C_FLAGS], %g7
	and	%g7, LTTES_FLAG, %g7
	brz,pt	%g7, 1f
	nop
	sethi	%hi(sfmmu_tsb_miss), %g1
	or	%g1, %lo(sfmmu_tsb_miss), %g1
	ba,pt	%xcc, 2f
	nop
1:
	HAT_GLOBAL_STAT(HATSTAT_PAGEFAULT, %g6, %g4)
	/*
	 * g2 = tag access reg
	 * g3 = type
	 */
	sethi	%hi(trap), %g1
	or	%g1, %lo(trap), %g1
	sllx	%g2, 32, %g5			/* arg4 =  tagaccess */
	or	%g3, %g5, %g2
	clr	%g3				/* arg3 (mmu_fsr)= null */
2:
	ba,pt	%xcc, sys_trap
	  mov	-1, %g4	
	SET_SIZE(sfmmu_pagefault)
	SET_SIZE(sfmmu_pagefault_inval)

	ENTRY_NP(sfmmu_mmu_trap)
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g6
	ldxa	[%g4] ASI_DMMU, %g5
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g6, FAST_IMMU_MISS_TT
	be,a,pn	%icc, 1f
	  mov	T_INSTR_MMU_MISS, %g3
	mov	%g5, %g2
	cmp	%g6, FAST_DMMU_MISS_TT
	move	%icc, T_DATA_MMU_MISS, %g3	/* arg2 = traptype */
	movne	%icc, T_DATA_PROT, %g3		/* arg2 = traptype */
1:
	/*
	 * g2 = tag access reg
	 * g3 = type
	 */
	sethi	%hi(sfmmu_tsb_miss), %g1
	or	%g1, %lo(sfmmu_tsb_miss), %g1
	ba,pt	%xcc, sys_trap
	  mov	-1, %g4	
	SET_SIZE(sfmmu_mmu_trap)

	ENTRY_NP(sfmmu_window_trap)
	/* user miss at tl>1. better be the window handler */
	rdpr	%tl, %g5
	sub	%g5, 1, %g3
	wrpr	%g3, %tl
	rdpr	%tt, %g2
	wrpr	%g5, %tl
	and	%g2, WTRAP_TTMASK, %g4
	cmp	%g4, WTRAP_TYPE	
	bne,a,pn %xcc, 1f
	 nop
	rdpr	%tpc, %g1
	andn	%g1, WTRAP_ALIGN, %g1	/* 128 byte aligned */
	add	%g1, WTRAP_FAULTOFF, %g1
	wrpr	%g0, %g1, %tnpc	
	/*
	 * some wbuf handlers will call systrap to resolve the fault
	 * we pass the trap type so they figure out the correct parameters.
	 * g5 = trap type, g6 = tag access reg
	 * only use g5, g6, g7 registers after we have switched to alternate
	 * globals.
	 */
	USE_ALTERNATE_GLOBALS
	mov	MMU_TAG_ACCESS, %g5
	ldxa	[%g5] ASI_DMMU, %g6
	rdpr	%tt, %g7
	cmp	%g7, FAST_IMMU_MISS_TT
	be,a,pn	%icc, ptl1_panic
	mov	PTL1_BAD_WTRAP, %g1
	cmp	%g7, FAST_DMMU_MISS_TT
	move	%icc, T_DATA_MMU_MISS, %g5
	movne	%icc, T_DATA_PROT, %g5
	done
1:
	ba,pt	%xcc, ptl1_panic
	mov	PTL1_BAD_WTRAP, %g1
	SET_SIZE(sfmmu_window_trap)
#endif /* lint */

#if defined (lint)
/*
 * sfmmu_tsb_miss handlers
 *
 * These routines are responsible for resolving tlb misses once they have also
 * missed in the TSB.  They traverse the hmeblk hash list.  In the
 * case of user address space, it will attempt to grab the hash mutex
 * and in the case it doesn't succeed it will go to tl = 0 to resolve the
 * miss.   In the case of a kernel tlb miss we grab no locks.  This
 * eliminates the problem of recursive deadlocks (taking a tlb miss while
 * holding a hash bucket lock and needing the same lock to resolve it - but
 * it forces us to use a capture cpus when deleting kernel hmeblks).
 * It order to eliminate the possibility of a tlb miss we will traverse
 * the list using physical addresses.  It executes at  TL > 0.
 * NOTE: the following routines currently do not support large page sizes.
 *
 * Parameters:
 *		%g2 = MMU_TARGET register
 *		%g3 = ctx number
 */
void
sfmmu_ktsb_miss()
{
}

void
sfmmu_utsb_miss()
{
}

void
sfmmu_kprot_trap()
{
}

void
sfmmu_uprot_trap()
{
}
#else /* lint */

#if (C_SIZE != (1 << CTX_SZ_SHIFT))
ERROR - size of context struct does not match with CTX_SZ_SHIFT
#endif

/*
 * Copies ism mapping for this ctx in param "ism" if this is a ISM 
 * dtlb miss and branches to label "ismhit". If this is not an ISM 
 * process or an ISM dtlb miss it falls thru.
 *
 * In the rare event this is a ISM process and a ISM dtlb miss has
 * not been detected in the first ism map block, it will branch
 * to "exitlabel".
 *
 * NOTE: We will never have any holes in our ISM maps. sfmmu_share/unshare
 *       will make sure of that. This means we can terminate our search on
 *       the first zero mapping we find.
 *
 * Parameters:
 * ctxptr  = 64 bit reg that points to current context structure (CLOBBERED)
 * vaddr   = 32 bit reg containing virtual address of tlb miss
 * tsbmiss = 32 bit address of tsb miss area
 * ism     = 64 bit reg where ism mapping will be stored: 
 *	 	 ism_sfmmu[63:32]
 *		 vbase    [31:16]
 *		 size     [15:0]
 * maptr   = 64 bit scratch reg
 * tmp1    = 64 bit scratch reg
 * tmp2    = 32 bit scratch reg
 * label:    temporary labels
 * ismhit:   label where to jump to if an ism dtlb miss
 * exitlabel:label where to jump if end of list is reached and there
 *	      is a next ismblk.
 */
#define ISM_CHECK(ctxptr, vaddr, tsbmiss, ism, maptr, tmp1, tmp2	\
	label, ismhit, exitlabel)					\
	ldx	[ctxptr + C_ISMBLKPA], tmp1	/* tmp1= phys &ismblk*/;\
	brlz,pt  tmp1, label/**/2		/* exit if -1 */	;\
	  add	tmp1, ISMBLK_MAPS, maptr	/* maptr = &ismblk.map[0] */;\
									;\
	st	ctxptr, [tsbmiss + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]	;\
	ldxa	[maptr] ASI_MEM, ism		/* ism = ismblk.map[0] */;\
	mov	tmp1, ctxptr			/* ctxptr = &ismblk */	;\
									;\
label/**/1:								;\
	brz,pt  ism, label/**/2			/* no mapping */	;\
	  srl	ism, ISM_VB_SHIFT, tmp2		/* tmp2 = vbase */	;\
	srl	vaddr, ISM_AL_SHIFT, tmp1 	/* tmp1 = 4MB va seg*/	;\
	sub	tmp1, tmp2, tmp2		/* tmp2 = va - vbase*/	;\
	sll	ism,  ISM_SZ_SHIFT, tmp1	/* tmp1 = size */	;\
	srl	tmp1, ISM_SZ_SHIFT, tmp1				;\
	cmp	tmp2, tmp1		 	/* check va <= offset*/	;\
	blu,pt	%icc, ismhit			/* ism hit */		;\
	  add	maptr, ISM_MAP_SZ, maptr 	/* maptr += sizeof map*/;\
									;\
	add	ctxptr, (ISMBLK_MAPS + ISM_MAP_SLOTS * ISM_MAP_SZ), tmp1;\
	cmp	maptr, tmp1						;\
	bl,pt	%icc, label/**/1		/* keep looking  */	;\
	  ldxa	[maptr] ASI_MEM, ism		/* ism = map[maptr] */	;\
									;\
	add	ctxptr, ISMBLK_NEXT, tmp1				;\
	lda	[tmp1] ASI_MEM, tmp2		/* check blk->next */	;\
	brnz,pt	tmp2, exitlabel			/* continue search */	;\
	  nop								;\
label/**/2:	

/*
 * Same as above except can be called from tl=0.
 * Also takes hatid as param intead of address of context struct.
 */
#define ISM_CHECK_TL0(hatid, vaddr, ism, maptr, tmp1, tmp2		\
	label, ismhit, exitlabel)					\
	ld	[hatid + SFMMU_ISMBLK], hatid /* hatid= &ismblk*/	;\
	brz,pt  hatid, label/**/2 	      /* exit if null */	;\
	  add	hatid, ISMBLK_MAPS, maptr /* maptr = &ismblk.map[0] */	;\
	ldx	[maptr], ism	   	  /* ism = ismblk.map[0]   */	;\
label/**/1:								;\
	brz,pt  ism, label/**/2			/* no mapping */	;\
	  srl	ism, ISM_VB_SHIFT, tmp2		/* tmp2 = vbase */	;\
	srl	vaddr, ISM_AL_SHIFT, tmp1 	/* tmp1 = 4MB va seg*/	;\
	sub	tmp1, tmp2, tmp2		/* tmp2 = va - vbase*/	;\
	sll	ism,  ISM_SZ_SHIFT, tmp1	 /* tmp1 = size */	;\
	srl	tmp1, ISM_SZ_SHIFT, tmp1				;\
	cmp	tmp2, tmp1		 	/* check va <= offset*/	;\
	blu,pt	%icc, ismhit			/* ism hit */		;\
	  add	maptr, ISM_MAP_SZ, maptr 	/* maptr += sizeof map*/;\
	add	hatid, (ISMBLK_MAPS + ISM_MAP_SLOTS * ISM_MAP_SZ), tmp1;\
	cmp	maptr, tmp1						;\
	bl,pt	%icc, label/**/1		/* keep looking  */	;\
	  ldx	[maptr], ism			/* ism = map[maptr] */	;\
	add	hatid, ISMBLK_NEXT, tmp1				;\
	ld	[tmp1], tmp2			/* check blk->next */	;\
	brnz,pt	tmp2, exitlabel			/* continue search */	;\
	  nop								;\
label/**/2:	


/*
 * returns the hme hash bucket (hmebp) given the vaddr, and the hatid
 * It also returns the virtual pg for vaddr (ie. vaddr << hmeshift)
 * Parameters:
 * vaddr = reg containing virtual address
 * hatid = reg containing sfmmu pointer
 * hashsz = global variable containing number of buckets in hash
 * hashstart = global variable containing start of hash
 * hmeshift = constant/register to shift vaddr to obtain vapg
 * hmebp = register where bucket pointer will be stored
 * vapg = register where virtual page will be stored
 * tmp1, tmp2 = tmp registers
 */
#define	HMEHASH_FUNC_ASM(vaddr, hatid, hashsz, hashstart, hmeshift,	\
	hmebp, vapg, tmp1, tmp2)					\
	sethi	%hi(hashsz), hmebp					;\
	sethi	%hi(hashstart), tmp1					;\
	ld	[hmebp + %lo(hashsz)], hmebp				;\
	ld	[tmp1 + %lo(hashstart)], tmp1				;\
	srl	vaddr, hmeshift, vapg					;\
	xor	vapg, hatid, tmp2	/* hatid ^ (vaddr >> shift) */	;\
	and	tmp2, hmebp, hmebp	/* index into khme_hash */	;\
	mulx	hmebp, HMEBUCK_SIZE, hmebp				;\
	add	hmebp, tmp1, hmebp

#define	HMEHASH_FUNC_ASM2(vaddr, hatid, tsbarea, hashsz, hashstart,	\
	hmeshift, hmebp, vapg, tmp1, tmp2)				\
	ld	[tsbarea + hashsz], hmebp				;\
	ld	[tsbarea + hashstart], tmp1				;\
	srl	vaddr, hmeshift, vapg					;\
	xor	vapg, hatid, tmp2	/* hatid ^ (vaddr >> shift) */	;\
	and	tmp2, hmebp, hmebp	/* index into khme_hash */	;\
	mulx	hmebp, HMEBUCK_SIZE, hmebp				;\
	add	hmebp, tmp1, hmebp

#define	MAKE_HASHTAG(vapg, hatid, hmeshift, hashno, hblktag)		\
	sll	vapg, hmeshift, vapg					;\
	or	vapg, hashno, vapg					;\
	sllx	vapg, HTAG_SFMMUPSZ, hblktag				;\
	or	hatid, hblktag, hblktag

/*
 * Function to traverse hmeblk hash link list and find corresponding match
 * The search is done using physical pointers. It returns the physical address
 * and virtual address pointers to the hmeblk that matches with the tag
 * provided.
 * Parameters:
 * hmebp = register that pointes to hme hash bucket, also used as tmp reg
 * hmeblktag = register with hmeblk tag match
 * hmeblkpa = register where physical ptr will be stored
 * hmeblkva = register where virtual ptr will be stored
 * tmp1 = 32bit tmp reg
 * tmp2 = 64bit tmp reg
 * label: temporary label
 * exitlabel: label where to jump if end of list is reached and no match found
 */
#define	HMEHASH_SEARCH(hmebp, hmeblktag, hmeblkpa, hmeblkva, tmp1, tmp2, \
	label, searchstat, linkstat)				 	 \
	ldx	[hmebp + HMEBUCK_NEXTPA], hmeblkpa			;\
	ld	[hmebp + HMEBUCK_HBLK], hmeblkva			;\
	HAT_GLOBAL_DBSTAT(searchstat, tmp2, tmp1)			;\
label/**/1:								;\
	brz,pn	hmeblkva, label/**/2					;\
	HAT_GLOBAL_DBSTAT(linkstat, tmp2, tmp1)				;\
	add	hmeblkpa, HMEBLK_TAG, tmp1				;\
	ldxa	[tmp1] ASI_MEM, tmp2		/* read hblk_tag */	;\
	cmp	hmeblktag, tmp2		/* compare tags */		;\
	be,a,pn	%xcc, label/**/2					;\
	  nop								;\
	add	hmeblkpa, HMEBLK_NEXT, tmp1				;\
	lda	[tmp1] ASI_MEM, hmeblkva	/* hmeblk ptr va */	;\
	add	hmeblkpa, HMEBLK_NEXTPA, tmp1				;\
	ba,pt	%xcc, label/**/1					;\
	  ldxa	[tmp1] ASI_MEM, hmeblkpa	/* hmeblk ptr pa */	;\
label/**/2:	


/*
 * HMEBLK_TO_HMENT is a macro that given an hmeblk and a vaddr returns
 * he offset for the corresponding hment.
 * Parameters:
 * vaddr = register with virtual address
 * hmeblkpa = physical pointer to hme_blk
 * hment = register where address of hment will be stored
 * hmentoff = register where hment offset will be stored
 * label1 = temporary label
 */
#define	HMEBLK_TO_HMENT(vaddr, hmeblkpa, hmentoff, tmp1, label1)	\
	add	hmeblkpa, HMEBLK_MISC, hmentoff				;\
	lda	[hmentoff] ASI_MEM, tmp1 				;\
	andcc	tmp1, HBLK_SZMASK, %g0	 /* tmp1 = get_hblk_sz(%g5) */	;\
	bnz,a,pn  %icc, label1		/* if sz != TTE8K branch */	;\
	  or	%g0, HMEBLK_HME1, hmentoff				;\
	srl	vaddr, MMU_PAGESHIFT, tmp1				;\
	and	tmp1, NHMENTS - 1, tmp1		/* tmp1 = index */	;\
	/* XXX use shift when SFHME_SIZE becomes power of 2 */		;\
	mulx	tmp1, SFHME_SIZE, tmp1 					;\
	add	tmp1, HMEBLK_HME1, hmentoff				;\
label1:									;\

/*
 * GET_TTE is a macro that returns a TTE given a tag and hatid.
 *
 * Parameters:
 * tag       = 32 bit reg containing tag access eg (vaddr + ctx)
 * hatid     = 64 bit reg containing sfmmu pointer (CLOBBERED)
 * tte       = 64 bit reg where tte will be stored.
 * hmeblkpa  = 64 bit reg where physical pointer to hme_blk will be stored)
 * hmeblkva  = 32 bit reg where virtual pointer to hme_blk will be stored)
 * hmentoff  = 64 bit reg where hment offset will be stored)
 * hashsz    = global variable containing number of buckets in hash
 * hashstart = global variable containing start of hash
 * hmeshift  = constant/register to shift vaddr to obtain vapg
 * hashno    = constant/register hash number
 * label     = temporary label
 * exitlabel = label where to jump to when tte is found. The hmebp lock
 *	 is still held at this time.
 * RFE: It might be worth making user programs update the cpuset field
 * in the hblk as well as the kernel.  This would allow us to go back to
 * one GET_TTE function for both kernel and user.
 */                                                             
#define GET_TTE(tag, hatid, tte, hmeblkpa, hmeblkva, tsbarea, hmentoff, \
		hashsz, hashstart, hmeshift, hashno, label, exitlabel,	\
		searchstat, linkstat)					\
									;\
	st	tag, [tsbarea + (TSBMISS_SCRATCH + TSB_TAGACC)]		;\
	HMEHASH_FUNC_ASM2(tag, hatid, tsbarea, hashsz, hashstart,	\
		hmeshift, tte, hmeblkpa, hmentoff, hmeblkva)		;\
									;\
	/*								;\
	 * tag   = vaddr						;\
	 * hatid = hatid						;\
	 * tsbarea = tsbarea						;\
	 * tte   = hmebp (hme bucket pointer)				;\
	 * hmeblkpa  = vapg  (virtual page)				;\
	 * hmentoff, hmeblkva = scratch					;\
	 */								;\
	MAKE_HASHTAG(hmeblkpa, hatid, hmeshift, hashno, hmentoff)	;\
									;\
	/*								;\
	 * tag   = vaddr						;\
	 * hatid = hatid						;\
	 * tte   = hmebp						;\
	 * hmeblkpa  = clobbered					;\
	 * hmentoff  = hblktag						;\
	 * hmeblkva  = scratch						;\
	 */								;\
	st	tte, [tsbarea + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]	;\
	HMELOCK_ENTER(tte, hmeblkpa, hmeblkva, label/**/3)		;\
	HMEHASH_SEARCH(tte, hmentoff, hmeblkpa, hmeblkva, hatid, tag, 	\
		label/**/1, searchstat, linkstat)			;\
	/*								;\
	 * tte = hmebp							;\
	 * hmeblkpa = hmeblkpa						;\
	 * hmeblkva = hmeblkva						;\
	 */								;\
	brz,pt	hmeblkva, exitlabel	/* exit if hblk not found */	;\
	  nop								;\
	ld	[tsbarea + (TSBMISS_SCRATCH + TSB_TAGACC)], tag		;\
	/*								;\
	 * We have found the hmeblk containing the hment.		;\
	 * Now we calculate the corresponding tte.			;\
	 *								;\
	 * tag   = vaddr						;\
	 * hatid = clobbered						;\
	 * tte   = hmebp						;\
	 * hmeblkpa  = hmeblkpa						;\
	 * hmentoff  = hblktag						;\
	 * hmeblkva  = hmeblkva 					;\
	 */								;\
	HMEBLK_TO_HMENT(tag, hmeblkpa, hmentoff, hatid, label/**/2)	;\
									;\
	add	hmentoff, SFHME_TTE, hmentoff				;\
	mov	tte, hatid						;\
	add     hmeblkpa, hmentoff, hmeblkpa				;\
	ldxa	[hmeblkpa] ASI_MEM, tte	/* MMU_READTTE through pa */	;\
	add     hmeblkva, hmentoff, hmeblkva				;\
	HMELOCK_EXIT(hatid)		/* drop lock */			;\
	/*								;\
	 * tag   = vaddr						;\
	 * hatid = scratch						;\
	 * tte   = tte							;\
	 * hmeblkpa  = tte pa						;\
	 * hmentoff  = scratch						;\
	 * hmeblkva  = tte va						;\
	 */


/*
 * TTE_MOD_REF is a macro that updates the reference bit if it is
 * not already set.
 *
 * Parameters:
 * tte      = reg containing tte
 * ttepa    = physical pointer to tte
 * tteva    = virtual ptr to tte
 * tmp1     = tmp reg
 * label    = temporary label
 */
#ifdef SF_ERRATA_12 /* atomics cause hang */
#define	TTE_MOD_REF(tte, hmeblkpa, hmeblkva, tmp1, label)		\
	/* check reference bit */					;\
	andcc	tte, TTE_REF_INT, %g0					;\
	bnz,a,pt %xcc, label/**/2	/* if ref bit set-skip ahead */	;\
	  nop								;\
	/* update reference bit */					;\
	sethi	%hi(dcache_line_mask), tmp1				;\
	ld	[tmp1 + %lo(dcache_line_mask)], tmp1			;\
	and	hmeblkva, tmp1, tmp1					;\
	stxa	%g0, [tmp1] ASI_DC_TAG /* flush line from dcache */	;\
	membar	#Sync							;\
label/**/1:								;\
	or	tte, TTE_REF_INT, tmp1					;\
	membar #Sync							;\
	casxa	[hmeblkpa] ASI_MEM, tte, tmp1 	/* update ref bit */	;\
	cmp	tte, tmp1						;\
	bne,a,pn %xcc, label/**/1					;\
	  ldxa	[hmeblkpa] ASI_MEM, tte	/* MMU_READTTE through pa */	;\
	or	tte, TTE_REF_INT, tte					;\
label/**/2:

#else /* SF_ERRATA_12 - atomics cause hang */

#define	TTE_MOD_REF(tte, hmeblkpa, hmeblkva, tmp1, label)		\
	/* check reference bit */					;\
	andcc	tte, TTE_REF_INT, %g0					;\
	bnz,a,pt %xcc, label/**/2	/* if ref bit set-skip ahead */	;\
	  nop								;\
	/* update reference bit */					;\
	sethi	%hi(dcache_line_mask), tmp1				;\
	ld	[tmp1 + %lo(dcache_line_mask)], tmp1			;\
	and	hmeblkva, tmp1, tmp1					;\
	stxa	%g0, [tmp1] ASI_DC_TAG /* flush line from dcache */	;\
	membar	#Sync							;\
label/**/1:								;\
	or	tte, TTE_REF_INT, tmp1					;\
	/* membar #Sync */						;\
	casxa	[hmeblkpa] ASI_MEM, tte, tmp1 	/* update ref bit */	;\
	cmp	tte, tmp1						;\
	bne,a,pn %xcc, label/**/1					;\
	  ldxa	[hmeblkpa] ASI_MEM, tte	/* MMU_READTTE through pa */	;\
	or	tte, TTE_REF_INT, tte					;\
label/**/2:
#endif /* SF_ERRATA_12 - atomics cause hang */

#define	TTE_SET_REF_ML(tte, ttepa, tteva, tsbarea, tmp1, label)		\
	/* check reference bit */					;\
	andcc	tte, TTE_REF_INT, %g0					;\
	bnz,a,pt %xcc, label/**/2	/* if ref bit set-skip ahead */	;\
	  nop								;\
	/* update reference bit */					;\
	ld	[tsbarea + TSBMISS_DMASK], tmp1				;\
	and	tteva, tmp1, tmp1					;\
	stxa	%g0, [tmp1] ASI_DC_TAG /* flush line from dcache */	;\
	membar	#Sync							;\
label/**/1:								;\
	or	tte, TTE_REF_INT, tmp1					;\
	casxa	[ttepa] ASI_MEM, tte, tmp1 	/* update ref bit */	;\
	cmp	tte, tmp1						;\
	bne,a,pn %xcc, label/**/1					;\
	  ldxa	[ttepa] ASI_MEM, tte	/* MMU_READTTE through pa */	;\
	or	tte, TTE_REF_INT, tte					;\
label/**/2:

/*
 * This function executes at both tl=0 and tl>0.
 * It executes using the mmu alternate globals.
 */
	ENTRY_NP(sfmmu_utsb_miss)
	/*
	 * USER TSB MISS
	 */
	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g5
	ldxa	[%g4] ASI_DMMU, %g3
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g5, FAST_IMMU_MISS_TT
	movne	%xcc, %g3, %g2			/* g2 = vaddr + ctx */
	CPU_INDEX(%g7)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or	%g6, %lo(tsbmiss_area), %g6
	add	%g6, %g7, %g6			/* g6 = tsbmiss area */
	HAT_PERCPU_STAT(%g6, TSBMISS_UTSBMISS, %g7)
	ld	[%g6 + TSBMISS_CTXS], %g1	/* g1 = ctxs */
	sll	%g2, TAGACC_CTX_SHIFT, %g3
	srl	%g3, TAGACC_CTX_SHIFT, %g3	/* g3 = ctx */
	/* calculate hatid given ctxnum */
	sll	%g3, CTX_SZ_SHIFT, %g3
	add	%g3, %g1, %g1			/* g1 = ctx ptr */
        ld      [%g1 + C_SFMMU], %g7            /* g7 = hatid */

	brz,pn	%g7, utsbmiss_tl0		/* if zero jmp ahead */
	  nop

	be,pt	%icc, 1f			/* not ism if itlb miss */
	  nop

	st	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)]

	ISM_CHECK(%g1, %g2, %g6, %g3, %g4, %g5, %g7, utsb_l1,
		  utsb_ism, utsbmiss_tl0)

	ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)], %g7

1:
	GET_TTE(%g2, %g7, %g3, %g4, %g5, %g6, %g1,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		HBLK_RANGE_SHIFT, 1, utsb_l2, utsb_fault_lock,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)

	/*
	 * g1 = scratch
	 * g2 = tagacc and in TSB_TAGACC
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsbmiss area
	 * g7 = scratch
	 * hmebp in TSBMISS_HMEBP
	 */
	brgez,a,pn %g3, utsb_pagefault	/* if tte invalid branch */
	  nop

        /* 
	 * If itlb miss check nfo bit.
	 * if set treat as invalid.
	 */
        rdpr    %tt, %g7
        cmp     %g7, FAST_IMMU_MISS_TT
        bne,a,pt %icc, 3f
         andcc  %g3, TTE_REF_INT, %g0
        sllx    %g3, TTE_NFO_SHIFT, %g7		/* if nfo bit is set treat */
        brlz,a,pn %g7, utsbmiss_tl0		/* it as invalid */
          nop
3:

	/*
	 * Set reference bit if not already set
	 */
	TTE_MOD_REF(%g3, %g4, %g5, %g7, utsb_l3) 

	/*
	 * Now, load into TSB/TLB
	 * g2 = tagacc
	 * g3 = tte
	 * g4 will equal tag target
	 */
	rdpr	%tt, %g5
	cmp	%g5, FAST_IMMU_MISS_TT
	be,a,pn	%icc, 8f
	  nop
	/* dmmu miss */
	srlx	%g3, TTE_SZ_SHFT, %g5
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,a,pn %icc, 4f
	  nop
	ldxa	[%g0]ASI_DMMU, %g4		/* tag target */
	GET_TSB_POINTER(%g2, %g1, %g5, %g6, %g7)
	TSB_UPDATE(%g1, %g3, %g4)
4:
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync
8:
	/* immu miss */
	srlx	%g3, TTE_SZ_SHFT, %g5
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,a,pn %icc, 4f
	  nop
	ldxa	[%g0]ASI_IMMU, %g4		/* tag target */
	GET_TSB_POINTER(%g2, %g1, %g5, %g6, %g7)
	TSB_UPDATE(%g1, %g3, %g4)
4:
	stxa	%g3, [%g0] ASI_ITLB_IN
	retry
	membar	#Sync
utsb_fault_lock:
	ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g5
	HMELOCK_EXIT(%g5)
	/* fall through */
utsb_pagefault:
	/*
	 * we get here if we couldn't find a valid tte in the hash.
	 * if we are at tl>0 we go to window handling code, otherwise
	 * we call pagefault.
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	bg,pn	%icc, sfmmu_window_trap
	  nop
	ba,pt	%icc, sfmmu_pagefault
	  nop

utsb_ism:
	/*
	 * This is an ISM dtlb miss. 
	 *
	 * g2 = vaddr + ctx
	 * g3 = ism mapping.
	 * g6 = tsbmiss area
	 */

	srlx	%g3, ISM_HAT_SHIFT, %g1		/* g1 = ism hatid */
	brz,a,pn %g1, ptl1_panic		/* if zero jmp ahead */
	mov	PTL1_BAD_ISM, %g1

	srl	%g3, ISM_VB_SHIFT, %g4		/* clr size field */
	sll	%g4, ISM_AL_SHIFT, %g4		/* g4 = ism vbase */
	set	TAGACC_CTX_MASK, %g7		/* mask off ctx number */
	andn	%g2, %g7, %g5			/* g6 = tlb miss vaddr */
	sub	%g5, %g4, %g4			/* g4 = offset in ISM seg */	

	/*
	 * ISM pages are always locked down.
	 * If we can't find the tte then pagefault
	 * and let the spt segment driver resovle it
	 *
	 * We first check if this ctx has large pages.
	 * If so hash for 4mb first. If that fails,
	 * hopefully rare, then rehash for 8k. We
	 * don't support 64k and 512k pages for ISM
	 * so no need to hash for them.
	 *
	 * g1 = ISM hatid
	 * g2 = orig tag (vaddr + ctx)
	 * g3 = ism mapping
	 * g4 = ISM vaddr (offset in ISM seg + ctx)
	 * g6 = tsb miss area
	 */

	ld	[%g6 + TSBMISS_SCRATCH + TSBMISS_HMEBP], %g7 /* ctx ptr */
	lduh	[%g7 + C_FLAGS], %g7		/* g7 = ctx->c_flags	*/
	and	%g7, LTTES_FLAG, %g7
	brz,pn	%g7, 1f				/* branch if not lpages */
	  nop

	/*
	 * 4mb hash.
	 */
	GET_TTE(%g4, %g1, %g3, %g5, %g7, %g6, %g2,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		MMU_PAGESHIFT4M, 3, utsb_l4, utsb_fault_lock,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)

	/*
	 * If tte is valid then skip ahead.
	 *
	 * g3 = tte
	 */
	brlz,pt %g3, 2f		/* if valid tte branch */
	  nop
	
1:
	/*
	 * 8k hash.
	 */
	GET_TTE(%g4, %g1, %g3, %g5, %g7, %g6, %g2,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		HBLK_RANGE_SHIFT, 1, utsb_l5, utsb_fault_lock,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)
2:

	/*
	 * If tte is invalid then pagefault and let the 
	 * spt segment driver resolve it.
	 *
	 * g3 = tte
	 * g5 = tte pa
	 * g7 = tte va
	 * g6 = tsbmiss area
	 * g2 = clobbered
	 * g4 = clobbered
	 */
	brgez,pn %g3, utsb_pagefault	/* if tte invalid branch */
	  nop

	/*
	 * Set reference bit if not already set
	 */
	TTE_MOD_REF(%g3, %g5, %g7, %g4, utsb_l6)

	/*
	 * load into the TSB
	 * g2 = vaddr
	 * g3 = tte
	 */
	srlx	%g3, TTE_SZ_SHFT, %g5
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,pn %icc, 4f
	  mov	MMU_TAG_ACCESS, %g4
	ldxa	[%g4] ASI_DMMU, %g2
	ldxa	[%g0]ASI_DMMU, %g4
	GET_TSB_POINTER(%g2, %g1, %g5, %g6, %g7)
	TSB_UPDATE(%g1, %g3, %g4)
4:

	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync

utsbmiss_tl0:
	/*
	 * We get here when we need to service this tsb miss at tl=0.
	 * Causes: ctx was stolen, more than ISM_MAP_SLOTS ism segments, 
	 *         possible large page, itlb miss on nfo page.
	 *
	 * g2 = tag access
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_mmu_trap
	  nop
	ba,pt	%icc, sfmmu_window_trap
	  nop
	SET_SIZE(sfmmu_utsb_miss)

#if (1<< TSBMISS_SHIFT) != TSBMISS_SIZE
ERROR - TSBMISS_SHIFT does not correspond to size of tsbmiss struct
#endif

/*
 * This routine can execute for both tl=0 and tl>0 traps.
 * When running for tl=0 traps it runs on the alternate globals,
 * otherwise it runs on the mmu globals.
 */

	ENTRY_NP(sfmmu_ktsb_miss)
	/*
	 * KERNEL TSB MISS
	 */
	mov	MMU_TAG_ACCESS, %g4
	rdpr	%tt, %g5
	ldxa	[%g4] ASI_DMMU, %g3
	ldxa	[%g4] ASI_IMMU, %g2
	cmp	%g5, FAST_IMMU_MISS_TT
	movne	%xcc, %g3, %g2			/* g2 = vaddr + ctx */
	CPU_INDEX(%g7)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or	%g6, %lo(tsbmiss_area), %g6
	add	%g6, %g7, %g6			/* g6 = tsbmiss area */
	HAT_PERCPU_STAT(%g6, TSBMISS_KTSBMISS, %g7)
	ld	[%g6 + TSBMISS_KHATID], %g1	/* g1 = ksfmmup */
	st	%g2, [%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)]
	/*
	 * I can't use the GET_TTE macro because I want to update the
	 * kcpuset field.
	 * RFE: It might be worth making user processess use the kcpuset field
	 * as well.  In that case we will return to one GET_TTE function.
	 * g1 = hatid
	 * g2 = tagaccess
	 */
	HMEHASH_FUNC_ASM2(%g2, %g1, %g6, TSBMISS_KHASHSZ, TSBMISS_KHASHSTART,
		HBLK_RANGE_SHIFT, %g7, %g4, %g5, %g3)
	/*
	 * g7 = hme bucket
	 * g4 = virtual page of addr
	 */
	st	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]
	MAKE_HASHTAG(%g4, %g1, HBLK_RANGE_SHIFT, 1, %g5)
	/*
	 * g5 = hblktag
	 */
	HMELOCK_ENTER(%g7, %g1, %g3, ktsb_l1)
	HMEHASH_SEARCH(%g7, %g5, %g4, %g2, %g1, %g3, ktsb_l2,
		HATSTAT_KHASH_SEARCH, HATSTAT_KHASH_LINKS)
	/*
	 * g7 = hmebp
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	brz,pn	%g2, ktsb_done
	  nop
	/*
	 * update hblk_cpuset
	 */
	CPU_INDEX(%g7)
	add	%g4, HMEBLK_CPUSET, %g3		/* g3 = cpuset ptr */
	CPU_INDEXTOSET(%g3, %g7, %g1)
	lda	[%g3] ASI_MEM, %g1
	mov	1, %g5
	sll	%g5, %g7, %g5
	andcc	%g1, %g5, %g0
	bnz,pt	%icc, 2f			/* cpuset set so skip */
	 nop
3:
	or	%g1, %g5, %g7
	casa	[%g3] ASI_MEM, %g1, %g7
	cmp	%g1, %g7
	bne,a,pn %icc, 3b
	 lda	[%g3] ASI_MEM, %g1
	membar	#StoreLoad

	/*
	 * we need to flush the line from the dcache so
	 * subsequent virtual reads will get the proper value. 
	 */
	ld	[%g6 + TSBMISS_DMASK], %g7
	add	%g2, HMEBLK_CPUSET, %g1
	and	%g7, %g1, %g7
	stxa	%g0, [%g7] ASI_DC_TAG	/* flush line from dcache */
	membar	#Sync

2:
	ld	[%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)], %g7
	/*
	 * g7 = tag access
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	HMEBLK_TO_HMENT(%g7, %g4, %g5, %g3, ktsb_l3)
	/* g5 = hmentoff */
	add	%g5, SFHME_TTE, %g5
	add	%g4, %g5, %g4
	add	%g2, %g5, %g5
	ldxa	[%g4] ASI_MEM, %g3
	/*
	 * g7 = vaddr + ctx
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsbmiss area
	 * g1 = clobbered
	 * g2 = clobbered
	 */
	brgez,a,pn %g3, ktsb_done	/* if tte invalid branch */
	  ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g7

#ifdef	DEBUG
	sllx	%g3, TTE_PA_LSHIFT, %g1
	srlx	%g1, 30 + TTE_PA_LSHIFT, %g1	/* if not memory continue */
	brnz,pn	%g1, 2f
	  nop
	andcc	%g3, TTE_CP_INT, %g0		/* if memory - check it is */
	bz,a,pn	%icc, ptl1_panic		/* ecache cacheable */
	mov	PTL1_BAD_TTE_PA, %g1
2:
#endif	/* DEBUG */

	/* 
	 * If an itlb miss check nfo bit.
	 * If set, pagefault. XXX
	 */ 
	rdpr    %tt, %g1
	cmp     %g1, FAST_IMMU_MISS_TT
	bne,pt %icc, 4f
	  sllx    %g3, TTE_NFO_SHIFT, %g1	/* if nfo bit is set treat */
	brlz,a,pn %g1, ktsb_done		/* it is invalid */
	  ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g7
4:
	ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g1
	HMELOCK_EXIT(%g1)
	/*
	 * Set reference bit if not already set
	 */
	TTE_SET_REF_ML(%g3, %g4, %g5, %g6, %g1, ktsb_l4)

#ifdef	DEBUG
	ldxa	[%g4] ASI_MEM, %g5		/* MMU_READTTE through pa */
	sllx	%g5, TTE_PA_LSHIFT, %g6
	srlx	%g6, 30 + TTE_PA_LSHIFT, %g6	/* if not memory continue */
	brnz,pn	%g6, 6f
	  nop
	andcc	%g5, TTE_CP_INT, %g0		/* if memory - check it is */
	bz,a,pn	%icc, ptl1_panic		/* ecache cacheable */
	mov	PTL1_BAD_TTE_PA, %g1
6:
#endif	/* DEBUG */
	/*
	 * Now, load into TSB/TLB
	 * g7 = tagacc
	 * g3 = tte
	 * g4 will equal tag target
	 */
	rdpr	%tt, %g6
	cmp	%g6, FAST_IMMU_MISS_TT
	be,pn	%icc, 8f
	  srlx	%g3, TTE_SZ_SHFT, %g6
	/* dmmu miss */
	cmp	%g6, TTESZ_VALID | TTE8K
	bne,a,pn %icc, 5f
	  nop
	ldxa	[%g0]ASI_DMMU, %g4
	GET_TSB_POINTER(%g7, %g1, %g5, %g6, %g2)
	TSB_UPDATE(%g1, %g3, %g4)
5:
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync
8:
	/* immu miss */
	cmp	%g6, TTESZ_VALID | TTE8K
	bne,a,pn %icc, 4f
	  nop
	ldxa	[%g0]ASI_IMMU, %g4
	GET_TSB_POINTER(%g7, %g1, %g5, %g6, %g2)
	TSB_UPDATE(%g1, %g3, %g4)
4:
	stxa	%g3, [%g0] ASI_ITLB_IN
	retry
	membar	#Sync

ktsb_done:
	HMELOCK_EXIT(%g7)	/* drop hashlock */
	/*
	 * we get here if we couldn't find valid hment in hash
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call pagefault
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%xcc, sfmmu_mmu_trap
	  nop
	ba,pt	%xcc, ptl1_panic
	mov	PTL1_BAD_KMISS, %g1
	SET_SIZE(sfmmu_ktsb_miss)


	ENTRY_NP(sfmmu_kprot_trap)
	/*
	 * KERNEL Write Protect Traps
	 *
	 * %g2 = MMU_TAG_ACCESS
	 */
	CPU_INDEX(%g7)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or	%g6, %lo(tsbmiss_area), %g6
	add	%g6, %g7, %g6			/* g6 = tsbmiss area */
	HAT_PERCPU_STAT(%g6, TSBMISS_KPROTS, %g7)
	ld	[%g6 + TSBMISS_KHATID], %g1	/* g1 = ksfmmup */
	st	%g2, [%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)] /* tagaccess */
	/*
	 * I can't use the GET_TTE macro because I want to update the
	 * kcpuset field.
	 * g1 = hatid
	 * g2 = tagaccess
	 * g6 = tsbmiss area
	 */
	HMEHASH_FUNC_ASM2(%g2, %g1, %g6, TSBMISS_KHASHSZ, TSBMISS_KHASHSTART,
		HBLK_RANGE_SHIFT, %g7, %g4, %g5, %g3)

	/*
	 * g7 = hme bucket
	 * g4 = virtual page of addr
	 */
	st	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)]
	MAKE_HASHTAG(%g4, %g1, HBLK_RANGE_SHIFT, 1, %g5)
	/*
	 * g5 = hblktag
	 */
	HMELOCK_ENTER(%g7, %g1, %g3, kprot_l1)
	HMEHASH_SEARCH(%g7, %g5, %g4, %g2, %g1, %g3, kprot_l2,
		HATSTAT_KHASH_SEARCH, HATSTAT_KHASH_LINKS)
	/*
	 * g7 = hmebp
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	brz,pn	%g2, kprot_tl0
	 nop

	/*
	 * update hblk_cpuset
	 */
	CPU_INDEX(%g7)
	add	%g4, HMEBLK_CPUSET, %g3		/* g3 = cpuset ptr */
	CPU_INDEXTOSET(%g3, %g7, %g1)
	lda	[%g3] ASI_MEM, %g1
	mov	1, %g5
	sll	%g5, %g7, %g5
	andcc	%g1, %g5, %g0
	bnz,pt	%icc, 2f			/* cpuset set so skip */
	 nop
3:
	or	%g1, %g5, %g7
	casa	[%g3] ASI_MEM, %g1, %g7
	cmp	%g1, %g7
	bne,a,pn %icc, 3b
	 lda	[%g3] ASI_MEM, %g1
	membar	#StoreLoad

	/*
	 * we need to flush the line from the dcache so
	 * subsequent virtual reads will get correct data.
	 */
	ld	[%g6 + TSBMISS_DMASK], %g7
	add	%g2, HMEBLK_CPUSET, %g1
	and	%g7, %g1, %g7
	stxa	%g0, [%g7] ASI_DC_TAG		/* flush line from dcache */
	membar	#Sync
2:
	ld	[%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)], %g7 /* tag access */
	/*
	 * g7 = tag access
	 * g4 = hmeblkpa
	 * g2 = hmeblkva
	 */
	HMEBLK_TO_HMENT(%g7, %g4, %g5, %g3, kprot_l3)
	/* g5 = hmentoff */
	add	%g5, SFHME_TTE, %g5
	add	%g4, %g5, %g4
	ldxa	[%g4] ASI_MEM, %g3		/* read tte */
	add	%g2, %g5, %g5
	/*
	 * g7 = vaddr + ctx
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsbmiss area
	 * g1 = clobbered
	 * g2 = clobbered
	 */
	ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g1
	HMELOCK_EXIT(%g1)
	brgez,pn %g3, kprot_inval	/* if tte invalid branch */
	 andcc	%g3, TTE_WRPRM_INT, %g0		/* check write permissions */
	bz,pn	%xcc, kprot_protfault		/* no, jump ahead */
	 andcc	%g3, TTE_HWWR_INT, %g0		/* check if modbit is set */
	bnz,pn	%xcc, 5f			/* yes, go load tte into tsb */
	 sethi	%hi(dcache_line_mask), %g7
	/* update mod bit  */
	ld	[%g7 + %lo(dcache_line_mask)], %g7
	and	%g7, %g5, %g5
	stxa	%g0, [%g5]ASI_DC_TAG		/* flush line from dcache */
	membar	#Sync
4:
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g1
	casxa	[%g4] ASI_MEM, %g3, %g1		/* update ref/mod bit */
	cmp	%g3, %g1
	bne,a,pn %xcc, 4b
	  ldxa	[%g4] ASI_MEM, %g3		/* MMU_READTTE through pa */
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g3
5:
	/*
	 * load into the TSB
	 * g2 = vaddr
	 * g3 = tte
	 */
	srlx	%g3, TTE_SZ_SHFT, %g5
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,pn	%icc, 6f
	  nop
	ld	[%g6 + (TSBMISS_SCRATCH + TSB_TAGACC)], %g2 /* tag access */
	GET_TSB_POINTER(%g2, %g1, %g4, %g5, %g7)
	ldxa	[%g0]ASI_DMMU, %g4
	TSB_UPDATE(%g1, %g3, %g4)
	/*
	 * Now, load into TLB
	 */
6:
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync

kprot_tl0:
	/*
	 * we get here if we couldn't find hmeblk in hash
	 * since we were able to find the tte in the tlb, the trap
	 * most likely ocurred on large page tte.
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call sfmmu_tsb_miss
	 * g7 = hme bucket
	 */
	HMELOCK_EXIT(%g7)
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_mmu_trap
	  nop
	ba,pt	%icc, ptl1_panic
	mov	PTL1_BAD_KPROT_TL0, %g1

kprot_inval:
	/*
	 * We get here if tte was invalid. XXX  We fake a data mmu miss.
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_pagefault
	  nop
	ba,pt	%icc, ptl1_panic
	mov	PTL1_BAD_KPROT_INVAL, %g1

kprot_protfault:
	/*
	 * We get here if we didn't have write permission on the tte.
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call pagefault
	 */
	rdpr	%tl, %g5
	cmp	%g5, 1
	ble,pt	%icc, sfmmu_pagefault
	  nop
	ba,pt %xcc, ptl1_panic
	mov	PTL1_BAD_KPROT_FAULT, %g1
	SET_SIZE(sfmmu_kprot_trap)


	ENTRY_NP(sfmmu_uprot_trap)
	/*
	 * USER Write Protect Trap
	 */
	mov	MMU_TAG_ACCESS, %g1
	ldxa	[%g1] ASI_DMMU, %g2		/* g2 = vaddr + ctx */
	sll	%g2, TAGACC_CTX_SHIFT, %g3
	srl	%g3, TAGACC_CTX_SHIFT, %g3	/* g3 = ctx */
	CPU_INDEX(%g7)
	sethi	%hi(tsbmiss_area), %g6
	sllx	%g7, TSBMISS_SHIFT, %g7
	or      %g6, %lo(tsbmiss_area), %g6
	add     %g6, %g7, %g6			/* g6 = tsbmiss area */
	HAT_PERCPU_STAT(%g6, TSBMISS_UPROTS, %g7)
	/* calculate hatid given ctxnum */
	ld	[%g6 + TSBMISS_CTXS], %g1	/* g1 = ctxs */
	sll	%g3, CTX_SZ_SHIFT, %g3
	add	%g3, %g1, %g1				/* g1 = ctx ptr */
	ld      [%g1 + C_SFMMU], %g7                   /* g7 = hatid */
	brz,pn	%g7, uprot_tl0			/* if zero jmp ahead */
	  nop

	/*
	 * If ism goto sfmmu_tsb_miss
	 */
	st	%g7, [%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)]
	ISM_CHECK(%g1, %g2, %g6, %g3, %g4, %g5, %g7, uprot_l1, 
		  uprot_tl0, uprot_tl0)
	ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HATID)], %g7

	GET_TTE(%g2, %g7, %g3, %g4, %g5, %g6, %g1,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		HBLK_RANGE_SHIFT, 1, uprot_l2, uprot_fault_lock,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)
	/*
	 * g3 = tte
	 * g4 = tte pa
	 * g5 = tte va
	 * g6 = tsb miss area
	 * hmebp in TSBMISS_HMEBP
	 */
	brgez,a,pn %g3, uprot_fault		/* if tte invalid goto tl0 */
	  nop

	andcc	%g3, TTE_WRPRM_INT, %g0		/* check write permissions */
	bz,a,pn	%xcc, uprot_wrfault		/* no, jump ahead */
	  nop
	andcc	%g3, TTE_HWWR_INT, %g0		/* check if modbit is set */
	bnz,a,pn %xcc, 6f			/* yes, go load tte into tsb */
	  nop
	/* update mod bit  */
	sethi	%hi(dcache_line_mask), %g7
	ld	[%g7 + %lo(dcache_line_mask)], %g7
	and	%g7, %g5, %g5
	stxa	%g0, [%g5]ASI_DC_TAG		/* flush line from dcache */
	membar	#Sync
9:
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g1
	casxa	[%g4] ASI_MEM, %g3, %g1		/* update ref/mod bit */
	cmp	%g3, %g1
	bne,a,pn %xcc, 9b
	  ldxa	[%g4] ASI_MEM, %g3		/* MMU_READTTE through pa */
	or	%g3, TTE_HWWR_INT | TTE_REF_INT, %g3
6:

	/*
	 * load into the TSB
	 * g2 = vaddr
	 * g3 = tte
	 * the TSB_8K ptr still has a correct value so take advantage of it.
	 */
	srlx	%g3, TTE_SZ_SHFT, %g5
	cmp	%g5, TTESZ_VALID | TTE8K
	bne,pn	%icc, 4f
	  nop
	ldxa	[%g0]ASI_DMMU, %g4		/* tag target */
	GET_TSB_POINTER(%g2, %g1, %g5, %g6, %g7)
	TSB_UPDATE(%g1, %g3, %g4)
4:
	/*
	 * Now, load into TLB
	 */
	stxa	%g3, [%g0] ASI_DTLB_IN
	retry
	membar	#Sync

uprot_fault_lock:
	ld	[%g6 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %g5
	HMELOCK_EXIT(%g5)
	/* fall through */

uprot_fault:
	/*
         * we get here if we couldn't find valid hment in hash
         * first check if we are tl > 1, in which case we call window_trap
         * otherwise call pagefault
         * g2 = tag access reg
         */

	rdpr	%tl, %g4
	cmp	%g4, 1
	bg,a,pn	%xcc, sfmmu_window_trap
	  nop
	ba,pt	%xcc, sfmmu_pagefault_inval
	  nop

uprot_wrfault:
	/*
	 * we get here if we didn't have write permissions
	 */
	rdpr	%tl, %g4
	cmp	%g4, 1
	bg,a,pn	%xcc, sfmmu_window_trap
	  nop
	ba,pt	%xcc, sfmmu_pagefault
	  nop


uprot_tl0:
	/*
	 * We get here in the case we need to service this protection
	 * in c code.  Causes:
	 * ctx was stolen
	 * write fault on ism segment.
	 *
	 * first check if we are tl > 1, in which case we panic.
	 * otherwise call sfmmu_tsb_miss
	 * g2 = tag access reg
	 */
	rdpr	%tl, %g4
	cmp	%g4, 1
	ble,pt	%icc, sfmmu_mmu_trap
	  nop
	ba,pt	%icc, sfmmu_window_trap
	  nop
	SET_SIZE(sfmmu_uprot_trap)

#endif /* lint */

#if defined (lint)
/*
 * This routine will look for a user or kernel vaddr in the hash
 * structure.  It returns a valid pfn or -1.  It doesn't
 * grab any locks.  It should only be used by other sfmmu routines.
 */
/* ARGSUSED */
u_int
sfmmu_vatopfn(caddr_t vaddr, sfmmu_t *sfmmup)
{
	return(0);
}

#else /* lint */

	ENTRY_NP(sfmmu_vatopfn)
	save	%sp, -SA(MINFRAME), %sp
	/*
	 * disable interrupts
	 */
	rdpr	%pstate, %i4
#ifdef DEBUG
	andcc	%i4, PSTATE_IE, %g0		/* if interrupts already */
	bnz,a,pt %icc, 1f			/* disabled, panic	 */
	  nop
	sethi	%hi(sfmmu_panic1), %o0
	call	panic
	 or	%o0, %lo(sfmmu_panic1), %o0
1:
#endif /* DEBUG */
	/*
	 * disable interrupts, clear Address Mask to access 64 bit physaddr
	 */
	wrpr	%i4, PSTATE_IE | PSTATE_AM, %pstate

	/*
	 * i0 = vaddr
	 * i1 = sfmmup
	 */
	srl	%i0, 0, %i0			/* clear upper 32 bits */
	srl	%i1, 0, %i1			/* clear upper 32 bits */
	CPU_INDEX(%o1)
	sethi	%hi(tsbmiss_area), %o2
	sllx	%o1, TSBMISS_SHIFT, %o1
	or	%o2, %lo(tsbmiss_area), %o2
	add	%o2, %o1, %o2			/* o2 = tsbmiss area */
	ld	[%o2 + TSBMISS_KHATID], %l1
	cmp	%l1, %i1
	bne,pn	%icc, vatopfn_user
	  mov	%i1, %o4			/* o4 = hatid */

vatopfn_kernel:
	/*
	 * i0 = vaddr
	 * i1 & o4 = hatid
	 * o2 = tsbmiss area
	 */
	mov	1, %l5				/* l5 = rehash # */
	mov	HBLK_RANGE_SHIFT, %l6
1:

	/*
	 * i0 = vaddr
	 * i1 & o4 = hatid
	 * l5 = rehash #
	 * l6 = hmeshift
	 */
	GET_TTE(%i0, %o4, %g1, %g2, %g3, %o2, %g4,
		TSBMISS_KHASHSZ, TSBMISS_KHASHSTART, %l6, %l5,
		vatopfn_l1, kvtop_nohblk,
		HATSTAT_KHASH_SEARCH, HATSTAT_KHASH_LINKS)

	/*
	 * i0 = vaddr
	 * g1 = tte
	 * g2 = tte pa
	 * g3 = tte va
	 * o2 = tsbmiss area
	 * i1 = hat id
	 */
	brgez,a,pn %g1, 6f			/* if tte invalid goto tl0 */
	  sub	%g0, 1, %i0			/* output = -1 */
	TTETOPFN(%g1, %i0, vatopfn_l2)		/* uses g1, g2, g3, g4 */
	/*
	 * i0 = vaddr
	 * g1 = pfn
	 */
	ba,pt	%icc, 6f
	  mov	%g1, %i0

kvtop_nohblk:
	/*
	 * we get here if we couldn't find valid hblk in hash.  We rehash
	 * if neccesary.
	 */
	ld	[%o2 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %o3
	HMELOCK_EXIT(%o3)
	ld	[%o2 + (TSBMISS_SCRATCH + TSB_TAGACC)], %i0
	cmp	%l5, MAX_HASHCNT
	be,a,pn	%icc, 6f
	 sub	%g0, 1, %i0			/* output = -1 */
	mov	%i1, %o4			/* restore hatid */
	add	%l5, 1, %l5
	cmp	%l5, 2
	move	%icc, MMU_PAGESHIFT512K, %l6
	ba,pt	%icc, 1b
	  movne	%icc, MMU_PAGESHIFT4M, %l6
6:
	wrpr	%g0, %i4, %pstate		/* enable interrupts */
	ret
	restore

vatopfn_user:
	/*
	 * First check for ISM. If not, just fall thru.
	 *
	 * i1, o4 = hatid
	 * i0 = vaddr
	 * o2 = tsbmiss area
	 */
	ISM_CHECK_TL0(%o4, %i0, %g1, %g2, %g4, %l5, vatopfn_l3, 
		  vatopfn_ism, vatopfn_rare)
	mov	%i1, %o4			/* restore hatid */

vatopfn_user_common:

	/*
	 * i0 = vaddr
	 * i1, o4 = hatid
	 */
	mov	1, %l5				/* l5 = rehash # */
1:
	cmp	%l5, 1
	be,a,pt	%icc, 2f
	  mov	HBLK_RANGE_SHIFT, %l6
	cmp	%l5, 2
	move	%icc, MMU_PAGESHIFT512K, %l6
	movne	%icc, MMU_PAGESHIFT4M, %l6
2:

	/*
	 * i0 = vaddr
	 * i1 & o4 = hatid
	 * l5 = rehash #
	 * l6 = hmeshift
	 */
	GET_TTE(%i0, %o4, %g1, %g2, %g3, %o2, %g4,
		TSBMISS_UHASHSZ, TSBMISS_UHASHSTART,
		%l6, %l5, vatopfn_l4, uvtop_nohblk,
		HATSTAT_UHASH_SEARCH, HATSTAT_UHASH_LINKS)
	/*
	 * i0 = vaddr
	 * i1 = hatid
	 * g1 = tte
	 * g2 = tte pa
	 * g3 = tte va
	 * o2 = tsbmiss area
	 */

	brgez,a,pn %g1, 6f			/* if tte invalid goto tl0 */
	  sub	%g0, 1, %i0			/* output = -1 */
	TTETOPFN(%g1, %i0, vatopfn_l5)		/* uses g1, g2, g3, g4 */
	/*
	 * i0 = vaddr
	 * g1 = pfn
	 */
	ba	%icc, 6f
	  mov	%g1, %i0

uvtop_nohblk:
	/*
	 * we get here if we couldn't find valid hment in hash.
	 */
	ld	[%o2 + (TSBMISS_SCRATCH + TSBMISS_HMEBP)], %o3
	HMELOCK_EXIT(%o3)
	ld	[%o2 + (TSBMISS_SCRATCH + TSB_TAGACC)], %i0
	cmp	%l5, MAX_HASHCNT
	mov	%i1, %o4			/* restore hatid */
	bne,pt	%icc, 1b
	  add	%l5, 1, %l5
	sub	%g0, 1, %i0			/* output = -1 */
6:
	wrpr	%g0, %i4, %pstate		/* enable interrupts */
	ret
	restore

	/*
	 * We branch here if we detect a lookup on an ISM
	 * address. Adjust hatid to point the correct ISM
	 * hatid and va to offset into ISM segment.
	 */
vatopfn_ism:

	/*
	 * i0 = vaddr
	 * i1 = hatid
	 * g1 = ism mapping
	 */
        srl     %g1, ISM_VB_SHIFT, %l4          /* clr ism hatid */
        sll     %l4, ISM_AL_SHIFT, %l4          /* l4 = ism vbase */
        sub     %i0, %l4, %i0                   /* i0 = offset in ISM seg */
	srlx	%g1, ISM_HAT_SHIFT, %i1		/* g3 = ism hatid */
	ba,pt	%xcc, vatopfn_user_common
	  mov	%i1, %o4

	/*
	 * In the rare case this is a user va and this process
	 * has more than ISM_MAP_SLOTS ISM segments we goto 
	 * C code and handle the lookup there.
	 */
vatopfn_rare:
	/*
	 * i0 = vaddr
	 * i1 = user hatid
	 */
	wrpr	%g0, %i4, %pstate		/* enable interrupts */
	mov	%i1, %o1
	call	sfmmu_user_vatopfn
	mov	%i0, %o0

	mov	%o0, %i0			/* ret value */
	ret
	restore

	SET_SIZE(sfmmu_vatopfn)
#endif /* lint */

#ifndef lint

/*
 * per cpu tsbmiss area to avoid cache misses in tsb miss handler.
 */
	.seg	".data"
	.align	64
	.global tsbmiss_area
tsbmiss_area:
	.skip	(TSBMISS_SIZE * NCPU)

#endif	/* lint */
