/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 */
#pragma ident	"@(#)ppc.il	1.1	95/06/08 SMI"
/*
 * In-line functions for PowerPC kernels.
 */


/*
 * sync_instruction_memory(addr, size);
 *
 * make the memory at {addr, addr+size} valid for instruction execution.
 * copied from uts/ppc/ppc.il
 *	NOTE: it is assumed that cache blocks are no smaller than 32 bytes.
 */
	.inline	sync_instruction_memory,8
	li	%r5,32  	! MINIMUM cache block size in bytes
	li	%r8,5		! corresponding shift

	subi	%r7,%r5,1	! cache block size in bytes - 1
	and	%r6,%r3,%r7	! align "addr" to beginning of cache block
	subf	%r3,%r6,%r3	! ... so that termination check is trivial
	add	%r4,%r4,%r6	! increase "len" to reach the end because
				! ... we're startign %r6 bytes before addr
	add	%r4,%r4,%r7	! round "len" up to cache block boundary
!!!	andc	%r4,%r4,%r7	! mask off low bit (not necessary because
				! the following shift throws them away)
	srw	%r4,%r4,%r8	! turn "len" into a loop count
	mtctr	%r4
1:
	dcbst	%r0,%r3 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r3 	! force out of instruction cache
	add	%r3,%r3,%r5
	bdnz	1b

	sync			! one sync for the last icbi
	isync
	.end
