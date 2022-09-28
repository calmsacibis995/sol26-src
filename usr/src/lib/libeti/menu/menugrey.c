/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)menugrey.c	1.2	92/07/14 SMI"	/* SVr4.0 1.3	*/

#include "private.h"

int
set_menu_grey (m, attr)
register MENU *m;
chtype attr;
{
  if (InvalidAttr(attr)) {
    return E_BAD_ARGUMENT;
  }
  if (m) {
    Grey(m) = attr;
    if (Posted(m)) {
      _draw (m);		/* Redraw the menu */
      _show (m);		/* Redisplay menu */
    }
  }
  else {
    Grey(Dfl_Menu) = attr;
  }
  return E_OK;
}

chtype
menu_grey (m)
register MENU *m;
{
  return Grey(m ? m : Dfl_Menu);
}
