/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tolower.c 1.6	96/07/15  SMI"

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
static char rcsid[] = "@(#)$RCSfile: tolower.c,v $ "
"$Revision: 1.6.2.5 $ (OSF) $Date: 1992/02/20 23:06:25 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCGEN) Standard C Library General Functions
 *
 * FUNCTIONS: tolower
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1985, 1989
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/tolower.c, libcchr, bos320, 9130320 7/17/91 15:20:02
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>

#ifdef tolower
#undef tolower
#endif

extern	_LC_ctype_t	*__lc_ctype;

int
tolower(int c)
{

	if ((c >= 0) && (c < 256))
		return (__lc_ctype->lower[c]);
	else
		return (c);

}
