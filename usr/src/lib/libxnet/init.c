/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)init.c	1.2	96/08/09 SMI"

static int libxnet_init(void);
#pragma	init(libxnet_init)

/*
 *	This bit of sillyness is necessary because an application which
 *	calls a function in this filter library when the application is
 *	out of the resources necessary to map in and link the library (e.g.
 *	if the file descriptor limit has been reached) will cause the
 *	dynamic linker to fail, and therefore exit.  Forcing the dynamic
 *	linker to perform a data relocation causes it to map this library
 *	and its dependents in initially and avoids the problem.
 */
static int
libxnet_init(void)
{
	extern int h_errno;

	return (h_errno);
}
