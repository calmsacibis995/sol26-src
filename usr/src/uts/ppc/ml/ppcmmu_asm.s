/*
 *	Copyright (c) 1995, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)ppcmmu_asm.s	1.13	95/08/03 SMI"

#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <vm/mach_ppcmmu.h>

#if defined(lint) || defined(__lint)

#include <sys/types.h>
#include <sys/param.h>
#include <sys/pte.h>

#else /* lint */

#include "assym.s"

/*
 * Low level functions for PowerPC MMU.
 *
 * These functions assume 32bit implementation of PowerPC.
 */

/*
 * Byte offsets to access byte or word within PTE structure.
 * PTE structure: (BIG ENDIAN format used as reference)
 *		0-------1-------2-------3-------
 *		v.........vsid...........h...api
 *		4-------5-------6-------7-------
 *		ppn.............000.R.C.WIMG.0.PP
 */
#ifdef _BIG_ENDIAN
#define	PTEBYTE0	0
#define	PTEWORD0	0
#define	PTEWORD1	4
#else /* _LITTLE_ENDIAN */
#define	PTEBYTE0	7
#define	PTEWORD0	4
#define	PTEWORD1	0
#endif

#endif /* lint */

/*
 * Special Purpose Register numbers.
 */
#define	BAT0U	528	/* 601-BAT0U or IBAT0U */
#define	BAT0L	529	/* 601-BAT0L or IBAT0L */
#define	BAT1U	530	/* 601-BAT1U or IBAT1U */
#define	BAT1L	531	/* 601-BAT1L or IBAT1L */
#define	BAT2U	532	/* 601-BAT2U or IBAT2U */
#define	BAT2L	533	/* 601-BAT2L or IBAT2L */
#define	BAT3U	534	/* 601-BAT3U or IBAT3U */
#define	BAT3L	535	/* 601-BAT3L or IBAT3L */
#define	IBAT0U	528
#define	IBAT0L	529
#define	IBAT1U	530
#define	IBAT1L	531
#define	IBAT2U	532
#define	IBAT2L	533
#define	IBAT3U	534
#define	IBAT3L	535
#define	DBAT0U	536
#define	DBAT0L	537
#define	DBAT1U	538
#define	DBAT1L	539
#define	DBAT2U	540
#define	DBAT2L	541
#define	DBAT3U	542
#define	DBAT3L	543

#ifndef MP

/*
 * Macro for TLB flush.
 *
 * Algorithm:
 * 	Disable the interrupts.
 *	tlbie
 *	sync
 *	Enable the interrupts.
 */
#define	TLBFLUSH(addr, reg1, reg2, reg3)	\
	/* clear MSR_EE to disable the interrupts */	;\
	mfmsr	reg1				;\
	rlwinm	reg2,reg1,0,17,15		;\
	mtmsr	reg2				;\
	/* UP: Flush the TLB */			;\
	tlbie	addr				;\
	sync					;\
	/* enable the interrupts */		;\
	mtmsr	reg1;				;

#else	/* MP */
/*
 * Macro for TLB flush. On PowerPC the 'tblie' or 'tlbsync'
 * (on non 601) can be executed on only one processor. So, this code is
 * under the ppcmmu_flush_lock spin lock.
 *
 * Algorithm:
 * 	Disable the interrupts.
 * 	If (ncpus == 1) then
 *		tlbie
 *		sync
 *	Else
 *		Get the ppcmmu_flush_lock.
 *		tlbie
 *		sync
 *		if (!mmu601) then
 *			tlbsync
 *			sync
 *		Release the ppcmmu_flush_lock.
 *	Enable the interrupts.
 */
#define	TLBFLUSH(addr, reg1, reg2, reg3)	\
	/* clear MSR_EE to disable the interrupts */	;\
	mfmsr	reg1				;\
	rlwinm	reg2,reg1,0,17,15		;\
	mtmsr	reg2				;\
	lis	reg2, ncpus@ha			;\
	lwz	reg2, ncpus@l(reg2)		;\
	cmpi	reg2, 1	/* (ncpus == 1) ? */	;\
	bne-	7f				;\
	/* UP: Flush the TLB */			;\
	tlbie	addr				;\
	sync					;\
	b	9f				;\
	/* MP: Get the ppcmmu_flush_lock */	;\
