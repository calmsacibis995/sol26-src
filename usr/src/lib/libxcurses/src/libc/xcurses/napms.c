/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)napms.c 1.1	95/12/22 SMI"

/*
 * napms.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/napms.c 1.1 1995/06/16 20:35:38 ant Exp $";
#endif
#endif

#include <private.h>
#include <sys/types.h>
#include <unistd.h>

int
napms(ms)
int ms;
{
#ifdef M_CURSES_TRACE
	__m_trace("napms(%d)", ms);
#endif

	if (0 < ms) {
#ifdef M_HAS_USLEEP
		if (1000000L < ms)
			return __m_return_code("napms", ERR);

		(void) usleep((usecond_t) ms);
#else
		(void) sleep((unsigned) (0 < ms / 1000 ? ms / 1000 : 1));
#endif
	}

	return __m_return_code("napms", OK);
}
