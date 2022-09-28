/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)symbols.c	1.3	96/02/26 SMI"

/* LINTLIBRARY */

/*
 * String conversion routines for symbol attributes.
 */
#include	<stdio.h>
#include	"_conv.h"
#include	"symbols_msg.h"

static const int types[] = {
	MSG_STT_NOTYPE,		MSG_STT_OBJECT,		MSG_STT_FUNC,
	MSG_STT_SECTION,	MSG_STT_FILE
};

const char *
conv_info_type_str(unsigned char type)
{
	static char	string[STRSIZE] = { '\0' };

	if (type >= STT_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)type, 0));
	else
		return (MSG_ORIG(types[type]));
}

static const int binds[] = {
	MSG_STB_LOCAL,		MSG_STB_GLOBAL,		MSG_STB_WEAK
};

const char *
conv_info_bind_str(unsigned char bind)
{
	static char	string[STRSIZE] = { '\0' };

	if (bind >= STB_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)bind, 0));
	else
		return (MSG_ORIG(binds[bind]));
}

const char *
conv_shndx_str(Half shndx)
{
	static	char	string[STRSIZE] = { '\0' };

	if (shndx == SHN_UNDEF)
		return (MSG_ORIG(MSG_SHN_UNDEF));
	else if (shndx == SHN_ABS)
		return (MSG_ORIG(MSG_SHN_ABS));
	else if (shndx == SHN_COMMON)
		return (MSG_ORIG(MSG_SHN_COMMON));
	else
		return (conv_invalid_str(string, STRSIZE, (int)shndx, 1));
}
