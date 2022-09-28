/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)dup2.c	1.12	96/02/29 SMI"	/* SVr4.0 1.8	*/

#ifdef __STDC__
#pragma weak dup2 = _dup2
#endif
#include 	"synonyms.h"
#include	<fcntl.h>

int
dup2(fildes, fildes2)
int	fildes,		/* file descriptor to be duplicated */
	fildes2;	/* desired file descriptor */
{
	return (fcntl(fildes, F_DUP2FD, fildes2));
}
