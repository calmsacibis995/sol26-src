/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ifndef __LOCALE_H
#define	__LOCALE_H

#pragma ident	"@(#)locale.h	1.16	96/08/28 SMI"

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
 * @(#)$RCSfile: locale.h,v $ $Revision: 1.11.4.3 $ (OSF)
 * $Date: 1992/10/26 20:29:12 $
 */
/*
 * COMPONENT_NAME: (LIBCLOC) Locale Related Data Structures and API
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1985, 1989
 * All Rights Reserved
 * Licensed Materials - Property of IBM
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.24  com/inc/locale.h, libcnls, bos320, 9132320h 8/7/91
 */

#if defined(__EXTENSIONS__) || __STDC__ - 0 == 0
#include <libintl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct lconv {
	char *decimal_point;	/* decimal point character */
	char *thousands_sep;	/* thousands separator */
	char *grouping;			/* digit grouping */
	char *int_curr_symbol;	/* international currency symbol */
	char *currency_symbol;	/* national currency symbol */
	char *mon_decimal_point;	/* currency decimal point */
	char *mon_thousands_sep;	/* currency thousands separator */
	char *mon_grouping;		/* currency digits grouping */
	char *positive_sign;	/* currency plus sign */
	char *negative_sign;	/* currency minus sign */
	char int_frac_digits;	/* internat currency fractional digits */
	char frac_digits;		/* currency fractional digits */
	char p_cs_precedes;		/* currency plus location */
	char p_sep_by_space;	/* currency plus space ind. */
	char n_cs_precedes;		/* currency minus location */
	char n_sep_by_space;	/* currency minus space ind. */
	char p_sign_posn;		/* currency plus position */
	char n_sign_posn;		/* currency minus position */
};

#define	LC_CTYPE	0	/* locale's ctype handline */
#define	LC_NUMERIC	1	/* locale's decimal handling */
#define	LC_TIME		2	/* locale's time handling */
#define	LC_COLLATE	3	/* locale's collation data */
#define	LC_MONETARY	4	/* locale's monetary handling */
#define	LC_MESSAGES	5	/* locale's messages handling */
#define	LC_ALL		6	/* name of locale's category name */

#define	_LastCategory	LC_MESSAGES	/* This must be last category */

#define	_ValidCategory(c) \
	(((int)(c) >= LC_CTYPE) && ((int)(c) <= _LastCategory) || \
	((int)c == LC_ALL))


#ifndef NULL
#define	NULL	0
#endif

#if	defined(__STDC__)
extern char	*setlocale(int, const char *);
extern struct lconv *localeconv(void);
#else
extern char   *setlocale();
extern struct lconv	*localeconv();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* __LOCALE_H */
