/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)strxfrm.c 1.9	96/07/02  SMI"


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
static char rcsid[] = "@(#)$RCSfile: strxfrm.c,v $ $Revision: 1.8.2.5 "
	"$ (OSF) $Date: 1992/02/20 23:05:40 $";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: strxfrm
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1989
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.18  com/lib/c/str/strxfrm.c, libcstr, bos320,9130320 7/17/91 15:06:35
 */

#include <sys/localedef.h>


#pragma weak strxfrm = _strxfrm

#ifdef	PIC
size_t
_strxfrm(char *s1, const char *s2, size_t n)
{
	return (METHOD(__lc_collate, strxfrm)(__lc_collate, s1, s2, n));
}


size_t
__strxfrm_C(_LC_collate_t *coll, char *s1, const char *s2, size_t n)
#else
size_t
__strxfrm_C(_LC_collate_t *coll, char *s1, const char *s2, size_t n) {}


size_t
_strxfrm(char *s1, const char *s2, size_t n)
#endif	/* PIC */
{
	if (n != 0) {
		strncpy(s1, s2, n);
		s1[n - 1] = '\0';
	}

	return (strlen(s2));
}
