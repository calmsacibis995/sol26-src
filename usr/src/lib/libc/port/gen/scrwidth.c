/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)scrwidth.c	1.5	96/05/31 SMI"

#include	<stdlib.h>
#include	<ctype.h>
#include	<wchar.h>

#pragma weak scrwidth=_scrwidth

int _scrwidth(wchar_t c)
{
   int ret;

   if (!iswprint(c))
      return (0);

   if (!(c & ~0xff))
      {
        return (1);
      }
   else {
		if ((ret = wcwidth(c)) == -1)
		   return (0);
		return (ret);
	}

}
