/*
 * Copyright (c) 1986 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cprboot_srt0.s	1.18	96/09/19 SMI"

/*
 * srt0.s - standalone startup code
 * Generate the code with a fake a.out header if INETBOOT is defined.
 * inetboot is loaded directly by the PROM, other booters are loaded via
 * a bootblk and don't need the fake a.out header.
 */

#include <sys/asm_linkage.h>
#include <sys/cpu.h>
#include <sys/privregs.h>
#include <sys/stack.h>
#include <sys/spitasi.h>
#include <sys/machparam.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <vm/hat_sfmmu.h>

#if defined(lint)

char _etext[1];
char _end[1];

extern int main(void *, char *, char *, char *);
	
/*ARGSUSED*/
void
_start(void *cif_handler, char *pathname, char *bpath, char *base)
{}

#else
	.seg	".text"
	.align	8
	.global	main
	.global	_start

/*
 * The following variables are machine-dependent and are set in fiximp.
 * Space is allocated there.
 */
	.seg	".data"
	.align	8

_local_p1275cif:
	.word	0


#define STACK_SIZE	0x14000
	.skip	STACK_SIZE
.estack:			! end (top) of boot stack

/*
 * The following variables are more or less machine-independent
 * (or are set outside of fiximp).
 */

	.seg	".text"
	.align	8
	.global	prom_exit_to_mon
	.type	prom_exit_to_mon, #function


!
! Careful, do not loose value of the SPARC v9 P1275 CIF handler in %o4
! Setup temporary 32 bit stack at _start.
!
! NB: Until the common startup code, AM may not be set.
!

	ENTRY(_start)

	sethi	%hi(_local_p1275cif), %o4
	st	%o0, [%o4 + %lo(_local_p1275cif)]
#if 1
	rdpr	%ver, %g1
	and	%g1, VER_MAXWIN, %g1
	wrpr	%g0, %g1, %cleanwin
	wrpr	%g0, %g0, %canrestore
	wrpr	%g0, %g0, %otherwin
	dec	%g1
	wrpr	%g0, %g1, %cansave
#endif

	set	_start, %o4
	save	%o4, -SA64(MINFRAME64), %sp	! %i4: 1275 sparcv9 CIF handler

!
! All booters end up here...
!

9:
	/*
	 *  Use our own 32 bit stack now. But, zero it first (do we have to?)
	 */
	set	.estack, %o4
	set	STACK_SIZE, %o3
	sub	%o4, %o3, %o3
1:	dec	4, %o4
	st	%g0, [%o4]
	cmp	%o4, %o3
	bne	1b
	nop

	set	.estack, %o4
	and	%o4, ~(STACK_ALIGN64-1), %o4
	sub	%o4, SA64(MINFRAME64), %sp

	/*
	 * Set the psr into a known state:
	 * Set AM, supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, PSTATE_PEF+PSTATE_AM+PSTATE_PRIV+PSTATE_IE, %pstate

	mov	%i3, %o3		! loadbase
	mov	%i2, %o2		! bpath
	mov	%i1, %o1		! pathname
	call	main			! main(prom-cookie) (main(0) - sunmon)
	mov	%i0, %o0		! SPARCV9/CIF 

	call	prom_exit_to_mon	! can't happen .. :-)
	nop
	SET_SIZE(_start)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
/* Register usage:
 *      %i0     prom cookie
 *      %i1     start of cprboot text pages
 *      %i2     end of cprboot text pages
 *      %i3     start of cprboot data pages
 *      %i4     end of cprboot data pages
 *      %o0-%o5 arguments
 * Any change to this register assignment requires
 * changes to cpr_resume_setup.s
 */
void
exit_to_kernel(char *cpr_thread, char *qsavp,
	int mmu_ctx_pri, int mmu_ctx_sec, char *tra_va,
	char *mapbuf_va, u_int mapbuf_size) 
{}

