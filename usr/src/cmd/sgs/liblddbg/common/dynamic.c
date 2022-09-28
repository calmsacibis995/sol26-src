/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dynamic.c	1.11	96/09/11 SMI"

/* LINTLIBRARY */

#include	<link.h>
#include	<stdio.h>
#include	"msg.h"
#include	"_debug.h"

/*
 * Print out the dynamic section entries.
 */
void
Elf_dyn_print(Dyn * dyn, const char * names)
{
	const char *	name;
	char		index[10];
	int		ndx;

	dbg_print(MSG_INTL(MSG_DYN_TITLE));

	for (ndx = 1; dyn->d_tag != DT_NULL; ndx++, dyn++) {
		/*
		 * Print the information numerically, and if possible
		 * as a string.
		 */
		if (names && ((dyn->d_tag == DT_NEEDED) ||
		    (dyn->d_tag == DT_SONAME) ||
		    (dyn->d_tag == DT_FILTER) ||
		    (dyn->d_tag == DT_AUXILIARY) ||
#ifdef	ENABLE_CACHE
		    (dyn->d_tag == DT_CACHE) ||
#endif
		    (dyn->d_tag == DT_RPATH) ||
		    (dyn->d_tag == DT_USED)))
			name = names + dyn->d_un.d_ptr;
		else if (dyn->d_tag == DT_FLAGS_1)
			name = conv_dynflag_1_str(dyn->d_un.d_ptr);
		else
			name = MSG_ORIG(MSG_STR_EMPTY);

		(void) sprintf(index, MSG_ORIG(MSG_FMT_INDEX), ndx);
		dbg_print(MSG_INTL(MSG_DYN_ENTRY), index,
		    conv_dyntag_str(dyn->d_tag), dyn->d_un.d_val, name);
	}
}
