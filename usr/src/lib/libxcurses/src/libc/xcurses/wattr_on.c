/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wattr_on.c 1.1	95/12/22 SMI"

/*
 * wattr_on.c
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

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wattr_on.c 1.3 1995/06/05 18:55:16 ant Exp $";
#endif
#endif

#include <private.h>

#undef wattr_on

int
wattr_on(WINDOW *w, attr_t at, void *opts)
{
#ifdef M_CURSES_TRACE
        __m_trace("wattr_on(%p, %x, %p)", w, at, opts);
#endif

	w->_fg._at |= at;

	return __m_return_code("wattr_on", OK);
}

#undef wattr_off

int
wattr_off(WINDOW *w, attr_t at, void *opts)
{
#ifdef M_CURSES_TRACE
        __m_trace("wattr_off(%p, %x, %p)", w, at, opts);
#endif

	w->_fg._at &= ~at;

	return __m_return_code("wattr_off", OK);
}

#undef wattr_set

int
wattr_set(WINDOW *w, attr_t at, short co, void *opts)
{
#ifdef M_CURSES_TRACE
        __m_trace("wattr_set(%p, %x, %d, %p)", w, at, co, opts);
#endif

	w->_fg._co = co;
	w->_fg._at = at;

	return __m_return_code("wattr_set", OK);
}

#undef wattr_get

int
wattr_get(WINDOW *w, attr_t *at, short *co, void *opts)
{
#ifdef M_CURSES_TRACE
        __m_trace("wattr_get(%p, %p, %p, %p)", w, at, co, opts);
#endif

	if (at != (attr_t *) 0)
		*at = w->_fg._at;

	if (co != (short *) 0)
		*co = w->_fg._co;

	return __m_return_int("wattr_get", OK);
}

#undef wcolor_set

int
wcolor_set(WINDOW *w, short co, void *opts)
{
#ifdef M_CURSES_TRACE
        __m_trace("wcolor_set(%p, %d, %p)", w, co, opts);
#endif

	w->_fg._co = co;

	return __m_return_code("wcolor_set", OK);
}

#undef wstandout

int
wstandout(WINDOW *w)
{
#ifdef M_CURSES_TRACE
        __m_trace("wstandout(%p)", w);
#endif

	w->_fg._at |= WA_STANDOUT;

	return __m_return_int("wstandout", 1);
}

#undef wstandend

int
wstandend(WINDOW *w)
{
#ifdef M_CURSES_TRACE
        __m_trace("wstandend(%p)", w);
#endif

	w->_fg._at = WA_NORMAL;

	return __m_return_int("wstandend", 1);
}