#else	/* lint */

	.seg	".data"
	.global kernelentry, starttext, endtext, startdata, enddata, newstack

	ENTRY(exit_to_kernel)
	rdpr	%ver, %g1
	and	%g1, VER_MAXWIN, %g1
	wrpr	%g0, %g1, %cleanwin
	wrpr	%g0, %g0, %canrestore
	wrpr	%g0, %g0, %otherwin
	dec	%g1
	wrpr	%g0, %g1, %cansave
	sethi	%hi(_local_p1275cif), %l1
	ld	[%l1 + %lo(_local_p1275cif)], %i0

	!
	! Use l2 as a temporary stack pointer.  We need the minimum
	! stack frame plus additional space for 2 fullword args (2*4)
	! because arguments beyond the 6th which must be passed on
	! the stack.
	!
	
	sethi	%hi(newstack), %l2
	ld	[%l2 + %lo(newstack)], %l2
	sub	%l2, SA((MINFRAME)+(2*4)), %l2

	! Transfer overflow arguments from current stack to newly
	! allocated stack.  The first stack argument location is the
	! field fr_argx in struct frame from uts/sparc/sys/frame.h
	!
	! XXX - Should use genassym to include the following symbol
	! in assym.s

#define FR_ARGX 0x5c
	ld	[%sp + FR_ARGX], %l1
	st	%l1, [%l2 + FR_ARGX]

	mov	%l2, %sp		! make new stack the "real" stack

	sethi	%hi(starttext), %l1
	ld	[%l1 + %lo(starttext)], %i1

	sethi	%hi(endtext), %l2
	ld	[%l2 + %lo(endtext)], %i2

	sethi	%hi(startdata), %l1
	ld	[%l1 + %lo(startdata)], %i3

	sethi	%hi(enddata), %l2
	ld	[%l2 + %lo(enddata)], %i4

	sethi	%hi(kernelentry), %l1
	ld	[%l1 + %lo(kernelentry)], %l3

	jmpl	%l3, %g0

	nop
	
	/*  there is no return from here */
	unimp	0
	SET_SIZE(exit_to_kernel)

#endif	/* lint */



/*
 * The interface for a 32-bit client program
 * calling the 64-bit romvec OBP.
 */

#if defined(lint)
#include <sys/promif.h>

/* ARGSUSED */
int
client_handler(void *cif_handler, void *arg_array)
{ return (0); }

#else	/* lint */

	ENTRY(client_handler)
	save	%sp, -SA64(MINFRAME64), %sp	! 32 bit frame, 64 bit sized
	srl	%i0, 0, %i0			! zero extend handler addr
	srl	%i1, 0, %o0			! zero extend first argument.
	srl     %sp, 0, %sp                     ! zero extend sp
	rdpr    %pstate, %l1                    ! Get the present pstate value
	wrpr    %l1, PSTATE_AM, %pstate         ! Set PSTATE_AM = 0
	jmpl    %i0, %o7                        ! Call cif handler
	sub     %sp, V9BIAS64, %sp              ! delay; Now a 64 bit frame
	add     %sp, V9BIAS64, %sp              ! back to a 32-bit frame
	rdpr    %pstate, %l1                    ! Get the present pstate value
	wrpr    %l1, PSTATE_AM, %pstate         ! Set PSTATE_AM = 1
	ret                                     ! Return result ...
	restore %o0, %g0, %o0			! delay; result in %o0

	SET_SIZE(client_handler)

#endif	/* lint */

#if defined (lint)
/* ARGSUSED */
void cpr_dtlb_wr_entry(int index, caddr_t tag, int ctx, tte_t *tte)
{
}

/* ARGSUSED */
void cpr_itlb_wr_entry(int index, caddr_t tag, int ctx, tte_t *tte)
{
}

#else	/* lint */

	ENTRY(cpr_dtlb_wr_entry)
	srl	%o1, MMU_PAGESHIFT, %o1
	sll	%o1, MMU_PAGESHIFT, %o1
	sllx    %o0, 3, %o0
	or	%o1, %o2, %o1
	ldx	[%o3], %o3
	set	MMU_TAG_ACCESS, %o5
	stxa	%o1, [%o5]ASI_DMMU
	stxa	%o3, [%o0]ASI_DTLB_ACCESS
	membar	#Sync
	retl
	nop
	SET_SIZE(cpr_dtlb_wr_entry)

	ENTRY(cpr_itlb_wr_entry)
	srl     %o1, MMU_PAGESHIFT, %o1
	sll     %o1, MMU_PAGESHIFT, %o1
	sllx	%o0, 3, %o0	
	or      %o1, %o2, %o1
	ldx	[%o3], %o3
	set     MMU_TAG_ACCESS, %o5
	stxa    %o1, [%o5]ASI_IMMU
	stxa	%o3, [%o0]ASI_ITLB_ACCESS
	retl
	nop
	SET_SIZE(cpr_itlb_wr_entry)
		
#endif	/* lint */
