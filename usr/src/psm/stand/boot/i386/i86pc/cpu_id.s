#ifdef	lint
int is486(void) { return(1); }
#else

/       Copyrighted as an unpublished work.
/       (c) Copyright 1991 Sun Microsystems, Inc.
/       All rights reserved.

/       RESTRICTED RIGHTS

/       These programs are supplied under a license.  They may be used,
/       disclosed, and/or copied only as permitted under such license
/       agreement.  Any copy must contain the above copyright notice and
/       this restricted rights notice.  Use, copying, and/or disclosure
/       of the programs is strictly prohibited unless otherwise provided
/       in the license agreement.


	.file   "cpu_id.s"

	.ident "@(#)cpu_id.s	1.4	96/04/08 SMI"
	.ident  "@(#) (c) Copyright Sun Microsystems, Inc. 1991"

	.text

/       Determine if CPU we are running on is a i486 cpu or higher (as
/       opposed to a i386).
/
/       This can be done since the i486 added a new flag (AC, alignment
/       check) to the EFLAGS register.  This bit was previously reserved
/       in the i386 flags.  It is only possible to set the bit on an i486.

	.globl  is486
is486:
	pushl   %edx
	movl    %esp, %edx      / save current stack pointer to align it
	pushl   %ecx
	/ andl    $-1!3, %esp     / chop last two bits to avoid AC fault - only necessary at iopl 3 (boot at 0)
	pushfl                  / push EFLAGS
	popl    %eax            / get EFLAGS value
	movl    %eax, %ecx      / save EFLAGS away for later
	xorl    $0x40000, %eax  / flip the AC bit
	pushl   %eax
	popfl                   / copy register to EFLAGS
	pushfl                  / get new EFLAGS
	pop     %eax            /       into EAX
	xorl    %ecx, %eax      / see if AC bit has changed
				/  EAX=0x4000 if 486cpu, 0 if 386cpu
	shrl    $18, %eax       / set EAX=1 if i486, 0 if i386
	andl    $1, %eax        / mask off other bits
	pushl   %ecx
	popfl                   / restore original EFLAGS reg
	popl	%ecx
	movl    %edx, %esp      / restore stack
	popl    %edx
	ret

#endif	/* !lint */
