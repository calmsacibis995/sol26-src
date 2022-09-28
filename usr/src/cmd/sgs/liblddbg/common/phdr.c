/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)phdr.c	1.4	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"

/*
 * Print out a single `program header' entry.
 */
void
Elf_phdr_entry(Phdr * phdr)
{
	dbg_print(MSG_ORIG(MSG_PHD_VADDR), phdr->p_vaddr,
	    conv_phdrflg_str(phdr->p_flags));
	dbg_print(MSG_ORIG(MSG_PHD_PADDR), phdr->p_paddr,
	    conv_phdrtyp_str(phdr->p_type));
	dbg_print(MSG_ORIG(MSG_PHD_FILESZ), phdr->p_filesz, phdr->p_memsz);
	dbg_print(MSG_ORIG(MSG_PHD_OFFSET), phdr->p_offset, phdr->p_align);
}
