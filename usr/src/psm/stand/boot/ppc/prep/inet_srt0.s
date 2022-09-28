/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)inet_srt0.s	1.7	96/03/14 SMI"

/*
 * inet_srt0.s - inet boot standalone startup code
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
_start(void *cif)
{}

int
get_sp(void)
{ return (0); }

#else /* lint */

!	_start(?, ?, void * cif, char **argv, int argc)


	.globl	_start
	.globl	argv
	.globl	argc
	.globl	netboot
	.globl	edata
	.globl	end
	.globl	data_start

	ENTRY(_start)
!
!	Save parameters first
!
	lis	%r9,cif_handler@ha
	stw	%r5,cif_handler@l(%r9)
	lis	%r10,argv@ha
	stw	%r6,argv@l(%r10)
	lis	%r11,argc@ha
	stw	%r6,argc@l(%r11)
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
!
!	Should not return
!
	b	prom_exit_to_mon
	SET_SIZE(_start)

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

	.data

data_start:
!	this cif_handler is a local variable for this file.
cif_handler:
	.long	0		! Client Interface Handler
argv:
	.long	0		! Arguments for Client Program
argc:
	.long	0		! Number of arguments for Client Program
netboot:
	.long	1		! network boot = true


! Note section

	.section	".note"
	.align		2
notes:
	.long		name_end - name_start
	.long		descr_end - descr_start
	.long		0x1275
name_start:
	.asciz		"inetboot"
name_end:

descr_start:
	.long		0		! real - mode
	.long		0xfe000000	! virt-base
	.long		0x1000000	! virt-size
descr_end:
#endif	/* lint */
