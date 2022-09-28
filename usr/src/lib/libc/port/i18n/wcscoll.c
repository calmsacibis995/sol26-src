/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)wcscoll.c 1.7	96/07/02  SMI"


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
 *
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: wcscoll.c,v $ $Revision: 1.3.2.6 "
	"$ (OSF) $Date: 1992/03/30 02:40:56 $";
#endif
 */

/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: wcscoll
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1989
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.3  com/lib/c/str/wcscoll.c, libcstr, bos320, 9130320 7/17/91 15:07:40
 */

#include <sys/localedef.h>


#pragma weak wcscoll = _wcscoll

#ifdef	PIC
int
_wcscoll(const wchar_t *wcs1, const wchar_t *wcs2)
{
	return (METHOD(__lc_collate, wcscoll)(__lc_collate, wcs1, wcs2));
}    


int
__wcscoll_C(_LC_collate_t *coll, const wchar_t *wcs1, const wchar_t *wcs2)
#else
int
__wcscoll_C(_LC_collate_t *coll, const wchar_t *wcs1, const wchar_t *wcs2) {}


int
_wcscoll(const wchar_t *wcs1, const wchar_t *wcs2)
#endif	/* PIC */
{
	return (wcscmp(wcs1, wcs2));
}
