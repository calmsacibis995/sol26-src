/*	Copyright (c) 1996 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)fcntl.c	1.1	96/03/11	SMI"
/*
 * fcntl() is being interposed here so that it can be made a cancellation
 * point. But fcntl() is a cancellation point only if cmd == F_SETLKW.
 *
 * fcntl()'s prototype has variable arguments. This makes very difficult
 * to pass arguments to libc version and cancel version.
 *
 * We are including <sys/fcntl.h> to bypass the prototype and to include
 * F_SETLKW definition. We are also assuming that fcntl() can only have
 * three arguments at the most.
 */
#ifdef __STDC__
#pragma	weak _ti_fcntl = fcntl
#endif /* __STDC__ */

#include <sys/types.h>
#include <sys/fcntl.h>

int fcntl(int fildes, int cmd, int arg)
{
	if (cmd == F_SETLKW) {
		_fcntl_cancel(fildes, cmd, arg);
	} else {
		_fcntl(fildes, cmd, arg);
	}
}

