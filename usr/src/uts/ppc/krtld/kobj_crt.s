/*
 * Copyright (c) 1994, Sun Microsystems, Inc.
 */

#pragma ident	"@(#)kobj_crt.s	1.7	95/11/10 SMI"

/*
 * exit routine from linker/loader to kernel
 */

#include <sys/asm_linkage.h>

/*
 *	exitto is called from kobj_init and transfers control to
 *	_start, its argument.  The 3 kernel arguments are loaded
 *	from where they were saved in kobj_init, namely, "romp",
 *	"dbvec", and "bopp".
 */

#if defined(lint)

/* ARGSUSED */
void
exitto(caddr_t entrypoint)
{}

#else	/* lint */

	ENTRY(exitto)
	mtlr	%r3
	lis	%r3,romp@ha
	lwz	%r3,romp@l(%r3)
	lis	%r4,dbvec@ha
	lwz	%r4,dbvec@l(%r4)
	lis	%r5,bopp@ha
	lwz	%r5,bopp@l(%r5)
	blrl			! onward to the next program
	.long	0		! PANIC if return to here.
	SET_SIZE(exitto)
#endif
