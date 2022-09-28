/*
 * Copyright (c) 1994, Sun Microsystems, Inc.
 */

#pragma ident	"@(#)kobj_isa.c	1.2	94/09/21 SMI"

/*
 * Miscellaneous ISA-specific code.
 */

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
	    (h->e_machine == EM_PPC));
}
