/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)boot_elf.s	1.20	96/06/07 SMI"

#include	"machdep.h"
#include	"profile.h"
#if	defined(lint)
#include	<sys/types.h>
#include	"_rtld.h"
#include	"_elf.h"
#else
#include	<sys/stack.h>

	.file	"boot_elf.s"
	.text
#endif

/*
 * We got here because the initial call to a function resolved to a procedure
 * linkage table entry.  That entry did a branch to the first PLT entry, which
 * in turn did a call to elf_rtbndr (refer elf_plt_init()).
 *
 * the code sequence that got us here was:
 *
 * PLT entry for foo():
 *	li	%r11, ptlndx * 4
 *	b	.plt0			! aka .pltresolve
 *
 * the plt entry is rewritten either:
 *
 * PLT entry for foo():
 *	bl	<real address>
 *	b	.pltresolve
 *
 * or:
 *	li	%r11, pltndx * 4
 *	b	.pltcall
 */

#if	defined(lint)

extern unsigned long	elf_bndr(unsigned long, Rt_map *, caddr_t);

/*
 * pc == &plt[index]
 * reloc == index * 4
 * lmp == private map for this loadable object
 * from == return address of the caller of plt[index]
 */
void
elf_rtbndr(unsigned long reloc, Rt_map * lmp, caddr_t from)
{
	(void) elf_bndr(reloc, lmp, from);
}


#else

/* (8 integer arguments + 1 condition register) * sizeof(int) */
#define	ARG_SAVE_SIZE	((8 + 1) * 4)

/*
 * The code below assumes that the run-time loader never does any floating
 * point operations.  If it *did* do floating point then all the floating
 * point argument registers would need to be saved and restored as
 * well as the integer registers.
 */
	.weak	_elf_rtbndr		! keep dbx happy as it likes to
	_elf_rtbndr = elf_rtbndr	! rummage around for our symbols

	.global	elf_rtbndr
	.type   elf_rtbndr, @function
	.align	2

elf_rtbndr:
	mflr	%r0			! save lr
	stw	%r0, 4(%r1)		!
	stwu	%r1, -SA(ARG_SAVE_SIZE + MINFRAME)(%r1)	! make room
	stw	%r3, 8(%r1)		! and save all argument register
	stw	%r4, 12(%r1)		!
	stw	%r5, 16(%r1)		!
	stw	%r6, 20(%r1)		!
	stw	%r7, 24(%r1)		!
	stw	%r8, 28(%r1)		!
	stw	%r9, 32(%r1)		!
	stw	%r10, 36(%r1)		!
	mfcr	%r3			!
	stw	%r3, 40(%r1)		!
	mr	%r3, %r11		! reloc
	mr	%r4, %r12		! lmp
	mr	%r5, %r0		! from
	bl	elf_bndr		! invoke elf run-time binder
	mtctr	%r3			! for bctr later
	lwz	%r3, 8(%r1)		! restore arguments
	lwz	%r4, 12(%r1)		!
	lwz	%r5, 16(%r1)		!
	lwz	%r6, 20(%r1)		!
	lwz	%r7, 24(%r1)		!
	lwz	%r8, 28(%r1)		!
	lwz	%r9, 32(%r1)		!
	lwz	%r10, 36(%r1)		!
	lwz	%r0, 40(%r1)		! and varargs/float indicator
	mtcrf	0xff,%r0		!
	addi	%r1, %r1, SA(ARG_SAVE_SIZE + MINFRAME)	! restore stack
	lwz	%r0, 4(%r1)		! restore lr
	mtlr	%r0			!
	bctr				! go to actual function we wanted
	.size 	elf_rtbndr, . - elf_rtbndr

#endif


/*
 * Update a relocation offset, the value is added to any original value
 * in the relocation offset.
 */

#if	defined(lint)

void
elf_reloc_write(unsigned long * offset, unsigned long value, int textrel)
{
	*offset = *offset + value;
	/* LINTED */
	if (textrel)			/* CSTYLED */
		;			/* iflush instruction */
}

#else
	.global	elf_reloc_write
	.type	elf_reloc_write, @function
	.align	2

