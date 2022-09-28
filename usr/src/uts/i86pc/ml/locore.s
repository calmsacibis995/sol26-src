/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved					*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation		*/
/*	  All Rights Reserved					*/

/*	This Module contains Proprietary Information of Microsoft */
/*	Corporation and should be treated as Confidential.	*/

/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)locore.s	1.119	96/10/28 SMI"

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include <sys/regset.h>
#include <sys/psw.h>

#if defined(lint) || defined(__lint)

#include <sys/types.h>
#include <sys/thread.h>

#else	/* lint */

#include <sys/segment.h>
#include <sys/pcb.h>
#include <sys/trap.h>
#include <assym.s>

/	XXX
/	Question: What is KADB and BOOT assuming about the contents of
/		  the segment registers?  When can GDT[1-4] be blasted?
/
/	XXX - arguments? (e.g., romp, bootops, dvec)
/
/	Our assumptions:
/		- We are running in protected-paged mode.
/		- Interrupts are disabled.
/		- The GDT and IDT are the callers; we need our copies.
/		- The kernel's text, initialized data and bss are mapped.
/		- We can easily tell whether or not KADB is around.
/
/	Our actions:
/		- Save arguments
/		- Initialize our stack pointer to the thread 0 stack (t0stack)
/		  and leave room for a phony "struct regs".
/		- Our GDT and IDT need to get munged.
/		- Since we are using the boot's GDT descriptors, we need
/		  to copy them into our GDT before we switch to ours.
/		- We start using our GDT by loading correct values in the
/		  selector registers (cs=KCSSEL, ds=es=ss=KDSSEL, fs=KFSSEL,
/		  gs=KGSSEL).
/		- When KADB is around, IDT entries need to get replaced.
/		  (NOTE: we choose the vectors we want KADB to handle)
/		- We change over to using our IDT.
/		- The default LDT entries for syscall and sigret are set.
/		- We load the default LDT into the hardware LDT register.
/		- We load the default TSS into the hardware task register.
/		- Check for cpu type, i.e. 386 vs. 486.
/		- mlsetup(%esp) gets called.
/		- We change our appearance to look like the real thread 0.
/		  (NOTE: making ourselves to be a real thread may be a noop)
/		- main() gets called.  (NOTE: main() never returns).
/		
/-----------------------------------------------------------------------------
/
/	NOW, the real code!
/
/	Globals:
/
	.globl _start
	.globl	mlsetup
	.globl	main

	.globl	t0stack
	.globl	t0
	.globl	scall_dscr
	.globl	sigret_dscr
	.globl	ldt_default
	.globl	gdt
	.globl	idt
	.globl	idt2
	.globl	KGDTptr
	.globl	KIDTptr
	.globl	KIDT2ptr
	.globl	kadb_is_running
	.globl	edata

/	NOTE: t0stack should be the first thing in the data section so that
/	      if it ever overflows, it will fault on the last kernel text page.
/
	.data
/ 3 pages of thread 0 stack, for now
#define T0STACK_SIZE 12288
	.comm	t0stack, T0STACK_SIZE, 32
	.comm	t0, 4094, 32
	.comm	df_stack, T0STACK_SIZE	/ stack for handling double faults

#ifdef DEBUG
	.globl	intr_thread_cnt
	.comm	intr_thread_cnt, 4
#endif

/ call back into boot - sysp (bootsvcs.h) and bootops (bootconf.h)
	.comm	sysp, 4
	.comm	syspp, 4
	.globl	bootops
	.comm	bootopsp, 4
	.comm	kadb_is_running, 4

KGDTptr:
	.value	GDTSZ\*8-1
	.long	gdt
	.value	0
KIDTptr:
	.value	IDTSZ\*8-1
	.long	idt
	.value	0

/ Used by VPIX processes
KIDT2ptr:
	.value	IDTSZ\*8-1
	.long	idt2
	.value	0

	.globl	Kernelbase;
	.type	Kernelbase,@object;
	.size	Kernelbase,0;
	.set	Kernelbase, KERNELBASE

	.globl	Sysbase;
	.type	Sysbase,@object;
	.size	Sysbase,0;
	.set	Sysbase, SYSBASE

	.globl	Syslimit;
	.type	Syslimit,@object;
	.size	Syslimit,0;
	.set	Syslimit, SYSLIMIT

#ifdef _VPIX
/ Counter to indicate active v86mode processes. Used by resume().
	.globl	v86enable
	.align 4
v86enable:
	.long	0
#endif
#ifdef MERGE386
	.globl	merge386enable
	.align 4
merge386enable:
	.long	0
#endif

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
_start()
{}

#else	/* lint */

	ENTRY_NP(_start)
	/ We jump to __start to make sure the  function enable_4mb_page_support
	/ does not span multiple 4K pages. This is  essential, since we need
	/ to turn paging off in enable_4mb_page_support
	jmp	__start

	/ It is very important that this function sit right here in the file
	/ This function should not span multiple pages
	/ This function has to be excuted of an address such that we have 1-1 
	/ mapping. First we disable paging, then we load %cr4 with %ebx
	/ and then enable paging. We return to  0xe00xxxx with paging 
	/ enabled
enable_4mb_page_support:
	movl	%cr0, %eax
	andl	$0x7fffffff, %eax
	movl	%eax, %cr0	/ disable paging
	movl	%cr3, %eax
	movl	%eax, %cr3
	/ 
	/	movl	%cr4, %eax
	/
	.byte 	0x0f, 0x20, 0xe0
	orl	%ebx, %eax
	/
	/ 	movl	%eax, %cr4	
	/
	.byte 	0x0f, 0x22, 0xe0
	nop
	movl	%cr0, %eax
	orl	$0x80000000, %eax	/ enable paging
	movl	%eax, %cr0
	jmp	enable_4mb_page_support_done	
enable_4mb_page_support_done:
	ret

__start:

	/		- Save arguments
	mov	$edata, %ebp		/ edata needs to be defined for ksyms
	mov	$0,(%ebp)		/ limit stack trace
	cmp	$0x12344321, %edx	/ check for kadb running
	jne	.no_kadb1
	mov	$1, kadb_is_running

	mov	(%ecx), %eax
	mov	%eax, sysp		/ call back for boot services
	mov	%ecx, syspp		/ address of kadb's callback pointer
					/ This is to be replaced when
					/ interrupts are enabled.
	mov	(%ebx), %eax
	mov	%eax, bootops		/ call back for boot services
	mov	%ebx, bootopsp		/ address of kadb's callback pointer
	jmp	.debugger_running
/		- check for kadb running?
.no_kadb1:
	mov	%ecx, sysp		/ call back for boot services
	mov	%ebx, bootops		/ call back for boot services
	mov	$sysp, syspp
	mov	$bootops, bootopsp

.debugger_running:
/		- Initialize our stack pointer to the thread 0 stack (t0stack)
/		  and leave room for a phony "struct regs".
/
	movl	$t0stack+T0STACK_SIZE-REGSIZE, %esp

/		- Our GDT and IDT need to get munged.
/
	movl	KGDTptr+2, %eax		/ pointer to GDT
	xorl	%ecx, %ecx
	movw	KGDTptr, %ecx		/ size of GDT
	call	munge_table		/ rearranges/builds table in place

	movl	KIDTptr+2, %eax		/ pointer to IDT
	xorl	%ecx, %ecx
	movw	KIDTptr, %ecx		/ size of IDT
	call	munge_table		/ rearranges/builds table in place

#ifdef _VPIX
	/ IDT for VPix needs to be munged
	movl	KIDT2ptr+2, %eax	/ pointer to IDT
	xorl	%ecx, %ecx
	movw	KIDT2ptr, %ecx		/ size of IDT
	call	munge_table		/ rearranges/builds table in place
#endif
	
/		- Since we are using the boot's GDT descriptors, we need
/		  to copy them into our GDT before we switch to ours.
/
	sgdt	t0stack		/ t0stack now has (limit, GDT address)
	movl	t0stack+2, %eax	/ &bootgdt
	movl	$gdt, %ebx	/ &gdt
	movl	8(%eax), %ecx	/ copy gdt[1]
	movl	%ecx, 8(%ebx)
	movl	12(%eax), %ecx
	movl	%ecx, 12(%ebx)
	movl	16(%eax), %ecx	/ copy gdt[2]
	movl	%ecx, 16(%ebx)
	movl	20(%eax), %ecx
	movl	%ecx, 20(%ebx)
	movl	24(%eax), %ecx	/ copy gdt[3]
	movl	%ecx, 24(%ebx)
	movl	28(%eax), %ecx
	movl	%ecx, 28(%ebx)
	movl	32(%eax), %ecx	/ copy gdt[4]
	movl	%ecx, 32(%ebx)
	movl	36(%eax), %ecx
	movl	%ecx, 36(%ebx)

/		- We start using our GDT by loading correct values in the
/		  selector registers (cs=KCSSEL, ds=es=ss=KDSSEL, fs=KFSSEL,
/		  gs=KGSSEL).
/
	lgdt	KGDTptr
	nop
	sgdt	t0stack+4
	nop
	ljmp	$KCSSEL, $.next
.next:
	mov	$KDSSEL, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	mov	$KFSSEL, %eax
	movw	%ax, %fs
	mov	$KGSSEL, %eax
	movw	%ax, %gs
	pushl	$F_OFF
	popfl				/ set flags with interrupts OFF

/		- When KADB is around, IDT entries need to get replaced.
/		  (NOTE: we choose the vectors we want KADB to handle)
/
	cmpl	$1, kadb_is_running
	jne	.no_kadb2
	sidt	t0stack		/ t0stack now has (limit, IDT address)
	movl	t0stack+2, %eax	/ &bootidt
	movl	$idt, %ebx	/ &idt
	movl	$KCSSEL\*65536, %ecx
	movw	8(%eax), %cx		/ copy idt[1]
	movl	%ecx, 8(%ebx)
	movl	12(%eax), %ecx
	andl	$0xfffffeff, %ecx
	movl	%ecx, 12(%ebx)
	movl	$KCSSEL\*65536, %ecx
	movw	24(%eax), %cx		/ copy idt[3]
	movl	%ecx, 24(%ebx)
	movl	28(%eax), %ecx
	andl	$0xfffffeff, %ecx
	movl	%ecx, 28(%ebx)
	movl	$KCSSEL\*65536, %ecx
	movw	20\*8(%eax), %cx	/ copy idt[20]
	movl	%ecx, 20\*8(%ebx)
	movl	20\*8+4(%eax), %ecx
	andl	$0xfffffeff, %ecx
	movl	%ecx, 20\*8+4(%ebx)
.no_kadb2:

/		- We change over to using our IDT.
/
	lidt	KIDTptr

/		- The default LDT entries for syscall and sigret are set.
/
	movl	$ldt_default, %ebx
	movl	scall_dscr, %eax
	movl	%eax, (%ebx)
	movl    %eax, 32(%ebx)      / copy to &ldt_default[USER_ALTSCALL];
	movl	scall_dscr+4, %eax
	movl	%eax, 4(%ebx)
	movl    %eax, 36(%ebx)      / copy to &ldt_default[USER_ALTSCALL];
	movl	sigret_dscr, %eax
	movl	%eax, 8(%ebx)
	movl    %eax, 40(%ebx)      / copy to &ldt_default[USER_ALTSIGCLEAN];
	movl	sigret_dscr+4, %eax
	movl	%eax, 12(%ebx)
	movl    %eax, 44(%ebx)      / copy to &ldt_default[USER_ALTSIGCLEAN];
	movl	$ldt_default, %eax
	movl	$MINLDTSZ\*8-1, %ecx
	call	munge_table

/		- We load the default LDT into the hardware LDT register.
/
	movw	$LDTSEL, %ax
	lldt	%ax

/		- We load the default TSS into the hardware task register.
/
	movw	$UTSSSEL, %ax
	ltr	%ax