7:						;\
	lis	reg3, ppcmmu_flush_lock@ha	;\
	la 	reg3, ppcmmu_flush_lock@l(reg3)	;\
	lwarx	reg2, %r0, reg3			;\
	cmpi	reg2, 0				;\
	bne	7b				;\
	li	reg2, 0xff			;\
	stwcx.	reg2, %r0, reg3			;\
	bne	7b				;\
	/* got the lock, do the flush */	;\
	tlbie	addr				;\
	sync					;\
	lis	reg2, mmu601@ha			;\
	lwz	reg2, mmu601@l(reg2)		;\
	cmpi	reg2, 0		/* mmu601 ? */	;\
	bne	8f				;\
	tlbsync					;\
	sync					;\
8:						;\
	li	reg2, 0				;\
	/* release the ppcmmu_flush_lock */	;\
	stw	reg2, 0(reg3)			;\
9:						;\
	/* enable the interrupts */		;\
	mtmsr	reg1				;

#endif

/*
 * Spin lock for protecting the execution of TLBFLUSH() on MP.
 */
#if !defined(lint) && !defined(__lint)
	.data
	.globl	ppcmmu_flush_lock
	.align	2
ppcmmu_flush_lock:
	.long	0
	.text
#endif /* lint */

/*
 * mmu_update_pte(hwpte_t *hwpte, pte_t *pte, int remap, caddr_t addr)
 *	Construct hw pte from software pte. And update the hw pte location
 *	with the new mapping.
 *	Steps:	if (remap)
 *			Invalidate the old mapping
 *			Sync
 *			TLB flush for the entry.
 *			Sync
 *			Tlbsync and sync (not on 601?)
 *			Or in the old rm bits into the new pte.
 *		Write the new hw pte without valid bit set
 *		Sync
 *		Update the valid bit
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
mmu_update_pte(hwpte_t *hwpte, pte_t *pte, int remap, caddr_t addr)
{}

#else	/* lint */

	ENTRY(mmu_update_pte)
	lwz	%r7, PTEWORD0(%r4)	! pte word 0
	lwz	%r8, PTEWORD1(%r4)	! pte word 1

	/* If remap, invalidate old mapping */
	cmpwi	%r5, 0
	beq+	.new_entry		! old mapping is not valid

	! invalidate the old mapping
	lbz	%r10, PTEBYTE0(%r3)
	andi.	%r10, %r10, 0x7F
	stb	%r10, PTEBYTE0(%r3)	! set valid bit to zero
	sync

	! flush the TLB entry
	TLBFLUSH(%r6, %r5, %r9, %r10)

	! copy the old rm bits into the new pte
	lwz	%r10, PTEWORD1(%r3)
	li	%r5, 0x0180
	and	%r11, %r10, %r5		! %r11 = rm
	or	%r8, %r8, %r11		! pte_w1 |= old_rm

.new_entry:
	stw	%r8, PTEWORD1(%r3)	! write the pte word 1
	sync
	stw	%r7, PTEWORD0(%r3)	! write the pte word 0 (valid bit set)
	blr
	SET_SIZE(mmu_update_pte)

#endif /* lint */

/*
 * mmu_pte_reset_rmbits(hwpte_t *hwpte, caddr_t addr)
 *	Clear the R and C bits in the PTE.
 *	Steps:	Invalidate the old mapping
 *		Sync
 *		TLB flush for the entry
 *		Sync
 *		Tlbsync and sync (not on 601?)
 *		Save the old rm bits
 *		Clear the R and C bits in the PTE.
 *		Sync
 *		Set the valid bit
 *		Return the old rm bits
 */
#if defined(lint)

/* ARGSUSED */
void
mmu_pte_reset_rmbits(hwpte_t *hwpte, caddr_t addr)
{}

