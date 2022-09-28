
#ident	"@(#)mcount.s	1.6	94/03/22 SMI"

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#include <sys/asm_linkage.h>
#include <sys/gprof.h>

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#else
#include "assym.s"
#endif

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
_mcount()
{ }

#else /* ! lint */

/* offsets from %ebp */
#define RETADDR 4	/* return address */
#define OLDEBP 0	/* pointer to next stack frame */

/ _mcount()
/ {
/ Reserved registers:
/	%esi	Holds curcpup()->cpu_profiling throughout
/	%eax	Holds frompc (return address)
/
	.text
	.type	_mcount,@function
	.text
	.globl	_mcount
	.align	4
_mcount:

#ifdef GPROF

	/* make sure there is a kern_profiling structure assigned to this cpu */
	/ prof = curcpup()->cpu_profiling;
	/ if (prof == NULL) return
	movl	%gs:CPU_PROFILING,%eax		/ 1 clock
	testl	%eax,%eax			/ 1 clock
	je	.fast_out			/ 3 clocks (usually taken)
						/ 5 clocks for ret
						/ 3 clocks for call
						/ 13 clocks total for
						/    most common case.

	/ if (prof->profiling != PROFILE_ON) return
	cmpl	$PROFILE_ON,PROFILING(%eax)
	jne	.fast_out

	/ make normal frame
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	movl	%eax,%esi

	/* grab the profiling lock; if we can't get it, return */
	movl	$1,%eax
	xchgb	%al,PROF_LOCK(%esi)
	testl	%eax,%eax
	jne	.out

	/*
	 * Go back through stack frames to find the address of the guy
	 * who called the guy who called mcount.
	 */
	movl	OLDEBP(%ebp),%eax		/ get previous stack frame
	movl	RETADDR(%eax),%eax		/ get "frompc"

	/* find where frompc is within kernel or module text space */
	/ if (frompc >= prof->module_lowpc && frompc < prof->module_highpc)
	cmpl	MODULE_LOWPC(%esi),%eax
	jnae	.check_kernel
	cmpl	MODULE_HIGHPC(%esi),%eax
	jb	.addr_inrange

	.align	4

.check_kernel:
	/ else if (frompc < prof->kernel_lowpc || frompc >= prof->kernel_highpc)
	/ 	goto done;
	cmpl	KERNEL_LOWPC(%esi),%eax
	jnae	.done
	cmpl	KERNEL_HIGHPC(%esi),%eax
	jnb	.done

.addr_inrange:
	/ Compute hash index into froms, fromssize is some
	/ power of two times sizeof(kp_call_t *). froms is an
	/ array of pointers to kp_call_t *.
	/
	/ link_addr = &froms[frompc & (fromssize/sizeof(kp_call_t *) - 1)]

	movl	PROF_FROMSSIZE(%esi),%ebx
	shrl	$2,%ebx			/ fromssize / sizeof(kp_call_t *)
	decl	%ebx			/ to make mask
	andl	%eax,%ebx		/ frompc & ...
	movl	PROF_FROMS(%esi),%edx
	leal	(%edx,%ebx,4),%edi

.testlink:
	movl	(%edi),%ebx		/ %ebx = *link_addr
	testl	%ebx,%ebx
	jne	.oldarc

	/ This is the first call from this address, get new
	/ kp_call structure and link it onto list.

	movl	PROF_TOSNEXT(%esi),%edx	/ Get next free array element
	movl	%edx,%ecx
	addl	$KPCSIZE,%ecx		/ increment pointer
	movl	%ecx,PROF_TOSNEXT(%esi)
	movl	PROF_TOS(%esi),%ecx
	addl	PROF_TOSSIZE(%esi),%ecx
	cmpl	%ecx,%edx		/ if >= array length, overflow
	jae	.overflow

	movl	%edx,(%edi)		/ *link_addr = %edx
	movl	%eax,KPC_FROM(%edx)	/ %eax still has frompc
	movl	RETADDR(%ebp),%ecx
	movl	%ecx,KPC_TO(%edx)
	movl	$1,KPC_COUNT(%edx)	/ set arc count to 1
	movl	$0,KPC_LINK(%edx)
	jmp	.done

	/ Check if current arc is the correct one, usual case
	/
	/ %ebx is pointer to kp_call_t
	/ %eax is still frompc

.oldarc:
	movl	KPC_TO(%ebx),%ecx
	cmpl	%ecx,RETADDR(%ebp)	/ top->topc == our retaddr?
	jne	.chainloop
	cmpl	%eax,KPC_FROM(%ebx)	/ top->frompc == frompc?
	jne	.chainloop
	incl	KPC_COUNT(%ebx)		/ increment count and done.
	jmp	.done

.chainloop:
	leal	KPC_LINK(%ebx),%edi
	jmp	.testlink

.done:
	/* clear profiling lock */
	movb	$0,PROF_LOCK(%esi)
.out:
	popl	%ebx
	popl	%esi
	popl	%edi
	mov	%ebp, %esp
	popl	%ebp
.fast_out:
	ret	
	.align	4

.overflow:
	/ printf(overmsg);
	pushl	$.overmsg
	call	printf
	addl	$4,%esp
	/ leave it locked, so no more messages
	jmp	.out
	.align	4

	.section	.rodata
.overmsg:
	/ NEEDSWORK: what should the printf say?
	.string "mcount overflow\n"

	.data
	/* flag to let kgmon know we were compiled with -DGPROF */
	.globl kernel_profiling
kernel_profiling:
	.long   1

#else
	ret
#endif /* GPROF */
#endif /* ! lint */
