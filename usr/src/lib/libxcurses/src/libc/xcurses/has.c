/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)has.c 1.1	95/12/22 SMI"

/*
 * has.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/has.c 1.2 1995/07/19 16:38:02 ant Exp $";
#endif
#endif

#include <private.h>

bool
(has_colors)()
{
	bool value;

#ifdef M_CURSES_TRACE
	__m_trace("has_colors(void)");
#endif

	value = 0 < max_colors;

	return __m_return_int("has_colors", value);
}

bool
(has_ic)()
{
	bool value;

#ifdef M_CURSES_TRACE
	__m_trace("has_ic(void)");
#endif

	value = ((insert_character != (char *) 0 || parm_ich != (char *) 0)
		&& (delete_character != (char *) 0 || parm_dch != (char *) 0))
		|| (enter_insert_mode != (char *) 0 && exit_insert_mode);

	return __m_return_int("has_ic", value);
}

bool
(has_il)()
{
	bool value;

#ifdef M_CURSES_TRACE
	__m_trace("has_il(void)");
#endif

        value = ((insert_line != (char *) 0 || parm_insert_line != (char *) 0)
                && (delete_line != (char *) 0 || parm_delete_line != (char *)0))
                || change_scroll_region != (char *) 0;

	return __m_return_int("has_il", value);
}

bool
(can_change_color)()
{
	bool value;

#ifdef M_CURSES_TRACE
	__m_trace("can_change_color(void)");
#endif

	value = 2 < max_colors && can_change && initialize_color != (char *) 0;

	return __m_return_int("can_change_color", value);
}

