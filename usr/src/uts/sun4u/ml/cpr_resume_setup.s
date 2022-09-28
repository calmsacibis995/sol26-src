/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_resume_setup.s	1.15	96/09/19 SMI"

#if defined(lint)
#include <sys/types.h>
#else   /* lint */
#include "assym.s"
#endif  /* lint */

#include <sys/asm_linkage.h>
#include <sys/machthread.h>	/* for reg definition */

#include <sys/spitasi.h>		/* sun4u ASI */
#include <sys/mmu.h>
#include <sys/privregs.h>
#include <sys/machparam.h>
#include <vm/hat_sfmmu.h>

/*
 * Entry point for resume from boot.
 * 	1. restore I/D TSB registers
 *	2. restore primary and secondary context registers
 *	3. initialize cpu state registers
 *	4. set up the thread and lwp registers for the cpr process
	5. switch to kernel trap
 *	6. restore checkpoint stack poniter
 *	7. long jmp back to kernel
 * Register usage:
 *    %i0     prom cookie
 *    %i1     start of cprboot text pages
 *    %i2     end of cprboot text pages
 *    %i3     start of cprboot data pages
 *    %i4     end of cprboot data pages
 *    %o0-%o5 arguments
 * Any change to this register assignment requires changes to cprboot_srt.s
 */
#if defined(lint)

/* ARGSUSED */
void
i_cpr_restore_hwregs(caddr_t thread, caddr_t qsav, u_int ctx_pri,
	u_int ctx_sec, caddr_t tba, caddr_t mapbuf_va, u_int mapbuf_size)
{}

/* ARGSUSED */
int
cpr_get_mcr(void)
{
	return (0);
}

#else	/* lint */


.align 128
	.global i_cpr_setup_tsb
	.global i_cpr_read_maps

ENTRY(i_cpr_restore_hwregs)

	!
	! set up original kernel mapping
	!
	save
	call	i_cpr_setup_tsb
	nop
	restore

	! Restore PCONTEXT
	set	MMU_PCONTEXT, %l2
	stxa	%o2, [%l2]ASI_DMMU
	sethi	%hi(FLUSH_ADDR), %l3
	flush	%l3

	! Restore SCONTEXT
	set	 MMU_SCONTEXT, %l2
	stxa	%o3, [%l2]ASI_DMMU
	sethi	%hi(FLUSH_ADDR), %l3
	flush	%l3	
	
	!
	! Initialize CPU state registers
	!
	wrpr	%g0, WSTATE_KERN, %wstate
	wrpr	%g0, PSTATE_KERN|PSTATE_PEF, %pstate


	! Setup thread pointer
	mov	%o0, THREAD_REG		! sets up the thread reg %g7
	ld	[%o0 + T_LWP], %l4
	mov	%l4, PROC_REG		! sets lwp reg %g6

#if 0
	! restore prom cookie which is in %i0
	call	p1275_sparc_cif_init
	mov	%i0, %o0
#endif

	! free cprboot text pages
	mov	%o1, %l1		! preserve %o1

	set	MMU_PAGESIZE, %l0
	cmp	%i1, %i2
	bge	tdone
	sub	%i1, %i2, %i2
tnext:
	mov	%l0, %o1
	call	prom_free
	mov	%i1, %o0
	addcc	%i2, %l0, %i2
	bneg	tnext
	add	%i1, %l0, %i1
tdone:
	! free cprboot data pages
	cmp	%i3, %i4
	bge	ddone
	sub	%i3, %i4, %i4
dnext:
	mov	%l0, %o1
	call	prom_free
	mov	%i3, %o0
	addcc	%i4, %l0, %i4
	bneg	dnext
	add	%i3, %l0, %i3
ddone:
	! restore %o1
	mov	%l1, %o1

	!
	! Call the C function cpr_read_maps (which the makefile places
	! directly following this assembly language module).
	!
	! Arguments beyond the 6th must go on the stack.  The symbolic
	! location of the first of these is fr_argx from struct frame
	! found in uts/sparc/sys/frame.h.
	!
	! XXX Should use genassym to get the following definition into
	! assym.s
	!

#define FR_ARGX 0x5c
	save
	mov	%i5, %o0		! mapbuf_va
	call	i_cpr_read_maps
	ld	[%fp + FR_ARGX], %o1	! mapbuf_size
	restore

	! Switch to kernel trap
	call	prom_set_traptable
	mov	%o4, %o0

	!
	! special shortened version of longjmp
	! at this point there is no stack to run on
	! Don't need to flushw 
	!

	ld	[%o1 + L_PC], %i7
	ld	[%o1 + L_SP], %fp	! restore sp on old stack
	ret				! return 1
	restore	%g0, 1, %o0		! takes underflow, switches stk

.Cmd:
	.ascii  "release\0"
!
!	The following strings are for the convenience of the C routine
!	cpr_read_maps() which must reference only stack-relative data
!	or global data residing in THIS page.
!
	.global i_cpr_avail_string, i_cpr_trans_string
	.global i_cpr_too_small_msg, i_cpr_read_failed_msg
i_cpr_avail_string:
	.asciz	"available"
i_cpr_trans_string:
	.asciz	"translations"
i_cpr_too_small_msg:
	.ascii	"cpr_read_mappings: Buffer too small to save "
	.asciz	"prom mappings.  Please reboot.\n"
i_cpr_read_failed_msg:
	.ascii	"cpr_read_mappings: Cannot read \"translations\" "
	.asciz	"property.  Please reboot\n"
	.align	4
prom_free:
	save    %sp,-136,%sp
        sethi   %hi(.Cmd),%o0
        st      %g0,[%fp-24]
        add     %o0,%lo(.Cmd),%o3
        mov     %g0,%o2
        st      %g0,[%fp-20]
        mov     0,%o4
        mov     2,%o5
        mov     %i0,%l1
        std     %o4,[%fp-32]
        mov     %g0,%l0
        mov     %i1,%l3
        mov     %g0,%l2
        std     %l2,[%fp-8]
        add     %fp,-40,%o0
        std     %l0,[%fp-16]
        call    p1275_sparc_cif_handler
        std     %o2,[%fp-40]
        ret
        restore

	SET_SIZE(i_cpr_restore_hwregs)

!
! mrc is not defined in Neutron and Electron yet. This might be needed in
! the further to find out if the hardware is being supported.
!
	ENTRY(cpr_get_mcr)		! We might not need this
	retl
!	lda     [%g0]ASI_MOD, %o0	! No such ASI in spitfire
	nop
	SET_SIZE(cpr_get_mcr)
!
!	The following word (and label) is used to see whether this module
!	and cpr_mappings.o fit in one page.  The makefile places
!	cpr_mappings.o followed by this module at the front of cpr image.
!
	.global	i_cpr_end_jumpback
i_cpr_end_jumpback:
	.word	1
#endif
