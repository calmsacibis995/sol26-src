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

#pragma ident	"@(#)int_entry.s	1.14	95/09/11 SMI"

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include <sys/trap.h>
#include <sys/psw.h>

/*
 * Nothing in this file is interesting to lint.
 */
#if !(defined(lint) || defined(__lint))

#include <assym.s>


#define MKIVCT(n) .align	4;\
	.globl	ivct/**/n;\
ivct/**/n/**/:;\
	pushl	$0;\
	pushl	$/**/n/**/ - 0x20;\
	jmp	cmnint;\

/The above #define will expand as follows for MKIVCT(32)
/ .align 4; .globl ivct32; ivct32:; pushl $0; pushl $32 -0x20; jmp cmnint

/ Dual-Mode floating point support:
/ Copyright (c) 1989 Phoenix Technologies Ltd.
/ All Rights Reserved

	.set	IS_LDT_SEL,	0x04

	.text
	.globl	cmnint
	.globl	cmntrap

	.align	4
	.globl	div0trap
div0trap:
	pushl	$0
	pushl	$0
	jmp	cmntrap

/	.globl	mon1sel
/	.globl	mon1off

	.align	4
	.globl	dbgtrap
dbgtrap:
/ This dmon stuff causes a kernel page-fault
/ when a there is a single-step through an lcall.
/ Otherwise, it almost works.
/testw	$IS_LDT_SEL, 4(%esp)
/jnz	nodbgmon
/testw	$0xFFFF, %cs:mon1sel
/jz	nodbgmon

/ A little explaination of the following code:
/	The 2 pushes make space for the selector and offset of
/	the monitor entry point on the stack AND save eax.
/	Note that the second xchgl operation will restore eax
/	to its original value, thus when we do the retl, it will
/	look to the monitor like we did the "int" directly to it.
/cli
/pushl	%eax
/pushl	%eax
/movl	%cs:mon1sel, %eax
/xchgl	%eax, 4(%esp)
/movl	%cs:mon1off, %eax
/xchgl	%eax, (%esp)
/lret
	.align	4
nodbgmon:
	pushl	$0
	pushl	$1
	jmp	cmntrap

	.align	4
	.globl	nmiint
	.globl	nmivect
nmiint:
	pushl	$0
	pushl	$2		/ just to have same reg layout.
	lidt	%cs:KIDTptr	/ Use CS since DS is not set yet
	pusha			/ save all registers
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	movw	$KDSSEL, %ax	/ Just set all the segment registers on traps
	movw	%ax, %ds
	movw	%ax, %es
	movw	$KFSSEL, %ax
	movw	%ax, %fs
	movw	$KGSSEL, %ax
	movw	%ax, %gs

	movl	%esp, %ebp
	pushl	%ebp			/ pointer to reg structure is 2nd
					/ argument.
	movl	nmivect, %esi		/ get autovect structure
loop1:
 	cmpl  	$0, %esi		/ if pointer is null 
 	je    	.intr_ret		/ 	we're done
 	movl	AV_VECTOR(%esi), %edx	/ get the interrupt routine
 	pushl	AV_INTARG(%esi)		/ get argument to interrupt routine
	call	*%edx			/ call interrupt routine with arg
	addl	$4,%esp
 	movl	AV_LINK(%esi), %esi	/ get next routine on list
	jmp	loop1			/ keep looping until end of list

.intr_ret:
	popl	%ebp
	jmp	_sys_rtt


/	.globl	mon3sel
/	.globl	mon3off

	.align	4
	.globl	brktrap
brktrap:
/ XXX - commented out by BillT.  Needs more analysis.
/	testw	$IS_LDT_SEL, 4(%esp)
/	jnz	nobrkmon
/	testw	$0xFFFF, %cs:mon3sel
/	jz	nobrkmon

/ See the comment in dbgtrap.
/	cli
/	pushl	%eax
/	pushl	%eax
/	movl	%cs:mon3sel, %eax
/	xchgl	%eax, 4(%esp)
/	movl	%cs:mon3off, %eax
/	xchgl	%eax, (%esp)
/	lret

	.align	4
nobrkmon:
	pushl	$0
	pushl	$3
	jmp	cmntrap

	.align	4
	.globl	ovflotrap
ovflotrap:
	pushl	$0
	pushl	$4
	jmp	cmntrap

	.align	4
	.globl	boundstrap
boundstrap:
	pushl	$0
	pushl	$5
	jmp	cmntrap

	.align	4
	.globl	invoptrap
