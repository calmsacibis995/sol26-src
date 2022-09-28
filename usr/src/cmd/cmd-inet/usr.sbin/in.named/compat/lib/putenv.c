/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)putenv.c	1.1	96/05/09 SMI"

#ifndef LINT
static char rcsid[] = "$Id: putenv.c,v 8.1 1994/12/15 06:23:51 vixie Exp $";
#endif

#include "../../conf/portability.h"

/*
 *  To give a little credit to Sun, SGI,
 *  and many vendors in the SysV world.
 */

#if !defined(NEED_PUTENV)
int __bindcompat_putenv;
#else
int
putenv(str)
	char *str;
{
	register char *tmp;

	for (tmp = str; *tmp && (*tmp != '='); tmp++)
		;

	return (setenv(str, tmp, 1));
}
#endif