.set EFL_AC, 0x40000			/ Alignment check in EFLAGS
.set EFL_ID, 0x200000			/ ID bit in EFLAGS
.set PAGESIZE, 0x1000


	pushal	
	pushfl				/ Push FLAGS value on stack
	popl	%eax
	pushl	%eax
	movl	%eax, %ecx
	xorl	$EFL_AC, %eax
	pushl	%eax
	popfl	
	pushfl
	popl	%eax
	cmpl	%eax, %ecx
	je	cpu_386	

 
	movl	%cr0, %eax
        orl     $[CR0_WP|CR0_AM],%eax   / enable WP for detecting faults.
                                        / and enable alignment checking
        andl    $-1![CR0_WT|CR0_CE],%eax
        movl    %eax, %cr0              / set the cr0 register correctly and
                                        / overide the BIOS setup

	movl	%ecx, %eax
	xorl	$EFL_ID, %eax
	pushl	%eax
	popfl
	pushfl
	popl    %eax
	cmpl	%eax, %ecx
	je	cpu_486	

	movl	$0, %eax
	.byte	0x0f, 0xa2

	andl	$0x7, %eax
	movl	%eax, cpu_id_parm
	testl	%eax, %eax
	je	cpu_486

	/ register ordering for vendor-id string is the same as
	/ the register ordering for PUSHALL/POPALL instructions
	
	movl	%ebx, cpu_vendor
	movl	%edx, cpu_vendor+4
	movl	%ecx, cpu_vendor+8

	leal	cpu_vendor, %esi
	leal	intel_cpu_id, %edi
	movl	$12, %ecx
	repz
	cmpsb
	jne	maybe_amd
	jmp	vendor_is_intel

maybe_amd:
	leal	cpu_vendor, %esi
	leal	amd_cpu_id, %edi
	movl	$12, %ecx
	repz
	cmpsb
	jne	cpu_486
	jmp	vendor_is_amd

vendor_is_intel:
	movw	$I86_P5_ARCH, cputype
	movl	$1, %eax
	.byte	0x0f, 0xa2	

	movl	%edx, cpu_features
	movl	%ecx, cpu_features+4
	movl	%ebx, cpu_features+8

	testl	$P5_PSE_SUPPORTED, %edx
	je	p5_pse_not_supported
	orl	$X86_LARGEPAGE, x86_feature
p5_pse_not_supported:
	testl	$P5_TSC_SUPPORTED, %edx
	je	p5_tsc_not_supported
	orl	$X86_TSC, x86_feature
p5_tsc_not_supported:
	testl	$P5_MSR_SUPPORTED, %edx
	je	p5_msr_not_supported
	orl	$X86_MSR, x86_feature
p5_msr_not_supported:
	testl	$P6_MTRR_SUPPORTED, %edx
	je	p6_mtrr_not_supported
	orl	$X86_MTRR, x86_feature
p6_mtrr_not_supported:
	testl	$P6_APIC_SUPPORTED, %edx
	je	p6_apic_not_supported
	orl	$X86_APIC, x86_feature
p6_apic_not_supported:
	testl	$P6_PGE_SUPPORTED, %edx
	je	p6_pge_not_supported
	orl	$X86_PGE, x86_feature
p6_pge_not_supported:
	testl	$P6_CMOV_SUPPORTED, %edx
	je      p6_cmov_not_supported
        orl     $X86_CMOV, x86_feature
p6_cmov_not_supported:
        testl   $P5_MMX_SUPPORTED, %edx
        je      p5_mmx_not_supported
        orl     $X86_MMX, x86_feature
p5_mmx_not_supported:

	movl	%eax, %ebx
	andl	$0x00F, %ebx
	movl	%ebx, cpu_stepping

	movl	%eax, %ebx
	andl	$0x0F0, %ebx
	shrl	$4, %ebx
	movl	%ebx, cpu_model

	movl	%eax, %ebx
	andl	$0xF00, %ebx
	shrl	$8, %ebx
	movl	%ebx, cpu_family

	cmpl	$4, cpu_family
	je	cpu_486
	cmpl	$5, cpu_family
	jne	maybe_p6
	leal	pentium_tm, %eax
	movl	%eax, cpu_namestrp
	orl	$X86_P5, x86_feature
	testl	$X86_LARGEPAGE, x86_feature
	je	cpu_done	
	movl	$P5_PSE, %eax
	jmp	enable_large_page_support
maybe_p6:
	cmpl	$6, cpu_family
	jne	cpu_unknown
	leal	p6_tm, %eax
	movl	%eax, cpu_namestrp
	orl	$X86_P6, x86_feature
cpu_unknown:
	testl	$X86_LARGEPAGE, x86_feature
	je	cpu_done	
	movl	$P5_PSE, %eax
	testl	$X86_PGE, x86_feature
	je	enable_large_page_support
	orl	$P6_GPE, %eax
	jmp	enable_large_page_support


vendor_is_amd:
	movw	$I86_P5_ARCH, cputype
	movl	$1, %eax
	.byte	0x0f, 0xa2	

	movl	%edx, cpu_features
	movl	%ecx, cpu_features+4
	movl	%ebx, cpu_features+8

	testl	$P5_PSE_SUPPORTED, %edx
	je	k5_pse_not_supported
	orl	$X86_LARGEPAGE, x86_feature
k5_pse_not_supported:
	testl	$P5_TSC_SUPPORTED, %edx
	je	k5_tsc_not_supported
	orl	$X86_TSC, x86_feature
k5_tsc_not_supported:
	testl	$P5_MSR_SUPPORTED, %edx
	je	k5_msr_not_supported
	orl	$X86_MSR, x86_feature
k5_msr_not_supported:
	testl	$K5_PGE, %edx
	je	k5_pge_not_supported
	orl	$X86_PGE, x86_feature
k5_pge_not_supported:

	movl	%eax, %ebx
	andl	$0x00F, %ebx
	movl	%ebx, cpu_stepping

	movl	%eax, %ebx
	andl	$0x0F0, %ebx
	shrl	$4, %ebx
	movl	%ebx, cpu_model

	movl	%eax, %ebx
	andl	$0xF00, %ebx
	shrl	$8, %ebx
	movl	%ebx, cpu_family

	cmpl	$4, cpu_family
	je	cpu_486
	leal	k5_tm, %eax
	movl	%eax, cpu_namestrp
	testl	$X86_LARGEPAGE, x86_feature
	je	cpu_done	
	orl	$X86_K5, x86_feature
	movl	$P5_PSE, %eax
	testl	$X86_PGE, x86_feature
	je	k5glbpge_disabled
	orl	$P6_GPE, %eax
k5glbpge_disabled:

enable_large_page_support:
	movl	%eax, cr4_value
	movl	$enable_4mb_page_support, %ecx
	shrl	$MMU_STD_PAGESHIFT, %ecx
	andl	$MMU_L2_MASK, %ecx	/ pagetable index
	movl	%cr3, %eax
	andl	$MMU_STD_PAGEMASK, %eax	/ pagedirectory pointer
	pushl	%eax
	movl	$_start, %edx
	shrl	$MMU_STD_PAGESHIFT+NPTESHIFT, %edx
	movl	(%eax, %edx, 4), %eax	/ pagetable  for _start
	andl	$MMU_STD_PAGEMASK, %eax		/ ignore ref and other bits
	movl	(%eax, %ecx, 4), %ecx	/ pte entry
	movl	%ecx, %edx
	shrl	$MMU_STD_PAGESHIFT, %ecx /pfnum of enable_4MB_page_support
	movl	%ecx, %ebx
	andl	$MMU_L2_MASK, %ecx
	shrl	$NPTESHIFT, %ebx
	movl	0(%esp), %eax
	leal	(%eax, %ebx, 4), %eax 	/ new pde entry  for the function
	pushl	%eax
	movl	(%eax), %eax
	testl	$1, %eax		/ is the pde entry valid
	jne	pagetable_isvalid
	movl	$one2one_pagetable, %ebx
	addl	$PAGESIZE, %ebx	/ move to next 4k
	shrl	$MMU_STD_PAGESHIFT+NPTESHIFT, %ebx
	movl	4(%esp), %eax
	movl	(%eax, %ebx, 4), %eax
	andl	$MMU_STD_PAGEMASK, %eax		/ ignore ref and other bits
	movl	$one2one_pagetable, %ebx
	addl	$PAGESIZE, %ebx	/ move to next 4k
	shrl	$MMU_STD_PAGESHIFT, %ebx
	andl	$MMU_L2_MASK, %ebx
	movl	(%eax, %ebx, 4), %eax	/ pte entry for one2one_pagetable
	movl	0(%esp), %ebx
	movl	%eax, (%ebx)		/ point pde entry at one2one_pagetable
	movl	$one2one_pagetable, %eax / load pagetable ptr
	addl	$PAGESIZE, %eax		/ 4K aligned virtual address
	andl	$0xfffff000, %eax
	movl	$0, 4(%esp)
	jmp	pagetable_madevalid
pagetable_isvalid:
	movl	%eax, 4(%esp)
pagetable_madevalid:
	andl	$MMU_STD_PAGEMASK, %eax	/ ignore lower 12 bits	

	pushl	(%eax, %ecx,4)		/ save the old pagetable entry that 
					/ belongs to boot

	movl	%edx, (%eax,%ecx,4)	/ We now have a 1-1 mapping for the
					/ function enable_4MB_page_support
	leal	(%eax, %ecx, 4), %eax
	pushl	%eax			/ save the address of pagetable entry
					/ we patched above

	movl	$enable_4mb_page_support, %eax
	andl	$MMU_PAGEOFFSET, %eax		/ pick the 4k offset 
	andl	$MMU_STD_PAGEMASK, %edx
	addl	%edx, %eax
	movl	cr4_value, %ebx
	call	*%eax			/ jump to enable_4mb_page_support
					/ we have 1-1 mapping
					/ for this particular page, ret address
					/ still points to space in 0xe00xxxxx

	popl	%eax			/ pop the pagetable entry address
	popl	%ecx			/ pop the pagetable entry
	movl	%ecx, (%eax)		/ restore the pte that we destroyed
	popl	%eax			/ pop the pagedirectory address
	popl	%ecx			/ pop pde
	movl	%ecx, (%eax)		/ restore pde
	movl	%cr3, %eax
	movl	%eax, %cr3
	jmp	cpu_done

cpu_386:
	movw	$I86_386_ARCH, cputype
	movl	$_i386_tm, cpu_namestrp
	jmp	cpu_done

cpu_486:
	movw	$1, cacheflsh
	movl	$_i486_tm, cpu_namestrp
	movw	$I86_486_ARCH, cputype

cpu_done:
	popfl					/ Restore original FLAGS
	popal					/ Restore all registers

/		- mlsetup(%esp) gets called.
/
	pushl	%esp
	call	mlsetup
	addl	$4, %esp

/		- We change our appearance to look like the real thread 0.
/		  (NOTE: making ourselves to be a real thread may be a noop)
/
/		- main() gets called.  (NOTE: main() never returns).
/
	call	main
/* NOT_REACHED */
	pushl	$return_from_main
	.globl	panic
	call	panic
	SET_SIZE(_start)

	.data
return_from_main:
	.string	"main() returned unexpectedly!"
	.set	F_OFF, 0x2		/ kernel mode flags, interrupts off
	.set	F_ON, 0x202		/ kernel mode flags, interrupts on
	.set	F_ID, 0x200000		/ kernel mode flags, cpuid opcode 
#ifdef _VPIX
	.set	VMFLAG, 0x20000		/ virtual 86 mode flag
	.set	NTBIT, 0x4000
	.set	TASK_BUSY, 0x2
	.set	XTSS_ACCESS_BYTE,GDT_XTSSSEL+0x5 / access byte in the descriptor
#endif

/--------------------- _start support functions ----------------------
/
/	munge_table
/		This procedure will 'munge' a descriptor table to
/		change it from initialized format to runtime format.
/
/		Assumes:
/			%eax -- contains the base address of table.
/			%ecx -- contains size of table.
munge_table:

	addl	%eax, %ecx	/* compute end of IDT array */
	movl	%eax, %esi	/* beginning of IDT */

