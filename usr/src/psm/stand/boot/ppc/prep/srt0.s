/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)srt0.s	1.8	96/03/13 SMI"

/*
 * srt0.s - standalone startup code
 */

/*
 * Standalone gets control from the bootblk code.
 * Interrupts are OFF, stack pointer (r1) is setup with atleast 32KB stack,
 * r3 contains client interface handler address.
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

#else	/* lint */


!	_start(void * cif)


	.globl	_start
	.globl	netboot
	.globl	edata
	.globl	end
	.globl	data_start

	ENTRY(_start)
!
!	Save parameters first
!
	lis	%r9,cif_handler@ha
	stw	%r3,cif_handler@l(%r9)
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
	.align		2

data_start:
!	this cif_handler is a local variable for this file.
cif_handler:
	.long	0		! Client Interface Handler
netboot:
	.long	0		! network boot = false

#endif	/* lint */
