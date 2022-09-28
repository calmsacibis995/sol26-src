/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright  (c) 1985 AT&T
 *	All Rights Reserved
 */
#ident	"@(#)vsig.c	1.5	92/07/14 SMI"       /* SVr4.0 1.5 */

/* 
 * This short program is called by a co-process if it wishes to 
 * communicate asynchronously with the controlling FMLI process during
 * the course of its execution.  It blocks til FMLI is ready for a
 * signal then sends a SIGUSR2. 
 */

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include "sizes.h"


char Semaphore[PATHSIZ] = "/tmp/FMLIsem.";

main(argc, argv)
int argc;
char *argv[];
{
	char *vpid;
	char *getenv();

	if ((vpid = getenv("VPID")) == NULL)
		exit(1);
	strcat(Semaphore, vpid);
	fflush(stdout);
	fflush(stderr);
	/*
	 * The reason for the close(open) is to
	 * block until FACE says its is OK
	 * to send a signal ... A signal when
	 * FACE is updating the screen
	 * can create garbage .....
	 */
	close(open(Semaphore, O_WRONLY));
	kill(strtol(vpid, (char **)NULL, 0), SIGUSR2); /* EFT abs k16 */
	exit(0);
}