invoptrap:
	pushl	$0
	pushl	$6
	jmp	cmntrap

	.align	4
	.globl	ndptrap
ndptrap:		/ We want to do this fast as every
			/ process using fp will take this
			/ after context switch
/we do below the frequent path in fpnoextflt
/ all other cases we let the trap code handle it
	pushl	%eax
	pushl	%ebx
	pushl	%ds
	pushl	%fs
	movl	$KDSSEL, %ebx
	movl	$KFSSEL, %eax
	movw	%bx, %ds
	movw	%ax, %fs
	LOADCPU(%ebx)
	cmpl	$0, fpu_exists
	je	.handle_in_trap		/ let trap handle no fp case
	movl	CPU_THREAD(%ebx), %eax	/ %eax = curthread
	movl	$FPU_EN, %ebx
	movl	T_LWP(%eax), %eax	/ eax = lwp
	testl	%eax, %eax
	jz	.handle_in_trap		/ should not happen?
#if	LWP_PCB_FPU != 0
	addl	$LWP_PCB_FPU, %eax 	/ &lwp->lwp_pcb.pcb_fpu
#endif
	testl	%ebx, PCB_FPU_FLAGS(%eax)
	jz	.handle_in_trap		/ must be the first fault
	clts
	frstor	(%eax)
	andl	$-1![FPU_VALID], PCB_FPU_FLAGS(%eax)
	popl	%fs
	popl	%ds
	popl	%ebx
	popl	%eax
	iret

.handle_in_trap:
	popl	%fs
	popl	%ds
	popl	%ebx
	popl	%eax
	pushl	$0
	pushl	$7
	jmp	cmninttrap

	.align	4
	.globl	syserrtrap
	.globl	KIDTptr
syserrtrap:
#ifdef  _VPIX
	cmp	$0, %gs:CPU_V86PROCFLAG  / Is the process a dual mode process?
	jz      cont_dbl        / No - process double fault
	lidt    %cs:KIDTptr     / Reset to original interrupt table
	addl    $4,%esp         / Get rid of error code on stack
	mov	$0, %gs:CPU_V86PROCFLAG  / Reset to normal mode
	iret                    / Task switch and refault (GP this time)
	jmp     syserrtrap      / Process double fault

	.align	4
cont_dbl:
#endif
	/*
	 * Check the CPL in the TSS to see what mode
	 * (user or kernel) we took the fault in.  At this
	 * point we are running in the context of the double
	 * fault task (dftss) but the CPU's task points to
	 * the previous task (ktss) where the process context
	 * has been saved as the result of the task switch.
	 */
	cli				/ disable interrupts
	movl	%gs:CPU_TSS, %eax	/ get the TSS
	movl	TSS_SS(%eax), %ebx	/ save the fault SS
	movl	TSS_ESP(%eax), %edx	/ save the fault ESP
	testw	$CPL_MASK, TSS_CS(%eax)	/ user mode ?
	jz	make_frame
	movw	TSS_SS0(%eax), %ss	/ get on the kernel stack
	movl	TSS_ESP0(%eax), %esp

	/*
	 * Clear the NT flag to avoid a task switch when the process
	 * finally pops the EFL off the stack via an iret.  Clear
	 * The TF flag since that is what the processor does for
	 * a normal exception. Clear the IE flag so that interrupts
	 * remain disabled.
	 */ 
	movl	TSS_EFL(%eax), %ecx
	andl    $-1![PS_NT|PS_T|PS_IE], %ecx
	pushl	%ecx
	popfl				/ restore the EFL
	movw	TSS_LDT(%eax), %cx	/ restore the LDT
	lldt	%cx

	/*
	 * Restore process segment selectors.
	 */
	movw	TSS_DS(%eax), %ds
	movw	TSS_ES(%eax), %es
	movw	TSS_FS(%eax), %fs
	movw	TSS_GS(%eax), %gs

	/*
	 * Restore task segment selectors.
	 */
	movl	$KDSSEL, TSS_DS(%eax)
	movl	$KDSSEL, TSS_ES(%eax)
	movl	$KDSSEL, TSS_SS(%eax)
	movl	$KFSSEL, TSS_FS(%eax)
	movl	$KGSSEL, TSS_GS(%eax)

	/*
	 * Clear the TS bit, the busy bits in both task
	 * descriptors, and switch tasks.
	 */
	clts
	leal	gdt, %ecx
	movl	DFTSSSEL+4(%ecx), %esi
	andl    $-1![0x200], %esi
	movl	%esi, DFTSSSEL+4(%ecx)
	movl	UTSSSEL+4(%ecx), %esi
	andl    $-1![0x200], %esi
	movl	%esi, UTSSSEL+4(%ecx)
	movw	$UTSSSEL, %cx
	ltr	%cx

	/*
	 * Restore part of the process registers.
	 */
	movl	TSS_EBP(%eax), %ebp
	movl	TSS_ECX(%eax), %ecx
	movl	TSS_ESI(%eax), %esi
	movl	TSS_EDI(%eax), %edi

