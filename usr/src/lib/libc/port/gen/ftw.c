/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ftw.c	1.17	96/04/18 SMI"	/* SVr4.0 1.6.1.11	*/

/*LINTLIBRARY*/

#if _FILE_OFFSET_BITS == 64
#pragma weak ftw64 = _ftw64
#else /* _FILE_OFFSET_BITS != 64 */
#pragma weak ftw = _ftw
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <string.h>
#include <stdlib.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>

#ifdef _REENTRANT
static mutex_t ftw_lock = DEFAULTMUTEX;
#endif _REENTRANT

int
_ftw(path, fn, depth)
	const char *path;
	int	(*fn)();
	int	depth;
{
	int retval;

	_mutex_lock(&ftw_lock);
	retval = _xftw(_XFTWVER, path, fn, depth);
	_mutex_unlock(&ftw_lock);
	return (retval);
}
