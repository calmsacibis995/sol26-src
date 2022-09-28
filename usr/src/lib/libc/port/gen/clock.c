/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)clock.c	1.10	96/10/04 SMI"	/* SVr4.0 1.6.2.3	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>	/* for HZ (clock frequency in Hz) */

#define	TIMES(B) (B.tms_utime+B.tms_stime+B.tms_cutime+B.tms_cstime)

clock_t
clock()
{
	struct tms buffer;
	static int Hz = 0;
	static long first;
	extern int gethz();

	if (times(&buffer) == (clock_t) -1)
		return ((clock_t) -1);
	if (Hz == 0) {
		if ((Hz = gethz()) == 0)
			Hz = HZ;
		first = TIMES(buffer);
	}
	return ((TIMES(buffer) - first) * (1000000L/Hz));
}
