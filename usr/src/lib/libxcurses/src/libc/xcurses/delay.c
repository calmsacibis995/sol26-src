/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)delay.c 1.1	95/12/22 SMI"

/*
 * delay.c
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

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/delay.c 1.1 1995/06/05 19:32:22 ant Exp $";
#endif
#endif

#include <private.h>

/*
 * Insert an N milli-second delay by inserting pad characters 
 * into the output stream.
 */
int
delay_output(ms)
int ms;
{
	int null = '\0';
	unsigned number, baud;

#ifdef M_CURSES_TRACE
	__m_trace("delay_output(%d)", ms);
#endif
	
	baud = baudrate();

	if (!no_pad_char) {
		if (pad_char != (char *) 0)
			null = *pad_char;
		number = (baud/10 * ms)/1000;
		while (0 < number--)
			(void) __m_putchar(null);
	}

	return __m_return_code("delay_output", OK);
}