#else	/* lint */

	ENTRY(mmu_pte_reset_rmbits)
	li	%r8, 0x0180		! mask for rm bits
	lbz	%r5, PTEBYTE0(%r3)
	andi.	%r7, %r5, 0x7F		! clear the valid bit
	stb	%r7, PTEBYTE0(%r3)
	sync

	! flush the TLB entry
	TLBFLUSH(%r4, %r6, %r7, %r10)

	lwz	%r6, PTEWORD1(%r3)	! pte word 1
	andc	%r7, %r6, %r8
	stw	%r7, PTEWORD1(%r3)	! clear rm bits in the PTE
	sync
	stb	%r5, PTEBYTE0(%r3)	! set PTE valid

	! return old rm bits in the least 2 significant bits
	li	%r3, 0
	rlwimi	%r3, %r6, 25, 30, 31	! get old rm bits
	blr
	SET_SIZE(mmu_pte_reset_rmbits)

#endif /* lint */

/*
 * mmu_update_pte_wimg(hwpte_t *hwpte, u_int wimg, caddr_t addr)
 *	Update the WIMG bits in the PTE.
 *	Steps:	Invalidate the old mapping (valid bit to 0)
 *		Sync
 *		TLB flush for the entry.
 *		Sync
 *		Tlbsync and sync (not on 601?)
 *		Set the new wimg bits in the PTE.
 *		Sync
 *		Set the valid bit.
 */
#if defined(lint)

/* ARGSUSED */
void
mmu_update_pte_wimg(hwpte_t *hwpte, u_int wimg, caddr_t addr)
{}

#else	/* lint */

	ENTRY(mmu_update_pte_wimg)
	lwz	%r10, PTEWORD1(%r3)	! pte word 1
	li	%r8, 0x78		! mask for wimg bits
	rlwimi	%r4, %r4, 3, 0, 31	! wimg <<= 3
	lbz	%r9, PTEBYTE0(%r3)	! pte byte 0
	andi.	%r11, %r9, 0x7F
	stb	%r11, PTEBYTE0(%r3)	! set valid bit to zero
	sync

	! flush the TLB entry
	TLBFLUSH(%r5, %r7, %r11, %r6)

	andc	%r10, %r10, %r8
	or	%r10, %r10, %r4
	stw	%r10, PTEWORD1(%r3)	! set new wimg bits in the PTE
	sync
	stb	%r9, PTEBYTE0(%r3)	! set the PTE back to VALID
	blr
	SET_SIZE(mmu_update_pte_wimg)

#endif /* lint */

/*
 * mmu_update_pte_prot(hwpte_t *, u_int prot, caddr_t addr)
 *	And update the pp bits in hw pte.
 *	Steps:	Invalidate the old mapping (valid bit to 0)
 *		Sync
 *		TLB flush for the entry.
 *		Sync
 *		Tlbsync and sync (not on 601?)
 *		Set the new pp bits in the PTE.
 *		Sync
 *		Set the valid bit.
 */
#if defined(lint)

/* ARGSUSED */
void
mmu_update_pte_prot(hwpte_t *hwpte, u_int prot, caddr_t addr)
{}

#else	/* lint */

	ENTRY(mmu_update_pte_prot)
	lbz	%r9, PTEBYTE0(%r3)
	lwz	%r10, PTEWORD1(%r3)	! pte word 1
	li	%r8, 0x3		! mask for pp bits (prot_mask)
	andi.	%r7, %r9, 0x7F
	stb	%r7, PTEBYTE0(%r3)	! set valid bit to zero
	sync

	! flush the TLB entry
	TLBFLUSH(%r5, %r7, %r11, %r6)

	andc	%r10, %r10, %r8		! pte-word-1 &= ~prot_mask
	or	%r10, %r10, %r4		! pte-word-1 |= prot
	stw	%r10, PTEWORD1(%r3)	! set new pp bits in the PTE
	sync
	stb	%r9, PTEBYTE0(%r3)	! set the PTE back to VALID
	blr
	SET_SIZE(mmu_update_pte_prot)

