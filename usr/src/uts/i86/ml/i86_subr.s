/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 *  Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *  Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
 *    All Rights Reserved
 *
 *  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
 *  UNIX System Laboratories, Inc.
 *  The copyright notice above does not evidence any
 *  actual or intended publication of such source code.
 */

#ident	"@(#)i86_subr.s	1.74	96/10/16 SMI"

/*
 * XXX 1. The ddi_ctlops()/ddi_dma_map()/ddi_dma_mctl() are stubs for now. These
 * XXX 	  routines if needed can be moved to a .c file for i86.
 * XXX 2. movtuc() is ifdef'd, C version from common/io/ldterm.c is used for i86
 */

/*
 * General assembly language routines.
 * It is the intent of this file to contain routines that are
 * independent of the specific kernel architecture, and those that are
 * common across kernel architectures.
 * As architectures diverge, and implementations of specific
 * architecture-dependent routines change, the routines should be moved
 * from this file into the respective ../`arch -k`/subr.s file.
 */

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include <sys/regset.h>
#ifdef GPROF
#include <sys/segment.h>	/* See #ifdef GPROF below, for KGSSEL */
#endif

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/archsystm.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

/* #define DEBUG */

/*
 * on_fault()
 * Catch lofault faults. Like setjmp except it returns one
 * if code following causes uncorrectable fault. Turned off
 * by calling no_fault().
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
on_fault(label_t *ljb)
{ return (0); }

#else	/* lint */

	ENTRY(on_fault)
	movl	4(%esp), %eax			/ jumpbuf address
	pushl	%ebx
	pushl	%edi
	movl	%gs:CPU_THREAD, %ebx
	movl	%eax, T_ONFAULT(%ebx)
	lea	catch_fault, %edi
	movl	%edi, T_LOFAULT(%ebx)		/ put catch_fault in u_lofault
	popl	%edi
	popl	%ebx
	jmp	setjmp				/ let setjmp do the rest

catch_fault:
	movl	%gs:CPU_THREAD, %ebx
	movl	T_ONFAULT(%ebx), %edx		/ address of save area
	subl	%eax, %eax
	movl	%eax, T_ONFAULT(%ebx)		/ turn off onfault
	movl	%eax, T_LOFAULT(%ebx)		/ turn off lofault
	pushl	%edx
	call	longjmp				/ let longjmp do the rest
	SET_SIZE(on_fault)

#endif	/* lint */

/*
 * no_fault()
 * turn off fault catching.
 */

#if defined(lint) || defined(__lint)

void
no_fault(void)
{}

#else	/* lint */

	ENTRY(no_fault)
	pushl	%ebx
	subl	%eax, %eax
	movl	%gs:CPU_THREAD, %ebx
	movl	%eax, T_ONFAULT(%ebx)
	movl	%eax, T_LOFAULT(%ebx)		/ turn off lofault
	popl	%ebx
	ret
	SET_SIZE(no_fault)

#endif	/* lint */

/*
 * Setjmp and longjmp implement non-local gotos using state vectors
 * type label_t.
 *
 * setjmp(lp)
 * label_t *lp;
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
setjmp(label_t *lp)
{ return (0); }

#else	/* lint */

	ENTRY(setjmp)
	movl    4(%esp), %edx           / address of save area
	movl    %edi, (%edx)
	movl    %esi, 4(%edx)
	movl    %ebx, 8(%edx)
	movl    %ebp, 12(%edx)
	movl    %esp, 16(%edx)
	movl    (%esp), %ecx            / %eip (return address)
	movl    %ecx, 20(%edx)
	subl	%eax, %eax		/ return 0
	ret
	SET_SIZE(setjmp)

#endif	/* lint */

/*
 * longjmp(lp)
 * label_t *lp;
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
longjmp(label_t *lp)
{}

#else	/* lint */

	ENTRY(longjmp)
	movl    4(%esp), %edx           / address of save area
	movl    (%edx), %edi
	movl    4(%edx), %esi
	movl    8(%edx), %ebx
	movl    12(%edx), %ebp
	movl    16(%edx), %esp
	movl    20(%edx), %ecx          / %eip (return address)
	movl    $1, %eax
	addl    $4, %esp                / pop ret adr
	jmp     *%ecx                   / indirect
	SET_SIZE(longjmp)

#endif	/* lint */

#ifdef XXX
/*
 * movtuc(length, from, to, table)
 *
 * VAX movtuc instruction (sort of).
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
movtuc(size_t length, u_char *from, u_char *to, u_char *table)
{ return (0); }

#else	/* lint */

	ENTRY(movtuc)
	tst     %o0
	ble     2f                      ! check length
	clr     %o4

	ldub    [%o1 + %o4], %g1        ! get next byte in string
0:
	ldub    [%o3 + %g1], %g1        ! get corresponding table entry
	tst     %g1                     ! escape char?
	bnz     1f
	stb     %g1, [%o2 + %o4]        ! delay slot, store it

	retl                            ! return (bytes moved)
	mov     %o4, %o0
1:
	inc     %o4                     ! increment index
	cmp     %o4, %o0                ! index < length ?
	bl,a    0b
	ldub    [%o1 + %o4], %g1        ! delay slot, get next byte in string
2:
	retl                            ! return (bytes moved)
	mov     %o4, %o0
	SET_SIZE(movtuc)

#endif	/* lint */
#endif	/* XXX */

/*
 * if a() calls b() calls caller(),
 * caller() returns return address in a().
 * (Note: We assume a() and b() are C routines which do the normal entry/exit
 *  sequence.)
 * XXX - review?
 */

#if defined(lint) || defined(__lint)

caddr_t
caller(void)
{ return (0); }

#else	/* lint */

	ENTRY(caller)
	movl	(%ebp), %eax		/ saved ebp of a()
	movl	4(%eax), %eax		/ return pc of a()
	ret
	SET_SIZE(caller)

#endif	/* lint */

/*
 * if a() calls callee(), callee() returns the
 * return address in a();
 */

#if defined(lint) || defined(__lint)

caddr_t
callee(void)
{ return (0); }

#else	/* lint */

	ENTRY(callee)
	movl	4(%ebp), %eax		/ return pc of a()
	ret
	SET_SIZE(callee)

#endif	/* lint */

/*
 * if a() calls b() calls c() calls caller(),
 * caller() returns return address in a().
 * (Note: We assume a(),b() and c() are C routines which do the normal
 *  entry/exit sequence.)
 */

#if defined(lint) || defined(__lint)

caddr_t
caller2(void)
{ return (0); }

#else	/* lint */

	ENTRY(caller2)
	movl	(%ebp), %eax		/ saved ebp of b()
	movl	(%eax), %eax		/ saved ebp of a()
	movl	4(%eax), %eax		/ return pc of a()
	ret
	SET_SIZE(caller2)

#endif	/* lint */

/*
 * return the current stack pointer
 */

#if defined(lint) || defined(__lint)

greg_t
getsp(void)
{ return (0); }

#else	/* lint */

	ENTRY(getsp)
	movl	%esp, %eax
	addl	$4, %eax		/ adjust the value for the size
					/ used by 'call' instr. XXX - check?
	ret
	SET_SIZE(getsp)

#endif	/* lint */

/*
 * return the current frame pointer
 */

#if defined(lint) || defined(__lint)

greg_t
getfp(void)
{ return (0); }

#else	/* lint */

	ENTRY(getfp)
	movl	%ebp, %eax
	ret
	SET_SIZE(getfp)

#endif	/* lint */

/*
 * invlpg(caddr_t m)
 *     invalid a page entry
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
invlpg(caddr_t m)
{}

#else	/* lint */
	ENTRY(invlpg)
	mov	4(%esp), %eax
	invlpg	(%eax)
	cmpw	$0, cacheflsh
	je	no_flush
	wbinvd
no_flush:
	ret
	SET_SIZE(invlpg)

#endif	/* lint */


