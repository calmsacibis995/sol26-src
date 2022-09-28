/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tell.c	1.12	96/02/26 SMI"	/* SVr4.0 1.9	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * return offset in file.
 */
#pragma weak tell = _tell
#include "synonyms.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

long
tell(f)
int	f;
{
	return (lseek64(f, 0, SEEK_CUR));
}
