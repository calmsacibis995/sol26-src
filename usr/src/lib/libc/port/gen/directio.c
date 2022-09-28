/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)directio.c 1.1	96/04/23 SMI"

#include <unistd.h>
#include <sys/filio.h>

/*
 * directio() allows an application to provide advise to the
 * filesystem to optimize read and write performance.
 */

int
directio(int fildes, int advice)
{
	return (ioctl(fildes, _FIODIRECTIO, advice));
}
