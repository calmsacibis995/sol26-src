/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)wctype.c 1.9	96/07/02  SMI"

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
static char rcsid[] = "@(#)$RCSfile: wctype.c,v $ "
"$Revision: 1.1.4.2 $ (OSF) $Date: 1992/11/20 19:37:53 $";
#endif
*/

/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: wctype
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/wctype.c, libcchr, bos320, 9130320 7/17/91 15:15:57
 */

#include <sys/localedef.h>
#include <string.h>

wctype_t
__wctype_std(_LC_ctype_t *hdl, const char *name)
{
	int i;

	/* look for mask value in the lc_bind table */
	for (i = 0; i < hdl->nbinds; i++) {
		if ((hdl->bindtab[i].bindtag == _LC_TAG_CCLASS) &&
			(strcmp(name, hdl->bindtab[i].bindname) == 0)) {
				return (wctype_t)(hdl->bindtab[i].bindvalue);
		}
	}

	return (0);	/* value not found */
}

#pragma weak wctype = __wctype

wctype_t
__wctype(const char *name)
{
	return (METHOD(__lc_ctype, wctype)(__lc_ctype, name));
}
