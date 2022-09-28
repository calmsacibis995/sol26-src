/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)util.c	1.1	96/02/27 SMI"

/* LINTLIBRARY */

#include	"msg.h"

/*
 * Messaging support - funnel everything through _dgettext() as this provides
 * a stub binding to libc, or a real binding to libintl.
 */
extern char *	_dgettext(const char *, const char *);

const char *
_librtld_msg(int mid)
{
	return (_dgettext(MSG_ORIG(MSG_SUNW_OST_SGS), MSG_ORIG(mid)));
}
