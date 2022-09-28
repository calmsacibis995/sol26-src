!
! Copyright(c) 1994, by Sun Microsystems, Inc.
! All rights reserved.
!
	.ident	"@(#)ppc_assm.s 1.3	96/05/17 SMI"
	.file	"ppc_assm.s"

	.data
	.align	2
	.globl	prb_callinfo
prb_callinfo:
	.long	0		! offset
	.long	0		! shift
	.long	0x03fffffc	! mask

	.text
	.align	2
	.globl	prb_chain_entry
	.globl	prb_chain_down
	.globl	prb_chain_next
	.globl	prb_chain_end
prb_chain_entry:
	mflr	%r0
	stw	%r0, 4(%r1)
	stwu	%r1, -0x20(%r1)
	stw	%r3, 8(%r1)
	stw	%r4, 12(%r1)
	stw	%r5, 16(%r1)
prb_chain_down:
chain_down:
	bl	chain_down
	lwz	%r3, 8(%r1)
	lwz	%r4, 12(%r1)
	lwz	%r5, 16(%r1)
prb_chain_next:
chain_next:
	bl	chain_next
	addi	%r1, %r1, 0x20
	lwz	%r0, 4(%r1)
	mtlr	%r0
	blr
	.type	prb_chain_entry, @function
	.size	prb_chain_entry, . - prb_chain_entry
prb_chain_end:
	nop
