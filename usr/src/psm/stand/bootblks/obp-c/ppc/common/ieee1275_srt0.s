/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ieee1275_srt0.s 1.8     96/03/13 SMI"

/*
 * ieee_srt0.s - standalone startup code
 */

/*
 * Standalone gets control from OpenFirmware.
 * This program is loaded at address determined by configuration variable
 * load-base.
 * Interrupts are OFF, stack pointer (r1) is setup with atleast 32KB stack,
 * r5 contains client interface handler address. r6 and r7 contain argument
 * array address and array length.
 */

#include	<sys/asm_linkage.h>

#if defined(lint)

/*ARGSUSED*/
void
start(void *cif, char **argv, int argc)
{}

#else	/* lint */

!	start(?, ?, void * cif, char **argv, int argc)


	.globl	start
	.globl	argv
	.globl	argc
	.globl	edata
	.globl	end
	.globl	data_start

	ENTRY(start)
!
!	Save parameters first
!
	lis	%r9,cif_handler@ha
	stw	%r5,cif_handler@l(%r9)
	lis	%r10,argv@ha
	stw	%r6,argv@l(%r10)
	lis	%r11,argc@ha
	stw	%r7,argc@l(%r11)
!
!	Clear bss
!
	addis	%r11,0,0
	lis	%r9,edata@h
	ori	%r9,%r9,edata@l
	addi	%r9,%r9,3
	rlwinm	%r9,%r9,0,0,29
	lis	%r10,end@h
	ori	%r10,%r10,end@l
	addi	%r9,%r9,-4
clear_bss:
	addi %r9,%r9,4
	stwu	%r11,4(%r9)
	cmp	%r9,%r10
	ble	clear_bss
!
!	We are using Stack setup by OpenFirmware.
!

	lis	%r9,cif_handler@ha
	lwz	%r3,cif_handler@l(%r9)
	bl	main
	SET_SIZE(start)

	.globl	get_sp

	ENTRY(get_sp)
	mr	%r3,%r1
	blr-
	SET_SIZE(get_sp)

	ENTRY(call_handler)
	MINSTACK_SETUP
	mtctr	%r3
	mr	%r3,%r4
	bctrl-
	MINSTACK_RESTORE
	blr-
	SET_SIZE(call_handler)
#endif	/* lint */

#ifdef lint
/*
 *      make the memory at {addr, size} valid for instruction execution.
 *
 *	NOTE: it is assumed that cache blocks are no smaller than 32 bytes.
 */
/*ARGSUSED*/
void sync_instruction_memory(caddr_t addr, u_int len)
{
}
#else
	ENTRY(sync_instruction_memory)
	li	%r5,32  	! MINIMUM cache block size in bytes
	li	%r8,5		! corresponding shift

	subi	%r7,%r5,1	! cache block size in bytes - 1
	and	%r6,%r3,%r7	! align "addr" to beginning of cache block
	subf	%r3,%r6,%r3	! ... so that termination check is trivial
	add	%r4,%r4,%r6	! increase "len" to reach the end because
				! ... we're startign %r6 bytes before addr
	add	%r4,%r4,%r7	! round "len" up to cache block boundary
!!!	andc	%r4,%r4,%r7	! mask off low bit (not necessary because
				! the following shift throws them away)
	srw	%r4,%r4,%r8	! turn "len" into a loop count
	mtctr	%r4
1:
	dcbst	%r0,%r3 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r3 	! force out of instruction cache
	add	%r3,%r3,%r5
	bdnz	1b

	sync			! one sync for the last icbi
	isync
	blr  
	SET_SIZE(sync_instruction_memory)

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
u_int
swap_int(u_int *addr)
{
        return (0);
}
#else   /* lint */
        .globl  swap_int

        ENTRY(swap_int)
        lwbrx   %r3,0,%r3
        blr-
        SET_SIZE(swap_int)
#endif /* lint */

	.data
data_start:
!	this cif_handler is a local variable for this file.
cif_handler:
	.long	0		! Client Interface Handler
argv:
	.long	0		! Arguments for Client Program
argc:
	.long	0		! Number of arguments for Client Program


! Note section

	.section	".note"
	.align		2
notes:
	.long		name_end - name_start
	.long		descr_end - descr_start
	.long		0x1275
name_start:
	.asciz		"bootblks"
name_end:

! configuration variables that SHOULD be used by OF
descr_start:
	.long		0		! real - mode
	.long		0xfe000000	! virt-base
	.long		0x1000000	! virt-size
descr_end:

#endif	/* lint */