/*
 * cr0()
 *      Return the value of register cr0
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
cr0(void)
{ return(0); }

#else	/* lint */

        ENTRY(cr0)
        movl    %cr0,%eax
        ret
	SET_SIZE(cr0)

#endif	/* lint */

/*
 * setcr0()
 *      Set the value of cr0
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
setcr0(int value)
{}

#else	/* lint */

        ENTRY(setcr0)
        movl    4(%esp),%eax
        movl    %eax,%cr0
        ret
	SET_SIZE(setcr0)

#endif	/* lint */

/*
 * cr2()
 *      Return the value of register cr2
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
cr2(void)
{ return(0); }

#else	/* lint */

        ENTRY(cr2)
        movl    %cr2,%eax
        ret
	SET_SIZE(cr2)

#endif	/* lint */

/*
 * cr3()
 *      Return the value of register cr3
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
u_int
cr3(void)
{ return(0); }

#else	/* lint */

        ENTRY(cr3)
        movl    %cr3,%eax
        ret
	SET_SIZE(cr3)

#endif	/* lint */

/*
 * setcr3()
 *      set the value of register cr3
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
setcr3(u_int val)
{}

#else	/* lint */

        ENTRY(setcr3)
        movl    4(%esp),%eax
        movl    %eax,%cr3
        ret
	SET_SIZE(setcr3)

#endif	/* lint */


/*
 * dr6()
 *      Return the value of the debug register dr6
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
dr6(void)
{ return(0); }

#else	/* lint */

        ENTRY(dr6)
        movl    %db6,%eax
        ret
	SET_SIZE(dr6)

#endif	/* lint */

/*
 * dr7()
 *      Return the value of the debug register dr7
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
dr7(void)
{ return(0); }

#else	/* lint */

        ENTRY(dr7)
        movl    %db7,%eax
        ret
	SET_SIZE(dr7)

#endif	/* lint */

/*
 * setdr6()
 *      Set the value of the debug register dr6
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
setdr6(int value)
{}

#else	/* lint */

        ENTRY(setdr6)
        movl    4(%esp), %eax
        movl    %eax, %db6
        ret
	SET_SIZE(setdr6)

#endif	/* lint */

/*
 * _insque(entryp, predp)
 *
 * Insert entryp after predp in a doubly linked list.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
_insque(caddr_t entryp, caddr_t predp)
{}

#else	/* lint */

	ENTRY(_insque)
	pushl	%ebx
	pushl	%edi
	movl	12(%esp), %ebx		/ entryp
	movl	16(%esp), %edi		/ predp
	movl	(%edi), %eax		/ predp->forw
	movl	%edi, 4(%ebx)		/ entryp->back = predp
	movl	%eax, (%ebx)		/ entryp->forw = predp->forw
	movl	%ebx, (%edi)		/ predp->forw = entryp
	movl	%ebx, 4(%eax)		/ predp->forw->back = entryp
	popl	%edi
	popl	%ebx
	ret
	SET_SIZE(_insque)

#endif	/* lint */

/*
 * _remque(entryp)
 *
 * Remove entryp from a doubly linked list
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
_remque(caddr_t entryp)
{}

#else	/* lint */

	ENTRY(_remque)
	pushl	%ebx
	pushl	%edi
	movl    12(%esp), %ebx
	movl	(%ebx), %eax		/ entryp->forw
	movl	4(%ebx), %edi		/ entryp->back
	movl	%eax, (%edi)		/ entryp->back->forw = entryp->forw
	movl	%edi, 4(%eax)		/ entryp->forw->back = entryp->back
	popl	%edi
	popl	%ebx
	ret
	SET_SIZE(_remque)

#endif	/* lint */

/*
 * strlen(str), ustrlen(str)
 *	ustrlen gets strlen from an user space address
 *	On x86 it is almost the same. We added additional checks
 *	with the arguments.
 *
 * Returns the number of
 * non-NULL bytes in string argument.
 *
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
size_t
strlen(const char *str)
{ return (0); }

/* ARGSUSED */
size_t
ustrlen(const char *str)
{ return (0); }

#else	/* lint */

	ENTRY(ustrlen)
	cmpl	$KERNELBASE, 4(%esp)
	jb	ustr_valid
	movl	KERNELBASE, %eax	/ dereference invalid addr to force trap
ustr_valid:
        pushl   %edi            / save register variables
        movl    8(%esp),%edi    / string address
        xorl    %eax,%eax       / %al = 0
        movl    $-1,%ecx        / Start count backward from -1.
        repnz ; scab
        incl    %ecx            / Chip pre-decrements.
        movl    %ecx,%eax       / %eax = return values
        notl    %eax            / Twos complement arith. rule.

        popl    %edi            / restore register variables
        ret
	SET_SIZE(ustrlen)

	ENTRY(strlen)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jae	str_valid

	pushl	$.str_panic_msg
	call	panic
#endif /* DEBUG */

str_valid:
        pushl   %edi            / save register variables
        movl    8(%esp),%edi    / string address
        xorl    %eax,%eax       / %al = 0
        movl    $-1,%ecx        / Start count backward from -1.
        repnz ; scab
        incl    %ecx            / Chip pre-decrements.
        movl    %ecx,%eax       / %eax = return values
        notl    %eax            / Twos complement arith. rule.

        popl    %edi            / restore register variables
        ret
	SET_SIZE(strlen)

#ifdef DEBUG
	.data
.str_panic_msg:
	.string "strlen: Argument is below KERNELBASE"
	.text
#endif /* DEBUG */
	

#endif	/* lint */

/*
 * copyinstr_noerr(s1, s2, len)
 *
 * Copy string s2 to s1.  s1 must be large enough and len contains the
 * number of bytes copied.  s1 is returned to the caller.
 * s2 is user space and s1 is in kernel space
 *
 * copyoutstr_noerr(s1, s2, len)
 *
 * Copy string s2 to s1.  s1 must be large enough and len contains the
 * number of bytes copied.  s1 is returned to the caller.
 * s2 is kernel space and s1 is in user space
 *
 * knstrcpy(s1, s2, len)
 *
 * This routine copies a string s2 in the kernel address space to string
 * s1 which is also in the kernel address space.
 *
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
char *
copyinstr_noerr(char *s1, char *s2, size_t *len)
{ return ((char *)0); }
 
/*ARGSUSED*/
char *
copyoutstr_noerr(char *s1, char *s2, size_t *len)
{ return ((char *)0); }

/* ARGSUSED */
char *
knstrcpy(char *s1, const char *s2, size_t *len)
{ return ((char)0); }

#else	/* lint */

	ENTRY(copyoutstr_noerr)
#ifdef DEBUG
	cmpl	$KERNELBASE, 8(%esp)
	jae	cpyouts1

cpyouts3:
	pushl	$.cpyout_panic_msg
	call	panic


cpyouts1:
	cmpl	$KERNELBASE, 4(%esp)
	jb	.do_cpy
	jmp	cpyouts3

#endif DEBUG

	ENTRY(copyinstr_noerr)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jae	cpyins1

cpyins3:
	pushl	$.cpyin_panic_msg
	call 	panic

cpyins1:
	cmpl	$KERNELBASE, 8(%esp)
	jb	.do_cpy
	jmp	cpyins3

#endif DEBUG

	ALTENTRY(knstrcpy)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jae	knstr1

knstr3:
	pushl	$.knstrcpy_panic_msg
	call	panic

knstr1:
	cmpl	$KERNELBASE, 8(%esp)
	jb	knstr3

#endif DEBUG


.do_cpy:
        pushl   %edi            / save register variables
        movl    %esi,%edx

        movl    12(%esp),%edi   / %edi = source string address
        xorl    %eax,%eax       / %al = 0 (search for 0)
        movl    $-1,%ecx        / length to look: lots
        repnz ; scab

        notl    %ecx            / %ecx = length to move
        movl    12(%esp),%esi   / %esi = source string address
        movl    8(%esp),%edi    / %edi = destination string address
        movl    %ecx,%eax       / %eax = length to move
        shrl    $2,%ecx         / %ecx = words to move
        rep ; smovl

        movl    %eax,%ecx       / %ecx = length to move
        andl    $3,%ecx         / %ecx = leftover bytes to move
        rep ; smovb

	movl	16(%esp), %edi
	movl	%eax, (%edi)	/ *len = number of bytes copied
        movl    8(%esp),%eax    / %eax = returned dest string addr
        movl    %edx,%esi       / restore register variables
        popl    %edi
        ret
	SET_SIZE(copyinstr_noerr)
	SET_SIZE(copyoutstr_noerr)
	SET_SIZE(knstrcpy)

