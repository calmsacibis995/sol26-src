/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)util.c	1.10	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"

/*
 * Print out the input file list.
 */
void
_Dbg_ifl_print(List * lfl)
{
	Listnode *	lnp;
	Ifl_desc *	ifl;
	Half		sec;

	dbg_print(MSG_INTL(MSG_UTL_LIST));
	for (LIST_TRAVERSE(lfl, lnp, ifl)) {
		dbg_print(MSG_INTL(MSG_UTL_FILE), ifl->ifl_name,
			ifl->ifl_ehdr->e_shnum);
		for (sec = 0; sec < ifl->ifl_ehdr->e_shnum; sec++) {
			if (ifl->ifl_isdesc[sec] == (Is_desc *)0)
				dbg_print(MSG_ORIG(MSG_UTL_SEC_1), sec);
			else
				dbg_print(MSG_ORIG(MSG_UTL_SEC_2), sec,
					ifl->ifl_isdesc[sec]->is_name);
		}
	}
}

/*
 * Generic new line generator.
 */
void
Dbg_util_nl()
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

/*
 * If any run-time linker debugging is being carried out always indicate the
 * fact and specify the point at which we transfer control to the main program.
 */
void
Dbg_util_call_main(const char * name)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_UTL_TRANS), name);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_util_call_init(const char * name)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_UTL_INIT), name);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_util_call_fini(const char * name)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_UTL_FINI), name);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_util_str(const char * name)
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_ORIG(MSG_FMT_STR), name);
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}
