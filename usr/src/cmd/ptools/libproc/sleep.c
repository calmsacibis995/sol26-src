/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sleep.c	1.4	96/06/18 SMI"

#include <stropts.h>
#include <poll.h>

void
msleep(unsigned int msec)	/* milliseconds to sleep */
{
	struct pollfd pollfd;

	pollfd.fd = -1;
	pollfd.events = 0;
	pollfd.revents = 0;

	if (msec)
		(void) poll(&pollfd, 0UL, msec);
}

/* This is for the !?!*%! call to sleep() in execvp() */
unsigned int
sleep(unsigned int sec)		/* seconds to sleep */
{
	msleep(sec*1000);
	return (0);
}
