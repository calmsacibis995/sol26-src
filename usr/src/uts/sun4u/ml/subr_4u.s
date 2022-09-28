/*
 * Copyright (c) 1990, 1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)subr_4u.s	1.38	96/09/09 SMI"

/*
 * General machine architecture & implementation specific
 * assembly language routines.
 */
#if defined(lint)
#include <sys/types.h>
#include <sys/machsystm.h>
#include <sys/t_lock.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/eeprom.h>
#include <sys/param.h>
#include <sys/async.h>
#include <sys/intreg.h>
#include <sys/machthread.h>
#include <sys/iocache.h>
#include <sys/privregs.h>
#include <sys/archsystm.h>

#if defined(lint)

/*ARGSUSED*/
int
getprocessorid(void)
{ return (0); }

#else	/* lint */

/*
 * Get the processor ID.
 * === MID reg as specified in 15dec89 sun4u spec, sec 5.4.3
 */

	ENTRY(getprocessorid)
	CPU_INDEX(%o0)			! get cpu id number 0..3
	retl
	nop
	SET_SIZE(getprocessorid)

#endif	/* lint */

#if defined(lint)

unsigned int
stub_install_common(void)
{ return (0); }

#else	/* lint */

	ENTRY(stub_install_common)

	save	%sp, -SA(MINFRAME), %sp
	call	install_stub,1
	mov	%g1, %o0
	jmpl	%o0, %g0
	restore
	SET_SIZE(stub_install_common)

#endif	/* lint */

#if defined (lint)
caddr_t	set_trap_table(void)
{
	return ((caddr_t) 0);
}
#else /* lint */

	ENTRY(set_trap_table)
	set	trap_table, %o1
	rdpr	%tba, %o0
	wrpr	%o1,%tba
	retl
	wrpr	%g0, WSTATE_KERN, %wstate
	SET_SIZE(set_trap_table)

#endif /* lint */

#if defined (lint)
u_int
get_error_enable_tl1(volatile u_longlong_t *neer, u_int dflag)
{
	dflag = dflag;
	return ((u_int) neer);
}
#else /* lint */

	ENTRY(get_error_enable_tl1)
	ldxa	[%g0]ASI_ESTATE_ERR, %g3	/* ecache error enable reg */
	stx	%g3, [%g1]
	st	%g0, [%g2]
	membar	#Sync
	retry
	SET_SIZE(get_error_enable_tl1)

#endif /* lint */

#if defined (lint)
u_int
set_error_enable_tl1(volatile u_longlong_t *neer)
{
	return ((u_int) neer);
}
#else /* lint */

	ENTRY(set_error_enable_tl1)
	ldx	[%g1], %g2	
	stxa	%g2, [%g0]ASI_ESTATE_ERR	/* ecache error enable reg */
	membar	#Sync
	retry
	SET_SIZE(set_error_enable_tl1)

#endif /* lint */

#if defined (lint)
void
get_asyncflt(volatile u_longlong_t * afsr)
{
	afsr = afsr;
}
#else /* lint */

	ENTRY(get_asyncflt)
	ldxa	[%g0]ASI_AFSR, %o1		! afsr reg
	retl
	stx	%o1, [%o0]
	SET_SIZE(get_asyncflt)

#endif /* lint */

#if defined (lint)
void
set_asyncflt(volatile u_longlong_t * afsr)
{
	afsr = afsr;
}
#else /* lint */

	ENTRY(set_asyncflt)
	ldx	[%o0], %o2	
	stxa	%o2, [%g0]ASI_AFSR		! afsr reg
	retl
	membar	#Sync
	SET_SIZE(set_asyncflt)

#endif /* lint */

#if defined (lint)
void
get_asyncaddr(volatile u_longlong_t * afar)
{
	afar = afar;
}
#else /* lint */

	ENTRY(get_asyncaddr)
	ldxa	[%g0]ASI_AFAR, %o1		! afar reg
	retl
	stx	%o1, [%o0]
	SET_SIZE(get_asyncaddr)

#endif /* lint */

#if defined(lint)

/*ARGSUSED*/
void
scrubphys(caddr_t vaddr)
{}

