/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)cuexit.s	1.7	96/07/23 SMI"	/* SVr4.0 1.7	*/

/* C library -- exit						*/
/* void exit (int status);					*/

	.file	"cuexit.s"

#include "SYS.h"

	ENTRY(exit)
	save	%sp, -SA(MINFRAME), %sp
	call	_exithandle
	nop
	restore
	SYSTRAP(exit)

	SET_SIZE(exit)
