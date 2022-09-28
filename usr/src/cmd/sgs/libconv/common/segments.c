/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)segments.c	1.6	96/04/22 SMI"

/* LINTLIBRARY */

/*
 * String conversion routine for segment flags.
 */
#include	<string.h>
#include	"libld.h"
#include	"segments_msg.h"

const char *
conv_segaflg_str(unsigned int flags)
{
	static	char	string[110] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		if (flags & FLG_SG_VADDR)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_VADDR));
		if (flags & FLG_SG_PADDR)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_PADDR));
		if (flags & FLG_SG_LENGTH)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_LENGTH));
		if (flags & FLG_SG_ALIGN)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_ALIGN));
		if (flags & FLG_SG_ROUND)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_ROUND));
		if (flags & FLG_SG_FLAGS)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_FLAGS));
		if (flags & FLG_SG_TYPE)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_TYPE));
		if (flags & FLG_SG_ORDER)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_ORDER));
		if (flags & FLG_SG_EMPTY)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_EMPTY));
		if (flags & FLG_SG_NOHDR)
			(void) strcat(string, MSG_ORIG(MSG_FLG_SG_NOHDR));
		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}