#ifdef DEBUG
	.data
.cpyout_panic_msg:
	.string "copyoutstr_noerr: argument not in correct address space"
.cpyin_panic_msg:
	.string "copyinstr_noerr: argument not in correct address space"
.knstrcpy_panic_msg:
	.string "knstrcpy: argument not in kernel address space"
	.text
#endif /* DEBUG */


#endif	/* lint */

	/*
	 * Berkley 4.3 introduced symbolically named interrupt levels
	 * as a way deal with priority in a machine independent fashion.
	 * Numbered priorities are machine specific, and should be
	 * discouraged where possible.
	 *
	 * Note, for the machine specific priorities there are
	 * examples listed for devices that use a particular priority.
	 * It should not be construed that all devices of that
	 * type should be at that priority.  It is currently were
	 * the current devices fit into the priority scheme based
	 * upon time criticalness.
	 *
	 * The underlying assumption of these assignments is that
	 * IPL 10 is the highest level from which a device
	 * routine can call wakeup.  Devices that interrupt from higher
	 * levels are restricted in what they can do.  If they need
	 * kernels services they should schedule a routine at a lower
	 * level (via software interrupt) to do the required
	 * processing.
	 *
	 * Examples of this higher usage:
	 *	Level	Usage
	 *	14	Profiling clock (and PROM uart polling clock)
	 *	12	Serial ports
	 *
	 * The serial ports request lower level processing on level 6.
	 *
	 * Also, almost all splN routines (where N is a number or a
	 * mnemonic) will do a RAISE(), on the assumption that they are
	 * never used to lower our priority.
	 * The exceptions are:
	 *	spl8()		Because you can't be above 15 to begin with!
	 *	splzs()		Because this is used at boot time to lower our
	 *			priority, to allow the PROM to poll the uart.
	 *	spl0()		Used to lower priority to 0.
	 *	splsoftclock()	Used by hardclock to lower priority.
	 */

#if defined(lint) || defined(__lint)

int spl8(void)		{ return (0); }
int spl7(void)		{ return (0); }
int splzs(void)		{ return (0); }
int splhigh(void)	{ return (0); }
int splhi(void)		{ return (0); }
int splclock(void)	{ return (0); }
int spltty(void)	{ return (0); }
int splbio(void)	{ return (0); }
int spl6(void)		{ return (0); }
int spl5(void)		{ return (0); }
int spl4(void)		{ return (0); }
int splimp(void)	{ return (0); }
int spl3(void)		{ return (0); }
int spl2(void)		{ return (0); }
int spl1(void)		{ return (0); }
int splnet(void)	{ return (0); }
int splsoftclock(void)	{ return (0); }
int spl0(void)		{ return (0); }

#else	/* lint */

/* reg = cpu[cpuno]->cpu_m.cpu_pri; */
#define GETIPL_NOGS(reg, cpup)	\
	movl	CPU_M+CPU_PRI(cpup), reg;

/* cpu[cpuno]->cpu_m.cpu_pri; */
#define SETIPL_NOGS(val, cpup)	\
	movl	val, CPU_M+CPU_PRI(cpup);

/* reg = cpu[cpuno]->cpu_m.cpu_pri; */
#define GETIPL(reg)	\
	movl	%gs:CPU_M+CPU_PRI, reg;

/* cpu[cpuno]->cpu_m.cpu_pri; */
#define SETIPL(val)	\
	movl	val, %gs:CPU_M+CPU_PRI;
/*
 * Macro to raise processor priority level.
 * Avoid dropping processor priority if already at high level.
 * Also avoid going below CPU->cpu_base_spl, which could've just been set by
 * a higher-level interrupt thread that just blocked.
 */
#define	RAISE(level) \
	cli;			\
	LOADCPU(%ecx);		\
	movl	$/**/level, %edx;\
	GETIPL_NOGS(%eax, %ecx);\
	cmpl 	%eax, %edx;	\
	jg	spl;		\
	jmp	setsplhisti

/*
 * Macro to set the priority to a specified level.
 * Avoid dropping the priority below CPU->cpu_base_spl.
 */
#define SETPRI(level) \
	cli;				\
	LOADCPU(%ecx);			\
	movl	$/**/level, %edx;	\
	jmp	spl

	/* locks out all interrupts, including memory errors */
	ENTRY(spl8)
	SETPRI(15)
	SET_SIZE(spl8)

	/* just below the level that profiling runs */
	ENTRY(spl7)
	RAISE(13)
	SET_SIZE(spl7)

	/* sun specific - highest priority onboard serial i/o asy ports */
	ALTENTRY(splzs)	
	SETPRI(12)	/* Can't be a RAISE, as it's used to lower us */
	SET_SIZE(splzs)

	/*
	 * should lock out clocks and all interrupts,
	 * as you can see, there are exceptions
	 */
	.align	16
	ALTENTRY(splhigh)
	ALTENTRY(splhi)

	/* the standard clock interrupt priority */
	ALTENTRY(splclock)

	/* highest priority for any tty handling */
	ALTENTRY(spltty)

	/* highest priority required for protection of buffered io system */
	ALTENTRY(splbio)

	/* machine specific */
	ENTRY2(spl6,spl5)
/*	RAISE(10)		/XXX Is it worth copying spl code*/
				/ here to avoid a jump and cmp?
	cli
	LOADCPU(%ecx)
	movl	$10, %edx
	movl	CPU_M+CPU_PRI(%ecx), %eax
	cmpl	%eax, %edx
	jle	setsplhisti
	SETIPL_NOGS(%edx, %ecx)		/ set new ipl

	pushl   %eax                    / save old ipl
	pushl	%edx			/ pass new ipl
	call	*setspl
	popl	%ecx			/ dummy pop
	popl    %eax                    / return old ipl
setsplhisti:
	nop                             / enable interrupts
					/ We will patch this to an sti
					/ once a proper setspl routine is 
					/ installed.
	ret
	SET_SIZE(splhigh)
	SET_SIZE(splhi)
	SET_SIZE(splclock)
	SET_SIZE(spltty)
	SET_SIZE(splbio)
	SET_SIZE(spl5)
	SET_SIZE(spl6)

	/*
	 * machine specific
	 * for sun, some frame buffers must be at this priority
	 */
	ENTRY(spl4)
	RAISE(8)
	SET_SIZE(spl4)

	/* highest level that any network device will use */
	ALTENTRY(splimp)	

	/*
	 * machine specific
	 * for sun, devices with limited buffering: tapes, ethernet
	 */
	ENTRY(spl3)
	RAISE(6)
	SET_SIZE(splimp)
	SET_SIZE(spl3)

	/*
	 * machine specific - not as time critical as above
	 * for sun, disks
	 */
	ENTRY(spl2)
	RAISE(4)
	SET_SIZE(spl2)

	ENTRY(spl1)
	RAISE(2)
	SET_SIZE(spl1)

	/* highest level that any protocol handler will run */
	ENTRY(splnet)
	RAISE(1)
	SET_SIZE(splnet)

	/* softcall priority */
	/* used by hardclock to LOWER priority */
	ENTRY(splsoftclock)
	SETPRI(1)
	SET_SIZE(splsoftclock)

	/* allow all interrupts */
	ENTRY(spl0)
	SETPRI(0)
	SET_SIZE(spl0)

#endif	/* lint */