moretable:
	cmpl	%esi, %ecx
	jl	donetable	/* Have we done every descriptor?? */

	movl	%esi, %ebx	/*long-vector/*short-selector/*char-rsrvd/*char-type */
			
	movb	7(%ebx), %al	/* Find the byte containing the type field */
	testb	$0x10, %al	/* See if this descriptor is a segment */
	jne	notagate
	testb	$0x04, %al	/* See if this destriptor is a gate */
	je	notagate
				/* Rearrange a gate descriptor. */
	movl	4(%ebx), %edx	/* Selector, type lifted out. */
	movw	2(%ebx), %ax	/* Grab Offset 16..31 */
	movl	%edx, 2(%ebx)	/* Put back Selector, type */
	movw	%ax, 6(%ebx)	/* Offset 16..31 now in right place */
	jmp	descdone

notagate:			/* Rearrange a non gate descriptor. */
	movw	4(%ebx), %dx	/* Limit 0..15 lifted out */
	movw	6(%ebx), %ax	/* acc1, acc2 lifted out */
	movb	%ah, 5(%ebx)	/* acc2 put back */
	movw	2(%ebx), %ax	/* 16-23, 24-31 picked up */
	movb	%ah, 7(%ebx)	/* 24-31 put back */
	movb	%al, 4(%ebx)	/* 16-23 put back */
	movw	(%ebx), %ax	/* base 0-15 picked up */
	movw	%ax, 2(%ebx)	/* base 0-15 put back */
	movw	%dx, (%ebx)	/* lim 0-15 put back */

descdone:
	addl	$8, %esi	/* Go for the next descriptor */
	jmp	moretable

donetable:
	ret

/---------------------------------------------------------------------------

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
_interrupt()
{}

#else	/* lint */

	.align	8
	ENTRY_NP2(cmnint,_interrupt)
	pusha
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	xorl	%edx,%edx
	movl	$KGSSEL, %eax
	movl	REGS_GS(%esp), %edi
	movl	$KDSSEL, %ebx
	movl	$KFSSEL, %ecx
	andl	$0xffff, %edi
	cmpl	%edi, %eax		/ test GS for return to user mode
	je	cmnint1
	movw	%ax, %gs
	movw	%bx, %ds
	movw	%bx, %es
	movw	%cx, %fs
	/ disable catching signal processing GP faults if from user mode
	testb	$CPL_MASK, REGS_CS(%esp)	/testb saves prefix
	jz	cmnint1
	movl	%gs:CPU_LWP, %eax
	movb	%dl, LWP_GPFAULT(%eax)
	.align	4
cmnint1:
	movl	%esp, %ebp
	cld				/ only flag bit we are concerned about
					/ is direction
	LOADCPU(%ebx)		/ get pointer to CPU struct. Avoid gs refs
 	leal    REGS_TRAPNO(%ebp), %ecx / get address of vector

	movl	CPU_M+CPU_PRI(%ebx), %edi	/get ipl

	/ raise interrupt priority level
	pushl	%ecx
	pushl	%edi
	call    *setlvl                 
	popl	%edi			/ save old priority level
	popl	%ecx

	/ check for spurious interrupt
	cmp	$-1, %eax		
	je	_sys_rtt

	movl	%eax, CPU_M+CPU_PRI(%ebx) / update ipl
 	movl    REGS_TRAPNO(%ebp), %ecx	/ update the interrupt vector 

 	/ At this point we can take one of three paths. If the vector
 	/ is a clock vector, then we jump to code that will run a predefined
 	/ clock thread.
 	/ If the new priority level is below LOCK LEVEL  then we
 	/ jump to code that will run this interrupt as a separate thread.
	/ Otherwise the interrupt is NOT run as a separate thread ( similar
 	/ to the way interrupts always used to work). 

	/ %edi - old priority level
	/ %ebp - pointer to REGS
	/ %ecx - translated vector
	/ %eax - ipl of isr.

	cmp	clock_vector,%ecx
	je	clock_intr

	cmpl 	$LOCK_LEVEL, %eax	/ compare to highest thread level
	jb	intr_thread		/ process as a separate thread
	/
	/ Handle high_priority nested interrupt on separate interrupt stack 
	/
	movl	CPU_ON_INTR(%ebx), %esi
	cmpl 	$0, %esi
	jne	onstack			/ already on interrupt stack
	movl	%esp, %eax
	movl	CPU_INTR_STACK(%ebx), %esp/ get on interrupt stack
	pushl	%eax			/ save the thread stack pointer
onstack:
	incl	%esi
	movl	%esi,CPU_ON_INTR(%ebx)
	movl	$autovect, %esi		/ get autovect structure before
					/ sti to save on AGI later
	sti				/ enable interrupts
	pushl	%ecx			/ save interrupt vector
	/
	/ Get handler address
	/
pre_loop1:
	movl	AVH_LINK(%esi,%ecx,8), %esi
	xorl	%ebx,%ebx		/ bh is no. of intpts in chain
					/ bl is DDI_INTR_CLAIMED status of chain
	testl	%esi, %esi		/ if pointer is null 
	jz	.intr_ret		/ then skip
loop1:
	incb	%bh
	movl	AV_VECTOR(%esi), %edx	/ get the interrupt routine
	testl	%edx, %edx		/ if func is null
	jz	.intr_ret		/ then skip
	pushl	AV_INTARG(%esi)		/ get argument to interrupt routine
	call	*%edx			/ call interrupt routine with arg
	addl	$4, %esp
	orb	%al,%bl			/ see if anyone claims intpt.
	movl	AV_LINK(%esi), %esi	/ get next routine on list
	testl	%esi, %esi		/ if pointer is non-null 
	jnz	loop1			/ then continue

.intr_ret:
	cmpb	$1,%bh			/ if only 1 intpt in chain, it is OK
	je	.intr_ret1
	orb	%bl,%bl			/ If no one claims intpt, then it is OK
	jz	.intr_ret1
	movl	(%esp), %ecx		/ else restore intr vector
	movl	$autovect, %esi		/ get autovect structure
	jmp	pre_loop1		/ and try again.

.intr_ret1:
	LOADCPU(%ebx)

	cli
	inc	CPU_SYSINFO_INTR(%ebx)	/ cpu_sysinfo.intr++
	movl	%edi, CPU_M+CPU_PRI(%ebx)
					/ interrupt vector already on stack
	pushl	%edi			/ old ipl
	call	*setlvlx	
	addl	$8, %esp		/ eax contains the current ipl

	decl	CPU_ON_INTR(%ebx)	/ reset stack pointer if no more
					/ HI PRI intrs.
	jnz	.intr_ret2
	popl	%esp			/ restore the thread stack pointer
.intr_ret2:
	movl	softinfo,%edx		/ any pending software interrupts
	orl	%edx,%edx
	jz	_sys_rtt
	jmp	dosoftint		/ check for softints before we return.
#endif	/* lint */


/*
 * Handle an interrupt in a new thread.
 *	Entry:  traps disabled.
 *		%edi - old priority level
 *		%ebp - pointer to REGS
 *		%ecx - translated vector
 *		%eax - ipl of isr.
 *		%ebx - pointer to CPU struct
 *	Uses:
 */
#if defined(lint) || defined(__lint)

void
intr_thread()
{}

#else	/* lint */

	.globl intr_thread
intr_thread:
	/ Get set to run interrupt thread.
	/ There should always be an interrupt thread since we allocate one
	/ for each level on the CPU, and if we release an interrupt, a new
	/ thread gets created.
	/
	movl	CPU_THREAD(%ebx), %edx		/ cur thread in edx
	movl 	CPU_INTR_THREAD(%ebx), %esi	/ intr thread in %esi
	movl	%esp, T_SP(%edx)	/ mark stack in curthread for resume
	pushl	%edi			/ get a temporary register

	movl	T_LINK(%esi), %edi		/ unlink thread from CPU's list
	movl	%edi, CPU_INTR_THREAD(%ebx)	/ unlinked

	movl	T_LWP(%edx), %edi
	movl	%edx, T_INTR(%esi)		/ push old thread
	movl	%edi, T_LWP(%esi)
	/
	/ Threads on the interrupt thread free list could have state already
	/ set to TS_ONPROC, but it helps in debugging if they're TS_FREE
	/
	movl	$ONPROC_THREAD, T_STATE(%esi)
	/
	/ chain the interrupted thread onto list from the interrupt thread.
	/ Set the new interrupt thread as the current one.
	/
	popl	%edi				/ Don't need a temp reg anymore
	movl	T_STACK(%esi), %esp		/ interrupt stack pointer
	movl	%esi, CPU_THREAD(%ebx)		/ set new thread
	pushl	%eax				/ save the ipl
	/
	/ Initialize thread priority level from intr_pri
	/
	xorl	%ebx, %ebx
	movw	intr_pri, %bx		/ XXX Can cause probs if new class is
					/ loaded on some other cpu.
	addl	%ebx, %eax		/ convert level to dispatch priority
	movw	%ax, T_PRI(%esi)

	/ The following 3 instructions need not be in cli.
	/ Putting them here only to avoid the AGI penalty on Pentiums.

	pushl	%ecx			/ save interrupt vector.
	pushl	%esi
	movl	$autovect, %esi		/ get autovect structure
	sti				/ enable interrupts
pre_loop2:
	movl	AVH_LINK(%esi,%ecx,8), %esi
	xorl	%ebx,%ebx		/ bh is cno. of intpts in chain
					/ bl is DDI_INTR_CLAIMED status of chain
	testl	%esi, %esi		/ if pointer is null 
	jz	loop_done2		/	we're done
loop2:
	movl	AV_VECTOR(%esi), %edx	/ get the interrupt routine
	testl	%edx, %edx		/ if pointer is null
	jz	loop_done2		/	we're done
	incb	%bh
	movl	AV_MUTEX(%esi), %ecx	
	pushl	AV_INTARG(%esi)		/ get argument to interrupt routine
	testl	%ecx, %ecx		/ if unsafe driver
	jnz	.unsafedriver		/ then branch
	call	*%edx			/ call interrupt routine with arg
	addl	$4,%esp
	orb	%al,%bl			/ see if anyone claims intpt.
	movl	AV_LINK(%esi), %esi	/ get next routine on list
	testl	%esi, %esi		/ if pointer is non-null 
	jnz	loop2			/ continue
loop_done2:
	cmpb	$1,%bh			/ if only 1 intpt in chain, it is OK
	je	.loop_done2_1
	orb	%bl,%bl			/ If no one claims intpt, then it is OK
	jz	.loop_done2_1
	movl	$autovect, %esi		/ else get autovect structure
	movl	4(%esp), %ecx		/ restore intr vector 
	jmp	pre_loop2		/ and try again.
.unsafedriver:
	pushl	%edx			/ save to protect intr removal
	pushl	%ecx
	call	mutex_enter
	addl	$4,%esp
	popl	%edx
	call	*%edx			/ call interrupt routine with arg
	addl	$4,%esp
	orb	%al,%bl			/ see if anyone claims intpt.
	pushl	AV_MUTEX(%esi)
	call	mutex_exit
	addl	$4,%esp
	movl	AV_LINK(%esi), %esi	/ get next routine on list
	testl	%esi, %esi		/ if pointer is non-null 
	jnz	loop2			/ continue
	jmp	loop_done2		/ see if we are really done
.loop_done2_1:
	popl	%esi			/ get thread pointer back.

	LOADCPU(%ebx)
	
	cli				/ protect interrupt thread pool
					/ and intr_actv
	/ do accounting
	incl	CPU_SYSINFO_INTR(%ebx)	/ cpu_sysinfo.intr++
	incl	CPU_SYSINFO_INTRTHREAD(%ebx) / cpu_sysinfo.intrthread++

	/ if there is still an interrupted thread underneath this one
	/ then the interrupt was never blocked or released and the
	/ return is fairly simple.  Otherwise jump to intr_thread_exit
	cmpl	$0, T_INTR(%esi)
	je	intr_thread_exit

	/
	/ link the thread back onto the interrupt thread pool
	movl	CPU_INTR_THREAD(%ebx), %edx	
	movl	%edx, T_LINK(%esi)
	movl	%esi, CPU_INTR_THREAD(%ebx)	/XXX See if order is important!

	movl	CPU_BASE_SPL(%ebx), %eax	/ used below.
	/ set the thread state to free so kadb doesn't see it
	movl	$FREE_THREAD, T_STATE(%esi)

	cmpl	%eax, %edi		/ if (oldipl >= basespl)
	jae	intr_restore_ipl	/ then use oldipl
	movl	%eax, %edi		/ else use basespl