#endif /* lint */

/*
 * mmu_delete_pte(hwpte_t *hwpte, caddr_t addr)
 *	Invalidate the PTE entry.
 *	Steps:	Invalidate the old mapping
 *		Sync
 *		TLB flush for the entry.
 *		Sync
 *		Tlbsync and sync (not on 601?)
 *		Copy the old rm bits into the new pte.
 *	Return old rm bits.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
u_int
mmu_delete_pte(hwpte_t *hwpte, caddr_t addr)
{ return (0); }

#else	/* lint */

	ENTRY(mmu_delete_pte)
	lbz	%r5, PTEBYTE0(%r3)
	andi.	%r5, %r5, 0x7F		! set valid bit to 0
	stb	%r5, PTEBYTE0(%r3)	! invalidate the mapping
	sync

	! flush the TLB entry
	TLBFLUSH(%r4, %r6, %r7, %r8)

	lwz	%r6, PTEWORD1(%r3)	! pte word 1
	li	%r3, 0
	rlwimi	%r3, %r6, 25, 30, 31	! get old rm bits
	blr
	SET_SIZE(mmu_delete_pte)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
u_int
get_sdr1(void)
{ return (0); }

#else	/* lint */

	ENTRY(get_sdr1)
	mfsdr1	%r3
	blr
	SET_SIZE(get_sdr1)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * Return the value of Bat Upper for the specified bat number.
 * Returns 0 for invalid bat number.
 *	Bat-Number	0 - 3	for IBATs or BATs on 601
 *	Bat-Number	4 - 7	for DBATs on PowerPC
 */
/*ARGSUSED*/
u_int
ppcmmu_get_bat_u(int batno)
{ return (0); }

#else	/* lint */

	ENTRY(ppcmmu_get_bat_u)
	cmpi	%r3, 0
	beq+	.bat_0_u
	cmpi	%r3, 1
	beq+	.bat_1_u
	cmpi	%r3, 2
	beq+	.bat_2_u
	cmpi	%r3, 3
	beq+	.bat_3_u
	! check if it is not 601 for DBATs
	lis	%r4, mmu601@ha
	lwz	%r4, mmu601@l(%r4)
	cmpi	%r4, 0
	beq	5f
	li	%r3, 0		! huh?
	blr
5:
	cmpi	%r3, 4
	beq+	.dbat_0_u
	cmpi	%r3, 5
	beq+	.dbat_1_u
	cmpi	%r3, 6
	beq+	.dbat_2_u
	cmpi	%r3, 7
	beq+	.dbat_3_u
	li	%r3, 0
	blr			! huh?
.bat_0_u:
	mfspr	%r3, BAT0U	! BAT0U or IBAT0U
	blr
.bat_1_u:
	mfspr	%r3, BAT1U	! BAT1U or IBAT1U
	blr
.bat_2_u:
	mfspr	%r3, BAT2U	! BAT2U or IBAT2U
	blr
.bat_3_u:
	mfspr	%r3, BAT3U	! BAT3U or IBAT3U
	blr
.dbat_0_u:
	mfspr	%r3, DBAT0U	! DBAT0U
	blr
.dbat_1_u:
	mfspr	%r3, DBAT1U	! DBAT1U
	blr
.dbat_2_u:
	mfspr	%r3, DBAT2U	! DBAT2U
	blr
.dbat_3_u:
	mfspr	%r3, DBAT3U	! DBAT3U
	blr
	SET_SIZE(ppcmmu_get_bat_u)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * Return the value of Bat Lower for the specified bat number.
 * Returns 0 for invalid bat number.
 *	Bat-Number	0 - 3	for IBATs or BATs on 601
 *	Bat-Number	4 - 7	for DBATs on PowerPC
 */
/*ARGSUSED*/
u_int
ppcmmu_get_bat_l(int batno)
{ return (0); }

