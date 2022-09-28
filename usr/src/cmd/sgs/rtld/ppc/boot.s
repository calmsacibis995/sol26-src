/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)boot.s	1.8	95/10/16 SMI"

/*
 * Bootstrap routine for run-time linker.
 * We get control from exec which has loaded our text and
 * data into the process' address space and created the process
 * stack.
 *
 * On entry, the process stack looks like this:
 *
 *	#			#
 *	#_______________________#  high addresses
 *	#	strings		#
 *	#_______________________#
 *	#	0 word		#
 *	#_______________________#
 *	#	Auxiliary	#
 *	#	entries		#
 *	#	...		#
 *	#	(size varies)	#
 *	#_______________________# <- %r6
 *	#	0 word		#
 *	#_______________________#
 *	#	Environment	#
 *	#	pointers	#
 *	#	...		#
 *	#	(one word each)	#
 *	#_______________________# <- %r5
 *	#	0 word		#
 *	#_______________________#
 *	#	Argument	# low addresses
 *	#	pointers	#
 *	#	Argc words	#
 *	#_______________________# <- %r4
 *	#	argc		# =  %r3
 *	#_______________________# <- %r1
 *
 *
 * We must calculate the address at which ld.so was loaded, build a boot vector,
 * find the addr of the dynamic section of ld.so and pass the thing to _setup
 * to handle.  We then call _rtld - on return we jump to the entry
 * point for the a.out.
 */

#if	defined(lint)

extern	unsigned long	_setup();
extern	void		call_fini();
void
main()
{
	(void) _setup();
	call_fini();
}

#else

#include	<sys/asm_linkage.h>
#include	<link.h>
#include	<sys/mman.h>
#include	<sys/machparam.h>

	.file	"boot.s"
	.text
	.global	_rt_boot
	.global	_setup
	.type	_rt_boot,@function
	.align	2

_rt_boot:
	mr	%r13,%r3		  # save argc
	mr	%r14,%r4		  # save argv
	mr	%r15,%r5		  # save envp
	mr	%r16,%r6		  # save auxv
	stwu	%r1,-SA(MINFRAME + (8 * EB_MAX))(%r1)	# create stack frame
	addi	%r8,%r1,SA(MINFRAME)			# %r8 = &eb[0]
	li	%r9,EB_ARGV		  # establish argv vector entry
	stw	%r9,0(%r8)		  #
	stw	%r4,4(%r8)		  #
	li	%r9,EB_ENVP		  # establish envp vector entry
	stw	%r9,8(%r8)		  #
	stw	%r5,12(%r8)		  #
	li	%r9,EB_AUXV		  # establish auxv vector entry
	stw	%r9,16(%r8)		  #
	stw	%r6,20(%r8)		  #
	li	%r9,EB_NULL		  # establish null vector entry
	stw	%r9,24(%r8)		  #

	bl	1f			  # get the address of GOT the hard way
	b	_GLOBAL_OFFSET_TABLE_@local
1:
	mflr	%r31			  # address of previous instruction
	lwz	%r30, 0(%r31)		  # contents of previous instruction
	slwi	%r30, %r30, 6		  # toss op code 
	srawi	%r30, %r30, 6		  # keep offset
	add	%r31, %r30, %r31	  # add offset to instruction and poof!

	addi	%r3, %r31, -4		  # &GOT[-1]
	rlwinm	%r3, %r3, 0, 0, 31 - PAGESHIFT # round down to 4k (page) boundry
	li	%r4, PAGESIZE		  # 1 page is plenty
	li	%r5, PROT_READ | PROT_WRITE | PROT_EXEC # rwx permissions
	bl	_mprotect		  # reprotect

	lwz	%r4,0(%r31)		  # 
	mr	%r3,%r8			  #
	bl	_setup			  # _setup(&eb[0], _DYNAMIC)
	addi	%r1,%r1,SA(MINFRAME + (8 \* EB_MAX))	# restore stack
	lwz	%r7,call_fini@got(%r31)	  # set %r7 to exit function
	mtctr	%r3			  # _setup returned address of _start
	mr	%r3,%r13		  #
	mr	%r4,%r14		  #
	mr	%r5,%r15		  #
	mr	%r6,%r16		  #
	bctr				  # _start(argc, argv, envp, auxv)
	.size	_rt_boot,.-_rt_boot
#endif
