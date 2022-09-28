/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)isalnum.c 1.5	96/07/02  SMI"

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
static char rcsid[] = "@(#)$RCSfile: isalnum.c,v "
"$ $Revision: 1.6.2.7 $ (OSF) $Date: 1992/03/17 02:10:54 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: isalnum
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/isalnum.c, libcchr, bos320, 9132320 7/23/91 18:16:46
 */
/*
 *
 * FUNCTION: Determines if a character is alpha-numeric
 *
 *
 * PARAMETERS: c  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if c is not part of the character class
 *                >0 - If c is part of the character class
 *
 *
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>

#ifdef isalnum
#undef isalnum
#endif

extern	_LC_ctype_t	*__lc_ctype;

int
isalnum(int c)
{

	if ((c >= 0) && (c < 256))
		return (__lc_ctype->mask[c] & _ISALNUM);
	else
		return (0);

}
