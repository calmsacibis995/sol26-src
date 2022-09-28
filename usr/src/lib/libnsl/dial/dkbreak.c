/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)dkbreak.c	1.5	92/07/14 SMI"	/* SVr4.0 1.1	*/

#ifndef DIAL
	static char	SCCSID[] = "@(#)dkbreak.c	2.2+BNU DKHOST 86/04/02";
#endif
/*
 *	COMMKIT(TM) Software - Datakit(R) VCS Interface Release 2.0 V1
 *			Copyright 1984 AT&T
 *			All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *     The copyright notice above does not evidence any actual
 *          or intended publication of such source code.
 */
#include "dk.h"
#include <rpc/trace.h>


static short	sendbreak[3] = {
	72, 0, 0
};	/* Asynch Break Level-D code */

GLOBAL void
dkbreak(fd)
{
	char	nothing[1];

	trace1(TR_dkbreak, 0);
	ioctl(fd, DIOCXCTL, sendbreak);

	write(fd, nothing, 0);
	trace1(TR_dkbreak, 1);
}