#else	/* lint */

	ENTRY(ppcmmu_get_bat_l)
	cmpi	%r3, 0
	beq+	.bat_0_l
	cmpi	%r3, 1
	beq+	.bat_1_l
	cmpi	%r3, 2
	beq+	.bat_2_l
	cmpi	%r3, 3
	beq+	.bat_3_l
	! check if it is not 601 for DBATs
	lis	%r4, mmu601@ha
	lwz	%r4, mmu601@l(%r4)
	cmpi	%r4, 0
	beq+	5f
	li	%r3, 0		! huh?
	blr
5:
	cmpi	%r3, 4
	beq+	.dbat_0_l
	cmpi	%r3, 5
	beq+	.dbat_1_l
	cmpi	%r3, 6
	beq+	.dbat_2_l
	cmpi	%r3, 7
	beq+	.dbat_3_l
	li	%r3, 0
	blr			! huh?
.bat_0_l:
	mfspr	%r3, BAT0L	! BAT0L or IBAT0L
	blr
.bat_1_l:
	mfspr	%r3, BAT1L	! BAT1L or IBAT1L
	blr
.bat_2_l:
	mfspr	%r3, BAT2L	! BAT2L or IBAT2L
	blr
.bat_3_l:
	mfspr	%r3, BAT3L	! BAT3L or IBAT3L
	blr
.dbat_0_l:
	mfspr	%r3, DBAT0L	! DBAT0L
	blr
.dbat_1_l:
	mfspr	%r3, DBAT1L	! DBAT1L
	blr
.dbat_2_l:
	mfspr	%r3, DBAT2L	! DBAT2L
	blr
.dbat_3_l:
	mfspr	%r3, DBAT3L	! DBAT3L
	blr
	SET_SIZE(ppcmmu_get_bat_l)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * Unmap the Bat mapping for the specified Bat number.
 *	Bat-Number	0 - 3	for IBATs or BATs on 601
 *	Bat-Number	4 - 7	for DBATs on PowerPC
 */
/*ARGSUSED*/
void
ppcmmu_unmap_bat(int batno)
{}

#else	/* lint */

	ENTRY(ppcmmu_unmap_bat)
	li	%r4, 0
	cmpi	%r3, 0
	beq+	.unmap_bat_0
	cmpi	%r3, 1
	beq+	.unmap_bat_1
	cmpi	%r3, 2
	beq+	.unmap_bat_2
	cmpi	%r3, 3
	beq+	.unmap_bat_3
	! check that it is not 601 for DBATs
	lis	%r5, mmu601@ha
	lwz	%r5, mmu601@l(%r5)
	cmpi	%r5, 0
	beq+	5f
	blr			! huh?
5:
	cmpi	%r3, 4
	beq+	.unmap_dbat_0
	cmpi	%r3, 5
	beq+	.unmap_dbat_1
	cmpi	%r3, 6
	beq+	.unmap_dbat_2
	cmpi	%r3, 7
	beq+	.unmap_dbat_3
	blr			! huh?
.unmap_bat_0:
	mtspr	BAT0U, %r4	! invalidate BAT0 or IBAT0
	mtspr	BAT0L, %r4
	blr
.unmap_bat_1:
	mtspr	BAT1U, %r4	! invalidate BAT1 or IBAT1
	mtspr	BAT1L, %r4
	blr
.unmap_bat_2:
	mtspr	BAT2U, %r4	! invalidate BAT2 or IBAT2
	mtspr	BAT2L, %r4
	blr
.unmap_bat_3:
	mtspr	BAT3U, %r4	! invalidate BAT3 or IBAT3
	mtspr	BAT3L, %r4
	blr
.unmap_dbat_0:
	mtspr	DBAT0U, %r4	! invalidate DBAT0
	mtspr	DBAT0L, %r4
	blr
.unmap_dbat_1:
	mtspr	DBAT1U, %r4	! invalidate DBAT1
	mtspr	DBAT1L, %r4
	blr
.unmap_dbat_2:
	mtspr	DBAT2U, %r4	! invalidate DBAT2
	mtspr	DBAT2L, %r4
	blr