make_frame:
	/*
	 * Make a trap frame.  Leave the error code (0) on 
	 * the stack since the first word on a trap stack is
	 * unused anyway.
	 */
	pushl   %ebx			/ fault SS
	pushl   %edx			/ fault ESP
	pushl   TSS_EFL(%eax)		/ fault EFL
	pushl   TSS_CS(%eax)		/ fault CS
	pushl   TSS_EIP(%eax)		/ fault EIP
	pushl   $0			/ error code
	pushl   $8			/ trap number
	movl	TSS_EBX(%eax), %ebx	/ restore EBX
	movl	TSS_EDX(%eax), %edx	/ resotre EDX
	movl	TSS_EAX(%eax), %eax	/ restore EAX
	sti				/ enable interrupts
	jmp	cmntrap

	.align	4
	.globl	overrun
overrun:
	pushl	$0
	pushl	$9
	jmp	cmninttrap

	.align	4
	.globl	invtsstrap
invtsstrap:
	pushl	$10		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	segnptrap
segnptrap:
	pushl	$11		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	stktrap
stktrap:
	pushl	$12		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	gptrap
gptrap:
	pushl	$13		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	pftrap
pftrap:
	pushl	$14		/ already have error code on stack
	jmp	cmntrap

	.align	4
	.globl	resvtrap
resvtrap:
	pushl	$0
	pushl	$15
	jmp	cmntrap

	.align	4
	.globl	ndperr
ndperr:
	pushl	$0
	pushl	$16
	jmp	cmninttrap

	.align	4
	.globl	achktrap
achktrap:
	pushl	$17
	jmp	cmntrap

	.align	4
	.globl	invaltrap
invaltrap:
	pushl	$0
	pushl	$18
	jmp	cmntrap

	.align	4
	.globl	invalint
invalint:
	pushl	$0
	pushl	$17
	jmp	cmnint

        .align  4
        .globl  fasttrap
        .globl  fasttable
fasttrap:
        cmpl    $T_LASTFAST, %eax
        ja      invalfast
        jmp     *%cs:fasttable(, %eax, 4)
invalfast:
/	Fast system call function number was illegal.  Make it look to the
/	user as though the INT failed.  Modify the EIP to point before the
/	INT, push the expected error code and fake a GP fault.
	subl	$2, (%esp)
	pushl	$T_FASTTRAP \* 8 + 2
	jmp	gptrap

        .align  4
        .globl  fast_null
fast_null:
        orw     $PS_C, 8(%esp)  / set carry bit in user flags
        iret

