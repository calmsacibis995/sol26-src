/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)memscrub_asm.s 1.14	96/06/12 SMI"

/*
 * General machine architecture & implementation specific
 * assembly language routines.
 */
#if defined(lint)
#include <sys/types.h>
#include <sys/machsystm.h>
#include <sys/t_lock.h>
#else   /* lint */
#include "assym.s"
#endif  /* lint */

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
void
memscrub_read(caddr_t vaddr, u_int blks)
{}

#else	/* lint */

	!
	! void	memscrub_read(caddr_t src, u_int blks)
	!
	! Since the scrubber is a kernel thread we really don't
	! have to save the FP registers.  But, for now we're
	! going to be safe and continue to save them if needed.
	!

	.seg ".text"
	.align	4

	ENTRY(memscrub_read)
	save	%sp, -424, %sp		! make space for fpregs on stack
	srl	%i1, 0, %i1		! clean up the upper word of blk count
	rd	%fprs, %o1		! check the status of fp
	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o1
	bz	1f
	wr	%g0, FPRS_FEF, %fprs	! enable fp

	! save in-use fpregs on stack
	membar	#Sync			! XXX
	add	%fp, -257, %o2
	and	%o2, -64, %o2
	stda	%d0, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d16, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d32, [%o2]ASI_BLK_P
	add	%o2, 64, %o2
	stda	%d48, [%o2]ASI_BLK_P
	membar	#Sync			! XXX

1:
	ldda	[%i0]ASI_BLK_P, %d0
	add	%i0, 64, %i0
	ldda	[%i0]ASI_BLK_P, %d16
	add	%i0, 64, %i0
	ldda	[%i0]ASI_BLK_P, %d32
	add	%i0, 64, %i0
	ldda	[%i0]ASI_BLK_P, %d48
	dec	%i1
	brnz,a	%i1, 1b
	add	%i0, 64, %i0

	btst	(FPRS_DU|FPRS_DL|FPRS_FEF), %o1
	bz,a	2f
	wr	%o1, 0, %fprs		! restore fprs (dly)

	! restore fpregs from stack
	membar	#Sync			! XXX
	add	%fp, -257, %o2
	and	%o2, -64, %o2
	ldda	[%o2]ASI_BLK_P, %d0
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d16
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d32
	add	%o2, 64, %o2
	ldda	[%o2]ASI_BLK_P, %d48
	membar	#Sync			! XXX

	wr	%o1, 0, %fprs		! restore fprs

2:
	ret
	restore
	SET_SIZE(memscrub_read)

#endif	/* lint */
