/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sections.c	1.6	96/08/19 SMI"

/* LINTLIBRARY */

/*
 * String conversion routines for section attributes.
 */
#include	<string.h>
#include	"_conv.h"
#include	"sections_msg.h"

static const int secs[] = {
	MSG_SHT_NULL,		MSG_SHT_PROGBITS,	MSG_SHT_SYMTAB,
	MSG_SHT_STRTAB,		MSG_SHT_RELA,		MSG_SHT_HASH,
	MSG_SHT_DYNAMIC,	MSG_SHT_NOTE,		MSG_SHT_NOBITS,
	MSG_SHT_REL,		MSG_SHT_SHLIB,		MSG_SHT_DYNSYM
};

const char *
conv_sectyp_str(unsigned int sec)
{
	static char	string[STRSIZE] = { '\0' };

	if (sec >= SHT_NUM) {
		if (sec == (unsigned int)SHT_SUNW_verdef)
			return (MSG_ORIG(MSG_SHT_SUNW_verdef));
		else if (sec == (unsigned int)SHT_SUNW_verneed)
			return (MSG_ORIG(MSG_SHT_SUNW_verneed));
		else if (sec == (unsigned int)SHT_SUNW_versym)
			return (MSG_ORIG(MSG_SHT_SUNW_versym));
		else
			return (conv_invalid_str(string, STRSIZE, (int)sec, 0));
	} else
		return (MSG_ORIG(secs[sec]));
}

const char *
conv_secflg_str(unsigned int flags)
{
	static	char	string[40] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		if (flags & SHF_WRITE)
			(void) strcat(string, MSG_ORIG(MSG_SHF_WRITE));
		if (flags & SHF_ALLOC)
			(void) strcat(string, MSG_ORIG(MSG_SHF_ALLOC));
		if (flags & SHF_EXECINSTR)
			(void) strcat(string, MSG_ORIG(MSG_SHF_EXECINSTR));
		if (flags & SHF_EXCLUDE)
			(void) strcat(string, MSG_ORIG(MSG_SHF_EXCLUDE));
		if (flags & SHF_ORDERED)
			(void) strcat(string, MSG_ORIG(MSG_SHF_ORDERED));
		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}
