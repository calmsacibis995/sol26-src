/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)toascii.c 1.4	96/07/02  SMI"

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
/*
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: toascii.c,v $ "
"$Revision: 1.3.2.2 $ (OSF) $Date: 1992/02/20 23:06:22 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCGEN) Standard C Library General Functions
 *
 * FUNCTIONS: toascii
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1989
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * Copyright (c) 1984 AT&T
 * All Rights Reserved
 *
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 *
 * toascii.c	1.5  com/lib/c/gen,3.1,8943 9/11/89 16:20:05
 */

#ifdef __STDC__
#pragma	weak toascii = _toascii
#define	toascii _toascii
#endif

/*
 *
 * This routine is not included in the ANSI C standard, but is included
 * for compatibility with prior releases
 *
 * Change to ascii (valid only for one byte charcters)
 */

int
toascii(c)
int c;
{
	c &= 0177;
	return (c);
}
