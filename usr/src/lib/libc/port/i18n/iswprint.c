/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)iswprint.c 1.6	96/07/02  SMI"

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
static char rcsid[] = "@(#)$RCSfile: iswprint.c,v $ "
"$Revision: 1.3.2.6 $ (OSF) $Date: 1992/03/17 02:12:01 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: iswprint
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/iswprint.c, libcchr, bos320, 9132320 7/23/91 18:19:44
 */
/*
 *
 * FUNCTION: Determines if the process code, pc, is a printable character
 *
 *
 * PARAMETERS: pc  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if pc is not printable
 *                >0 - If c is printable
 *
 *
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>

#ifdef iswprint
#undef iswprint
#endif

#pragma weak iswprint = _iswprint

int
_iswprint(wint_t pc)
{
	return (METHOD(__lc_ctype, iswctype)(__lc_ctype, pc, _ISPRINT));
}
