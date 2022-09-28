/*
 * Copyright (c) 1986 - 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)srt0.s	1.11	96/09/19 SMI"

/*
 * srt0.s - cprbooter startup code
 */

#include <sys/asm_linkage.h>
#include <sys/cpu.h>
#include <sys/privregs.h>
#include <sys/stack.h>


#if defined(lint)

char _end[1];

/*ARGSUSED*/
void
_start(void *a, void *b, void *c, void *cif_handler)
{}

#else
	.seg	".text"
	.align	8
	.global	end
	.global	edata
	.global	main

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
.ebootstack:			! end (top) of boot stack

/*
 * The following variables are more or less machine-independent
 * (or are set outside of fiximp).
 */

	.seg	".text"
	.align	8
	.global	prom_exit_to_mon
	.type	prom_exit_to_mon, #function


!
! Careful: don't touch %o4 until the save, since it contains the
! address of the IEEE 1275 SPARC v9 CIF handler (linkage to the prom).
!
!
! We cannot write to any symbols until we are relocated.
! Note that with the advent of 5.x boot, we no longer have to
! relocate ourselves, but this code is kept around cuz we *know*
! someone would scream if we did the obvious.
!
!
! Enter here for all booters loaded by a bootblk program.
! Careful, do not loose value of the SPARC v9 P1275 CIF handler in %o4
! Setup temporary 32 bit stack at _start.
!
! NB: Until the common startup code, AM may not be set.
!

	ENTRY(_start)
	set	_start, %o1
	save	%o1, -SA64(MINFRAME64), %sp	! %i4: 1275 sparcv9 CIF handler
	!
	! zero the bss
	!
	sethi	%hi(edata), %o0			! Beginning of bss
	or	%o0, %lo(edata), %o0
	set	end, %i2
	call	cpr_bzero
	sub	%i2, %o0, %o1			! end - edata = size of bss


!
! All booters end up here...
!

9:
	/*
	 *  Use our own 32 bit stack now. But, zero it first (do we have to?)
	 */
	set	.ebootstack, %o0
	set	STACK_SIZE, %o1
	sub	%o0, %o1, %o1
1:	dec	4, %o0
	st	%g0, [%o0]
	cmp	%o0, %o1
	bne	1b
	nop

	set	.ebootstack, %o0
	and	%o0, ~(STACK_ALIGN64-1), %o0
	sub	%o0, SA64(MINFRAME64), %sp

	/*
	 * Set the psr into a known state:
	 * Set AM, supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, PSTATE_PEF+PSTATE_AM+PSTATE_PRIV+PSTATE_IE, %pstate

	sethi	%hi(_local_p1275cif), %o1
	st	%i4, [%o1 + %lo(_local_p1275cif)]
	call	main			! main(prom-cookie) (main(0) - sunmon)
	mov	%i4, %o0		! SPARCV9/CIF 

	call	prom_exit_to_mon	! can't happen .. :-)
	nop
	SET_SIZE(_start)

#endif	/* lint */

/*
 *	exitto is called from main() and It jumps directly to the
 *	just-loaded standalone.  There is NO RETURN from exitto().
 */

#if defined(lint)

/* ARGSUSED */
void
exitto(int (*entrypoint)(), char *loadbase)
{}

#else	/* lint */

	ENTRY(exitto)
	save	%sp, -SA(MINFRAME), %sp


	sethi	%hi(_local_p1275cif), %o0 ! pass the 1275 CIF handler to callee.
	ld	[%o0 + %lo(_local_p1275cif)], %o0
	set	cpr_statefile, %o1	! pass state file name to cprboot
	set	cpr_filesystem, %o2	! pass file system path to cprboot
	jmpl	%i0, %o7		! call thru register to the standalone
	mov	%i1, %o3		! pass loadbase to cprboot
	nop
	unimp	0
	/*  there is no return from here */
	SET_SIZE(exitto)

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
