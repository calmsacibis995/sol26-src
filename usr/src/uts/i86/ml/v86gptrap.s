/*
 *	Copyright (c) 1992 by Sun Microsystems, Inc.
 *	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
 *	UNIX System Laboratories, Inc.
 *	the copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 */

#pragma	ident "@(#)v86gptrap.s	1.5	96/05/29 SMI"

#include <sys/asm_linkage.h>

#ifdef _VPIX

#if !defined(lint)
#include "assym.s"
#endif	/* lint */

/ CGA Status Port Register Extension:
/ Copyright (c) 1989 Phoenix Technologies Ltd.
/ All Rights Reserved
/
/
/	Copyright (c) 1987, 1988 Microsoft Corporation
/	  All Rights Reserved

/	This Module contains Proprietary Information of Microsoft
/	Corporation and should be treated as Confidential.

/ VP/ix General Protection Fault handler.  Emulate legal non-I/O
/ exceptions directly: int, cli, sti, pushf, popf, iret.
/ Make others (I/O and illegals) cause a task switch to the ECT at user
/ level.
/ NOTE: THIS CODE IS REACHED THROUGH AN INTERRUPT GATE.  Interrupts
/ are off when we enter here.  We leave them off so that device interrupts
/ do not occur here and so will always return to user mode through v86vint.
/ We can take page faults here, but they are rare enough for it not to
/ matter that v86vint is not called on return to user mode.
/
/

	.set    OLDIEFL, 12

	.set    VMFLAG,  0x00020000
	.set    CLNFLAGS,0x00007000	 / Set iopl and nt to 0
	.set    IFLAG,   0x00000200
	.set    TFLAG,   0x00000100
	.set    OPSAVED, 2

	.set    OP_ARPL, 0x63
	.set    OP_PUSHF,0x9C
	.set    OP_POPF, 0x9D
	.set    OP_INT,  0xCD
	.set    OP_INTO, 0xCE
	.set    OP_IRET, 0xCF
	.set    OP_CLI,  0xFA
	.set    OP_STI,  0xFB
	.set    OFLOVEC, 4

	.set	OP_INB_R,	0xEC
	.set	PORT_VTR,	0x3DA
	.set	EN_VTR,		0x0001

/*
 * The following table is required for the CGA Status Port Register.
 */
	.data
	.align 4
	.globl cs_table
cs_table:
	.byte	0x00, 0x01, 0x00, 0x01
	.byte	0x00, 0x01, 0x00, 0x01
	.byte	0x00, 0x01, 0x00, 0x01
	.byte	0x01, 0x01, 0x09, 0x09
	.byte	0x09, 0x09, 0x09, 0x09
	.byte	0x09, 0x09, 0x09, 0x09
	.byte	0x09, 0x09, 0x09, 0x01
	.byte	0x00, 0x01, 0x00, 0x01
cs_table_end:
	.long .
	.align 4
cs_table_beg:
	.long cs_table
cs_table_ptr:
	.long cs_table
	.align 4

	.text

	.globl gptrap

#if defined(lint)

/* ARGSUSED */
void
v86gptrap()
{}

#else	/* lint */

	/
	/ NOTE: seg_vpix maps the pages read/write so no copy-on-write
	/	faults are expected.
	/

	ENTRY_NP(v86gptrap)
	testl   $VMFLAG,OLDIEFL(%esp)   / Was Virtual 8086 the culprit?
	je      gotrap			/ No..go to regular gp trap logic
					/ (after turning on interrupts)

	/ Set up regs structure

	pushl	$13			/ already have error code on stack
	lidt	%cs:KIDTptr		/ Use CS since DS is not set yet
	pusha				/ save all registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	movl    %esp,%ebp		/ Set up a stack frame

	/ load kernel selectors

	lidt    %cs:KIDTptr		/  Set IDT for kernel operations
	movw	$KDSSEL, %ax
	movw	%ax, %ds
	movw	$KGSSEL, %ax
	movw	%ax, %gs
	movw	$KFSSEL, %ax
	movw	%ax, %fs

	movzwl  REGS_CS(%ebp),%eax	/ Get address of offending byte
	addl    %eax, %eax		/ prepare for multiply of 16 for cs
	movzwl  REGS_PC(%ebp),%ebx
	leal    (%ebx, %eax, 8), %eax   / Compute %eax = cs * 16 + ip
	movl	%gs:CPU_THREAD, %esi	
	movl	$gpfetch_done, T_LOFAULT(%esi)
	movb	(%eax), %bl
