/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)longname.c 1.1	95/12/22 SMI"

/*
 * longname.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/longname.c 1.2 1995/06/26 14:55:00 ant Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

char *
longname()
{
	static char buffer[128];

#ifdef M_CURSES_TRACE
	__m_trace("longname(void)");
#endif

	(void) strncpy(
		buffer, strrchr(cur_term->_names, '|') + 1, sizeof buffer - 1
	);

	return __m_return_pointer("longname", buffer);
}

char *
(termname)()
{
#ifdef M_CURSES_TRACE
	__m_trace("termname(void)");
#endif

	return __m_return_pointer("termname", cur_term->_term);
}