#else	/* lint */

	!
	! read/write at virtual address
	!
	! This routine is called by ecc handler to scrub the
	! faulty address. See Sun5 System Arch. Manual, section 6.7.2.2,
	! Programming Notes on Scrub
	!
	! void	scrubphys(vaddr)
	!
	.seg	".data"
	.global	dreg_save
	.align	64
dreg_save:
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

	.seg ".text"
	.align	4

	ENTRY(scrubphys)
	rd	%fprs, %o1			! save fprs
	andcc	%o1, FPRS_FEF, %o2		! is fp in use?
	bz,a,pt	%icc, 1f			! not in use, don't save regs
	wr	%g0, FPRS_FEF, %fprs		! normally it's not in use

	set	dreg_save, %g1
	stda	%d32, [%g1]ASI_BLK_PL
	membar	#Sync
1:
	ldda	[%o0]ASI_BLK_P, %d32
	membar	#Sync
	stda    %d32, [%o0]ASI_BLK_COMMIT_P
	membar	#Sync
	ldda	[%o0]ASI_BLK_P, %d32
	andcc	%o1, FPRS_FEF, %o2		! check if fp in use again?
	bz	2f				! not in use, don't restore regs
	membar	#Sync

	ldda	[%g1]ASI_BLK_PL, %d32
	membar	#Sync
2:
	retl
	wr	%o1, %fprs			! restore fprs
	SET_SIZE(scrubphys)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
reset_ecc(caddr_t vaddr)
{}

#else	/* lint */

	!
	! write/read at virtual address
	!
	! This routine is called by the ecc handler to clear ecc at the
	! faulty address. 
	!
	! void	reset_ecc(vaddr)
	!
	.seg	".data"
	.global	dregs_zero
	.align	64
dregs_zero:
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

	.global	dregs_save
	.align	64
dregs_save:
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

	.seg ".text"
	.align	4

	ENTRY(reset_ecc)
	rd	%fprs, %o1			! save fprs
	andcc	%o1, FPRS_FEF, %o2		! is fp in use?
	bz,a,pt	%icc, 1f			! not in use, don't save regs
	wr	%g0, FPRS_FEF, %fprs		! enable fprs

	set	dregs_save, %g1			! save fp regs
	stda	%d32, [%g1]ASI_BLK_PL
	membar	#Sync
1:
	set	dregs_zero, %g2
	ldda	[%g2]ASI_BLK_P, %d32
	membar	#Sync
	stda    %d32, [%o0]ASI_BLK_COMMIT_P
	membar	#Sync
	andcc	%o1, FPRS_FEF, %o2		! check if fp in use again?
	bz,a,pt	%icc, 2f			! not in use, don't restore regs
	nop

	ldda	[%g1]ASI_BLK_PL, %d32		! restore fp regs
	membar	#Sync
2:
	retl
	wr	%o1, %fprs			! restore fprs
	SET_SIZE(reset_ecc)

#endif	/* lint */

/*
 * Answer questions about any extended SPARC hardware capabilities.
 * On this platform, for now, it is NONE. XXXXX 
 */

#if	defined(lint)

/*ARGSUSED*/
int
get_hwcap_flags(int inkernel)
{ return (0); }

#else   /* lint */

	ENTRY(get_hwcap_flags)
#define FLAGS   AV_SPARC_HWMUL_32x32 | AV_SPARC_HWDIV_32x32 | AV_SPARC_HWFSMULD
        sethi   %hi(FLAGS), %o0
	retl
	or      %o0, %lo(FLAGS), %o0
#undef	FLAGS
	SET_SIZE(get_hwcap_flags)

#endif  /* lint */

#if defined(lint)

/*ARGSUSED*/
void
stphys(u_longlong_t physaddr, int value)
{}

/*ARGSUSED*/
int
ldphys(u_longlong_t physaddr)
{ return(0); }

/*ARGSUSED*/   
void
stdphys(u_longlong_t physaddr, u_longlong_t value)
{}

/*ARGSUSED*/
u_longlong_t
lddphys(u_longlong_t physaddr)
{ return(0x0ull); }

