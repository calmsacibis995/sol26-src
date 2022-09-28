/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)globals.c	1.4	96/02/26 SMI"

/* LINTLIBRARY */

#include	<stdio.h>
#include	"globals_msg.h"

/* ARGSUSED */
const char *
conv_invalid_str(char * string, int size, int value, int decimal)
{
	int	format;

	if (decimal)
		format = MSG_GBL_FMT_DEC;
	else
		format = MSG_GBL_FMT_HEX;

	(void) sprintf(string, MSG_ORIG(format), value);
	return ((const char *)string);
}