/*
 * splr is like splx but will only raise the priority and never drop it
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
splr(int level)
{ return (0); }

#else	/* lint */

	ENTRY(splr)
	cli
	LOADCPU(%ecx)
	movl	4(%esp), %edx		/ get new spl level
	GETIPL_NOGS(%eax, %ecx)
	cmpl 	%eax, %edx		/ if new level > current level
	jg	spl			/ then set ipl to new level
splr_setsti:
	nop
	ret				/ else return the current level value
	SET_SIZE(splr)

#endif	/* lint */



/*
 * splx - set PIL back to that indicated by the level passed as an argument,
 * or to the CPU's base priority, whichever is higher.
 * Needs to be fall through to spl to save cycles.
 *algorithm for spl:
 *
 *      turn off interrupts
 *
 *	if (CPU->cpu_base_spl > newipl)
 *		newipl = CPU->cpu_base_spl;
 *      oldipl = CPU->cpu_pridata->c_ipl;
 *      CPU->cpu_pridata->c_ipl = newipl;
 *
 *	/indirectly call function to set spl values (usually setpicmasks)
 *      setspl();  // load new masks into pics
 *
 * Be careful not to set priority lower than CPU->cpu_base_pri,
 * even though it seems we're raising the priority, it could be set
 * higher at any time by an interrupt routine, so we must block interrupts
 * and look at CPU->cpu_base_pri
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
splx(int level)
{ return (0); }


#else	/* lint */

	ENTRY(splx)
	cli                             / disable interrupts
	LOADCPU(%ecx)
	movl	4(%esp), %edx		/ get new spl level
	/ fall down into spl

	.align	4			/ get spl aligned
	ALTENTRY(spl) 			/ new priority level is in %edx
					/ doing this early to avoid an AGI
					/ in the next instruction
	GETIPL_NOGS(%eax, %ecx)		/ get current ipl
	cmpl	%edx, CPU_BASE_SPL(%ecx)/ if ( base spl > new ipl)
	ja	set_to_base_spl		/ then use base_spl

setprilev:
	SETIPL_NOGS(%edx, %ecx)			/ set new ipl

	pushl   %eax                    / save old ipl
	pushl	%edx			/ pass new ipl
	call	*setspl
setsplsti:
	nop                             / enable interrupts
					/ We will patch this to an sti
					/ once a proper setspl routine is 
					/ installed.
	popl	%eax
	popl    %eax                    / return old ipl
	ret
