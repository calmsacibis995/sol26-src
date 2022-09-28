/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)V3.vidattr.c	1.5	92/07/14 SMI"	/* SVr4.0 1.3	*/

#include	"curses_inc.h"
#ifdef __STDC__
extern	int	_outchar(char);
#else
extern	int	_outchar();
#endif

#ifdef	_VR3_COMPAT_CODE
#undef	vidattr

vidattr(a)
_ochtype	a;
{
    vidupdate(_FROM_OCHTYPE(a), cur_term->sgr_mode, _outchar);
    return (OK);
}
#endif	/* _VR3_COMPAT_CODE */
