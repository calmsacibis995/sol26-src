/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)i386.il	1.2	96/08/14	SMI"

	.inline	syscall, 0
	movl	0(%esp),%eax
	lcall	$0x27,$0
	jae	.+7
	movl	$-1,%eax
	.end
