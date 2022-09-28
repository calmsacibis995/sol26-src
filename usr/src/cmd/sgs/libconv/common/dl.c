/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dl.c	1.12	96/10/01 SMI"

/* LINTLIBRARY */

#include	<string.h>
#include	"_conv.h"
#include	"dl_msg.h"

/*
 * String conversion routine for dlopen() attributes.
 */
const char *
conv_dlmode_str(int mode)
{
	static	char	string[28] = { '\0' };

	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

	if (mode & RTLD_GLOBAL)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_GLOBAL));
	else if ((mode & RTLD_NOLOAD) != RTLD_NOLOAD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_LOCAL));

	if ((mode & RTLD_NOW) == RTLD_NOW)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NOW));
	else if ((mode & RTLD_NOLOAD) != RTLD_NOLOAD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_LAZY));

	if (mode & RTLD_PARENT)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_PARENT));
	if (mode & RTLD_WORLD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_WORLD));
	if (mode & RTLD_GROUP)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_GROUP));
	if (mode & RTLD_NODELETE)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NODELETE));
	if (mode & RTLD_NOLOAD)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NOLOAD));

	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

	return ((const char *)string);
}

/*
 * String conversion routine for dldump() flags.
 */
const char *
conv_dlflag_str(int flags)
{
	static	char	string[32] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));

	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

	if ((flags & RTLD_REL_ALL) == RTLD_REL_ALL)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_ALL));
	else {
		if (flags & RTLD_REL_RELATIVE)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_RELATIVE));
		if (flags & RTLD_REL_EXEC)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_EXEC));
		if (flags & RTLD_REL_DEPENDS)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_DEPENDS));
		if (flags & RTLD_REL_PRELOAD)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_PRELOAD));
		if (flags & RTLD_REL_SELF)
			(void) strcat(string, MSG_ORIG(MSG_RTLD_REL_SELF));
	}

	if (flags & RTLD_MEMORY)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_MEMORY));
	if (flags & RTLD_STRIP)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_STRIP));
	if (flags & RTLD_NOHEAP)
		(void) strcat(string, MSG_ORIG(MSG_RTLD_NOHEAP));

	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));
	return ((const char *)string);
}
