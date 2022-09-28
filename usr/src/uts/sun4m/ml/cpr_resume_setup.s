/*
 * Copyright (c) 1990 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_resume_setup.s	1.12	96/04/16 SMI"

#if defined(lint)
#include <sys/types.h>
#else   /* lint */
#include "assym.s"
#endif  /* lint */

#include <sys/asm_linkage.h>
#include <sys/psr.h>		/* for the PSR_xx defines */
#include <sys/machthread.h>	/* for reg definition */
#include <sys/module_ross625.h>

#define ASI_MOD         0x4     /* Modlule control/status register */
#define RMMU_CTP_REG    0x100   /* srmmu context table pointer register */
#define RMMU_CTX_REG    0x200   /* srmmu context register */
#define PSR_ET 0x00000020
#define FT_ALL 0x00000004
#define ASI_FLPR 0x3

/*
 * Entry point for resume from boot.
 * 	1. restore mmu to checkpoint context
 *	2. set up the thread and lwp registers for the cpr process
 *	3. init cwp and wim 
 *	4. restore checkpoint stack poniter
 *	5. long jmp back to kernel
 */
#if defined(lint)

/* ARGSUSED */
void
i_cpr_restore_hwregs(u_int ctp, u_int ctx, caddr_t thread, caddr_t qsav, u_int ctl)
{}

/* ARGSUSED */
u_int
cpr_get_mcr(void)
{ return (0);}

#else	/* lint */


.align 128

ENTRY(i_cpr_restore_hwregs)


	! bug 1170275 ported from module_srmmu_asm.s
	mov	%psr, %l5;
	andn	%l5, PSR_ET, %l4;
	mov	%l4, %psr;		! Disable traps
	nop;nop;nop;			! PSR delay

	!
	! set up original kernel mapping
	!
	set	RMMU_CTP_REG, %o5	! set srmmu context table pointer
	sta	%o0, [%o5]ASI_MOD
	set	RMMU_CTX_REG, %o5	! set srmmu context number 
	sta	%o1, [%o5]ASI_MOD
	! compare saved MCR adn current MCR
	! if different MCR value, restore
	set	RMMU_CTL_REG, %o5
	lda	[%o5]ASI_MOD, %o0
	cmp	%o0, %o4
	bne,a	1f
	sta	%o4, [%o5]ASI_MOD
1:
	or      %g0, FT_ALL<<8, %o0     ! flush entire mmu
	sta     %g0, [%o0]ASI_FLPR      ! do the flush for tsunami flush problem
	!
	mov	%l5, %psr;		! Enable traps
	nop;nop;nop;			! PSR delay

	mov	%o2, THREAD_REG		! sets up the thread reg
	ld	[%o2 + T_LWP], %o5
	mov	%o5, PROC_REG		! sets lwp reg

	!
	! init the cwp and wim before longjmp
	!
        mov     %o3, %g5                ! save qsav while we play w/ wins
        mov     0x02, %wim              ! setup wim
        set     PSR_S|PSR_PIL|PSR_ET, %g1
        mov     %g1, %psr               ! initialize psr: supervisor, splmax
        nop                             ! and leave traps enabled for monitor
        nop                             ! psr delay
        nop                             ! psr delay

        set     scb, %g1                ! restore the kernel trap handler
        mov     %g1, %tbr
        nop
        nop
        nop
 
        mov     %g0, %wim               ! note psr has cwp = 0
        save                            ! decrement cwp, wraparound to NW-1
        mov     %psr, %g1
        and     %g1, PSR_CWP, %g1       ! we now have nwindows-1
        restore                         ! get back to orignal window
        mov     2, %wim                 ! reset initial wim

	!
	! special shortened version of longjmp
	! at this point there is no stack to run on
	! XXX: tayfun, is it ok not to flush windows?
	!
	ld	[%g5 + L_PC], %i7
	ld	[%g5 + L_SP], %fp	! restore sp on old stack
	ret				! return 1
	restore	%g0, 1, %o0		! takes underflow, switches stk

	SET_SIZE(i_cpr_restore_hwregs)

	ENTRY(cpr_get_mcr)
	retl
	lda     [%g0]ASI_MOD, %o0
	SET_SIZE(cpr_get_mcr)

#endif

