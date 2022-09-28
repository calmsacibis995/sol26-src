
#ifdef XXX_later
#ident	"#(@)kprof.s 1.0	5/24/91 Sun Soft Inc."
#ifdef GPROF
!	@(#)kprof.s 1.4 89/05/05 SMI
!	Copyright (c) 1988 by Sun Microsystems, Inc.

	.section 	".text"
	.align	4

#include <sys/psw.h>
#include <sys/intreg.h>
#include <sys/machthread.h>
#include "assym.s"

	.section	".data"
	.global	kpri, kticks, kprimin4

kpri:
	.word 0, 0, 0, 0, 0, 0, 0, 0
	.word 0, 0, 0, 0, 0, 0, 0, 0
kticks:
	.word 0
kprimin4:
	.word 0*4			! Lowest prio to trace PC of, times 4.

	.section	".text"
	.global	kprof
/*
 * Kernel profiling interrupt, called once it is determined
 * a valid clock interrupt at level 14 is pending.
 *
 * Use the local registers in the trap window and return.
 * entry state:	%l0 = psr
 *		%l1 = pc
 *		%l2 = npc
 */

kprof:
/*
 *	b 	kdone
 *	nop
 */
!	these are nice measurements, time at each priority, but nobdy 
!	ever uses them
!	and	%l0, PSR_PIL, %l4	! priority(times 4) into %l4
!	srl	%l4, PSR_PIL_BIT-2, %l4
!	set	kpri, %l6		! address of priority buckets
!	ld	[%l6 + %l4], %l7	! increment priority bucket
!	inc	%l7
!	st	%l7, [%l6 + %l4]

!	sethi	%hi(kprimin4), %l7	! minimum priority to profile
!	lduh	[%l7 + %lo(kprimin4)], %l7
!	cmp	%l4, %l7		! if current priority < min*4, return
!	bl	kdone
	btst	PSR_PS, %l0		! check pS bit
	bz	kdone			! not in supervisor mode, return
	nop
	CPU_ADDR(%l7, %l6)		! find current cpu
	ld	[%l7 + CPU_PROFILING], %l6	! see if there is a cpu struct
	tst	%l6
	be	kdone
	nop
	ld	[%l6 + PROFILING], %l7
	tst	%l7			! find out if profiling is turned on
	bne	kdone			
	nop

	ld	[%l6 + MODULE_HIGHPC], %l7
	cmp	%l1, %l7
	bge	kdone
        nop
        ld      [%l6 + MODULE_LOWPC], %l7
        cmp     %l1, %l7
        bl     KERNEL_ADDRESS
        nop

! a module routine's base address to subtract to get an index into the froms
! array is: index = base address-(kernel_lowpc + (module_lowpc - kernel_highpc))

        ld      [%l6 + KERNEL_HIGHPC], %l5
        sub     %l7, %l5, %l4
        ld      [%l6 + KERNEL_LOWPC], %l7
        add     %l7, %l4, %l5
	sub	%l1, %l5, %l3
        b       HAVE_INDEX
        nop

KERNEL_ADDRESS:
	ld	[%l6 + KERNEL_HIGHPC], %l7
        cmp     %l1, %l7
	bge     kdone
	nop
        ld      [%l6 + KERNEL_LOWPC], %l7
        cmp     %l1, %l7
        bl     kdone
        nop
	sub	%l1, %l7, %l3

HAVE_INDEX:
	ld	[%l6 + PROF_KCOUNT], %l4	! get kcount array address
	srl	%l3, 2, %l3		! divide offset by 2
	sll	%l3, 1, %l3		! this only divides the offset by 2
	lduh	[%l4 + %l3], %l7	! increment a bucket
	inc	%l7
	sth	%l7, [%l4 + %l3]
kdone:
	!
	! return from trap
	!
	mov	%l0, %psr		! reinstall PSR_CC
	jmp	%l1			! return from trap
	rett	%l2
#endif  /* GPROF */
#endif
