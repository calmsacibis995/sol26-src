/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)iswpunct.c 1.6	96/07/02  SMI"

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
static char rcsid[] = "@(#)$RCSfile: iswpunct.c,v $ "
"$Revision: 1.3.2.6 $ (OSF) $Date: 1992/03/17 02:12:06 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: iswpunct
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/iswpunct.c, libcchr, bos320, 9132320 7/23/91 18:19:55
 */
/*
 *
 * FUNCTION: Determines if the process code, pc, is punctuation
 *
 *
 * PARAMETERS: pc  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if pc is not punctuation
 *                >0 - If c is punctuation
 *
 *
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>

#ifdef iswpunct
#undef iswpunct
#endif

#pragma weak iswpunct = _iswpunct

int
_iswpunct(wint_t pc)
{
	return (METHOD(__lc_ctype, iswctype)(__lc_ctype, pc, _ISPUNCT));
}