MKIVCT(32)
MKIVCT(33)
MKIVCT(34)
MKIVCT(35)
MKIVCT(36)
MKIVCT(37)
MKIVCT(38)
MKIVCT(39)
MKIVCT(40)
MKIVCT(41)
MKIVCT(42)
MKIVCT(43)
MKIVCT(44)
MKIVCT(45)
MKIVCT(46)
MKIVCT(47)
MKIVCT(48)
MKIVCT(49)
MKIVCT(50)
MKIVCT(51)
MKIVCT(52)
MKIVCT(53)
MKIVCT(54)
MKIVCT(55)
MKIVCT(56)
MKIVCT(57)
MKIVCT(58)
MKIVCT(59)
MKIVCT(60)
MKIVCT(61)
MKIVCT(62)
MKIVCT(63)
MKIVCT(64)
MKIVCT(65)
MKIVCT(66)
MKIVCT(67)
MKIVCT(68)
MKIVCT(69)
MKIVCT(70)
MKIVCT(71)
MKIVCT(72)
MKIVCT(73)
MKIVCT(74)
MKIVCT(75)
MKIVCT(76)
MKIVCT(77)
MKIVCT(78)
MKIVCT(79)
MKIVCT(80)
MKIVCT(81)
MKIVCT(82)
MKIVCT(83)
MKIVCT(84)
MKIVCT(85)
MKIVCT(86)
MKIVCT(87)
MKIVCT(88)
MKIVCT(89)
MKIVCT(90)
MKIVCT(91)
MKIVCT(92)
MKIVCT(93)
MKIVCT(94)
MKIVCT(95)
MKIVCT(96)
MKIVCT(97)
MKIVCT(98)
MKIVCT(99)
MKIVCT(100)
MKIVCT(101)
MKIVCT(102)
MKIVCT(103)
MKIVCT(104)
MKIVCT(105)
MKIVCT(106)
MKIVCT(107)
MKIVCT(108)
MKIVCT(109)
MKIVCT(110)
MKIVCT(111)
MKIVCT(112)
MKIVCT(113)
MKIVCT(114)
MKIVCT(115)
MKIVCT(116)
MKIVCT(117)
MKIVCT(118)
MKIVCT(119)
MKIVCT(120)
MKIVCT(121)
MKIVCT(122)
MKIVCT(123)
MKIVCT(124)
MKIVCT(125)
MKIVCT(126)
MKIVCT(127)
MKIVCT(128)
MKIVCT(129)
MKIVCT(130)
MKIVCT(131)
MKIVCT(132)
MKIVCT(133)
MKIVCT(134)
MKIVCT(135)
MKIVCT(136)
MKIVCT(137)
MKIVCT(138)
MKIVCT(139)
MKIVCT(140)
MKIVCT(141)
MKIVCT(142)
MKIVCT(143)
MKIVCT(144)
MKIVCT(145)
MKIVCT(146)
MKIVCT(147)
MKIVCT(148)
MKIVCT(149)
MKIVCT(150)
MKIVCT(151)
MKIVCT(152)
MKIVCT(153)
MKIVCT(154)
MKIVCT(155)
MKIVCT(156)
MKIVCT(157)
MKIVCT(158)
MKIVCT(159)
MKIVCT(160)
MKIVCT(161)
MKIVCT(162)
MKIVCT(163)
MKIVCT(164)
MKIVCT(165)
MKIVCT(166)
MKIVCT(167)
MKIVCT(168)
MKIVCT(169)
MKIVCT(170)
MKIVCT(171)
MKIVCT(172)
MKIVCT(173)
MKIVCT(174)
MKIVCT(175)
MKIVCT(176)
MKIVCT(177)
MKIVCT(178)
MKIVCT(179)
MKIVCT(180)
MKIVCT(181)
MKIVCT(182)
MKIVCT(183)
MKIVCT(184)
MKIVCT(185)
MKIVCT(186)
MKIVCT(187)
MKIVCT(188)
MKIVCT(189)
MKIVCT(190)
MKIVCT(191)
MKIVCT(192)
MKIVCT(193)
MKIVCT(194)
MKIVCT(195)
MKIVCT(196)
MKIVCT(197)
MKIVCT(198)
MKIVCT(199)
MKIVCT(200)
MKIVCT(201)
MKIVCT(202)
MKIVCT(203)
MKIVCT(204)
MKIVCT(205)
MKIVCT(206)
MKIVCT(207)
MKIVCT(208)
MKIVCT(209)
MKIVCT(210)
MKIVCT(211)
MKIVCT(212)
MKIVCT(213)
MKIVCT(214)
MKIVCT(215)
MKIVCT(216)
MKIVCT(217)
MKIVCT(218)
MKIVCT(219)
MKIVCT(220)
MKIVCT(221)
MKIVCT(222)
MKIVCT(223)
MKIVCT(224)
MKIVCT(225)
MKIVCT(226)
MKIVCT(227)
MKIVCT(228)
MKIVCT(229)
MKIVCT(230)
MKIVCT(231)
MKIVCT(232)
MKIVCT(233)
MKIVCT(234)
MKIVCT(235)
MKIVCT(236)
MKIVCT(237)
MKIVCT(238)
MKIVCT(239)
MKIVCT(240)
MKIVCT(241)
MKIVCT(242)
MKIVCT(243)
MKIVCT(244)
MKIVCT(245)
MKIVCT(246)
MKIVCT(247)
MKIVCT(248)
MKIVCT(249)
MKIVCT(250)
MKIVCT(251)
MKIVCT(252)
MKIVCT(253)
MKIVCT(254)
MKIVCT(255)

#ifdef _VPIX
	.align	4
	.globl	setem
setem:
	pushl	%eax
	movl	%cr0,%eax
	orl	$CR0_EM,%eax
	movl	%eax,%cr0
	popl	%eax
	ret
#endif	/* _VPIX */

#endif	/* lint */