intr_restore_ipl:
	movl	%edi, CPU_M+CPU_PRI(%ebx)
					/ intr vector already on stack
	pushl	%edi			/ old ipl
	call	*setlvlx		/ eax contains the current ipl	
/	addl	$8, %esp	/ no need to pop - switch to another stack
	/
	/ Switch back to the interrupted thread
	movl	T_INTR(%esi), %edx
	movl	T_SP(%edx), %esp	/ restore stack pointer
	movl	%edx, CPU_THREAD(%ebx)

	movl	softinfo,%edx		/ any pending software interrupts
	orl	%edx,%edx
	jz	_sys_rtt
	jmp	dosoftint		/ check for softints before we return.

	/
	/ An interrupt returned on what was once (and still might be)
	/ an interrupt thread stack, but the interrupted process is no longer
	/ there.  This means the interrupt must've blocked or called 
	/ release_interrupt().
	/
	/ There is no longer a thread under this one, so put this thread back
	/ on the CPU's free list and resume the idle thread which will dispatch
	/ the next thread to run.
	/
	/ All interrupts are disabled here
	/

intr_thread_exit:
#ifdef DEBUG
	incl	intr_thread_cnt
#endif
	incl	CPU_SYSINFO_INTRBLK(%ebx)	/ cpu_sysinfo.intrblk++
	/
	/ Put thread back on either the interrupt thread list if it is
	/ still an interrupt thread, or the CPU's free thread list, if it
	/ did a release_interrupt
	/ As a reminder, the regs at this point are
	/	esi	interrupt thread
	/	edi	old ipl
	/	ebx	ptr to CPU struct

	movl	T_STACK(%esi), %eax
#if	T_INTR_THREAD < 255
	testb	$T_INTR_THREAD, T_FLAGS(%esi)
#else
	testw	$T_INTR_THREAD, T_FLAGS(%esi)
#endif
	jz	rel_intr
	
	/
	/ This was an interrupt thread, so clear the pending interrupt flag
	/ for this level
	/
	/ movl	T_STACK(%esi), %eax	/ moved above for Pentium pipeline
	movl	-4(%eax), %ecx		/ IPL of this thread
	/ We could optimise code below with shl and xor.
	/ Also inline setbasespl to avoid reload of INTR_ACTV & BASE_SPL
	/ But, is it worth it?
	btrl	%ecx, CPU_INTR_ACTV(%ebx) / clear interrupt flag
	call	set_base_spl		/ set CPU's base SPL level

	movl	CPU_BASE_SPL(%ebx), %edi
	movl	%edi, CPU_M+CPU_PRI(%ebx)
					/ interrupt vector already on stack
	pushl	%edi
	call	*setlvlx		
	addl	$8, %esp		/ XXX - don't need to pop since 
					/ we are ready to switch
	call	splhigh			/ block all intrs below lock level
	/
	/ Put thread on either the interrupt pool or the free pool and
	/ call swtch() to resume another thread.
	/
	movl	CPU_INTR_THREAD(%ebx), %edx	/ get list pointer
	/
	/ Set the thread state to free so kadb doesn't see it
	/
	movl	$FREE_THREAD, T_STATE(%esi)	
	movl	%edx, T_LINK(%esi)
	movl	%esi, CPU_INTR_THREAD(%ebx)
	call 	swtch
	
	/ swtch() shouldn't return

rel_intr:
	movl	CPU_BASE_SPL(%ebx), %edi
	movl	%edi, CPU_M+CPU_PRI(%ebx)
					/ interrupt vector already on stack
	pushl	%edi
	call	*setlvlx		
	addl	$8, %esp		/ eax contains the current ipl

	movl	$TS_ZOMB, T_STATE(%esi)	/ set zombie so swtch will free
	call	swtch_from_zombie

#endif	/* lint */

/*
 * Set Cpu's base SPL level, base on which interrupt levels are active
 *	Called at spl7 or above.
 */

#if defined(lint) || defined(__lint)

void
set_base_spl(void)
{}

#else	/* lint */

	ENTRY_NP(set_base_spl)
	movl	%gs:CPU_INTR_ACTV, %eax	/ load active interrupts mask
	testl	%eax, %eax			/ is it zero?
	jz	setbase
	testl	$0xff00,%eax
	jnz	ah_set
	shl	$24, %eax		/ shift 'em over so we can find
					/ the 1st bit faster	
	bsrl	%eax, %eax
	subl	$24, %eax
setbase:
	movl	%eax, %gs:CPU_BASE_SPL	/ store base priority
	ret
ah_set:
	shl	$16, %eax
	bsrl	%eax,%eax
	subl	$16, %eax
#if	CLOCK_LEVEL < 8
	fix the code
#endif
	cmpl	$CLOCK_LEVEL, %eax	/ don't block clock interrupts,
	jnz	setbase
	subl	$1, %eax		/   instead drop PIL one level
	jmp	setbase
	SET_SIZE(set_base_spl)

#endif	/* lint */

/*
 *	Clock interrupt - entered with interrupts disabled
 *	May switch to new stack
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
clock_intr()
{}

#else	/* lint */

/*
 * This code assumes that the real time clock interrupts 100 times per
 * second.
 *
 * clock is called in a special thread called the clock_thread.
 * It sort-of looks like and acts like an interrupt thread, but doesn't
 * hold the spl while it's active.  If a clock interrupt occurs and
 * the clock_thread is already active, the clock_pend flag is set to
 * cause clock_thread to repeat for another tick
 */
	.align 4
	.comm clk_intr, 4
	.data
	.align	8
	.globl	hrestime, hrtime_base, hres_lock, clock_reenable, timedelta
	.globl	hrestime_adj, hres_last_tick
hrestime:
	.long 0, 0
	.size hrestime, 8
hrestime_adj:
	.long 0, 0
	.size hrestime_adj, 8
hres_last_tick:
	.long 0, 0
	.size hres_last_tick, 8
timedelta:
	.long 0,0
	.size timedelta, 8
hrtime_base:
	.long NSEC_PER_CLOCK_TICK\*6, 0	/ non zero value is used to make
					/ pc_gethrtime() to work correctly
					/ even before clock is initialized
	.size hrtime_base, 8
hres_lock:
	.long 0
	.size hres_lock, 4
clock_reenable:
	.long	0
	.size clock_reenable, 4

	.text

	ENTRY_NP(clock_intr)
	xorl	%edx, %edx
	movl	$hres_lock, %eax
	cmpl	%edx,clock_reenable
	jne	reenable
.CL0:
	movl	$-1,%edx
.CL1:
	xchgb	%dl, (%eax)
	testb	%dl, %dl
	jz	.CL3			/ got it
.CL2:
	cmpb	$0, (%eax)		/ possible to get lock?
	jne	.CL2
	jmp	.CL1			/ yes, try again
reenable:
	call	*clock_reenable	/ call routine to pull clock level low
				/ or whatever else is needed.
	xorl	%edx, %edx
	movl	$hres_lock, %eax
	jmp	.CL0
.CL3:
	/	Add NSEC_PER_CLOCK_TICK to hrestime which is 64 bits long
	leal	hrtime_base, %eax
	movl	(%eax), %ebx
	movl	4(%eax), %edx
	addl	nsec_per_tick, %ebx / add tick value in nano seconds
	adcl	$0, %edx
	movl	%ebx, (%eax)
	movl	%edx, 4(%eax)
	/get hrestime at this moment. Used to calculate nslt in pc_gethrestime
	call	*gethrtimef
	leal	hres_last_tick, %esi
	movl	%eax, (%esi)
	movl	%edx,  4(%esi)
	/
	/ Apply adjustment, if any
	/
	/ #define HRES_ADJ	(NSEC_PER_CLOCK_TICK >> ADJ_SHIFT)
	/ (max_hres_adj)
	/
	/ void
	/ adj_hrestime()
	/ {
	/	long long adj;
	/ 
	/	if (hrestime_adj == 0)
	/		adj = 0;
	/	else if (hrestime_adj > 0) {
	/		if (hrestime_adj < HRES_ADJ)
	/			adj = hrestime_adj;
	/		else
	/			adj = HRES_ADJ;
	/	}
	/	else {
	/		if (hrestime_adj < -(HRES_ADJ))
	/			adj = -(HRES_ADJ);
	/		else
	/			adj = hrestime_adj;
	/	}
	/ 
	/	timedelta -= adj;
	/	hrestime_adj = timedelta;
	/	hrestime.tv_nsec += adj;
	/	hrestime.tv_nsec += NSEC_PER_CLOCK_TICK;
	/ 
	/	if (hrestime.tv_nsec >= NANOSEC) {
	/		one_sec = 1;
	/		hrestime.tv_sec++;
	/		hrestime.tv_nsec -= NANOSEC;
	/	}
	/ }
__adj_hrestime:
	movl	hrestime_adj,%esi	/ if (hrestime_adj == 0)
	movl	hrestime_adj+4,%edx
	andl	%esi,%esi
	jne	.CL4			/ no
	andl	%edx,%edx
	jne	.CL4			/ no
	subl	%ecx,%ecx		/ yes, adj = 0;
	subl	%edx,%edx
	jmp	.CL5
.CL4:
	subl	%ecx,%ecx
	subl	%eax,%eax
	subl	%esi,%ecx
	sbbl	%edx,%eax
	andl	%eax,%eax		/ if (hrestime_adj > 0)
	jge	.CL6			/ no
	movl	%esi,%ecx		/ yes, hrestime_adj is positive
	subl	max_hres_adj,%ecx
	movl	%edx,%eax
	sbbl	$0,%eax
	andl	%eax,%eax		/ if (hrestime_adj < HRES_ADJ)
	jl	.CL7			/ no
	movl	max_hres_adj,%ecx	/ yes, adj = HRES_ADJ;
	subl	%edx,%edx
	jmp	.CL5
.CL6:
	movl	%esi,%ecx		/ hrestime_adj is negative
	addl	max_hres_adj,%ecx
	movl	%edx,%eax
	sbbl	$-1,%eax
	andl	%eax,%eax		/ if (hrestime_adj < -(HRES_ADJ))
	jge	.CL7			/ no
	xor	%ecx,%ecx
	subl	max_hres_adj,%ecx	/ yes, adj = -(HRES_ADJ);
	movl	$-1,%edx
	jmp	.CL5
.CL7:
	movl	%esi,%ecx		/ adj = hrestime_adj;
.CL5:
	movl	timedelta,%esi
	subl	%ecx,%esi
	movl	timedelta+4,%eax
	sbbl	%edx,%eax
	movl	%esi,timedelta
	movl	%eax,timedelta+4	/ timedelta -= adj;
	movl	%esi,hrestime_adj
	movl	%eax,hrestime_adj+4	/ hrestime_adj = timedelta;
	addl	hrestime+4,%ecx
	addl	nsec_per_tick, %ecx 	/ hrestime.tv_nsec += 
	movl	%ecx,hrestime+4	   	/	(adj + NSEC_PER_CLOCK_TICK);
	movl	%ecx,%eax
	cmpl	$NANOSEC,%eax		/ if (hrestime.tv_nsec >= NANOSEC)
	jl	.CL8			/ no
	incl	one_sec			/ yes, one_sec++;
	incl	hrestime		/      hrestime.tv_sec++;
	addl	$-NANOSEC,%eax
	movl	%eax,hrestime+4		/      hrestime.tv_nsec -= NANOSEC;
