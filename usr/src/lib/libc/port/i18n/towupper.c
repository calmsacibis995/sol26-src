/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)towupper.c 1.7	96/07/15  SMI"

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
static char rcsid[] = "@(#)$RCSfile: towupper.c,v $ "
"$Revision: 1.3.2.6 $ (OSF) $Date: 1992/02/20 23:06:35 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: towupper
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/towupper.c, libcchr, bos320, 9130320 7/17/91 15:17:24
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>

#ifdef towupper
#undef towupper
#endif

#pragma weak	towupper = _towupper

wint_t
__towupper_std(_LC_ctype_t *hdl, wint_t wc)
{
	if ((wc > hdl->max_upper) || (wc < 0))
		return (wc);
	return (hdl->upper[wc]);
}

wint_t
_towupper(wint_t wc)
{
	return (METHOD(__lc_ctype, towupper)(__lc_ctype, wc));
}
