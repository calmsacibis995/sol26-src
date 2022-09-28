/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.		*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T		*/
/*	  All Rights Reserved						*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF			*/
/*	UNIX System Laboratories, Inc.					*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.		*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation			*/
/*	  All Rights Reserved						*/

/*	This Module contains Proprietary Information of Microsoft 	*/
/*	Corporation and should be treated as Confidential.		*/

/*
 *	Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)mpcore.s	1.16	95/09/09 SMI"

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include <sys/regset.h>

#if !(defined(lint) || defined(__lint))
#include <sys/segment.h>
#include <assym.s>
#endif

/*
 *	Our assumptions:
 *		- We are running in real mode.
 *		- Interrupts are disabled.
 *		- The GDT,IDT ,ktss and page directory has been built for us
 *
 *	Our actions:
 *		- We start using our GDT by loading correct values in the
 *		  selector registers (cs=KCSSEL, ds=es=ss=KDSSEL, fs=KFSSEL,
 *		  gs=KGSSEL).
 *		- We change over to using our IDT.
 *		- We load the default LDT into the hardware LDT register.
 *		- We load the default TSS into the hardware task register.
 *		- call ap_mlsetup(void) 
 *		- call mp_startup(void) indirectly through the T_PC
 */

#if defined(lint) || defined(__lint)

void
real_mode_start(void)
{}

#else	/* lint */

	ENTRY_NP(real_mode_start)
	cli
	data16
	movw	%cs,%eax
	movw	%eax,%ds		/load cs into ds
	movw	%eax,%ss		/and into ss
	data16
	movl	$0xffc,%esp		/ Helps in debugging by giving us the
					/ fault addr. Remember to patch a hlt
					/ (0xf4) at cmntrap to get a good stk.
	data16
	addr16
	lgdt	%cs:GDTROFF

	data16
	addr16
	lidt	%cs:IDTROFF



	data16
	addr16
	movl	%cs:CR4OFF,%eax
	data16
	andl	%eax, %eax
	data16
	addr16
	je	no_cr4

	data16
	movl	%eax, %ecx

	data16
	.byte 	0x0f, 0x20, 0xe0	/ movl	%cr4, %eax

	data16
	orl	%ecx, %eax
	
	data16
	.byte 	0x0f, 0x22, 0xe0	/ movl	%eax, %cr4

no_cr4:


	data16
	addr16
	movl	%cs:CR3OFF,%eax
	addr16
	movl	%eax,%cr3

	movl	%cr0,%eax
	data16
	orl	$[CR0_PG|CR0_PE|CR0_WP], %eax 	/ set both PROT and PAGEING
						/ and Write Protect
	movl	%eax,%cr0
	jmp	pestart
pestart:
	data16
	pushl	$KCSSEL

	data16
	pushl	$kernel_cs_code

	data16
	lret
	.globl real_mode_end
real_mode_end:
	nop

	.globl	kernel_cs_code
kernel_cs_code:
	/ at this point we are with kernel's cs and proper eip.
	/ we will be executing not from the copy in real mode platter,
	/ but from the original code where boot loaded us.
	/ By this time GDT and IDT are loaded as is cr3. 
	movw	$KFSSEL,%eax
	movw	%eax,%fs
	movw	$KGSSEL,%eax
	movw	%eax,%gs
	movw	$KDSSEL,%eax
	movw	%eax,%ds
	movw	%eax,%es
	movl	%gs:CPU_TSS,%esi
	movw	%eax,%ss
	movl	TSS_ESP0(%esi),%esp
	movw	$UTSSSEL,%ax
	ltr	%ax
	movw	$LDTSEL, %ax
	lldt	%ax
	movl	%cr0,%edx
	andl    $-1![CR0_TS|CR0_EM],%edx	/ clear emulate math chip bit
	movw	cputype, %ax
	andw	$CPU_ARCH, %ax
	cmpw	$I86_386_ARCH, %ax
	je	cpu_386

	orl     $[CR0_MP|CR0_NE],%edx
	jmp	cont

	/
	/ For AT386 support we need an interrupt handler to process FP
	/ exceptions. The interrupt comes at IRQ 13. (See configure())
	/
cpu_386:
	orl     $CR0_MP,%edx
cont:
	movl    %edx,%cr0               / set machine status word
	call	*ap_mlsetup
	movl	%gs:CPU_THREAD, %eax	/ get thread ptr
	call	*T_PC(%eax)		/ call mp_startup
	/ not reached
	int	$20

	SET_SIZE(real_mode_start)

#endif /* lint */

#if defined(lint) || defined(__lint)

void
halt_kadb(void)
{}

#else	/* lint */

	ENTRY_NP(halt_kadb)
	hlt
	ret
	SET_SIZE(halt_kadb)

#endif /* lint */


#if defined(lint) || defined(__lint)

void
return_instr(void)
{}

#else	/* lint */

	ENTRY_NP(return_instr)
	ret
	SET_SIZE(return_instr)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
send_dirint(int cpuix, int int_level)
{}

#else	/* lint */

	ENTRY_NP(send_dirint)
	jmp	*send_dirintf		/ jump indirect to the send_dirint
					/ function. Set to return if machine
					/ type module doesnt redirect it.
	SET_SIZE(send_dirint)

#endif /* lint */
