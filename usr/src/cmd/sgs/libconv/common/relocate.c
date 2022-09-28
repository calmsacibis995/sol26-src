/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */
#pragma ident	"@(#)relocate.c 1.1	96/02/26 SMI"

/* LINTLIBRARY */

/*
 * String conversion routine for relocation types.
 */
#include	<stdio.h>
#include	"_conv.h"

/*
 * Generic front-end that determines machine specific relocations.
 */
const char *
conv_reloc_type_str(Half mach, Word rel)
{
	static char	string[STRSIZE] = { '\0' };

	if (mach == EM_386)
		return (conv_reloc_386_type_str(rel));

	if (mach == EM_PPC)
		return (conv_reloc_PPC_type_str(rel));

	if ((mach == EM_SPARC) || (mach = EM_SPARC32PLUS))
		return (conv_reloc_SPARC_type_str(rel));

	return (conv_invalid_str(string, STRSIZE, rel, 0));
}