.CL8:

	LOADCPU(%ebx)
	incl	hres_lock		/ release the hres_lock

	inc	clk_intr		/ count clock interrupt
	inc	CPU_SYSINFO_INTR(%ebx)	/ cpu_sysinfo.intr++
	movl	clock_thread, %esi	
	lock
	btsl	$0,T_LOCK(%esi)		/ try to set clock_thread->t_lock
	jc	clock_done		/ clock already running
	
	/
	/ Check state.  If it isn't TS_FREE (FREE_THREAD), it must be
	/ blocked on a mutex or something.
	/
	movl	CPU_THREAD(%ebx), %eax	/ interrupted thread
	cmpl	$FREE_THREAD, T_STATE(%esi) 
	jne	rel_clk_lck

	/
	/ consider the clock thread part of the same LWP as current thread
	/ 
	movl	T_LWP(%eax), %ecx
	movl	%ecx, T_LWP(%esi)
	movl	$ONPROC_THREAD, T_STATE(%esi)	/ set running state

	/
	/ Push the interrupted thread onto list from the clock thread.
	/ Set the clock thread as the current one.
	/
	movl	%esp, T_SP(%eax)	/ mark stack
	movl	%eax, T_INTR(%esi)	/ point clock at old thread
	movl	%ebx, T_CPU(%esi)	/ set new thread's CPU pointer
	
	movl	%ebx, T_BOUND_CPU(%esi)	/ set cpu binding for thread
	movl	%esi, CPU_THREAD(%ebx)	/ point CPU at new thread
	leal	CPU_THREAD_LOCK(%ebx), %eax / pointer to onproc thread lock
	movl	%eax, T_LOCKP(%esi)	/ set thread's disp lock ptr
	movl	T_STACK(%esi), %esp	/ set new stack pointer

	pushl	$LOCK_LEVEL		/ save the interrupt level on stack
	/
	/ Initialize clock thread priority level based on intr_pri
	/
	xorl	%eax, %eax
	movw	intr_pri, %ax		/ XXX Can cause probs if new class is
					/ loaded on some other cpu.
	addl	$LOCK_LEVEL, %eax
	movw	%ax, T_PRI(%esi)

	/
	/ Now that we're on the new stack, enable traps and do trace XXX
	/
	sti				/ enable interrupts
	pushl	%edi			/ save the old ipl which is used
					/ for setlvlx in resume when blocked
	/
	/ Finally call clock()
	/
#ifdef GPROF
	/ clock() will call through kprof_tick to profile_intr
	movl	CPU_PROFILING(%ebx),%eax
	testl	%eax,%eax
	je	.cl_noprof		/ is profiling enabled?
	movl	%ebp,PROF_RP(%eax)	/ stash pointer to regs for profile_intr
.cl_noprof:
#endif
	call	clock

	cli
	/ 
	/ On return, we must determine whether the interrupted thread is
	/ still pinned or not.  If not, just call swtch.
	/
	movl	$FREE_THREAD, T_STATE(%esi)	/ free thread
	movl	T_INTR(%esi), %eax
	testl	%eax, %eax			/ if no pinned thread 
	je	clk_thread_exit			/ then clock thread was blocked

	movl	%eax, CPU_THREAD(%ebx)	/ set CPU thread back to pinned one
	movb	$0, T_LOCK(%esi)	/ unlock clock_thread->t_lock
	movl	T_SP(%eax), %esp	/ restore %sp

clock_intr_exit:
	movl	CPU_BASE_SPL(%ebx), %eax
	cmpl	%eax, %edi		/ if (oldipl >= basespl)
	jae	clk_restore_ipl		/ then use oldipl
	movl	%eax, %edi		/ else use basespl
clk_restore_ipl:
	movl	clock_vector, %eax
	movl	%edi, CPU_M+CPU_PRI(%ebx)

	pushl	%eax
	pushl	%edi			/ old level
	call	*setlvlx
	addl	$8, %esp		
	movl	softinfo,%edx		/ any pending software interrupts
	orl	%edx,%edx
	jz	_sys_rtt
 	jmp	dosoftint		/ check for softints before we return.

rel_clk_lck:
	movb	$0, T_LOCK(%esi)	/ clear lock

	/
	/ returning for clockintr without calling clock().
	/ increment clock_pend so clock() will rerun tick processing.
	/
clock_done:
	pushl	$clock_lock
	call	lock_set
	add	$4, %esp

	inc	clock_pend

	movb	$0, clock_lock		/ clear lock
	jmp	clock_intr_exit

clk_thread_exit:
	/ No pinned (interrupted) thread to return to,
	/ so clear the pending interrupt flag for this level and call swtch
	/
	btrl	$CLOCK_LEVEL, %gs:CPU_INTR_ACTV / clear interrupt flag
	call	set_base_spl		/ set CPU's base SPL level
	incl	CPU_SYSINFO_INTRBLK(%ebx)	/ cpu_sysinfo.intrblk++
	call	swtch


	SET_SIZE(clock_intr)
	
/---------------------------------------------------------------------------

#endif	/* lint */

/*
 *  For stack layout, see reg.h
 *  When cmntrap gets called, the error code and trap number have been pushed.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
cmntrap()
{}

#else	/* lint */
	.globl	trap		/ C handler called below

	.text
	.align	4

	ENTRY_NP(cmntrap)
	pusha			/ save all registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	/ Just set all the segment registers on traps
	/ since kernel traps are infrequent.
	movl	$KDSSEL, %ebx
	movl	$KFSSEL, %ecx
	movl	$KGSSEL, %eax
	movw	%bx, %ds
	movw	%bx, %es
	movw	%cx, %fs
	movw	%ax, %gs

	movl	%esp, %ebp

/	XXX - check for NMIs????

	/ disable catching signal processing GP faults if we came from user mode
	testw	$CPL_MASK, REGS_CS(%ebp)
	jz	.L1
	movl	%gs:CPU_LWP, %eax
	movb	$0, LWP_GPFAULT(%eax)
.L1:
	/ If this is a debug trap, save the debug registers in the pcb
	cmpl	$T_SGLSTP, REGS_TRAPNO(%ebp)
	jne	.L11
	movl	%gs:CPU_LWP, %eax
	orl	%eax, %eax
	jz	.L11		/ null lwp pointer; can't happen?
	movl	%db0, %ecx
	movl	%ecx, LWP_PCB_DREGS(%eax)
	movl	%db1, %ecx
	movl	%ecx, LWP_PCB_DREGS+4(%eax)
	movl	%db2, %ecx
	movl	%ecx, LWP_PCB_DREGS+8(%eax)
	movl	%db3, %ecx
	movl	%ecx, LWP_PCB_DREGS+12(%eax)
	/ movl	%db4, %ecx
	/ movl	%ecx, LWP_PCB_DREGS+16(%eax)
	/ movl	%db5, %ecx
	/ movl	%ecx, LWP_PCB_DREGS+20(%eax)
	movl	%db6, %ecx
	movl	%ecx, LWP_PCB_DREGS+24(%eax)
	movl	%db7, %ecx
	movl	%ecx, LWP_PCB_DREGS+28(%eax)
	movl	$0, %ecx
	movl	%ecx, %db6	/ clear debug status register
	movl	LWP_PCB_FLAGS(%eax), %ecx
	orl	$DEBUG_ON, %ecx
	movl	%ecx, LWP_PCB_FLAGS(%eax)
.L11:
	movl	%cr2, %eax	/ fault address for PGFLTs
	pushl	$F_ON		/ interrupts disabled till read of cr2
				/ in case of pgflts, since we enter
	popfl			/ through a interrupt gate.
	
	pushl	%eax
	pushl	%ebp
	call	trap		/ trap(rp, addr) handles all traps
	addl	$8, %esp	/ get argument off stack

	jmp	_sys_rtt
	SET_SIZE(cmntrap)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
cmninttrap()
{}

#else	/* lint */
	.globl	trap		/ C handler called below

	.text
	.align	4

	ENTRY_NP(cmninttrap)
	pusha			/ save all registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	cld				/ only flag bit we are concerned about
					/ is direction

	/ Just set all the segment registers on traps
	/ since kernel traps are infrequent.
	movl	$KDSSEL, %ebx
	movl	$KFSSEL, %ecx
	movl	$KGSSEL, %eax
	movw	%bx, %ds
	movw	%bx, %es
	movw	%cx, %fs
	movw	%ax, %gs

	movl	%esp, %ebp

	/ disable catching signal processing GP faults if we came from user mode
	testw	$CPL_MASK, REGS_CS(%esp)
	jz	.L2
	movl	%gs:CPU_LWP, %eax
	movb	$0, LWP_GPFAULT(%eax)
.L2:
	movl	%esp, %ecx
	movl	%cr2, %eax	/ fault address for PGFLTs
	pushl	%eax
	pushl	%ecx
	call	trap		/ trap(rp, addr) handles all traps
	addl	$8, %esp	/ get argument off stack

	jmp	_sys_rtt
	SET_SIZE(cmninttrap)

#endif	/* lint */


/*
 * System call handler.  This is the destination of the call gate.
 *
 * Stack frame description in syscall() and callees.
 *
 * |------------|
 * | regs	| +(8*4)+4	registers
 * |------------|
 * | argc	| +(8*4)   number of args actually copied in (for loadable sys)
 * |------------|
 * | 8 args	| <- %esp	MAXSYSARGS (currently 8) arguments
 * |------------|
 * 
 */
#define SYS_DROP	[MAXSYSARGS\*4+4]	/* drop for args and argc */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
sys_call()
{}

#else	/* lint */
	ENTRY_NP(sys_call)
	/ on entry	eax = system call number
	/ set up the stack to look as in reg.h
	subl    $8, %esp        / pad the stack with ERRCODE and TRAPNO
	pusha			/ save user registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	/ gs should be the last segment register to be set
	/ to make the check in cmnint valid. Others can be set
	/ in any order.
	movl	$KFSSEL, %ecx
	movl	$KDSSEL, %ebx
	movl	$KGSSEL, %edx
	movw	%cx, %fs
	movw	%bx, %ds
	movw	%bx, %es
	pushfl
	popl	%ecx
	movw	%dx, %gs

	movl	%ecx, REGS_EFL(%esp)

watch_do_syscall:
	movl	%esp, %ebp

	movl	%gs:CPU_THREAD, %ebx
	xorl	%ecx, %ecx
	movl	T_LWP(%ebx), %esi

	cld				/ only flag bit we are concerned about
					/ is direction

	/ disable catching signal processing GP faults
	movb	%cl, LWP_GPFAULT(%esi)
	movb	$LWP_SYS,LWP_STATE(%esi)	/ set lwp state to SYSTEM

	subl	$SYS_DROP, %esp		/ make room for count, args and handler

/begin inline syscall_entry
	movl	%eax, %edi		/ sysnum in edi
	incl	%gs:CPU_SYSINFO_SYSCALL
#if	SYSENT_SIZE != 16
	fix shll below
#endif
	shll	$4, %edi		/edi now has index into sysent.
	incl   LWP_RU_SYSC(%esi)
#if	NORMALRETURN != 0
	movb   $NORMALRETURN, LWP_EOSYS(%esi)
#else
	movb   %cl, LWP_EOSYS(%esi)
#endif

	leal	sysent(%edi), %edi
	movw	%ax, T_SYSNUM(%ebx)
	cmpl	$NSYSCALL, %eax
	jae	.syscall2

.syscall1:
	movl	%esp, LWP_AP(%esi)	/ for get_syscall_args()

	cmpb	%cl, T_PRE_SYS(%ebx)
	jne	.syscall_pre
#ifdef	SYSCALLTRACE
	cmpl	%ecx, syscalltrace
	jne	.syscall_pre
#endif

