/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tgoto.c 1.2	96/05/30 SMI"

/*
 * tgoto.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/tgoto.c 1.1 1995/07/21 14:20:26 ant Exp $";
#endif
#endif

#include <private.h>

char *
(tgoto)(const char *cap, int col, int row)
{
	const char *str;

#ifdef M_CURSES_TRACE
	__m_trace("tgoto(%p = \"%s\", %d, %d)", cap, col, row);
#endif

	str = tparm((char *)cap, (long) row, (long) col, 0L, 0L, 0L, 0L, 0L, 0L, 0L); 

	return __m_return_pointer("tgoto", (char *)str);
}