set_to_base_spl:
	movl	CPU_BASE_SPL(%ecx), %edx
	jmp	setprilev

	SET_SIZE(splx)
	SET_SIZE(spl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
install_spl(void)
{}

#else	/* lint */

	ENTRY_NP(install_spl) 			
	movl	%cr0, %eax
	movl	%eax, %edx
	andl	$-1![CR0_WP],%eax	/ we do not want to take a fault
	movl	%eax, %cr0
	jmp	.install_spl1
.install_spl1:
	movb	$0xfb, setsplsti
	movb	$0xfb, setsplhisti
	movb	$0xfb, splr_setsti
	movl	%edx, %cr0
	ret
	SET_SIZE(install_spl)

#endif	/* lint */


/*
 * Get current processor interrupt level
 */

#if defined(lint) || defined(__lint)

greg_t
getpil(void)
{ return (0); }

#else

	ENTRY(getpil)
	GETIPL(%eax)			/ priority level into %eax
	ret
	SET_SIZE(getpil)

#endif

/***************************************************************************
 *			C callable in and out routines
 **************************************************************************/

/*
 * outl(port address, val)
 *   write val to port
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
outl(int port_address, unsigned long val)
{}

#else	/* lint */

	.set	PORT, 8
	.set	VAL, 12

	ENTRY(outl)
	pushl	%ebp
	movl	%esp, %ebp
	movw	PORT(%ebp), %dx
	movl	VAL(%ebp), %eax
	outl	(%dx)
	popl	%ebp
	ret
	SET_SIZE(outl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
outw(int port_address, unsigned short val)
{}

#else	/* lint */

	ENTRY(outw)
	pushl	%ebp
	movl	%esp, %ebp
	movw	PORT(%ebp), %dx
	movw	VAL(%ebp), %ax
	data16
	outl	(%dx)
	popl	%ebp
	ret
	SET_SIZE(outw)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
outb(int port_address, unsigned char val)
{}

#else	/* lint */
	ENTRY(outb)
	pushl	%ebp
	movl	%esp, %ebp
	movw	PORT(%ebp), %dx
	movb	VAL(%ebp), %al
	outb	(%dx)
	popl	%ebp
	ret
	SET_SIZE(outb)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned long
inl(int port_address)
{ return (0); }

#else	/* lint */

	ENTRY(inl)
	pushl	%ebp
	movl	%esp, %ebp
	movw	PORT(%ebp), %dx
	inl	(%dx)
	popl	%ebp
	ret
	SET_SIZE(inl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned short
inw(int port_address)
{ return (0); }

#else	/* lint */

	ENTRY(inw)
	pushl	%ebp
	movl	%esp, %ebp
	subl    %eax, %eax
	movw	PORT(%ebp), %dx
	data16
	inl	(%dx)
	popl	%ebp
	ret
	SET_SIZE(inw)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned char
inb(int port_address)
{ return (0); }

#else	/* lint */

	ENTRY(inb)
	pushl	%ebp
	movl	%esp, %ebp
	subl    %eax, %eax
	movw	PORT(%ebp), %dx
	inb	(%dx)
	popl	%ebp
	ret
	SET_SIZE(inb)

#endif	/* lint */

/*
 * The following routines move strings to and from an I/O port.
 * loutw(port, addr, count);
 * linw(port, addr, count);
 * repinsw(port, addr, cnt) - input a stream of 16-bit words
 * repoutsw(port, addr, cnt) - output a stream of 16-bit words
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
loutw(int port_address, int count)
{}

/* ARGSUSED */
void
repoutsw(int port, unsigned short *addr, int cnt)
{}

#else	/* lint */

	.set	PORT, 8
	.set	ADDR, 12
	.set	COUNT, 16

	ENTRY2(loutw,repoutsw)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx
	pushl	%esi
	pushl	%ecx
	movl	PORT(%ebp),%edx
	movl	ADDR(%ebp),%esi
	movl	COUNT(%ebp),%ecx
	rep
	data16
	outsl
	popl	%ecx
	popl	%esi
	popl	%edx
	popl	%ebp
	ret
	SET_SIZE(loutw)
	SET_SIZE(repoutsw)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
linw(int port_addr, int count)
{}

/* ARGSUSED */
/* input a stream of 16-bit words */
void
repinsw(int port_addr, unsigned short *addr, int cnt)
{}

#else	/* lint */

	ENTRY2(linw,repinsw)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edx
	pushl	%edi
	pushl	%ecx
	movl	PORT(%ebp),%edx
	movl	ADDR(%ebp),%edi
	movl	COUNT(%ebp),%ecx
	rep
	data16
	insl
	popl	%ecx
	popl	%edi
	popl	%edx
	popl	%ebp
	ret
	SET_SIZE(linw)
	SET_SIZE(repinsw)

#endif	/* lint */


/*
 * repinsb(port_addr, cnt) - input a stream of bytes
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
repinsb(int port, unsigned char *addr, int count)
{}

#else	/* lint */

	.set	BPARGBAS, 8
	.set	io_port, BPARGBAS
	.set	io_addr, BPARGBAS+4
	.set	io_cnt,  BPARGBAS+8

/ repinsb(port, addr, count);
/ NOTE: count is a BYTE count

	ENTRY(repinsb)
	push	%ebp
	mov	%esp,%ebp
	push	%edi

	mov	io_addr(%ebp),%edi
	mov	io_cnt(%ebp),%ecx
	mov	io_port(%ebp),%edx	/ read from right port

	rep
	insb				/ read them bytes

	pop	%edi
	pop	%ebp
	ret
	SET_SIZE(repinsb)

#endif	/* lint */


/*
 * repinsd(port, addr, cnt) - input a stream of 32-bit words
 * NOTE: count is a DWORD count
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
repinsd(int port, unsigned long *addr, int count)
{}

#else	/* lint */

	ENTRY(repinsd)
	push	%ebp
	mov	%esp,%ebp
	push	%edi

	mov	io_addr(%ebp),%edi
	mov	io_cnt(%ebp),%ecx
	mov	io_port(%ebp),%edx	/ read from right port

	rep
	insl				/ read them dwords

	pop	%edi
	pop	%ebp
	ret
	SET_SIZE(repinsd)

#endif	/* lint */

/*
 * repoutsb(port, addr, cnt) - output a stream of bytes
 *    NOTE: count is a byte count
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
repoutsb(int port, unsigned char *addr, int count)
{}

#else	/* lint */

	ENTRY(repoutsb)
	push	%ebp
	mov	%esp,%ebp
	push	%esi

	mov	io_addr(%ebp),%esi
	mov	io_cnt(%ebp),%ecx
	mov	io_port(%ebp),%edx

	rep
	outsb

	pop	%esi
	pop	%ebp
	ret
	SET_SIZE(repoutsb)

#endif	/* lint */

/*
 * repoutsd(port, addr, cnt) - output a stream of 32-bit words
 * NOTE: count is a DWORD count
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
repoutsd(int port, unsigned long *addr, int count)
{}

#else	/* lint */

	ENTRY(repoutsd)
	push	%ebp
	mov	%esp,%ebp
	push	%esi

	mov	io_addr(%ebp),%esi
	mov	io_cnt(%ebp),%ecx
	mov	io_port(%ebp),%edx

	rep
	outsl

	pop	%esi
	pop	%ebp
	ret
	SET_SIZE(repoutsd)

#endif	/* lint */

/*
 * void int20(void)
 */

#if defined(lint) || defined(__lint)

void
int20(void)
{}

#else	/* lint */

	ENTRY(int20)
	cmpl	$0, kadb_is_running
	je	no_debugger
	int	$20
dropping_into_kadb:
	ret
no_debugger:
	ret
	SET_SIZE(int20)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/*
 * Overlapping bcopy (source and target may overlap arbitrarily).
 */
/* ARGSUSED */
void
ovbcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(ovbcopy)
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%esi	/ %esi = source address
	movl	16(%esp),%edi	/ %edi = dest address
	movl	20(%esp),%ecx	/ %ecx = length of string
	movl	%ecx,%edx	/ %edx = number of bytes to move
	shrl	$2,%ecx		/ %ecx = number of words to move
	cmpl	%edi,%esi
	jnb	.stmv
	std			/ reverse direction
	addl	20(%esp),%esi
	addl	20(%esp),%edi
	cmpl	$2,%ecx
	jle	.rbytemv
	subl	$4,%esi		/ Predecrement for correct addresses
	subl	$4,%edi
	rep ; smovl
	addl	$3,%esi
	addl	$3,%edi
	movl	%edx,%ecx	/ %ecx = number of bytes to move
	andl	$0x3,%ecx	/ %ecx = number of bytes left to move
	rep ; smovb		/ move the bytes
	cld			/ reset direction flag (struct moves)
	jmp	.exit

.rbytemv:
	decl	%esi		/ Predecrement for correct addresses
	decl	%edi
	movl	%edx,%ecx	/ %ecx = number of bytes to move
	rep ; smovb
	cld			/ reset direction flag (struct moves)
	jmp	.exit

.stmv:
	rep ; smovl		/ move the words

	movl	%edx,%ecx	/ %ecx = number of bytes to move
	andl	$0x3,%ecx	/ %ecx = number of bytes left to move
.bytemv:
	rep ; smovb		/ move the bytes

.exit:
	movl	20(%esp),%eax	/ length of string
	popl	%esi
	popl	%edi
	ret
	SET_SIZE(ovbcopy)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
scanc(u_int size, u_char *cp, u_char *table, u_char mask)
{ return 0; }

#else	/* lint */

	ENTRY(scanc)
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movb	28(%esp),%cl		/ mask = %cl
	movl	20(%esp),%esi		/ cp = %esi
	movl	24(%esp),%ebx		/ table = %ebx
	movl	%esi,%edi
	addl	16(%esp),%edi		/ end = &cp[size];
.scanloop:
	cmpl	%edi,%esi		/ while (cp < end
	jnb	.scandone
	lodsb				/ byte at %esi to %al, inc %esi
	xlat				/ table[*cp] to %al
	testb	%al, %cl
	jz	.scanloop		/     && table[*cp] & mask] == 0 )
	dec	%esi			/ lodsb advanced us 1 too many
.scandone:
	movl	%edi,%eax
	subl	%esi,%eax		/ return (end - cp)
	popl	%ebx
	popl	%esi
	popl	%edi
	ret	
	SET_SIZE(scanc)

#endif	/* lint */

/*
 *	Replacement functions for ones that are normally inlined.
 *	In addition to the copy in i86.il, they are defined here just in case.
 */

#if defined(lint) || defined(__lint)

int
intr_clear(void)
{ return 0; }

int
clear_int_flag(void)
{ return 0; }

#else	/* lint */

	ENTRY(intr_clear)
	ENTRY(clear_int_flag)
	pushfl
	cli
	popl	%eax
	ret
	SET_SIZE(clear_int_flag)
	SET_SIZE(intr_clear)

#endif	/* lint */

#if defined(lint) || defined(__lint)

struct cpu *
curcpup(void)
{ return 0; }

#else	/* lint */

	ENTRY(curcpup)
	movl	%fs:0, %eax
	ret
	SET_SIZE(curcpup)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned long
htonl(unsigned long i)
{ return 0; }

#else	/* lint */

	ENTRY(htonl)
	movl	4(%esp), %eax
	xchgb	%ah, %al
	rorl	$16, %eax
	xchgb	%ah, %al
	ret
	SET_SIZE(htonl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned short
htons(unsigned short i)
{ return 0; }

#else	/* lint */

	ENTRY(htons)
	movl	4(%esp), %eax
	xchgb	%ah, %al
	ret
	SET_SIZE(htons)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ipltospl(int i)
{ return 0; }

#else	/* lint */

	ENTRY(ipltospl)
	mov	4(%esp), %eax
	ret
	SET_SIZE(ipltospl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned long
ntohl(unsigned long i)
{ return 0; }

#else	/* lint */

	ENTRY(ntohl)
	movl	4(%esp), %eax
	xchgb	%ah, %al
	rorl	$16, %eax
	xchgb	%ah, %al
	ret
	SET_SIZE(ntohl)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned short
ntohs(unsigned short i)
{ return 0; }

#else	/* lint */

	ENTRY(ntohs)
	movl	4(%esp), %eax
	xchgb	%ah, %al
	ret
	SET_SIZE(ntohs)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
intr_restore(int i)
{ return (0); }

/* ARGSUSED */
void
restore_int_flag(int i)
{ return; }

#else	/* lint */

	ENTRY(intr_restore)
	ENTRY(restore_int_flag)
	pushl	4(%esp)
	popfl
	ret
	SET_SIZE(restore_int_flag)
	SET_SIZE(intr_restore)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
sti(void)
{ return; }

#else	/* lint */

	ENTRY(sti)
	sti
	ret
	SET_SIZE(sti)

#endif	/* lint */

#if defined(lint) || defined(__lint)

kthread_id_t
threadp(void)
{ return ((kthread_id_t)0); }

#else	/* lint */

	ENTRY(threadp)
	movl	%gs:CPU_THREAD, %eax
	ret
	SET_SIZE(threadp)

#endif	/* lint */

/*
 *   Checksum routine for Internet Protocol Headers
 *
 *   unsigned short int
 *   ip_ocsum (address, count, sum)
 *       u_short   *address;     / Ptr to 1st message buffer
 *       int        count;       / Length of data
 *       u_short    sum          / partial checksum
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ip_ocsum(address, count, sum)
{ return 0; }

#else

	.text
	.align	4
	ENTRY(ip_ocsum)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
        movl	12(%ebp),%ecx   / count of half words
	movl	16(%ebp),%edx	/ partial checksum
  	movl	8(%ebp),%esi
	xorl	%eax, %eax
	testl	%ecx, %ecx
	jz	.ip_ocsum_done

	testl	$3, %esi
	jnz	.ip_csum_notaligned
.ip_csum_aligned:
.next_iter:
	subl	$32, %ecx
	jl	.less_than_32

	addl	0(%esi), %edx
.only60:
	adcl	4(%esi), %eax
.only56:
	adcl	8(%esi), %edx
.only52:
	adcl	12(%esi), %eax
.only48:
	adcl	16(%esi), %edx
.only44:
	adcl	20(%esi), %eax
.only40:
	adcl	24(%esi), %edx
.only36:
	adcl	28(%esi), %eax
.only32:
	adcl	32(%esi), %edx
.only28:
	adcl	36(%esi), %eax
.only24:
	adcl	40(%esi), %edx
.only20:
	adcl	44(%esi), %eax
.only16:
	adcl	48(%esi), %edx
.only12:
	adcl	52(%esi), %eax
.only8:
	adcl	56(%esi), %edx
.only4:
	adcl	60(%esi), %eax	/ We could be adding -1 and -1 with a carry
.only0:
	adcl	$0, %eax	/ we could be adding -1 in eax with a carry
	adcl	$0, %eax
	
	addl	$64, %esi
	andl	%ecx, %ecx
	jnz	.next_iter
	
.ip_ocsum_done:
	addl	%eax, %edx
	adcl	$0, %edx
	movl	%edx,%eax	/ form a 16 bit checksum by
	shrl	$16,%eax	/ adding two halves of 32 bit checksum
	addw	%dx,%ax
	adcw	$0,%ax
	andl	$0xffff,%eax
	popl	%edi		/ restore registers
	popl	%esi
	popl	%ebx
	leave
	ret

.ip_csum_notaligned:
	xorl	%edi, %edi
	movw	(%esi), %edi
	addl	%edi, %edx
	adcl	$0, %edx
	addl	$2, %esi
	decl	%ecx
	jmp	.ip_csum_aligned



.less_than_32:
	addl	$32, %ecx
	testl	$1, %ecx
	jz	.size_aligned
	andl	$0xfe, %ecx
	movzwl	(%esi, %ecx, 2), %edi
	addl	%edi, %edx
	adcl	$0, %edx
.size_aligned:
	movl	%ecx, %edi
	shrl	$1, %ecx
	shl	$1, %edi
	subl	$64, %edi
	addl	%edi, %esi	
	movl	$.ip_ocsum_jmptbl, %edi
	leal	(%edi, %ecx, 4), %edi
	xorl	%ecx, %ecx
	clc	
	jmp 	*(%edi)
	SET_SIZE(ip_ocsum)



	.data
	.align	4

.ip_ocsum_jmptbl:
	.long	.only0, .only4, .only8, .only12, .only16, .only20
	.long	.only24, .only28, .only32, .only36, .only40, .only44
	.long	.only48, .only52, .only56, .only60
#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ip_ocsum_copy(address, count, sum, dest)
{ return 0; }

#else	/* lint */

		// for now this is just a dummy entry point
		// if it is ever used, it will need to do
		// the equivalent of ip_ocsum followed by a bcopy
	.text
	.align	4
	ENTRY(ip_ocsum_copy)
	ret
	SET_SIZE(ip_ocsum_copy)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ip_ocsum_copyout(kaddr, count, sump, uaddr)
{ return 0; }

#else	/* lint */

		// for now this is just a stub entry point
		// to tell the caller to do it their self.
	.text
	.align	4
	ENTRY(ip_ocsum_copyout)
	movl	$48, %eax	// ENOTSUP
	ret
	SET_SIZE(ip_ocsum_copyout)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
ip_ocsum_copyin(uaddr, count, sump, kaddr)
{ return 0; }

#else	/* lint */

		// for now this is just a stub entry point
		// to tell the caller to do it their self.
	.text
	.align	4
	ENTRY(ip_ocsum_copyin)
	movl	$48, %eax	// ENOTSUP
	ret
	SET_SIZE(ip_ocsum_copyin)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
long long
__mul64(long long a, long long b)
{ return (0); }

#else   /* lint */

/
/   function __mul64(A,B:Longint):Longint;
/	{Overflow is not checked}
/
/ We essentially do multiply by longhand, using base 2**32 digits.
/               a       b	parameter A
/	     x 	c       d	parameter B
/		---------
/               ad      bd
/       ac	bc
/       -----------------
/       ac	ad+bc	bd
/
/       We can ignore ac and top 32 bits of ad+bc: if <> 0, overflow happened.
/
	ENTRY(__mul64)
	push	%ebp
	mov    	%esp,%ebp
	pushl	%esi
	mov	12(%ebp),%eax	/ A.hi (a)
	mull	16(%ebp)	/ Multiply A.hi by B.lo (produces ad)
	xchg	%ecx,%eax	/ ecx = bottom half of ad.
	movl    8(%ebp),%eax	/ A.Lo (b)
	movl	%eax,%esi	/ Save A.lo for later
	mull	16(%ebp)	/ Multiply A.Lo by B.LO (dx:ax = bd.)
	addl	%edx,%ecx	/ cx is ad
	xchg	%eax,%esi       / esi is bd, eax = A.lo (d)
	mull	20(%ebp)	/ Multiply A.lo * B.hi (producing bc)
	addl	%ecx,%eax	/ Produce ad+bc
	movl	%esi,%edx
	xchg	%eax,%edx
	popl	%esi
	movl	%ebp,%esp
	popl	%ebp
	ret     $16
	SET_SIZE(__mul64)

#endif	/* lint */

/*
 * multiply two long numbers and yield a u_longlong_t result, callable from C.
 * Provided to manipulate hrtime_t values.
 */
#if defined(lint) || defined(__lint)

/* result = a * b; */

/* ARGSUSED */
unsigned long long
mul32(ulong a, ulong b)
{ return (0); }

#else	/* lint */

	ENTRY(mul32)
	movl	8(%esp), %eax
	movl	4(%esp), %ecx
	mull	%ecx
	ret
	SET_SIZE(mul32)

#endif	/* lint */

#if defined(GPROF)

#if defined(lint) || defined(__lint)

unchar
sysp_getchar(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(sysp_getchar)
	pushl	%fs
	pushl	%gs
	pushfl
	movl	$KFSSEL,%eax
	movw	%ax, %fs
	movl	$KGSSEL,%eax
	movw	%ax, %gs
	cli
	call	*getcharptr
	popfl
	popl	%gs
	popl	%fs
	ret
	SET_SIZE(sysp_getchar)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
sysp_putchar(int arg)
{ return (0); }

#else	/* lint */

	ENTRY_NP(sysp_putchar)
	pushl	%fs
	pushl	%gs
	pushfl
	movl	$KFSSEL,%eax
	movw	%ax, %fs
	movl	$KGSSEL,%eax
	movw	%ax, %gs
	cli
	pushl	16(%esp)	/ pass our arg on to the next guy
	call	*putcharptr
	addl	$4, %esp
	popfl
	popl	%gs
	popl	%fs
	ret
	SET_SIZE(sysp_putchar)

#endif	/* lint */

#if defined(lint) || defined(__lint)

int
sysp_ischar(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(sysp_ischar)
	pushl	%fs
	pushl	%gs
	pushfl
	movl	$KFSSEL,%eax
	movw	%ax, %fs
	movl	$KGSSEL,%eax
	movw	%ax, %gs
	cli
	call	*ischarptr
	popfl
	popl	%gs
	popl	%fs
	ret
	SET_SIZE(sysp_ischar)

#endif	/* lint */

#endif	/* GPROF */

/*
 * 64-bit integer division.
 *
 *
 *  MetaWare Runtime Support: 64 bit division
 *  (c) Copyright by MetaWare,Inc 1992.
 *
 *
 *	unsigned long long __udiv64(unsigned long long a,
 *				    unsigned long long b)
 *	   -- 64-bit unsigned division: a/b
 *
 *  Quotient in %edx/%eax
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned long long
__udiv64(unsigned long long a, unsigned long long b)
{ return (0); }

#else	/* lint */

	ENTRY(__udiv64)
	pushl	%esi
	call	divmod64
	popl	%esi
	ret	$16
	SET_SIZE(__udiv64)

#endif	/* lint */

/*
 *	unsigned long long __urem64(long long a,long long b)
 *	      {64 BIT UNSIGNED MOD}
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
unsigned long long
__urem64(long long a,long long b)
{ return (0); }

#else	/* lint */

	ENTRY(__urem64)
	pushl	%esi
	call	divmod64
	mov	%esi,%eax
	mov	%ecx,%edx
	popl	%esi
	ret	$16
	SET_SIZE(__urem64)

#endif	/* lint */

/*
 *	long long __div64(long long a, long long b)
 *	   -- 64-bit signed division: a/b
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
long long
__div64(long long a, long long b)
{ return (0); }

#else	/* lint */

	ENTRY(__div64)
	pushl	%esi
	cmpl	$0,20(%esp)
/  if B < 0
	jge	Lpos
	negl	16(%esp); adcl $0,20(%esp); negl 20(%esp)
	cmpl	$0,12(%esp)
	jge	Lnegate_result
	negl	8(%esp); adcl $0,12(%esp); negl 12(%esp)
    Ldoit:
	call	divmod64
	popl	%esi
	ret	$16
Lpos:   cmpl	$0,12(%esp)
	jge	Ldoit
	negl	8(%esp); adcl $0,12(%esp); negl 12(%esp)
Lnegate_result:
	call	divmod64
	negl	%eax
	adcl	$0,%edx
	negl	%edx
	popl	%esi
	ret	$16
	SET_SIZE(__div64)

#endif	/* lint */

/*
 *	long long __rem64(long long a, long long b)
 *	   -- 64-bit signed moduloo: a%b
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
long long
__rem64(long long a, long long b)
{ return (0); }

#else	/* lint */

	ENTRY(__rem64)
	pushl	%esi
	cmpl	$0,12(%esp)
/  if A < 0
	jge	L_pos
	negl	8(%esp); adcl $0,12(%esp); negl 12(%esp)
	cmpl	$0,20(%esp)
	jge	L_neg
	negl	16(%esp); adcl $0,20(%esp); negl 20(%esp)
    L_neg:
	call	divmod64
	subl	%edx,%edx
	subl	%eax,%eax
	subl	%esi,%eax
	sbbl	%ecx,%edx
	popl	%esi
	ret	$16
L_pos:   cmpl	$0,20(%esp)
	jge	L_doit
	negl	16(%esp); adcl $0,20(%esp); negl 20(%esp)
L_doit:
	call	divmod64
	movl	%esi,%eax
	movl	%ecx,%edx
	popl	%esi
	ret	$16
	SET_SIZE(__rem64)

#endif	/* lint */

/*
 *	long long __udivrem64(long long a, long long b)
 *	   -- 64-bit unsigned division and modulo: a%b
 *	quotient in EDX/EAX;   remainder in ECX/ESI
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
long long
__udivrem64(long long a, long long b)
{ return (0); }

#else	/* lint */

	ENTRY(__udivrem64)
	push	%ebx	// Anything to set the stack up right
	call	divmod64
	popl	%ebx
	ret	$16
	SET_SIZE(__udivrem64)

#endif	/* lint */

/*
 *	long long __divrem64(long long a, long long b)
 *	   -- 64-bit unsigned division and modulo: a%b
 *	quotient in EDX/EAX;   remainder in ECX/ESI
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
long long
__divrem64(long long a, long long b)
{ return (0); }

#else	/* lint */

	ENTRY(__divrem64)
	cmpl	$0,16(%esp)
	jl	Ldr
	cmpl	$0,8(%esp)
	jl	Ldr
	push	%ebx		// Anything to set the stack up right
	call	divmod64
	popl	%ebx
	ret	$16
Ldr:
	/
	pushl	16(%esp)
	pushl	16(%esp)
	pushl	16(%esp)
	pushl	16(%esp)
	call	__div64
	pushl	%edx		// Save quotient
	pushl	%eax
	pushl	24(%esp)	// high half of divisor (16+8)
	pushl	24(%esp)	// low half of divisor (12+12)
	pushl	%edx
	pushl	%eax
	call	__mul64
	movl	12(%esp),%esi	// Low half of dividend
	movl	16(%esp),%ecx	// High half of dividend
	subl	%eax,%esi
	sbbl	%edx,%ecx
	popl	%eax		//pop quotient
	popl	%edx
	/
	ret	$16
	SET_SIZE(__divrem64)

/
/ General function for computing quotient and remainder for unsigned
/ 64-bit division.
/
/ First argument is at 16(%ebp),   second at 24(%ebp)
/
/ Quotient left in %edx/%eax;   remainder in %ecx,%esi
/
/ NOTE: clobbers %esi as side-effect.

divmod64:
	pushl	%ebp
	movl	%esp,%ebp
	/
	movl	28(%ebp),%ecx	/ high half of divisor (operand b)
	andl	%ecx,%ecx
	jnz	L		/ Do full division
	/ If Bhi is 0, we can do long-hand division
	/ using the machine instructions, essentially treating the dividend
	/ as two digits in base 2**32.  For example:
	/                      4 560ffc74
	/             --------------
	/   10000000  | 4560ffc7 43000103
	/              -40000000
	/		--------
	/		 560ffc7 43000103
	/		-560ffc7 40000000
	/	         ----------------
	/			  3000103
	/
	/ Answer: 4_560ffc74, remainder 3000103
	/
	movl	24(%ebp),%ecx	/ Move low half of operand b
	movl	20(%ebp),%eax	/ Move high half of operand a
	subl	%edx,%edx	/ Zero out upper word
	divl	%ecx
	movl	%eax,%esi	/ Save high 32-bits of answer
	movl	16(%ebp),%eax	/ move low half of opereand a
	divl	%ecx            / Divide remainder of previous + low digit.
	subl	%ecx,%ecx	/ Top half of remainder is zero.
	xchg    %esi,%edx	/ Quotient in %edx/%eax, remainder in %ecx/%esi
	/
	movl	%ebp,%esp
	popl	%ebp		/ Quotient is in dx:ax; remainder in cx:bx.
	ret
L: 	/ Come here to do full 64-bit division in which divisor is > 32 bits
        / Since the divisor is > 32 bits, the answer cannot be greater than
        / 32 bits worth.  We can divide dividend and divisor by equal amounts
        / and the answer will still be the same, except possibly off by one,
        / as long as we don't divide too much.  This division is done
        / by shifting.
        / We stop such division the first time that the top 32 bits of the
        / divisor are 0; then we use the machine divide.  We then test if
        / we're off by one by comparing the dividend with the quotient*divisor.
        / E.g.:
        / 	Ahi Alo      Ahi
        / 	-------  =>  ---
        / 	Bhi Blo      Bhi
        / If the top bit of Bhi is 1, the two answers are always the same
        / except that the second can possibly be 1 bigger than the first.
        / For example, consider two base 32 digits (rather than base 2**32):
        / 	A   0         A
        / 	-------  =>  ---
        / 	A   F         A
        / The second answer is one more than the first.
        / But the second answer can never be less than the first.
        / To prove this, consider minimizing the first divisor digit,
        / maximizing the second, and maximizing the second dividend digit:
        / 	X   F         X
        / 	-------  =>  ---
        / 	1   0         1
        / Since F is < 10, XF/10 can never be greater than X/1.  qed.
        /
        / To test if the result is 1 less we multiply by the answer and
        / form the remainder.  If the remainder is negative we add 1 to
        / the answer and add the divisor to the remainder.
        /movl	28(%ebp),%ecx		/ Already there.
	movl	24(%ebp),%esi		/ Low half of divisor (b)
	mov	20(%ebp),%edx		/ High half of a (dividend)
	mov	16(%ebp),%eax		/ Low half of dividend (a)
	/ Now shift cx:si and dx:ax right until the last 1 bit enters bx.
	/ Optimize away 16 bits of loop if top 16 bits of divisor <> 0.
	testl	$0xFFFF,%ecx
	jz	Lcheck_ch
	shrdl	$16,%edx,%eax
	shrl	$16,%edx
	shrdl	$16,%ecx,%esi
	shrl	$16,%ecx
Lcheck_ch:
	andb	%ch,%ch
	jz	Lmore
	/ Shift right by 8.
L8:	
	shrdl	$8,%edx,%eax
	shrl	$8,%edx
	shrdl	$8,%ecx,%esi
	shrl	$8,%ecx
	cmpb	$80,%cl  / Rare case: divisor >= 8000_0000.  Shift 8 again.
	jb 	L2    / If we do, we of course won't do it again.
	jmp	L8
	/ Since Bhi <> 0 we can always shift at least once.
Lmore:  shrl	%ecx
	rcrl	%esi
	shrl	%edx
	rcrl	%eax
L2:	andl	%ecx,%ecx		/ Test top word of divisor.
	jnz	Lmore
	/ Divisor = 0000_xxxx where xxxx >= 8000.
	divl    %esi		/ eax is answer, or answer + 1.
	pushl	%eax		/ Save answer.
	subl	%edx,%edx
	pushl	%edx
	pushl	%eax
	pushl	28(%ebp)             / Multiply answer by divisor.
	pushl	24(%ebp)      / Subtract from dividend to obtain remainder.
	call	__mul64
	/ If product is > dividend, we went over by 1.
	popl	%ecx		/ Get answer back.
	cmpl	20(%ebp),%edx	/ Compare high half of dividend
	ja	LToo_big
	jne	LNot_too_big
	cmpl	16(%ebp),%eax	/ Compare low half of dividend
	jna     LNot_too_big
LToo_big:	/ Add divisor to remainder; decrement answer.
	decl	%ecx
	subl	24(%ebp),%eax
	sbbl	28(%ebp),%edx
LNot_too_big:	/ Compute remainder.	
	movl	16(%ebp),%esi
	subl	%eax,%esi
	movl	%ecx,%eax	/ put low half of quotient in eax
	movl	20(%ebp),%ecx	/ put high half of quotient in ecx
	sbbl	%edx,%ecx
	subl	%edx,%edx	/ High half of quotient is zero
	/ Top 32 bits of quotient = 0 when top 32 bits of dividend <> 0.
	/
	movl	%ebp,%esp
	popl	%ebp		/ Quotient is in dx:ax; remainder in cx:bx.
	ret

#endif	/* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
user_pde_clear(u_int *pde_nzmask, u_int *pagedir, u_int **pde_mask)
{}

#else	/* lint */


	/ user_pde_clear(&pde_nzmask[cpuid], pagedir, &pde_mask[cpuid])
	.globl	user_pde_clear
user_pde_clear:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	movl	0x08(%ebp), %ebx
	movl	0x0c(%ebp), %esi
	movl	0x10(%ebp), %edi
	
next_chunk:
	bsfl	(%ebx), %eax
	jz	i86mmu_pde_clear_done
	btrl	%eax, (%ebx)
	movl	%eax, %edx
	rol	$0x07, %edx
	movl	0x0c(%ebp), %esi
	addl	%edx, %esi
	movl	(%edi,%eax,4), %edx
	movl	$0x0, (%edi, %eax, 4)
next_entry:
	bsfl	%edx, %ecx
	jz	next_chunk
	btrl	%ecx, %edx
	movl	$0x0, (%esi, %ecx, 4)
	jmp	next_entry

i86mmu_pde_clear_done:
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp
	ret
		
#endif	/* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
atomic_add_word(u_int *word, int value, struct mutex *lock)
{}

/*ARGSUSED*/
void
atomic_add_hword(u_short *hword, int value, struct mutex *lock)
{}
#else
	.globl	atomic_add_word
	.globl	atomic_add_hword
atomic_add_word:
	movl	4(%esp), %eax
	movl	8(%esp), %ecx
	lock
	addl	%ecx, (%eax)
	ret

atomic_add_hword:
	movl	4(%esp), %eax
	movl	8(%esp), %ecx
	lock
	addw	%cx, (%eax)
	ret
#endif


#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
atomic_dec(int *val)
{}

/*ARGSUSED*/
void
atomic_decw(int *val)
{}
#else	/* lint */
	.globl	atomic_dec

atomic_dec:
	movl	4(%esp), %eax
	lock
	decl	(%eax)
	ret
	.globl	atomic_decw

atomic_decw:
	movl	4(%esp), %eax
	lock
	decw	(%eax)
	ret
#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
atomic_inc(int *val)
{}

/* ARGSUSED */
void
atomic_incw(int *val)
{}

#else	/* lint */

	.globl	atomic_inc
atomic_inc:
	movl	4(%esp), %eax
	lock
	incl	(%eax)
	ret
	.globl	atomic_incw
atomic_incw:
	movl	4(%esp), %eax
	lock
	incw	(%eax)
	ret
#endif	/* lint */
	

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
atomic_dec_retzflag(int *val)
{ return (0); }

/* ARGSUSED */
int
atomic_decw_retzflag(int *val)
{ return (0); }
#else	/* lint */

	.globl	atomic_dec_retzflg
atomic_dec_retzflg:
	xorl	%ecx, %ecx
	movl	4(%esp), %eax
	lock
	decl	(%eax)
	jnz	zflg_notset
	incl	%ecx
zflg_notset:
	movl	%ecx, %eax
	ret


	.globl	atomic_decw_retzflg
atomic_decw_retzflg:
	xorl	%ecx, %ecx
	movl	4(%esp), %eax
	lock
	decw	(%eax)
	jnz	zflg_notsetw
	incl	%ecx
zflg_notsetw:
	movl	%ecx, %eax
	ret
#endif	/* lint */




#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
cas(u_int *ptr, u_int old_value, u_int new_value)
{ return (0); }

#else	/* lint */

	.globl	cas
cas:
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	movl	12(%esp), %ecx
	lock
	cmpxchgl %ecx, (%edx)
	ret

#endif	/* lint */

/*
 * Kernel probes assembly routines
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int tnfw_b_get_lock(u_char *lp)
{ return (0); }

/* ARGSUSED */
void tnfw_b_clear_lock(u_char *lp)
{}

/* ARGSUSED */
u_long tnfw_b_atomic_swap(u_long *wp, u_long x)
{ return (0); }

#else  /* lint */

	ENTRY(tnfw_b_get_lock)
	movl	4(%esp), %edx
	subl	%eax, %eax
	lock
	btsl	$0, (%edx)
	jnc	.L1
	incl	%eax
.L1:
	ret
	SET_SIZE(tnfw_b_get_lock)

	ENTRY(tnfw_b_clear_lock)
	movl	4(%esp), %eax
	movb	$0, (%eax)
	ret
	SET_SIZE(tnfw_b_clear_lock)

	ENTRY(tnfw_b_atomic_swap)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	xchgl	%eax, (%edx)
	ret
	SET_SIZE(tnfw_b_atomic_swap)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int rdmsr(u_int r, u_int *mtr)
{ return (0); }

/* ARGSUSED */
int wrmsr(u_int r, u_int *mtr)
{ return (0); }

#else  /* lint */

	.globl	rdmsr
rdmsr:
	movl	4(%esp), %ecx
	.byte 0x0f, 0x32
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)	
	movl	%edx, 4(%ecx)	
	ret

	.globl	wrmsr
wrmsr:
	movl	8(%esp), %ecx
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	movl	4(%esp), %ecx
	.byte 0x0f, 0x30
	ret

	.globl	invalidate_cache
invalidate_cache:
	wbinvd
	ret

#endif	/* lint */
