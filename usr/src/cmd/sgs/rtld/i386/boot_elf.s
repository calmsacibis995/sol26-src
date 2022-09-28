/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1991,1992,1993 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)boot_elf.s	1.6	93/10/04 SMI"

#if	defined(lint)
#include	<sys/types.h>
#include	"_rtld.h"
#include	"_elf.h"
#endif

/*
 * We got here because a call to a function resolved to a procedure
 * linkage table entry.  That entry did a JMPL to the first PLT entry, which
 * in turn did a call to elf_rtbndr.
 *
 * the code sequence that got us here was:
 *
 * PLT entry for foo:
 *	jmp	*name1@GOT(%ebx)
 *	pushl	$rel.plt.foo
 *	jmp	PLT0
 *
 * 1st PLT entry (PLT0):
 *	pushl	4(%ebx)
 *	jmp	*8(%ebx)
 *	nop; nop; nop;nop;
 *
 */
#if defined(lint)

extern unsigned long	elf_bndr(Rt_map *, unsigned long, caddr_t);

void
elf_rtbndr(Rt_map * lmp, unsigned long reloc, caddr_t pc)
{
	(void) elf_bndr(lmp, reloc, pc);
}

#else
	.file	"boot_elf.s"

	.text
	.globl	elf_bndr
	.globl	elf_rtbndr
	.weak	_elf_rtbndr
	_elf_rtbndr = elf_rtbndr	/ Make dbx happy
	.type   elf_rtbndr,@function
	.align	4

elf_rtbndr:
	call	elf_bndr@PLT		/ call the C binder code
	addl	$8,%esp			/ pop args
	jmp	*%eax			/ invoke resolved function
	.size 	elf_rtbndr, .-elf_rtbndr
#endif
