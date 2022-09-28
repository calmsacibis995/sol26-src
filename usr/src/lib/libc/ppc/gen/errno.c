/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)errno.c 1.3	94/09/09 SMI"

#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#undef errno
#ifdef __STDC__
/* These two pragmas should disappear */
#pragma weak _thr_errno_addr = ___errno
#pragma weak __errno_fix = ___errno
#endif __STDC__


int *
___errno()
{
	extern int errno;
	extern int *_thr_errnop();

	if (_thr_main())
		return (&errno);
	else
		return (_thr_errnop());
}