gpfetch_done:
	movl	$0, T_LOFAULT(%esi)

	movl    XT_VFLPTR, %edi 	/ EDI = Ptr to 8086 virtual flags

	cmpb    $OP_CLI, %bl
	je      op_cli
	cmpb    $OP_STI, %bl
	je      op_sti

	cmpb    $OP_INT, %bl
	je      op_int
	cmpb    $OP_IRET, %bl
	je      op_iret

	cmpb    $OP_PUSHF, %bl
	je      op_pushf
	cmpb    $OP_POPF, %bl
	je      op_popf
	cmpb    $OP_INTO, %bl
	je      op_into

	testl	$EN_VTR,XT_OP_EMUL	/ is this opcode enabled?
	je	go_ECT			/ no - passthru to ECT.
	cmpb	$OP_INB_R,%bl		/ is this an inb()?
	jne	go_ECT			/ no - passthru to ECT.
	cmpw	$PORT_VTR,REGS_EDX(%ebp)/ yes - is it for PORT_VTR?
	je	op_inb_vtr		/ yes - go emulate

/
/  Can't emulate here.  Force trap to ECT.
/
go_ECT:
	movb    %bl,XT_MAGICTRAP       / Save trapped code in XTSS
	movb    $OPSAVED,XT_MAGICSTAT  / Set flag in XTSS

	movl	$go_ECT_done, T_LOFAULT(%esi)
	movb	$OP_ARPL, (%eax)
go_ECT_done:
	movl	$0, T_LOFAULT(%esi)

	lidt    KIDT2ptr
	addl	$16, %esp		/ no need to reload user selectors
	popa				/ restore user general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	iret

	.align	4
go1_ECT:
	movzwl  REGS_CS(%ebp),%eax      / Get address of offending byte
	addl    %eax, %eax		/ prepare for mult of 16 for cs
	movzwl  REGS_PC(%ebp),%ebx
	leal    (%ebx, %eax, 8), %eax   / Compute %eax = cs * 16 + ip
	movl	$go_ECT_done, T_LOFAULT(%esi)
	movb	(%eax), %bl
	jmp     go_ECT			/ Force ECT execution

/
/  Emulate 'cli' opcode
/
	.align	4
op_cli:
	incw    REGS_PC(%ebp)		/ Pass opcode on return
	movl	$op_cli_done, T_LOFAULT(%esi)	/ $op_cli_done is lofault value
	movl	$0,(%edi)		/ Turn off ints in 8086 virtual flags
op_cli_done:
	movl	$0, T_LOFAULT(%esi)	/ clear t_lofault

	lidt    KIDT2ptr
	addl	$16, %esp		/ no need to reload user selectors
	popa				/ restore user general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	iret

/
/  Emulate 'sti' opcode
/
	.align	4
op_sti:
	incw    REGS_PC(%ebp)		/ Pass opcode on return
	movl	$op_sti_done, T_LOFAULT(%esi)	/ $op_sti_done is lofault value
	movl	$IFLAG,(%edi)		/ Turn on ints in 8086 virtual flags
op_sti_done:
	movl	$0, T_LOFAULT(%esi)	/ clear t_lofault
	testb   $0xFF,XT_INTR_PIN   	/ Interrupts for virtual machine?
	jnz     go1_ECT			/ Yes - force ECT to post it

	lidt    KIDT2ptr
	addl	$16, %esp		/ no need to reload user selectors
	popa				/ restore user general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	iret

/
/  Emulate 'pushf' opcode
/
	.align	4
