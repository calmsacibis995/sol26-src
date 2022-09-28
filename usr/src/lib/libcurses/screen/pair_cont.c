/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pair_cont.c	1.5	92/07/14 SMI"	/* SVr4.0 1.4	*/

#include "curses_inc.h"

pair_content(pair, f, b)
short pair, *f, *b;
{
    register _Color_pair *ptp;

    if (pair < 1 || pair >= COLOR_PAIRS ||
	(ptp = cur_term->_pairs_tbl) == (_Color_pair *) NULL)
	return (ERR);

    ptp += pair;

    if (!ptp->init)
        return (ERR);

    *f = ptp->foreground;
    *b = ptp->background;
    return (OK);
}
