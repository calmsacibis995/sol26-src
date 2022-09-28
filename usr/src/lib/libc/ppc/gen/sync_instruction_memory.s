/*
 *   Copyright (c) 1995, by Sun Microsystems, Inc.
 *   All rights reserved.
 *   Solaris C Library Routine
 *   ====================================================================
 *
 *   Function:	sync_instruction_memory
 *
 *   Syntax:	void sync_instruction_memory(caddr_t addr, int len)
 *
 */

.ident "@(#)sync_instruction_memory.s 1.2	96/11/25 SMI"

#include <sys/asm_linkage.h>

#ifdef lint
/*
 *	make the memory at {addr, addr+len} valid for instruction execution.
 *
 *	NOTE: it is assumed that cache blocks are no smaller than 32 bytes.
 */
void sync_instruction_memory(caddr_t addr, int len)
{
}
#else
	ENTRY(sync_instruction_memory)
	li	%r5,32  	! MINIMUM cache block size in bytes
	li	%r8,5		! corresponding shift

	subi	%r7,%r5,1	! cache block size in bytes - 1
	and	%r6,%r3,%r7	! align "addr" to beginning of cache block
	subf	%r3,%r6,%r3	! ... so that termination check is trivial
	add	%r4,%r4,%r6	! increase "len" to reach the end because
				! ... we're starting %r6 bytes before addr
	add	%r4,%r4,%r7	! round "len" up to cache block boundary
!!!	andc	%r4,%r4,%r7	! mask off low bit (not necessary because
				! the following shift throws them away)
	srw	%r4,%r4,%r8	! turn "len" into a loop count
	mr	%r6,%r3		! copy of r3

	mtctr	%r4
1:
	dcbst	%r0,%r3 	! force to memory
	add	%r3,%r3,%r5
	bdnz	1b

	sync			! guarantee dcbst is done before icbi
				! one sync for all the dcbsts

	mr	%r3,%r6
	mtctr	%r4
1:
	icbi	%r0,%r3 	! force out of instruction cache
	add	%r3,%r3,%r5
	bdnz	1b

	sync			! one sync for all the icbis
	isync
	blr
	SET_SIZE(sync_instruction_memory)
#endif