op_pushf:
	incw    REGS_PC(%ebp)		/ Pass opcode on return
	subw    $2,REGS_UESP(%ebp)      / Pushing 1 16-bit word
	movzwl  REGS_SS(%ebp),%eax      / Get address of stack
	addl    %eax, %eax		/ prepare for multiply of 16 for ss
	movzwl  REGS_UESP(%ebp),%ebx
	leal    (%ebx, %eax, 8), %eax   / Compute %eax = ss * 16 + sp
	movw    REGS_EFL(%ebp),%bx
	andw    $-1!IFLAG,%bx		/ Prepare to set correct IF bit
	movl	$pushf_done, T_LOFAULT(%esi)	/ $pushf_done is lofault value
	orw	(%edi),%bx		/ Include IF bit in 8086 virtual flags
	movw    %bx,(%eax)		/ Put new value on stack
pushf_done:
	movl	$0, T_LOFAULT(%esi)	/ clear t_lofault
	movw	%bx,%ax			/ Get the flags value
	andw	$IFLAG,%ax		/ Isolate the IF status
	jmp     comf			/ %ax = Virtual IF status

/
/  Emulate 'popf' opcode
/
	.align	4
op_popf:
	incw    REGS_PC(%ebp)	   	/ Pass opcode on return
	movzwl  REGS_SS(%ebp),%eax    	/ Get address of stack
	addl    %eax, %eax		/ prepare for multiply of 16 for ss
	movzwl  REGS_UESP(%ebp),%ebx
	addw    $2,REGS_UESP(%ebp)      / Pop 1 16-bit word
	leal    (%ebx, %eax, 8), %eax   / Compute %eax = ss * 16 + sp
	movl	$popf_done, T_LOFAULT(%esi)	/ $popf_done is lofault value
	movw    (%eax),%bx		/ Get new value from stack
	movw    %bx,%ax
	andw    $IFLAG,%ax
	movw	%ax,(%edi)		/ Save the interrupt flag status
popf_done:
	movl	$0, T_LOFAULT(%esi)	/ clear t_lofault
	andw    $-1!CLNFLAGS,%bx	/ Make sure IOPL=0, NT=0
	orw     $IFLAG,%bx		/ Don't let real I bit be off
	movw    %bx,REGS_EFL(%ebp)      / Replace flags plus I-bit

comf:				   	/ NOTE: %ax = Virtual IF status
	cmpw    $0,%ax		  	/ Interrupt flag set?
	je      comf1		   	/ No - no interrupt processing
	testb   $0xFF,XT_INTR_PIN   	/ Interrupts for virtual machine?
	jnz     go1_ECT		 	/ Yes - force ECT to post it
comf1:
	lidt    KIDT2ptr
	addl	$16, %esp		/ no need to reload user selectors
	popa				/ restore user general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	iret

/
/  Emulate 'iret' opcode
/
	.align	4
op_iret:
	incw    REGS_PC(%ebp)	   	/ Pass opcode on return
	movzwl  REGS_SS(%ebp),%eax      / Get address of stack
	addl    %eax, %eax	      	/ prepare for multiply of 16 for ss
	movzwl  REGS_UESP(%ebp),%ebx
	addw    $6,REGS_UESP(%ebp)
	leal    (%ebx, %eax, 8), %eax   / Compute %eax = ss * 16 + sp
	movl	$iret_done, T_LOFAULT(%esi)
	movl    (%eax),%ebx	     	/ Get new CS & IP from user stack
	movw	%bx, REGS_PC(%ebp)	/ Fix up low EIP on current stack
	roll    $16,%ebx		/ Exchange %bxh & %bxl
	movw    %bx,REGS_CS(%ebp)       / Fix up CS on current stack
	movw    4(%eax),%bx	     	/ Get flags
	andw    $-1!CLNFLAGS,%bx	/ Set IOPL=0, NT=0
	movw    %bx,%ax
	andw    $IFLAG,%ax
	movw	%ax,(%edi)