elf_reloc_write:
	lwz	%r6, 0(%r3)		! *offset
	add	%r6, %r6, %r4		! *offset + value
	stw	%r6, 0(%r3)		! *offset = *offset + value
	cmpi	%r5, 0			! was it in the text?
	beqlr+				! no, just return
	icbi	%r0, %r3		! flush the instruction cache
	blr				! all done
	.size	elf_reloc_write, . - elf_reloc_write

#endif

#if defined(lint)

/*
 * DATA CACHE BLOCK STORE
 *
 * If data pointed to is in a data cache initiate it's writing
 * to memory.
 */
/* ARGSUSED 0 */
void
_dcbst(unsigned long * addr)
{
}	

/*
 * Synchronize
 *
 * Make sure that all subsequent instructions have been completed before
 * execting any more instructions.
 */
void
_sync(void)
{
}

/*
 * Instruction Cache Block Invalidate
 *
 * If the instruction pointed to by '*addr' is in a cache the cache
 * is invalidated.
 */
/* ARGSUSED 0 */
void
_icbi(unsigned long * addr)
{
}

/*
 * Instruction Synchronize
 *
 * This fuction waits for all previous instructions to complete and
 * then discards any fetched instructions, causing subsequent instructions to
 * to be fetched (or refeteched) from memory and to execute in the
 * context established by the previous instructions.  This instruction
 * has no effect on other processors or their caches.
 */
void
_isync(void)
{
}

#else
	.global	_dcbst
	.type	_dcbst, @function
	.align	2

_dcbst:
	dcbst	%r0,%r3
	blr
	.size	_dcbst, . - _dcbst

	.global	_sync
	.type	_sync, @function
	.align	2

_sync:
	sync
	blr
	.size	_sync, . - _sync

	.global	_icbi
	.type	_icbi, @function
	.align	2

_icbi:
	icbi	%r0, %r3
	blr
	.size	_icbi, . - _icbi

	.global	_isync
	.type	_isync, @function
	.align	2

_isync:
	isync
	blr
	.size	_isync, . - _isync
#endif

#if defined(PROF)

#if defined(lint)

void
elf_plt_cg(int ndx, caddr_t from)
{
	(void) plt_cg_interp(ndx, from, (caddr_t) 0);
}

#else /* ! defined(lint) */

	.globl	elf_plt_cg
	.type	elf_plt_cg, @function
	.align	2

elf_plt_cg:
	mflr	%r0			! save lr
	stw	%r0, 4(%r1)		!
	stwu	%r1, -SA(ARG_SAVE_SIZE + MINFRAME)(%r1)	! make room
	stw	%r3, 8(%r1)		! and save all argument registers
	stw	%r4, 12(%r1)		!
	stw	%r5, 16(%r1)		!
	stw	%r6, 20(%r1)		!
	stw	%r7, 24(%r1)		!
	stw	%r8, 28(%r1)		!
	stw	%r9, 32(%r1)		!
	stw	%r10, 36(%r1)		!
	mfcr	%r3			! save condition register
	stw	%r3, 40(%r1)		!
	mr	%r3, %r11		! ndx
	mr	%r4, %r0		! from
	li	%r5, 0			! 0
	bl	plt_cg_interp		! invoke elf run-time binder
	mtctr	%r3			! for bctr later
	lwz	%r3, 8(%r1)		! restore arguments
	lwz	%r4, 12(%r1)		!
	lwz	%r5, 16(%r1)		!
	lwz	%r6, 20(%r1)		!
	lwz	%r7, 24(%r1)		!
	lwz	%r8, 28(%r1)		!
	lwz	%r9, 32(%r1)		!
	lwz	%r10, 36(%r1)		!
	lwz	%r0, 40(%r1)		! and varargs/float indicator
	mtcrf	0xff,%r0		!
	addi	%r1, %r1, SA(ARG_SAVE_SIZE + MINFRAME)	! restore stack
	lwz	%r0, 4(%r1)		! restore lr
	mtlr	%r0			!
	bctr				! go to actual function we wanted
	.size 	elf_plt_cg, . - elf_plt_cg

#endif /* ! defined(lint) */

#endif /* defined(PROF) */
