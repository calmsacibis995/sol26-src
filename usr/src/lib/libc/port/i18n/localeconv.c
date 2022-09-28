/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)localeconv.c 1.8	96/07/02  SMI"

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
 * static char rcsid[] = "@(#)$RCSfile: localeconv.c,v $ $Revision: 1.10.2.7"
 * " $ (OSF) $Date: 1992/02/20 22:56:45 $";
 */
/*
 * COMPONENT_NAME: (LIBCGEN) Standard C Library General Functions
 *
 * FUNCTIONS: localeconv
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1989
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 *  1.2  com/lib/c/loc/localeconv.c, libcloc, 9130320 7/17/91 15:05:17
 */
/*
 * FUNCTION: localeconv
 *
 * DESCRIPTION: stub function which invokes the locale specific method
 * which implements the localeconv() function.
 *
 * RETURN VALUE:
 * struct lconv * ptr to populated lconv structure
 */

#include <sys/localedef.h>
#include <locale.h>

extern struct lconv *__localeconv_std(_LC_locale_t *);

struct lconv *
localeconv(void)
{
	return (METHOD(__lc_locale, localeconv)(__lc_locale));
}