.syscall3:
	movb	(%edi), %cl		/ nargs in %ecx

	movl	SY_CALLC(%edi), %eax	/ get syscall pointer

	movl	%ecx, [MAXSYSARGS\*4](%esp)
	testb	%cl,%cl
	jz	.syscall_noargs
					/ copy args to lwp stack
					/ first test for watchpoints
	movw	T_PROC_FLAG(%ebx), %edx
	and	$TP_WATCHPT, %edx
	jz	.syscall4

	/ We have watchpoints active in the process.
	/ Call the special copyin_args function to deal with it.
	movl	%esp, %edx
	movl	SYS_DROP+REGS_UESP(%esp), %esi
	pushl	%eax	/ remember the syscall handler
	pushl	%ecx		/ nargs
	pushl	%edx		/ kernel esp
	addl    $4, %esi
	pushl	%esi		/ user esp + 4
	call	copyin_args	/ copyin_args(uesp+4, kesp, nargs)
	addl	$12, %esp
	popl	%edx	/ syscall handler
	test	%eax, %eax
	jnz	.syscall_fault
	movl	%edx, %eax
	jmp	.syscall_call

.syscall4:
	movl	$.syscall_fault, %edx
	movl	SYS_DROP+REGS_UESP(%esp), %esi
	movl	%edx, T_LOFAULT(%ebx)	/ new lofault
	addl    $4, %esi                / skip over return addr
	movl	%esp, %edi

	repz
	smovl				/ copy words

	movl	%ecx, T_LOFAULT(%ebx)	/ clear lofault
.syscall_noargs:
/end inline syscall_entry

.syscall_call:
	call	*%eax			/ call handler with args on stack

.syscall_call_done:
#ifdef	DEBUG
	addl	$[6\*4], %esp		/ pop 6 args for kadb stack traceback
	xorl	%ecx, %ecx
	addl	$[SYS_DROP-[6\*4]], %esp / pop rest of syscall args and count
	movl	T_LWP(%ebx), %esi
#else
	xorl	%ecx, %ecx
	movl	T_LWP(%ebx), %esi
	addl	$SYS_DROP, %esp
#endif

/begin inline syscall_exit
	cmpl	%ecx, T_POST_SYS_AST(%ebx)
	jne	.syscall_post
#ifdef	SYSCALLTRACE
	cmpl	%ecx, syscalltrace
	jne	.syscall_post
#endif

	movw	%cx, T_SYSNUM(%ebx)		/ invalidate args
	movb	$LWP_USER, LWP_STATE(%esi)	/ set lwp state to User

	andb	$[0xFFFF - PS_C], REGS_EFL(%esp)	/clear Carry bit
	movl	%eax, REGS_EAX(%esp)		/ save return values
	movl	%edx, REGS_EDX(%esp)
	jmp	sys_rtt_syscall			/ normal return

.syscall_post:
					/ handle signals & other events
	pushl	%edx			/ push return values
	pushl	%eax
	call	post_syscall		/ handle signals and post-call events
	addl	$8, %esp		/ pop args

	movw	$0, T_SYSNUM(%ebx)	/ set sysnum to 0.
	jmp	sys_rtt_syscall
/end inline syscall_exit
.syscall2:
	movl	$nosys_ent, %edi
	jmp	.syscall1

.syscall_pre:
	call	pre_syscall
	movl	%esp,LWP_AP(%esi)
	xorl	%ecx, %ecx
	test	%eax,%eax
	jz	.syscall3

	push	%eax
	jmp	.syscall_fault_1

.syscall_fault:
	push	$0xe			/ EFAULT
.syscall_fault_1:
	call	set_errno
	addl	$4, %esp
	xorl	%eax, %eax		/ fake syscall_err()
	xorl	%edx, %edx
	jmp	.syscall_call_done
	SET_SIZE(sys_call)
#endif	/* lint */


/*
 * Call syscall().  Called from trap() on watchpoint at lcall 0,7
 */

#if defined(lint) || defined(__lint)

void
watch_syscall(void)
{}

#else	/* lint */
	ENTRY_NP(watch_syscall)
	movl	%gs:CPU_THREAD, %ebx
	movl	T_STACK(%ebx), %esp		/ switch to the thread stack
	movl	REGS_EAX(%esp), %eax		/ recover original syscall#
	jmp	watch_do_syscall
	SET_SIZE(watch_syscall)
#endif	/* lint */


/*
 * Loadable syscall handler.
 *	This takes MAXSYSARGS words of args in the C fashion, followed by
 *	the arg count as saved by syscall().   This pretty much must be
 *	called only by syscall().
 *
 *	Since the number of args for this entry might have been zero when
 *	the first call was done, this must re-copy the args in that case.
 */
#if defined(lint) || defined(__lint)
/* ARGSUSED */
longlong_t
loadable_syscall()
{
	return (0);
}

#else	/* lint */
	ENTRY(loadable_syscall)
	popl	%eax			/ toss return address - must be syscall
	pushl	%esp			/ push address for args
	call	loadable_syscall_entry	/ load and lock syscall and get args
	addl	$4, %esp		/ pop arg
	call	*%eax			/ call handler
	addl	$[6\*4], %esp		/ pop 6 args for kadb stack traceback
	addl	$[SYS_DROP-[6\*4]], %esp / pop rest of syscall args and count
	pushl	%edx			/ push return vals for syscall_exit
	pushl	%eax
	call	loadable_syscall_exit	/ drop loadable module lock
	call	syscall_exit		/ call wrap-up handler in C
	addl	$8, %esp		/ pop args
	jmp	sys_rtt_syscall
	SET_SIZE(loadable_syscall)
#endif	/* lint */


/* Entry point for cleanup after user signal handling */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
sig_clean()
{}

#else	/* lint */

	.globl  sigclean	/ C handler for signal return

	.align	4
	ENTRY_NP(sig_clean)
	/ set up the stack to look as in reg.h
	subl    $8, %esp        / pad the stack with ERRCODE and TRAPNO
	pusha			/ save user registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	pushfl
	popl	%eax
	movl    %eax, REGS_EFL(%esp)

	movw	$KDSSEL, %ax	/ Just set all the segment registers on sig_ret
	movw	%ax, %ds	/ since kernel traps are infrequent.
	movw	%ax, %es
	movw	$KFSSEL, %ax
	movw	%ax, %fs
	movw	$KGSSEL, %ax
	movw	%ax, %gs

	movl	%esp, %ebp
	pushl	$F_ON
	popfl

	pushl	%esp		/ argument to sigclean
	call    sigclean        / restore pre-signal state
	addl    $4, %esp        / pop arg

	cli                     / Don't allow interrupts that could clear the
				/ lwp_gpfault flag before a possible kernel
				/ GP violation on the iret.
	movl	%gs:CPU_LWP, %eax	/ enable catching signal processing 
	movb	$1, LWP_GPFAULT(%eax)	/ GP faults (see trap.c)
	jmp	set_user_regs	/ Go all the way out to user mode
	SET_SIZE(sig_clean)

#endif	/* lint */

/*
 * Return from _sys_trap routine.
 */

#if defined(lint) || defined(__lint)

void
lwp_rtt(void)
{}

void
_sys_rtt(void)
{}

#else	/* lint */
	ENTRY_NP(lwp_rtt)
	movl	%gs:CPU_THREAD, %eax
	movl	T_STACK(%eax), %esp	/ switch to the thread stack
	movl	REGS_EDX(%esp), %edx
	movl	REGS_EAX(%esp), %eax
	pushl	%edx
	pushl	%eax
	call	post_syscall		/ post_syscall(rval1, rval2)
	addl	$8, %esp
	movl	%cr0, %eax
	orl	$CR0_TS, %eax
	movl	%eax, %cr0		/ set up to take fault
					/ on first use of fp

	ALTENTRY(_sys_rtt)
#ifdef _VPIX
	/ CS is not the appropriate thing to check for v86mode processes
	testl	$VMFLAG, REGS_EFL(%esp)	/ v86mode process?
	jnz	sys_rtt_syscall		/ yes
#endif
	testw	$CPL_MASK, REGS_CS(%esp) / test CS for return to user mode
	jz	sr_sup
	
	/*
	 * Return to User.
	 */

	ALTENTRY(sys_rtt_syscall)
	/* check for an AST that's posted to the lwp */
	movl	%gs:CPU_THREAD, %eax
	cli				/ disable interrupts
	cmpb	$0, T_ASTFLAG(%eax)	/ test for signal or AST flag
	jne	handle_ast

	/*
	 * Restore user regs before doing iret to user mode.
	 */
set_user_regs:
#ifdef _VPIX
	cmpb	$0, %gs:CPU_V86PROCFLAG
	je	do_ret
	movl	%gs:CPU_THREAD, %eax
	movl	T_V86DATA(%eax), %eax
	/ 
	/ If this is a vpix thread then this call won't return. This
	/ may not be the case with MERGE386 thread.
	/
	call	*V86_RTT(%eax)		/ if it returns fall through...
do_ret:
#endif
.sel_reload:
	popl	%gs			/ restore user segment selectors
	popl	%fs
	popl	%es
	popl	%ds
	popa				/ restore general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack

	/ common iret to user mode
	.globl	common_iret
common_iret:
	iret

handle_ast:
	/* Let trap() handle the AST */
	sti				/ enable interrupts
	movl	$T_AST, REGS_TRAPNO(%esp) / set trap type to AST
	movl	%esp, %eax
	pushl	$0
	pushl	%eax
	call	trap			/ trap(rp, 0)
	addl	$8, %esp
	jmp	_sys_rtt


	/*
	 * Return to supervisor
	 */
sr_sup:
	/ Check for a kernel preemption request
	cmpb	$0, %gs:CPU_KPRUNRUN
	jne	sr_sup_preempt		/ preemption
	/*
	 * Restore regs before doing iret to kernel mode
	 */
set_sup_regs:
	cmpw	$KGSSEL, REGS_GS(%esp)	/ test GS for return to user mode
	jne	.sel_reload		/ ... (this should not be necessary)
	addl	$16, %esp		/ don't care restoring seg selectors
	popa				/ restore general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	iret

sr_sup_preempt:
	sti
	pushl	$1			/ indicate asynchronous call
	call	kpreempt		/ kpreempt(1)
	addl	$4, %esp
	jmp	set_sup_regs

#endif	/* lint */

/*
 * int
 * intr_passivate(from, to)
 *      thread_id_t     from;           interrupt thread
 *      thread_id_t     to;             interrupted thread
 *
 *	intr_passivate(t, itp) makes the interrupted thread "t" runnable.
 *
 *	Since t->t_sp has already been saved, t->t_pc is all that needs
 *	set in this function.  The SPARC register window architecture
 *	greatly complicates this.
 *
 *	Returns interrupt level of the thread.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
intr_passivate(kthread_id_t from, kthread_id_t to)
{ return (0); }

#else	/* lint */

	ENTRY(intr_passivate)
	movl	8(%esp),%eax		/ interrupted thread 
	movl	$_sys_rtt,T_PC(%eax)	/ set T_PC for interrupted thread

	movl	4(%esp), %eax		/ interrupt thread
	movl	T_STACK(%eax), %eax	/ get the pointer to the start of
					/ of the interrupt thread stack
	movl	-4(%eax), %eax		/ interrupt level was the first
					/ thing pushed onto the stack
	ret
	SET_SIZE(intr_passivate)

#endif	/* lint */
 
/*
 * Return a thread's interrupt level.
 * This isn't saved anywhere but on the interrupt stack at interrupt 
 * entry at the bottom of interrupt stack.
 *
 * Caller 'swears' that this really is an interrupt thread.
 *
 * int
 * intr_level(t)
 *      kthread_id_t    t;
 */
 
#if defined(lint) || defined(__lint)
 
/* ARGSUSED */
int
intr_level(kthread_id_t t)
{ return (0); }
 
#else	/* lint */
 
        ENTRY(intr_level)
	movl	4(%esp), %eax
	movl	T_STACK(%eax), %eax	/ get the pointer to the start of
					/ of the interrupt thread stack
	movl	-4(%eax), %eax		/ interrupt level was the first
					/ thing pushed onto the stack
	ret
        SET_SIZE(intr_level)


/*
 * dosoftint(old_pil in %edi, softinfo in %edx)
 * Process software interrupts
 * Interrupts are disabled here.
 */
