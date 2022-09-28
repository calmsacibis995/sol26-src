/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)shdr.c	1.5	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"

/*
 * Print out a single `section header' entry.
 */
void
Elf_shdr_entry(Shdr * shdr)
{
	dbg_print(MSG_ORIG(MSG_SHD_ADDR), shdr->sh_addr,
	    conv_secflg_str(shdr->sh_flags));
	dbg_print(MSG_ORIG(MSG_SHD_SIZE), shdr->sh_size,
	    conv_sectyp_str(shdr->sh_type));
	dbg_print(MSG_ORIG(MSG_SHD_OFFSET), shdr->sh_offset, shdr->sh_entsize);
	dbg_print(MSG_ORIG(MSG_SHD_LINK), shdr->sh_link, shdr->sh_info);
	dbg_print(MSG_ORIG(MSG_SHD_ALIGN), shdr->sh_addralign);
}
