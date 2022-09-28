/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 *		All rights reserved.
 */

#ifndef	_PRIVATE_H
#define	_PRIVATE_H

#pragma ident	"@(#)private.h	1.5	96/07/20 SMI"

/*
 * This file is in the public domain, so clarified as of
 * June 5, 1996 by Arthur David Olson (arthur_david_olson@nih.gov).
 */

/*
 * This header is for use ONLY with the time conversion code.
 * There is no guarantee that it will remain unchanged,
 * or that it will remain at all.
 * Do NOT copy it to any system include directory.
 * Thank you!
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Nested includes
 */
#include <sys/types.h>	/* for time_t */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>	/* for CHAR_BIT */
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * ID
 */

#ifndef lint
#ifndef NOID
static char	privatehid[] = "@(#)private.h	7.43";
#endif /* !defined NOID */
#endif /* !defined lint */

/*
 * Defaults for preprocessor symbols.
 * You can override these in your C compiler options, e.g. `-DHAVE_ADJTIME=0'.
 */

#ifndef HAVE_ADJTIME
#define	HAVE_ADJTIME		1
#endif /* !defined HAVE_ADJTIME */

#ifndef HAVE_GETTEXT
#define	HAVE_GETTEXT		1
#endif /* !defined HAVE_GETTEXT */

#ifndef HAVE_STRERROR
#define	HAVE_STRERROR		1
#endif /* !defined HAVE_STRERROR */

#ifndef HAVE_UNISTD_H
#define	HAVE_UNISTD_H		1
#endif /* !defined HAVE_UNISTD_H */

#ifndef HAVE_UTMPX_H
#define	HAVE_UTMPX_H		1
#endif /* !defined HAVE_UTMPX_H */

#ifndef LOCALE_HOME
#define	LOCALE_HOME		"/usr/lib/locale"
#endif /* !defined LOCALE_HOME */

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX.  */
#define	is_digit(c)		((unsigned)(c) - '0' <= 9)

/*
 * Workarounds for compilers/systems.
 */

/*
 * SunOS 4.1.1 cc lacks const.
 */

#ifndef const
#ifndef __STDC__
#define	const
#endif /* !defined __STDC__ */
#endif /* !defined const */

/*
 * SunOS 4.1.1 cc lacks prototypes.
 */

#ifndef P
#ifdef __STDC__
#define	P(x)	x
#endif /* defined __STDC__ */
#ifndef __STDC__
#define	P(x)	()
#endif /* !defined __STDC__ */
#endif /* !defined P */

/*
 * Finally, some convenience items.
 */

#ifndef TRUE
#define	TRUE	1
#endif /* !defined TRUE */

#ifndef FALSE
#define	FALSE	0
#endif /* !defined FALSE */

#ifndef TYPE_BIT
#define	TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#endif /* !defined TYPE_BIT */

#ifndef TYPE_SIGNED
#define	TYPE_SIGNED(type) (((type) -1) < 0)
#endif /* !defined TYPE_SIGNED */

#ifndef INT_STRLEN_MAXIMUM
/*
 * 302 / 1000 is log10(2.0) rounded up.
 * Subtract one for the sign bit if the type is signed;
 * add one for integer division truncation;
 * add one more for a minus sign if the type is signed.
 */
#define	INT_STRLEN_MAXIMUM(type) \
	((((TYPE_BIT(type) - TYPE_SIGNED(type)) * 302) / 100) + \
		1 + TYPE_SIGNED(type))
#endif /* !defined INT_STRLEN_MAXIMUM */

/*
 * INITIALIZE(x)
 */

#ifndef GNUC_or_lint
#ifdef lint
#define	GNUC_or_lint
#endif /* defined lint */
#ifndef lint
#ifdef __GNUC__
#define	GNUC_or_lint
#endif /* defined __GNUC__ */
#endif /* !defined lint */
#endif /* !defined GNUC_or_lint */

#ifndef INITIALIZE
#ifdef GNUC_or_lint
#define	INITIALIZE(x)	((x) = 0)
#endif /* defined GNUC_or_lint */
#ifndef GNUC_or_lint
#define	INITIALIZE(x)
#endif /* !defined GNUC_or_lint */
#endif /* !defined INITIALIZE */

#ifndef TZ_DOMAIN
#define	TZ_DOMAIN "tz"
#endif /* !defined TZ_DOMAIN */

/*
 * UNIX was a registered trademark of UNIX System Laboratories in 1993.
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _PRIVATE_H */