dosoftint:
	bsrl	%edx, %edx		/ find highest pending interrupt
	cmpl 	%edx, %edi		/ if curipl >= pri soft pending intr
	jae	_sys_rtt		/ skip

	movl	%gs:CPU_BASE_SPL, %eax	/ check for blocked interrupt threads
	cmpl	%edx, %eax		/ if basespl >= pri soft pending
	jae	_sys_rtt		/ skip

	lock				/ MP protect 
	btrl	%edx, softinfo		/ clear the selected interrupt bit
	jnc	dosoftint_again

	movl	%edx, CPU_M+CPU_PRI(%ebx) / set IPL to sofint level
	pushl	%edx
	call	*setspl			/ mask levels upto the softint level.
	popl	%eax			/ priority we are at in %eax.

	/ Get set to run interrupt thread.
	/ There should always be an interrupt thread since we allocate one
	/ for each level on the CPU, and if we release an interrupt, a new
	/ thread gets created.

	movl	CPU_INTR_THREAD(%ebx), %esi
	movl	T_LINK(%esi), %edx		/ unlink thread from CPU's list
	movl	%edx, CPU_INTR_THREAD(%ebx)
	/
	/ XXX Consider the new thread part of the same LWP so that
	/ XXX window overflow code can find the PCB.
	/
	movl	CPU_THREAD(%ebx), %edx
	movl	T_LWP(%edx), %ecx
	movl	%ecx, T_LWP(%esi)
	/
	/ Threads on the interrupt thread free list could have state already
	/ set to TS_ONPROC, but it helps in debugging if they're TS_FREE
	/ Could eliminate the next two instructions with a little work.
	/
	movl	$ONPROC_THREAD, T_STATE(%esi)
	/
	/ Push interrupted thread onto list from new thread.
	/ Set the new thread as the current one.
	/ Set interrupted thread's T_SP because if it is the idle thread,
	/ Resume() may use that stack between threads.
	/
	movl	%esp, T_SP(%edx)		/ mark stack for resume
	movl	%edx, T_INTR(%esi)		/ push old thread
	movl	%esi, CPU_THREAD(%ebx)		/ set new thread
	movl	T_STACK(%esi), %esp		/ interrupt stack pointer

	pushl	%eax			/ push ipl as first element in stack.
					/ see intr_passivate()

	/
	/ Initialize thread priority level from intr_pri
	/
	xorl	%ecx,%ecx
	movw	intr_pri, %cx		/ XXX global variable access???
	addl	%eax, %ecx		/ convert level to dispatch priority
	movw	%cx, T_PRI(%esi)

	sti				/ enable interrupts

	/
	/ Enabling interrupts (above) could raise the current
	/ IPL and base SPL. But, we continue processing the current soft
	/ interrupt and we will check the base SPL next time in the loop
	/ so that blocked interrupt thread would get a chance to run.
	/ 

	/
	/ Get handler address 
	/
	movl	$softvect, %esi		/ get softvect structure
	movl	AVH_LINK(%esi,%eax,8), %esi
loop3:
	testl	%esi, %esi		/ if pointer is null 
	je	loop_done3		/	we're done
	movl	AV_VECTOR(%esi), %edx	/ get the interrupt routine
	testl	%edx, %edx		/ if func is null
	je	loop_done3		/ then skip 

	pushl	AV_INTARG(%esi)		/ get argument to interrupt routine
	movl	AV_MUTEX(%esi), %ecx	/ unsafe driver ?
	cmpl	$0, %ecx
	je	.safedriver2
	pushl	%edx
	pushl	%ecx
	call	mutex_enter
	addl	$4, %esp
	popl	%edx
	call	*%edx			/ call interrupt routine with arg
	addl	$4,%esp
	pushl	AV_MUTEX(%esi)
	call	mutex_exit
	addl	$4,%esp
	movl	AV_LINK(%esi), %esi	/ get next routine on list
	jmp	loop3			/ keep looping until end of list
.safedriver2:
	call	*%edx			/ call interrupt routine with arg
	addl	$4,%esp
	movl	AV_LINK(%esi), %esi	/ get next routine on list
	jmp	loop3			/ keep looping until end of list
loop_done3:

	/ do accounting
	inc	CPU_SYSINFO_INTR(%ebx)	/ cpu_sysinfo.intr++

	cli				/ protect interrupt thread pool
					/ and softinfo & sysinfo
	movl	CPU_THREAD(%ebx),%esi	/ restore thread pointer

	/ if there is still an interrupt thread underneath this one
	/ then the interrupt was never blocked or released and the
	/ return is fairly simple.  Otherwise jump to softintr_thread_exit
	/ softintr_thread_exit expect esi to be curthread & ebx to be ipl.
	cmpl	$0, T_INTR(%esi)
	je	softintr_thread_exit

	/
	/ link the thread back onto the interrupt thread pool
	movl	CPU_INTR_THREAD(%ebx), %edx	
	movl	%edx, T_LINK(%esi)
	movl	%esi, CPU_INTR_THREAD(%ebx)
	/ set the thread state to free so kadb doesn't see it
	movl	$FREE_THREAD, T_STATE(%esi)
	/
	/ Switch back to the interrupted thread
	movl	T_INTR(%esi), %edx
	movl	%edx, CPU_THREAD(%ebx)
	movl	T_SP(%edx), %esp	/ restore stack pointer

	movl	%edi, CPU_M+CPU_PRI(%ebx) / set IPL to old level
	pushl	%edi
	call	*setspl			
	popl	%eax
dosoftint_again:
	movl	softinfo,%edx		/ any pending software interrupts
	orl	%edx,%edx
	jz	_sys_rtt
	jmp	dosoftint		/     process more software interrupts

softintr_thread_exit:
	/
	/ Put thread back on either the interrupt thread list if it is
	/ still an interrupt thread, or the CPU's free thread list, if it
	/ did a release_interrupt
	/ As a reminder, the regs at this point are
	/	%esi	interrupt thread
	testw	$T_INTR_THREAD, T_FLAGS(%esi)
	jz	softrel_intr
	
	/
	/ This was an interrupt thread, so clear the pending interrupt flag
	/ for this level
	/
	movl	T_STACK(%esi), %ebx
	movl	-4(%ebx), %ebx		/ IPL of this thread
	btrl	%ebx, %gs:CPU_INTR_ACTV / clear interrupt flag
	call	set_base_spl		/ set CPU's base SPL level
					/ interrupt vector already on stack
	/
	/ Set the thread state to free so kadb doesn't see it
	/
	movl	$FREE_THREAD, T_STATE(%esi)	
	/
	/ Put thread on either the interrupt pool or the free pool and
	/ call swtch() to resume another thread.
	/
	movl	%gs:CPU_INTR_THREAD, %edx	/ get list pointer
	movl	%edx, T_LINK(%esi)
	movl	%esi, %gs:CPU_INTR_THREAD
	call	splhigh				/ block all intrs below lock lvl
	call	swtch
	
	/ swtch() shouldn't return

softrel_intr:
	movl	$TS_ZOMB, T_STATE(%esi)		/ set zombie so swtch will free
	call	swtch
 
#endif	/* lint */

/*
 * long long add and subtract routines, callable from C.
 * Provided to manipulate hrtime_t values.
 * result may point to the same storage as either a or b.
 */
#if defined(lint) || defined(__lint)

/* result = a + b; */

/* ARGSUSED */
void
hrtadd(hrtime_t *result, hrtime_t *a, hrtime_t *b)
{}

#else	/* lint */

	ENTRY(hrtadd)
	pushl	%ebx
	movl	12(%esp), %eax		/ address of a
	movl	16(%esp), %ecx		/ address of b
	movl	(%eax), %edx		/ low word of a to edx
#ifndef _NO_LONGLONG
	movl	4(%eax), %ebx		/ high word of a to ebx
	addl	(%ecx), %edx		/ add low word of a to low word of b
	adcl	4(%ecx), %ebx		/ add high word of a to low word of b
	movl	8(%esp), %eax
	movl	%edx, (%eax)
	movl	%ebx, 4(%eax)
#else
	addl	(%ecx), %edx		/ add low word of a to low word of b
	movl	8(%esp), %eax
	movl	%edx, (%eax)
#endif
	popl	%ebx
	ret
	SET_SIZE(hrtadd)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/* result = a - b; */
/* ARGSUSED */
void
hrtsub(hrtime_t *result, hrtime_t *a, hrtime_t *b)
{}

#else	/* lint */
	ENTRY(hrtsub)
	pushl	%ebx
	movl	12(%esp), %eax		/ address of a
	movl	16(%esp), %ecx		/ address of b
	movl	(%eax), %edx		/ low word of a to edx
#ifndef _NO_LONGLONG
	movl	4(%eax), %ebx		/ high word of a to ebx
	subl	(%ecx), %edx		/ sub low word of a to low word of b
	sbbl	4(%ecx), %ebx		/ sub high word of a to low word of b
	movl	8(%esp), %eax
	movl	%edx, (%eax)
	movl	%ebx, 4(%eax)
#else
	subl	(%ecx), %edx		/ sub low word of a to low word of b
	movl	8(%esp), %eax
	movl	%edx, (%eax)
#endif
	popl	%ebx
	ret
	SET_SIZE(hrtsub)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* result = 0; */
/* ARGSUSED */
void
hrtzero(hrtime_t *result)
{}

#else	/* lint */

	ENTRY(hrtzero)
	movl	4(%esp), %eax
	movl	$0, (%eax)		/ zero low word
#ifndef _NO_LONGLONG
	movl	$0, 4(%eax)		/ zero high word
#endif
	ret
	SET_SIZE(hrtzero)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* return a == 0; */
/* ARGSUSED */
int
hrtiszero(hrtime_t *a)
{ return (0); }

#else	/* lint */

	ENTRY(hrtiszero)
	movl	4(%esp), %eax
	cmpl	$0, (%eax)
	jne	.hrtfalse
#ifndef _NO_LONGLONG
	cmpl	$0, 4(%eax)
	jne	.hrtfalse
#endif
	movl	$1, %eax
	ret
.hrtfalse:
	movl	$0, %eax
	ret
	SET_SIZE(hrtiszero)
#endif	/* lint */

/*
 * The following code is used to generate a 10 microsecond delay
 * routine.  It is initialized in pit.c.
 */
#if defined(lint) || defined(__lint)

/* return a == 0; */
/* ARGSUSED */
void
tenmicrosec(void)
{ }

#else	/* lint */

	.globl	microdata
	ENTRY(tenmicrosec)
	movl	microdata, %ecx		/ Loop uses ecx.
