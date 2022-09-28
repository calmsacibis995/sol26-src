/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)phdr.c	1.3	96/02/26 SMI"

/* LINTLIBRARY */

/*
 * String conversion routines for program header attributes.
 */
#include	<string.h>
#include	"_conv.h"
#include	"phdr_msg.h"

static const int phdrs[] = {
	MSG_PT_NULL,		MSG_PT_LOAD,		MSG_PT_DYNAMIC,
	MSG_PT_INTERP,		MSG_PT_NOTE,		MSG_PT_SHLIB,
	MSG_PT_PHDR,
};

const char *
conv_phdrtyp_str(unsigned phdr)
{
	static char	string[STRSIZE] = { '\0' };

	if (phdr >= PT_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)phdr, 0));
	else
		return (MSG_ORIG(phdrs[phdr]));
}

const char *
conv_phdrflg_str(unsigned int flags)
{
	static	char	string[22] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		if (flags & PF_X)
			(void) strcat(string, MSG_ORIG(MSG_PF_X));
		if (flags & PF_W)
			(void) strcat(string, MSG_ORIG(MSG_PF_W));
		if (flags & PF_R)
			(void) strcat(string, MSG_ORIG(MSG_PF_R));
		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}
