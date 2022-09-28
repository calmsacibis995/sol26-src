/*
 * Copyright (c) 1992, by Sun Microsystems Inc.
 */

	.ident "@(#)inst_sync.s 1.5	94/09/09 SMI"

/*
 * System call:
 *		int inst_sync(char *pathname, int flags);
 */

#include <sys/asm_linkage.h>
#include <sys/syscall.h>

	.file	"inst_sync.s"

	ENTRY(inst_sync)

	li	%r0, SYS_inst_sync
	sc
	bns+    .ok
	POTENTIAL_FAR_BRANCH(_cerror)
.ok:
	li	%r3, 0
	blr-

	SET_SIZE(inst_sync)