microloop:
	loop	microloop
	ret
	SET_SIZE(tenmicrosec)

	ENTRY(usec_delay)
	jmp	drv_usecwait		/ use the ddi routine
	SET_SIZE(usec_delay)

	ENTRY(___getefl)
	pushfl
	popl	%eax
	ret
	SET_SIZE(___getefl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/*
 * gethrtime() returns returns high resolution timer value
 */
hrtime_t
gethrtime(void)
{ return (0); }

hrtime_t
get_hrtime(void)
{ return (0); }

void
gethrestime(timespec_t *tp)
{
	tp->tv_sec = 0;
	tp->tv_nsec = 0;
}

hrtime_t
get_hrestime(void)
{
	hrtime_t ts;

	gethrestime((timespec_t *) &ts);
	return ts;
}

hrtime_t
gethrvtime(void)
{
	extern int hz; /* declaration in param.h protected against _ASM */

	klwp_t *lwp = ttolwp(curthread);
	struct mstate *ms = &lwp->lwp_mstate;

	if (curthread->t_proc_flag & TP_MSACCT)
		return gethrtime() - ms->ms_state_start + ms->ms_acct[LMS_USER];
	return (lwp->lwp_utime * hz * 1000 * 1000);
}

#else	/* lint */

	.globl	gethrtimef
	ENTRY_NP(gethrtime)
	jmp	*gethrtimef
	SET_SIZE(gethrtime)

	.globl	gethrestimef
	.globl	pc_gethrestime
gethrestimef:
	.long	pc_gethrestime
	.size	gethrestimef, 4

	ENTRY_NP(gethrestime)
	jmp	*gethrestimef
	SET_SIZE(gethrestime)

	ENTRY_NP(get_hrtime)
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	movl	$KDSSEL, %eax
	movl	$KFSSEL, %ecx
	movl	$KGSSEL, %edx
	movw	%ax, %ds
	movw	%ax, %es
	movw	%cx, %fs
	movw	%dx, %gs
	call	gethrtime
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	iret
	SET_SIZE(get_hrtime)

	ENTRY_NP(get_hrestime)
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	movl	$KDSSEL, %eax
	movl	$KFSSEL, %ecx
	movl	$KGSSEL, %edx
	subl	$8,%esp
	movw	%ax, %ds
	movw	%ax, %es
	movw	%cx, %fs
	movw	%dx, %gs
	pushl	%esp
	call	gethrestime
	movl	4(%esp),%eax
	movl	8(%esp),%edx
	addl	$12,%esp
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	iret
	SET_SIZE(get_hrestime)

	ENTRY_NP(gethrvtime)
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	movl	$KDSSEL, %eax
	movl	$KFSSEL, %ecx
	movl	$KGSSEL, %edx
	movw	%ax, %ds
	movw	%ax, %es
	movw	%cx, %fs
	movw	%dx, %gs
	call	gethrtime			/ get current time since boot
	movl	%gs:CPU_THREAD,%ecx
	movw	T_PROC_FLAG(%ecx),%ecx
	and	$TP_MSACCT,%ecx
	jz	gethrvtime1			/ jump if TP_MSACCT flag off
	movl	%gs:CPU_LWP,%ecx		/ current lwp
	subl	LWP_STATE_START(%ecx),%eax	/ subtract ms->ms_state_start
	sbbl	LWP_STATE_START+4(%ecx),%edx	/
	addl	LWP_ACCT_USER(%ecx),%eax	/ add ms->ms_acct[LMS_USER]
	adcl	LWP_ACCT_USER+4(%ecx),%edx	/
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	iret

	/ microstate-accounting OFF
gethrvtime1:
	movl	%gs:CPU_LWP,%ecx
	subl	LWP_MS_START(%ecx),%eax		/ subtract process start time
	sbbl	LWP_MS_START+4(%ecx),%edx
	pushl	%edx
	pushl	%eax				/ save elapsed time
	movl	$10000000,%eax			/ HZ * 1000000 to convert to ns
	movl	LWP_UTIME(%ecx),%ecx		/ get user time estimate
	mull	%ecx				/ convert to 64-bit ns
	cmpl	4(%esp),%edx			/ compare MSW to elapsed time
	jne	gethrvtime2
	cmpl	0(%esp),%eax			/ compare LSW
gethrvtime2:
	jb	gethrvtime3
	popl	%eax				/ elapsed time is smaller
	popl	%edx
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	iret

gethrvtime3:
	add	$8,%esp				/ discard elapsed
	popl	%gs				/ restore %gs
	popl	%fs
	popl	%es
	popl	%ds
	iret
	SET_SIZE(gethrvtime)

#endif	/* lint */

#if defined(lint) || defined(__lint)

int
get_tr()
{ return (0); }

#else	/* lint */

	ENTRY(get_tr)
	xorl	%eax, %eax
	str	%ax
	ret
	SET_SIZE(get_tr)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
loadtr(int sel)
{}

#else	/* lint */

	ENTRY(loadtr)
	movw	4(%esp), %ax
	ltr	%ax
	ret
	SET_SIZE(loadtr)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
loadldt(int sel)
{}

#else	/* lint */

	ENTRY(loadldt)
	movw	4(%esp), %ax
	lldt	%ax
	ret
	SET_SIZE(loadldt)

#endif	/* lint */


#if defined(lint) || defined(__lint)

void
pc_reset(void)
{}

#else	/* lint */

	ENTRY(pc_reset)
	movw	$0x64, %dx
	movb	$0xfe, %al
	outb	(%dx)
	hlt
/	NOT REACH
	SET_SIZE(pc_reset)

#ifdef XXX	/* not used in x86	*/
#define	REBOOT_PROT_32_START	0x10000
#define	REBOOT_PROT_16_START	0x1001f
#define	CS_REALMODE		0x1000
#define	EIP_REALMODE		0x100
#define	CS_16			0x100
#define	NO_PAGE		0x7fffffff
#define	REAL_MODE	0xfffffffe
#define	NOP_8		nop;nop;nop;nop;nop;nop;nop;nop
	.globl	asm_reboot
	.globl	asm_start_16
asm_reboot:
	movl	$gdt,%eax
	leal	256(%eax),%eax
	movl	$0xffff,(%eax)
	movl	$0x8f9b00,4(%eax)
	ljmp $CS_16, $REBOOT_PROT_16_START
asm_start_16:
	data16
	mov	%cr0,%eax
	data16
	and	$NO_PAGE,%eax
	data16
	movl	%eax,%cr0
	jmp	asm_start_16_1
asm_start_16_1:
	NOP_8
	NOP_8
	data16
	mov	%cr0,%eax
	data16
	and	$REAL_MODE,%eax
	data16
	movl	%eax,%cr0
	addr16
	data16
	ljmp	$CS_REALMODE,$EIP_REALMODE


#define	MAGIC_LOCATION	0x472 
#define	MAGIC_COOKIE	0x1234
	.globl	start_realmode_reboot_code
	.globl	end_realmode_reboot_code
start_realmode_reboot_code:
	xor	%eax,%eax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	data16
	mov	$MAGIC_LOCATION,%ebx
	data16
	mov	$MAGIC_COOKIE,(%ebx)
	addr16
	ljmp	$0xFFFF,$0x0	
end_realmode_reboot_code:
	NOP_8
#endif	/* XXX	*/

#endif	/* lint */


#if defined(lint) || defined(__lint)

void
swtch_to_4mbpage()
{}

#else	/* lint */

.globl 	swtch_to_4mbpage
	
	/ swtch_to_4mbpage(pde, index, num_entries)
	/ This code is executed of virtual address 0xc00xxxxx, but the ret
	/ instruction points to 0xe0xxxxxx, we load pagedir[0xe0x] with the
	/ 4Mb page frame number and return to it. 
swtch_to_4mbpage:
	movl	%ebp, %ecx
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	movl	%esp, %edx
	movl	%edx, %edi
	andl	$FOURMB_PAGEOFFSET, %edi		
	addl	$KERNELSHADOW_VADDR, %edi
					/ KERNELSHADOW_VADDR is c0000000
					/ ie 512MB below KERNELBASE
	movl	%edx, %esi
	subl	%edx, %ecx		/ size of stack frame of the caller
					/ of this function + this functions
					/ stack frame
	cld				
	repz
	smovb				/ copy stack frame to the new physical 
					/ page mapped at 0xc00xxxxx

	movl	0x08(%ebp), %edx
	movl	0x0c(%ebp), %ecx
	movl	0x10(%ebp), %esi

	
	
	movl	%cr3, %eax	
	leal	(%eax, %ecx, 4), %ecx
	movl	%eax, %cr3		/ flush tlb, 
	xorl	%eax, %eax

load_entry:
	movl	%edx, (%ecx, %eax, 4)		/ switch to 4Mb page

	incl	%eax
	cmpl	%eax, %esi
	jz	swtch_to_4mbpage_done
	addl	$0x400000, %edx
	jmp	load_entry

swtch_to_4mbpage_done:
	movl	%cr3, %eax
	movl	%eax, %cr3		/ flush tlb again

	popl	%edi			/ All these are being poped of the new
					/ physical address
	popl	%esi
	popl	%ebp
	ret
#endif	/* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
find_cpufrequency(int *pit_counter, int *processor_clks)
{
	return (0);
}

#else	/* lint */

.set	PITCTL_PORT,	0x43
.set	PIT_COUNTDOWN,	0x34
.set	PITCTRO_PORT,	0x40

	.globl	find_cpufrequency
find_cpufrequency:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	cmpw	$I86_P5_ARCH, cputype
	jl	not_a_p5_or_p6	
	testl	$X86_TSC, x86_feature
	jne	findcpufreq_again
	xorl	%eax, %eax
	jmp	find_cpufrequency_end
findcpufreq_again:
	movl	$PITCTL_PORT, %edx
	movl	$PIT_COUNTDOWN, %eax
	outb	(%dx)
	movl	$PITCTRO_PORT, %edx
	movl	$0xffffffff, %eax
	outb	(%dx)
	outb	(%dx)
	.byte 0x0f, 0x31
	movl	%eax, %ebx
	movl	%edx, %edi
	movl	$0x1000, %ecx
me_as_P5:	nop
	loop	me_as_P5
	.byte 0x0f, 0x31
	movl	%eax, %ecx
	movl	%edx, %esi
	xorl	%eax, %eax
	movl	$PITCTRO_PORT, %edx
	inb	(%dx)
	shll	$8, %eax
	inb	(%dx)
	xorl	%edx, %edx
	movb	%al, %dh
	movb	%ah, %dl
	movl	%edx, %eax
	cmpl	%esi, %edi
	jne	findcpufreq_again	
	subl	%ebx, %ecx
	movl	$0xffff, %edx
	subl	%eax, %edx			
	movl	8(%ebp), %eax
	movl	%edx, (%eax)
	movl	12(%ebp), %eax
	movl	%ecx, (%eax)
	movl	$1, %eax
find_cpufrequency_end:	
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret

not_a_p5_or_p6:
	movl	$0x1000, %ecx
	movl	$PITCTL_PORT, %edx
	movl	$PIT_COUNTDOWN, %eax
	outb	(%dx)
	movl	$PITCTRO_PORT, %edx
	movl	$0xffffffff, %eax
	outb	(%dx)
	outb	(%dx)
me_as486: decl	%ecx		/ 1 clock	on 386 its 2 clock
	nop			/ 1 clock		   3 clock
	jnz	me_as486	/ 3 clock		   8 clock
	nop
	xorl	%eax, %eax
	movl	$PITCTRO_PORT, %edx
	inb	(%dx)
	shll	$8, %eax
	inb	(%dx)
	xorl	%edx, %edx
	movb	%al, %dh
	movb	%ah, %dl
	movl	%edx, %eax
	movl	$0xffff, %edx
	subl	%eax, %edx			
	movl	8(%ebp), %eax
	movl	%edx, (%eax)
	movl	12(%ebp), %eax
	testw	$I86_486_ARCH, cputype
	je	its_386
	movl	$20512, (%eax)	/ Makes sense only if ecx has 0x1000 and is
				/ a 486 from intel
	jmp	find_cpufrequency_done
its_386:
	movl	$53280, (%eax)	/ Makes sense only if ecx has 0x1000 
	
find_cpufrequency_done:
	movl	$1, %eax
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret




.data
.align	4
.globl	cpu_vendor
.globl	cpu_family
.globl	cpu_model
.globl	cpu_stepping
.globl	cpu_features

cpu_vendor:	  .long 0,0,0		/ Vendor ID string returned
cpu_family:	  .long 0		/ CPU Family returned
cpu_model:	  .long 0		/ CPU Model returned
cpu_stepping: 	  .long 0		/ CPU Stepping returned
cpu_features:				/ Features, reserve 27 dwords
		  .long 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cpu_id_parm:	  .long 0	     	/ Highest value CPUID invocation
intel_cpu_id:     .string 	"GenuineIntel"
amd_cpu_id:	  .string	"AuthenticAMD"

pentium_tm:	  .string	"Pentium"
p6_tm:		  .string	"Pentium Pro"
k5_tm:		  .string	"K5"
_i386_tm:	  .string	"i386"
_i486_tm:	  .string	"i486"

	.globl	cpu_namestrp
	.globl	x86_feature
	.globl	cr4_value
cr4_value:	  .long 0
cpu_namestrp:	  .long	0
x86_feature:		  .long 0
	/ pagetable that would allow a one to one mapping of the 
	/ first 4K of kernel text.
	.comm	one2one_pagetable, 8192
#endif		/* lint */
