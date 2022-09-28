/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */
#pragma ident	"@(#)relocate_ppc.c	1.6	96/03/11 SMI"

/* LINTLIBRARY */

/*
 * String conversion routine for relocation types.
 */
#include	<stdio.h>
#include	<sys/elf_ppc.h>
#include	"_conv.h"
#include	"relocate_ppc_msg.h"

/*
 * PPC specific relocations.
 */
static const int rels[] = {
	MSG_R_PPC_NONE,			MSG_R_PPC_ADDR32,
	MSG_R_PPC_ADDR24,		MSG_R_PPC_ADDR16,
	MSG_R_PPC_ADDR16_LO,		MSG_R_PPC_ADDR16_HI,
	MSG_R_PPC_ADDR16_HA,		MSG_R_PPC_ADDR14,
	MSG_R_PPC_ADDR14_BRTAKEN,	MSG_R_PPC_ADDR14_BRNTAKEN,
	MSG_R_PPC_REL24,		MSG_R_PPC_REL14,
	MSG_R_PPC_REL14_BRTAKEN,	MSG_R_PPC_REL14_BRNTAKEN,
	MSG_R_PPC_GOT16,		MSG_R_PPC_GOT16_LO,
	MSG_R_PPC_GOT16_HI,		MSG_R_PPC_GOT16_HA,
	MSG_R_PPC_PLTREL24,		MSG_R_PPC_COPY,
	MSG_R_PPC_GLOB_DAT,		MSG_R_PPC_JMP_SLOT,
	MSG_R_PPC_RELATIVE,		MSG_R_PPC_LOCAL24PC,
	MSG_R_PPC_UADDR32,		MSG_R_PPC_UADDR16,
	MSG_R_PPC_REL32,		MSG_R_PPC_PLT32,
	MSG_R_PPC_PLTREL32,		MSG_R_PPC_PLT16_LO,
	MSG_R_PPC_PLT16_HI,		MSG_R_PPC_PLT16_HA,
	MSG_R_PPC_SDAREL16,		MSG_R_PPC_SECTOFF,
	MSG_R_PPC_SECTOFF_LO,		MSG_R_PPC_SECTOFF_HI,
	MSG_R_PPC_SECTOFF_HA,		MSG_R_PPC_ADDR30
};

const char *
conv_reloc_PPC_type_str(Word rel)
{
	static char	string[STRSIZE] = { '\0' };

	if (rel >= R_PPC_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)rel, 0));
	else
		return (MSG_ORIG(rels[rel]));
}
