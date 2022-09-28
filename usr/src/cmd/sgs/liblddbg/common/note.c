/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)note.c	1.5	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"

/*
 * Print out a single `note' entry.
 */
void
Elf_note_entry(long * np)
{
	long	namesz, descsz;
	int	cnt;

	namesz = *np++;
	descsz = *np++;

	dbg_print(MSG_ORIG(MSG_NOT_TYPE), *np++);
	if (namesz) {
		char *	name = (char *)np;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_ORIG(MSG_NOT_NAME), name);
		name += (namesz + (sizeof (long) - 1)) &
			~(sizeof (long) - 1);
		/* LINTED */
		np = (long *)name;
	}
	if (descsz) {
		for (cnt = 1; descsz; np++, cnt++, descsz -= sizeof (long))
			dbg_print(MSG_ORIG(MSG_NOT_DESC), cnt, *np, *np);
	}
}
