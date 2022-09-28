/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 *
 * This code is MKS code ported to Solaris with minimum modifications so that
 * upgrades from MKS will readily integrate. Avoid modification if possible!
 */
#ident	"@(#)mks.h 1.11	96/07/31 SMI"

/*
 * MKS header file.  Defines that make programming easier for us.
 * Includes MKS-specific things and posix routines.
 *
 * Copyright 1985, 1993 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 *
 * $Header: /u/rd/h/RCS/mks.h,v 1.215 1994/02/11 19:42:25 tj Exp $
 */

/*
 * This header file in now only access by routines under ../port/regex.
 * It has been updated to remove unused portion of MKS specific things
 * that will no longer to be used, as Solaris has removed MKS regex
 * functions completely.
 */


#ifndef	__M_MKS_H__
#define	__M_MKS_H__
#include <sys/types.h>


/*
 * Write function declarations as follows:
 *	extern char	*function ANSI((char *cp, int flags, NODE *np));
 * Expansion of this happens only when __STDC__ is set.
 */
#ifdef	__STDC__
#define	ANSI(x)	x
#define	_void	void		/* Used in VOID *malloc() */
#else
#define	const
#define	signed
#define	volatile
#define	ANSI(x)	()
#define	_void	char		/* Used in _VOID *malloc() */
#endif


#ifndef	STATIC
#define	STATIC	static		/* Used for function definition */
#endif	/* STATIC */

#ifndef	STATREF
#ifdef	__STDC__
#define	STATREF	static
#else
#define	STATREF		/* Used in local function forward declaration */
#endif
#endif	/* STATREF */

#define	LEXTERN	extern		/* Library external reference */
#define	LDEFN			/* Define Loadable library entry */

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "m_invari.h"

/*
 * Useful additions to sys/stat.h.
 */

#ifndef	M_NULLNAME
#define	M_NULLNAME "/dev/null"
#endif


/*
 * MKS-specific library entry points.
 */
LEXTERN	pid_t	fexecve(const char *, char *const *, char *const *);


/*
 * XXX Porting hacks
 */

#include <wctype.h>

/*
 * Redfine these to avoid exporting names in libc
 */
#define       fexecve         _fexecve

#endif	/* __M_MKS_H__ */
