/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _CTYPE_H
#define	_CTYPE_H

#pragma ident	"@(#)ctype.h	1.28	96/08/21 SMI"	/* SVr4.0 1.18	*/

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	_U	0x00000001	/* Upper case */
#define	_L	0x00000002	/* Lower case */
#define	_N	0x00000004	/* Numeral (digit) */
#define	_S	0x00000008	/* Spacing character */
#define	_P	0x00000010	/* Punctuation */
#define	_C	0x00000020	/* Control character */
#define	_B	0x00000040	/* Blank */
#define	_X	0x00000080	/* heXadecimal digit */

#define	_ISUPPER	_U
#define	_ISLOWER	_L
#define	_ISDIGIT	_N
#define	_ISSPACE	_S
#define	_ISPUNCT	_P
#define	_ISCNTRL	_C
#define	_ISBLANK	_B
#define	_ISXDIGIT	_X
#define	_ISGRAPH	0x00002000
#define	_ISALPHA	0x00004000
#define	_ISPRINT	0x00008000
#define	_ISALNUM	(_ISALPHA | _ISDIGIT)

#if defined(__STDC__)

extern int isalnum(int);
extern int isalpha(int);
extern int iscntrl(int);
extern int isdigit(int);
extern int isgraph(int);
extern int islower(int);
extern int isprint(int);
extern int ispunct(int);
extern int isspace(int);
extern int isupper(int);
extern int isxdigit(int);
extern int tolower(int);
extern int toupper(int);

#if defined(__EXTENSIONS__) || ((__STDC__ == 0 && \
		!defined(_POSIX_C_SOURCE)) || defined(_XOPEN_SOURCE))

extern int isascii(int);
extern int toascii(int);
extern int _tolower(int);
extern int _toupper(int);

#endif

extern unsigned char	__ctype[];
extern unsigned int	*__ctype_mask;
extern long		*__trans_upper;
extern long		*__trans_lower;

/*
 * Note that the following construct, "!#lint(on)", is a non-standard
 * extension to ANSI-C.  It is maintained here to provide compatibility
 * for existing compilations systems, but should be viewed as transitional
 * and may be removed in a future release.  If it is required that this
 * file not contain this extension, edit this file to remove the offending
 * condition.
 */

#if !#lint(on) && !defined(__lint)

#if defined(__XPG4_CHAR_CLASS__) || defined(_XPG4_2) || \
	(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4))
#define	isalpha(c)	(__ctype_mask[c] & _ISALPHA)
#define	isupper(c)	(__ctype_mask[c] & _ISUPPER)
#define	islower(c)	(__ctype_mask[c] & _ISLOWER)
#define	isdigit(c)	(__ctype_mask[c] & _ISDIGIT)
#define	isxdigit(c)	(__ctype_mask[c] & _ISXDIGIT)
#define	isalnum(c)	(__ctype_mask[c] & _ISALNUM)
#define	isspace(c)	(__ctype_mask[c] & _ISSPACE)
#define	ispunct(c)	(__ctype_mask[c] & _ISPUNCT)
#define	isprint(c)	(__ctype_mask[c] & _ISPRINT)
#define	isgraph(c)	(__ctype_mask[c] & _ISGRAPH)
#define	iscntrl(c)	(__ctype_mask[c] & _ISCNTRL)
#else
#define	isalpha(c)	((__ctype + 1)[c] & (_U | _L))
#define	isupper(c)	((__ctype + 1)[c] & _U)
#define	islower(c)	((__ctype + 1)[c] & _L)
#define	isdigit(c)	((__ctype + 1)[c] & _N)
#define	isxdigit(c)	((__ctype + 1)[c] & _X)
#define	isalnum(c)	((__ctype + 1)[c] & (_U | _L | _N))
#define	isspace(c)	((__ctype + 1)[c] & _S)
#define	ispunct(c)	((__ctype + 1)[c] & _P)
#define	isprint(c)	((__ctype + 1)[c] & (_P | _U | _L | _N | _B))
#define	isgraph(c)	((__ctype + 1)[c] & (_P | _U | _L | _N))
#define	iscntrl(c)	((__ctype + 1)[c] & _C)
#endif  /* defined(__XPG4_CHAR_CLASS__) || defined(_XPG4_2) || ... */

#if defined(__EXTENSIONS__) || \
	((__STDC__ == 0 && !defined(_POSIX_C_SOURCE)) || \
	defined(_XOPEN_SOURCE)) || defined(__XPG4_CHAR_CLASS__)
#define	isascii(c)	(!((c) & ~0177))
#define	toascii(c)	((c) & 0177)
#if defined(__XPG4_CHAR_CLASS__) || defined(_XPG4_2) || \
	(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4))
#define	_toupper(c)	((int)(__trans_upper[c]))
#define	_tolower(c)	((int)(__trans_lower[c]))
#else
#define	_toupper(c)	((__ctype + 258)[c])
#define	_tolower(c)	((__ctype + 258)[c])
#endif /* defined(__XPG4_CHAR_CLASS__) || defined(_XPG4_2) || ... */

#endif /* defined(__EXTENSIONS__) || ((__STDC__ == 0 && ... */


#endif	/* __lint */



#else	/* __STDC__ */

extern unsigned char	_ctype[];

#if !defined(lint) && !defined(__lint)

#define	isalpha(c)	((_ctype + 1)[c] & (_U | _L))
#define	isupper(c)	((_ctype + 1)[c] & _U)
#define	islower(c)	((_ctype + 1)[c] & _L)
#define	isdigit(c)	((_ctype + 1)[c] & _N)
#define	isxdigit(c)	((_ctype + 1)[c] & _X)
#define	isalnum(c)	((_ctype + 1)[c] & (_U | _L | _N))
#define	isspace(c)	((_ctype + 1)[c] & _S)
#define	ispunct(c)	((_ctype + 1)[c] & _P)
#define	isprint(c)	((_ctype + 1)[c] & (_P | _U | _L | _N | _B))
#define	isgraph(c)	((_ctype + 1)[c] & (_P | _U | _L | _N))
#define	iscntrl(c)	((_ctype + 1)[c] & _C)
#define	isascii(c)	(!((c) & ~0177))
#define	_toupper(c)	((_ctype + 258)[c])
#define	_tolower(c)	((_ctype + 258)[c])
#define	toascii(c)	((c) & 0177)

#endif	/* __lint */

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _CTYPE_H */
