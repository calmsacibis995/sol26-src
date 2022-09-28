/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tcdrain.c	1.8	96/05/02 SMI"	/* SVr4.0 1.1	*/

#include "synonyms.h"
#include <sys/termios.h>

/*
 * wait until all output on the filedes is drained
 */
#ifdef __STDC__
#pragma weak	_libc_tcdrain = _tcdrain
#endif

int tcdrain(fildes)
int fildes;
{
	return(ioctl(fildes,TCSBRK,1));
}
