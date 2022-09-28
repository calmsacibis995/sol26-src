/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)sh_data.c 1.4	95/03/03 SMI"
#include <synch.h>

#ifdef DSHLIB
int	_environ;
#else
#ifdef __STDC__
	#pragma weak environ = _environ
int	_environ = 0;
#else
int	environ = 0;
#endif
#endif

#ifdef _REENTRANT
mutex_t __environ_lock = DEFAULTMUTEX;
#endif _REENTRANT
