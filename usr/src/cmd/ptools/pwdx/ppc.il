/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ppc.il	1.1	96/08/01	SMI"

	.inline	syscall, 0
	mr	%r0, %r3
	mr	%r3, %r4
	mr	%r4, %r5
	mr	%r5, %r6
	mr	%r6, %r7
	mr	%r7, %r8
	mr	%r8, %r9
	mr	%r9, %r10
	sc
	bns+	.+8
	li	%r3, -1
	.end