iret_done:
	movl	$0, T_LOFAULT(%ebp)
	orw     $IFLAG,%bx	      	/ Don't let real I bit be off
	movw    %bx,REGS_EFL(%ebp)
	jmp     comf		    	/ %ax = Virtual IF status

/
/  Emulate 'int'  and 'into' opcodes
/  Note: int3 opcode is emulated in the ECT
/
	.align	4
op_into:
	movl    $OFLOVEC,%ecx	   	/ Point to vector
	btl     %ecx,XT_IMASKBITS  	/ Does ECT want control for INTO?
	jc      go_ECT	      		/ Yes - force control to ECT
	decw    REGS_PC(%ebp)	   	/ Kludge for add of 2 below
	jmp     all_ints

	.align	4
op_int:
	movzwl  REGS_CS(%ebp),%eax      / Get address of offending byte
	addl    %eax, %eax	      	/ prepare for multiply of 16 for cs
	movzwl  REGS_PC(%ebp),%ecx
	leal    (%ecx, %eax, 8), %eax   / Compute %eax = cs * 16 + ip
	movl	$int_done, T_LOFAULT(%esi)
	movzbl	1(%eax), %ecx		/ Get interrupt number
	btl     %ecx,XT_IMASKBITS  	/ Does ECT want control for INT?
	jc      go_ECT	      		/ Yes - force control to ECT
all_ints:
	shll    $2,%ecx		 	/ Multiply by 4 for vector ptr
	subw    $6,REGS_UESP(%ebp)      / Adjust stack for user CS:IP, flags
	movzwl  REGS_SS(%ebp),%eax      / Get address of stack
	addl    %eax, %eax	      	/ prepare for multiply of 16 for ss
	movzwl  REGS_UESP(%ebp),%ebx
	leal    (%ebx, %eax, 8), %eax   / Compute %eax = ss * 16 + sp
	movw    REGS_CS(%ebp),%bx       / Get old CS
	shll    $16,%ebx		/ Move up the old CS to top
	movw    REGS_PC(%ebp),%bx       / Get old IP from curr stack
	addw    $2,%bx		  / Skip INT inst. when done
	movl	%ebx, (%eax)
	movw    REGS_EFL(%ebp),%bx      / Get current flags
	andw    $-1![IFLAG|TFLAG],%bx   / Prepare to set correct IF,TF bits
	orw	(%edi),%bx		/ Include virtual flags I-bit
	movw    %bx,4(%eax)	     	/ Push flags on user stack
	movl	$0,(%edi)
	movl    (%ecx),%ebx	     	/ Get 8086 interrupt vector
	movw    %bx,REGS_PC(%ebp)       / Set him up to go back to isr
	shrl    $16,%ebx
	movw    %bx,REGS_CS(%ebp)
int_done:
	movl	$0, T_LOFAULT(%esi)
	lidt    KIDT2ptr
	addl	$16, %esp		/ no need to reload user selectors
	popa				/ restore user general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	iret

	.align	4
gotrap:
	/XXX_check sti		  	/ Because ints were off on entry
	jmp     gptrap

/
/ Extension for CGA Status Port Read
/
	.align	4
op_inb_vtr:
	movl	cs_table_ptr,%edi	/ get address of current status
	movl	REGS_EAX(%ebp),%eax	/ get original eax (ah) contents
	movb	(%edi),%al		/ get new status in AL
	movl	%eax, REGS_EAX(%ebp)	/ put new status back in EAX
	incl	%edi
	cmpl	%edi, cs_table_end	/ have we overflowed?
	ja	vtr_cs_ok		/ no
	movl	$cs_table,%edi		/ yes - reset the ptr
vtr_cs_ok:
	movl	%edi,cs_table_ptr
	incw	REGS_PC(%ebp)		/ skip OPcode on return

	lidt    KIDT2ptr
	addl	$16, %esp		/ no need to reload user selectors
	popa				/ restore user general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	iret
	SET_SIZE(v86gptrap)

#endif /* lint */

#endif /* _VPIX */
