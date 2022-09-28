/*
 * Copyright (c) 1994, Sun Microsystems, Inc.
 */

#pragma ident	"@(#)kobj_isa.c	1.3	96/05/29 SMI"

/*
 * Miscellaneous ISA-specific code.
 */
#include <sys/types.h>
#include <sys/elf.h>

/*
 * Check that an ELF header corresponds to this machine's
 * instruction set architecture.  Used by kobj_load_module()
 * to not get confused by a misplaced driver or kernel module
 * built for a different ISA.
 */
int
elf_mach_ok(Elf32_Ehdr *h)
{
	return ((h->e_ident[EI_DATA] == ELFDATA2LSB) &&
	    (h->e_machine == EM_386));
}

/*
 * Flush instruction cache after updating text
 * 	This is a nop for this machine arch.
 */
/*ARGSUSED*/
void
kobj_sync_instruction_memory(caddr_t addr, int len)
{
}