.unmap_dbat_3:
	mtspr	DBAT3U, %r4	! invalidate DBAT3
	mtspr	DBAT3L, %r4
	blr
	SET_SIZE(ppcmmu_unmap_bat)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * Reload segment registers for the specified vsid range
 * if it is not already loaded.
 */
/*ARGSUSED*/
void
mmu_segload(u_int range)
{}

#else	/* lint */

	ENTRY(mmu_segload)
	mfsr	%r4, 0
	rlwinm	%r4, %r4, 28, 12, 31
	cmp	%r3, %r4
	beqlr-				! vsid range is same, return
	rlwinm	%r3, %r3, 4, 8, 31	! vsid-base = VSID_RANGE_ID << 4
	lis	%r4, SR_KU >> 16
	or	%r3, %r3, %r4		! segment register value
	addi	%r4,%r3,1
	mtsr	0,%r3
	addi	%r3,%r4,1
	mtsr	1,%r4
	addi	%r4,%r3,1
	mtsr	2,%r3
	addi	%r3,%r4,1
	mtsr	3,%r4
	addi	%r4,%r3,1
	mtsr	4,%r3
	addi	%r3,%r4,1
	mtsr	5,%r4
	addi	%r4,%r3,1
	mtsr	6,%r3
	addi	%r3,%r4,1
	mtsr	7,%r4
	addi	%r4,%r3,1
	mtsr	8,%r3
	addi	%r3,%r4,1
	mtsr	9,%r4
	addi	%r4,%r3,1
	mtsr	10,%r3
	addi	%r3,%r4,1
	mtsr	11,%r4
	addi	%r4,%r3,1
	mtsr	12,%r3
	mtsr	13,%r4
	sync
	blr
	SET_SIZE(mmu_segload)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * mmu_cache_flushpage(addr)
 *
 * Flush the I-cache for the specified page.
 * The sequence is to flush D-cache and invalidate the I-cache.
 *	On 603 systems the sequence is (This is true for UP systems)
 *		dcbst
 *		sync
 *		icbi
 *		isync
 *	On 604 the sequence is (This is true for MP systems)
 *		dcbst
 *		sync
 *		icbi
 *		sync
 *		isync
 */
void
mmu_cache_flushpage(void)
{}

#else	/* lint */

	ENTRY(mmu_cache_flushpage)
	rlwinm	%r3, %r3, 0, 0, 19	! clear the page offset
	lis	%r4, dcache_blocksize@ha
	lwz	%r4, dcache_blocksize@l(%r4)	! get cache block size

	!
	! loop to flush the D-cache in the page.
	!
	addi	%r5, %r3, 0x1000
	mr	%r6, %r3
1:
	dcbst	0,%r6
	add	%r6, %r6, %r4
	cmpw	%r6, %r5
	bne+	1b
	sync

	!
	! now invalidate the I-cache
	!

	! loop for I-cache invalidation on 603
2:
	icbi	0,%r3
	add	%r3, %r3, %r4
	cmpw	%r3, %r5
	bne+	2b
	! we need 'sync/isync' on MP systems.
	lis	%r7, ncpus@ha
	lwz	%r7, ncpus@l(%r7)
	cmpi	%r7, 1
	beq	3f
	sync
3:
	isync
	blr
	SET_SIZE(mmu_cache_flushpage)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * mfsrin(segn)
 *	Return the contents of the segment register 'segn'.
 */
u_int
mfsrin(void)
{ return (0); }

#else	/* lint */

	ENTRY(mfsrin)
	mfsrin	%r3, %r3
	blr
	SET_SIZE(mfsrin)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*
 * mtsrin(seg_val, segn)
 *	Load the segment register 'segn' with 'seg_val'.
 */
void
mtsrin(void)
{}

#else	/* lint */

	ENTRY(mtsrin)
	mtsrin	%r3, %r4
	sync
	blr
	SET_SIZE(mtsrin)

#endif /* lint */
