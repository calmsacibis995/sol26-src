/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)isblank.c 1.1	95/12/22 SMI"

/*
 * isblank.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/isblank.c 1.1 1995/09/27 18:28:01 ant Exp $";
#endif
#endif

#include <private.h>
#include <wchar.h>

int
(isblank)(int c)
{
	return c == ' ' || c == '\t';
}

int
(iswblank)(wint_t wc)
{
	return wc == ' ' || wc == '\t' || wc == M_MB_L(' ');
}
