/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)trace.c 1.1	95/12/22 SMI"

/*
 * trace.c
 *
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All right reserved.
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/trace.c 1.3 1995/06/12 20:24:05 ant Exp $";
#endif
#endif

#include <private.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

static int __m_tracing = FALSE;

/*f
 *  Write a formatted string into a trace file.
 */
void
__m_trace(const char *fmt, ...)
{
	va_list vp;
	static FILE *fp;
	static int initialized = FALSE;

	if (!__m_tracing)
		return;

	if (!initialized) {
		fp = fopen("trace.out", "w");
		if (fp == (FILE *) 0) {
			fprintf(stderr, "Program cannot open \"trace.out\".\n");
			exit(1);
		}
		initialized = TRUE;
	}

	va_start(vp, fmt);
	(void) vfprintf(fp, fmt, vp);
	va_end(vp);
	fputc('\n', fp);
}

int
(__m_return_code)(const char *s, int code)
{
	switch (code) {
	case OK:
		__m_trace("%s returned OK.", s);
		break;
	case ERR:
		__m_trace("%s returned ERR.", s);
		break;
	case KEY_CODE_YES:
		__m_trace("%s returned KEY_CODE_YES.", s);
		break;
	default:
		__m_trace("%s returned code %d", s, code);
	}

	return code;
}

int
(__m_return_int)(const char *s, int value)
{
	__m_trace("%s returned %d", s, value);

	return value;
}

chtype
(__m_return_chtype)(const char *s, chtype ch)
{
	__m_trace("%s returned %lx", s, ch);

	return ch;
}

void *
(__m_return_pointer)(const char *s, const void *ptr)
{
	if (ptr == (void *) 0)
		__m_trace("%s returned NULL.", s);
	else
		__m_trace("%s returned %p.", s, ptr);

	return (void *) ptr;
}

#undef __m_return_void

void
__m_return_void(const char *s)
{
	__m_trace("%s returns void.");
}

/*f
 *  Turn tracing on
 */
void
traceon()
{
    	__m_tracing = TRUE;
	__m_trace("traceon()\ntraceon() returns void.");
}

/*f
 *  Turn tracing off
 */
void
traceoff()
{
	__m_trace("traceoff()\ntraceoff() returns void.");
    	__m_tracing = FALSE;
}