#else

        ! Store long word value at physical address
        !
        ! void  stdphys(u_longlong_t physaddr, u_longlong_t value)
        !
        ENTRY(stdphys)
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)

        sllx    %o2, 32, %o2    ! shift upper 32 bits
	srl	%o3, 0, %o3	! clear upper 32 bits
        or      %o2, %o3, %g1   ! form 64 bit value in %g1 using (%o2,%o3)

	/*
	 * disable interrupts, clear Address Mask to access 64 bit physaddr
	 */
	rdpr	%pstate, %o4
	wrpr	%o4, PSTATE_IE | PSTATE_AM, %pstate	! clear IE, AM bits
        stxa    %g1, [%o0]ASI_MEM
        retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
        SET_SIZE(stdphys)
 
 
        !
        ! Load long word value at physical address
        !
        ! unsigned long long lddphys(u_longlong_t physaddr)
        !
        ENTRY(lddphys)
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)

	rdpr	%pstate, %o4
	wrpr	%o4, PSTATE_IE | PSTATE_AM, %pstate	! clear IE, AM bits
        ldxa    [%o0]ASI_MEM, %g1
	srlx	%g1, 32, %o0	! put the high 32 bits in low part of o0
	srl	%g1, 0, %o1	! put lower 32 bits in o1, clear upper 32 bits
        retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
	SET_SIZE(lddphys)

        !
        ! Store value at physical address
        !
        ! void  stphys(u_longlong_t physaddr, int value)
        !
        ENTRY(stphys)
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)

	rdpr	%pstate, %o4
	wrpr	%o4, PSTATE_IE | PSTATE_AM, %pstate	! clear IE, AM bits
        sta     %o2, [%o0]ASI_MEM
        retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
        SET_SIZE(stphys)


        !
        ! load value at physical address
        !
        ! int   ldphys(u_longlong_t physaddr)
        !
        ENTRY(ldphys)
	sllx	%o0, 32, %o0	! shift upper 32 bits
	srl	%o1, 0, %o1	! clear upper 32 bits
	or	%o0, %o1, %o0	! form 64 bit physaddr in %o0 using (%o0,%o1)

	rdpr	%pstate, %o4
	wrpr	%o4, PSTATE_IE | PSTATE_AM, %pstate	! clear IE, AM bits
        lda     [%o0]ASI_MEM, %o0
	srl	%o0, 0, %o0	! clear upper 32 bits
        retl
	wrpr	%g0, %o4, %pstate	! restore earlier pstate register value
        SET_SIZE(ldphys)


#endif

/*
 * save_gsr(fp)
 *	struct v9_fpu *fp;
 * Store the graphics status register
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
save_gsr(struct v9_fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(save_gsr)
	rd	%gsr, %g2			! save gsr
	retl
	stx	%g2, [%o0 + FPU_GSR]
	SET_SIZE(save_gsr)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
restore_gsr(struct v9_fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(restore_gsr)
	ldx	[%o0 + FPU_GSR], %g2
	wr	%g2, %g0, %gsr
	retl
	nop
	SET_SIZE(restore_gsr)

#endif	/* lint */

/*
 * get_gsr(fp, buf)
 *	struct v9_fpu *fp;
 *	caddr_t buf;
 * Get the graphics status register info from fp and store it to buf
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
get_gsr(struct v9_fpu *fp, caddr_t buf)
{}

#else	/* lint */

	ENTRY_NP(get_gsr)
	ldx	[%o0 + FPU_GSR], %g1
	retl
	stx	%g1, [%o1]
	SET_SIZE(get_gsr)

#endif	/* lint */

/*
 * set_gsr(buf, fp)
 *	caddr_t buf;
 *	struct v9_fpu *fp;
 * Set the graphics status register info to fp from buf
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
set_gsr(caddr_t buf, struct v9_fpu *fp)
{}

#else	/* lint */

	ENTRY_NP(set_gsr)
	ldx	[%o0], %g1
	retl
	stx	%g1, [%o1 + FPU_GSR]
	SET_SIZE(set_gsr)

#endif	/* lint */
