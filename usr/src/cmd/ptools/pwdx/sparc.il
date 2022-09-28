/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sparc.il	1.1	96/08/01	SMI"

	.inline	syscall, 0
	mov	0, %g1
	ta	8
	bcs,a	.+8
	mov	-1, %o0
	.end
