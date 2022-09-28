/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)ppc.il	1.6	95/11/06 SMI"

/*
 * In-line functions for PowerPC kernels.
 */

	.inline	threadp,0
	mr	%r3, %r2
	.end

	.inline	htonl,4
	mr	%r4, %r3
	rlwimi	%r3, %r4, 24, 0, 31	! %r3 = B0 B3 B2 B1
	rlwimi	%r3, %r4, 8, 8, 15	! %r3 = B0 B1 B2 B1
	rlwimi	%r3, %r4, 8, 24, 31	! %r3 = B0 B1 B2 B3
	.end

	.inline	ntohl,4
	mr	%r4, %r3
	rlwimi	%r3, %r4, 24, 0, 31	! %r3 = B0 B3 B2 B1
	rlwimi	%r3, %r4, 8, 8, 15	! %r3 = B0 B1 B2 B1
	rlwimi	%r3, %r4, 8, 24, 31	! %r3 = B0 B1 B2 B3
	.end

	.inline	htons,4
	rlwimi	%r3, %r3, 8, 0, 31	! %r3 = X B1 B0 X
	rlwimi	%r3, %r3, 16, 24, 31	! %r3 = X B1 B0 B1
	andi.	%r3, %r3, 0xffff	! %r3 = 0 0 B0 B1
	.end

	.inline	ntohs,4
	rlwimi	%r3, %r3, 8, 0, 31	! %r3 = X B1 B0 X
	rlwimi	%r3, %r3, 16, 24, 31	! %r3 = X B1 B0 B1
	andi.	%r3, %r3, 0xffff	! %r3 = 0 0 B0 B1
	.end

/*
 * cntlzw() returns the number of leading zeros in the word.
 */
	.inline cntlzw, 0
	cntlzw	%r3, %r3
	.end

/*
 * eieio() is just the eieio instruction.
 */
	.inline eieio, 0
	eieio
	.end

/*
 * The following two functions need to be inlined by krtld because
 * they are used before functions in unix & genunix can be called.
 * kobj_sync_one_instruction() is called before any function can be
 * called, i.e. before _kobj_boot() can correctly call kobj_init().
 */

/*
 * kobj_sync_instruction_memory(addr, size);
 *
 *      make the memory at {addr, addr+size} valid for instruction execution.
 *
 *	NOTE: it is assumed that cache blocks are no smaller than 32 bytes.
 */

	.inline	kobj_sync_instruction_memory,2
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
			/* force instruction to be visible to icache */
			kobj_sync_one_instruction(offptr);

	kobj_sync_instruction_memory(s_text, e_text);

/*
 * kobj_sync_one_instruction(addr);
 *
 *      make the instruction at addr valid for instruction execution.
 */

	.inline	kobj_sync_one_instruction,1
	dcbst	%r0,%r3 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r3 	! force out of instruction cache
	sync			! sync for the icbi
	isync
	.end
