/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)sync_instruction_memory.s	1.2	95/04/28 SMI"

	.file	"sync_instruction_memory.s"

#include <sys/asm_linkage.h>

/*
 * void sync_instruction_memory(caddr_t addr, int len)
 *
 * Make the memory at {addr, addr+len} valid for instruction execution.
 */

#ifdef lint
#define	nop
void
sync_instruction_memory(caddr_t addr, int len)
{
	caddr_t end = addr + len;
	caddr_t start = addr & ~7;
	for (; start < end; start += 8)
		flush(start);
	nop; nop; nop; nop; nop;
	return;
}
#else
	ENTRY(sync_instruction_memory)
	add	%o0, %o1, %o2
	andn	%o0, 7, %o0
1:
	cmp	%o0, %o2
	add	%o0, 8, %o0
	blu,a	1b
	flush	%o0
	!
	! when we get here, we have executed 3 instructions after the
	! last flush; SPARC V8 requires 2 more for flush to be visible
	! The retl;nop pair will do it.
	!
	retl
	clr	%o0
	SET_SIZE(sync_instruction_memory)

	ENTRY(nop)
	retl
	nop
	SET_SIZE(nop)
#endif
