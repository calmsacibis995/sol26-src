/*
 * Copyright (c) 1986-1992, Sun Microsystems, Inc.
 */

#pragma ident	"@(#)sun4u_srt0.s	1.14	96/04/02 SMI"

/*
 * srt0.s - standalone startup code
 */

#include <sys/asm_linkage.h>
#include <sys/cpu.h>
#include <sys/privregs.h>
#include <sys/stack.h>


#if defined(lint)

/*ARGSUSED*/
void
_start(void *a, ...)	/* void *a, void *b, void *c, void *cif_handler */
{}

char end[1], _end[1];	/* Defined by the linker */

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


! Each standalone program is responsible for its own stack. Our strategy
! is that each program which uses this runtime code creates a stack just
! below its relocation address. Previous windows may (and probably do)
! have frames allocated on the prior stack; leave them alone. Starting with
! this window, allocate our own stack frames for our windows. (Overflows
! or a window flush would then pass seamlessly from our stack to the old.)
! RESTRICTION: A program running at some relocation address must not exec
! another which will run at the very same address: the stacks would collide.
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
! Enter here for all booters loaded by a bootblk program or OBP.
! Careful, do not lose value of the SPARC v9 P1275 CIF handler in %o4
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
	call	bzero
	sub	%i2, %o0, %o1			! end - edata = size of bss

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
exitto(int (*entrypoint)())
{}

#else	/* lint */

	ENTRY(exitto)
	save	%sp, -SA(MINFRAME), %sp

	sethi	%hi(_local_p1275cif), %o0 ! pass the 1275 CIF handler to callee.
	ld	[%o0 + %lo(_local_p1275cif)], %o0
	clr	%o1			! boot passes no dvec
	set	bootops, %o2		! pass bootops vector to callee
	sethi	%hi(elfbootvec), %o3	! pass elf bootstrap vector
	ld	[%o3 + %lo(elfbootvec)], %o3
	jmpl	%i0, %o7		! call thru register to the standalone
	mov	%o0, %o4		! 1210378: Pass cif in both %o0 & %o4
	/*  there is no return from here */
	SET_SIZE(exitto)

#endif	/* lint */

#ifdef MPSAS

#if defined(lint)

/*
 * void
 * sas_command(cmdstr) - send a "user interface" cmd to SAS
 *
 * FOR SAS only ....
 */
/* ARGSUSED */
int
sas_command(char *cmdstr)
{ return (0); }

#else   /* lint */

#define	ST_SAS_COMMAND	330-256

	ENTRY_NP(sas_command)
	ta	ST_SAS_COMMAND
	nop
	retl
	nop
	SET_SIZE(sas_command)

#endif  /* lint */
#endif /* MPSAS */

/*
 * The interface for a 32-bit client program
 * calling the 64-bit romvec OBP.
 */

#if defined(lint)
#include <sys/promif.h>
#include <sys/prom_isa.h>

/* ARGSUSED */
int
client_handler(void *cif_handler, void *arg_array)
{ return (0); }

#else	/* lint */

	ENTRY(client_handler)
	save	%sp, -SA64(MINFRAME64), %sp	! 32 bit frame, 64 bit sized
	srl	%i0, 0, %i0			! zero extend handler addr
	srl	%i1, 0, %o0			! zero extend first argument.
	srl	%sp, 0, %sp			! zero extend sp
	rdpr	%pstate, %l1			! Get the present pstate value
	wrpr	%l1, PSTATE_AM, %pstate		! Set PSTATE_AM = 0
	jmpl	%i0, %o7			! Call cif handler
	sub	%sp, V9BIAS64, %sp		! delay; Now a 64 bit frame
	add	%sp, V9BIAS64, %sp		! back to a 32-bit frame
	rdpr	%pstate, %l1			! Get the present pstate value
	wrpr	%l1, PSTATE_AM, %pstate		! Set PSTATE_AM = 1
	ret					! Return result ...
	restore %o0, %g0, %o0			! delay; result in %o0

	SET_SIZE(client_handler)

#endif	/* lint */
