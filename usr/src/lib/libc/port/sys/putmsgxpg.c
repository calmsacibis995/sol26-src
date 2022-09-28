/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)putmsgxpg.c	1.2	96/10/18 SMI"
/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 *
 */

#include	"synonyms.h"
#include	<stropts.h>

int
__xpg4_putmsg(fildes, ctlptr, dataptr, flags)
	int fildes;
	const struct strbuf *ctlptr;
	const struct strbuf *dataptr;
	int flags;
{
	return (putmsg(fildes, ctlptr, dataptr, flags|MSG_XPG4));
}

int
__xpg4_putpmsg(fildes, ctlptr, dataptr, band, flags)
	int fildes;
	const struct strbuf *ctlptr;
	const struct strbuf *dataptr;
	int band;
	int flags;
{
	return (putpmsg(fildes, ctlptr, dataptr, band, flags|MSG_XPG4));
}
