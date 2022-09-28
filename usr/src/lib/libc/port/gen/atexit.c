/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)atexit.c	1.11	96/07/19 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <stdlib.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>

#define	MAXEXITFNS	37

static void	(*exitfns[MAXEXITFNS])();
static int	numexitfns = 0;
#ifdef _REENTRANT
static mutex_t exitfns_lock = DEFAULTMUTEX;
#endif _REENTRANT

int
atexit(void (*func)())
{
	int ret = 0;

	(void) _mutex_lock(&exitfns_lock);
	if (numexitfns >= MAXEXITFNS)
		ret = -1;
	else
		exitfns[numexitfns++] = func;
	(void) _mutex_unlock(&exitfns_lock);
	return (ret);
}


void
_exithandle(void)
{
	(void) _mutex_lock(&exitfns_lock);
	while (--numexitfns >= 0)
		(*exitfns[numexitfns])();
	(void) _mutex_unlock(&exitfns_lock);
}
