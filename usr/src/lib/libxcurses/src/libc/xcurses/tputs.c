/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tputs.c 1.1	95/12/22 SMI"

/*
 * tputs.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/tputs.c 1.4 1995/07/19 12:44:45 ant Exp $";
#endif
#endif

#include <private.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

int
__m_putchar(byte)
int byte;
{
	return putchar(byte);
}

int
(putp)(const char *s)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("putp(%p = \"%s\")", s, s);
#endif

	code = tputs(s, 1, __m_putchar);

	return __m_return_code("putp", code);
}

/*f
 * Apply padding information to a string and write it out.
 * Note the '/' option is not supported.
 */
int
tputs(string, affcnt, putout)
const char *string;
int affcnt;
int (*putout)(int);
{
	char *mark;
	int i, baud, len, null, number;

#ifdef M_CURSES_TRACE
	__m_trace("tputs(%p = \"%s\", %d, %p)", string, string, affcnt, putout);
#endif

	baud = baudrate();
	null = pad_char == (char *) 0 ? '\0' : pad_char[0];

	for (len = 0; *string; ++string){
		/* Look for "$<num.????>" */
		if (*string == '$' 
		&& string[1] == '<' 
		&& (isdigit(string[2]) || string[2] == '.')
		&& (mark = strchr(string, '>'))){
			number = atoi(string+2) * 10;
			if ((string = strchr(string, '.')) != (char *) 0)
				number += *++string-'0';	
			string = mark;
			if (*--mark == '*')
				number *= affcnt;
			if (padding_baud_rate &&  baud >= padding_baud_rate 
			&& !xon_xoff) {
				number = (baud/10 * number)/1000;
				len += number;
				if (putout != (int (*)(int)) 0) {
					for (i=0; i < number; i++)
						(void) (*putout)(null);
				}
			}
		} else {
			++len;
			if (putout != (int (*)(int)) 0)
				(void) (*putout)(*string);
		}
	}

	return __m_return_int("tputs", len);
}


