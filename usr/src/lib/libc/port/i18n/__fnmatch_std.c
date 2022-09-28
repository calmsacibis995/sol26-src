/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)__fnmatch_std.c 1.13	96/07/11  SMI"


/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)69  1.6.1.3  "
	"src/bos/usr/ccs/lib/libc/__fnmatch_std.c, bos, bos410 "
	"1/12/93 11:09:07";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCPAT) Standard C Library Pattern Functions
 *
 * FUNCTIONS: __fnmatch_std
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991, 1992
 * All Rights Reserved
 * Licensed Materials - Property of IBM
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */


#include <sys/types.h>
#include <stdlib.h>
#include <sys/localedef.h>
#include <fnmatch.h>
#include "patlocal.h"


static int bracket(_LC_collate_t *, const char *, const char **, wchar_t,
			wchar_t, int);


int
__fnmatch_std(_LC_collate_t *phdl, const char *ppat, const char *string,
			const char *pstr, int flags)
{
	int	mblen_p;	/* # bytes in next pattern character	*/
	int	mblen_s;	/* # bytes in next string character	*/
	char	*pse;		/* ptr to next string character		*/
	int	stat;		/* recursive rfnmatch() return status	*/
	wchar_t	ucoll_s;	/* string unique coll value		*/
	wchar_t	wc_p;		/* next patatern character		*/
	wchar_t	wc_s;		/* next string character		*/

/*
 * Loop through pattern, matching string characters with pattern
 * Return success when end-of-pattern and end-of-string reached simultaneously
 * Return no match if pattern/string mismatch
 */
	while (*ppat != '\0') {
		mblen_p = METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
				__lc_collate->cmapp, &wc_p, ppat,
				__lc_collate->cmapp->cm_mb_cur_max);
		if (mblen_p < 0) {
			mblen_p = 1;
			wc_p = *ppat & 0xff;
		}

		mblen_s = METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
				__lc_collate->cmapp, &wc_s, pstr,
				__lc_collate->cmapp->cm_mb_cur_max);
		if (mblen_s < 0) {
			mblen_s = 1;
			wc_s = *pstr & 0xff;
		}

		switch (wc_p) {
/*
 * <backslash> quotes the next character if FNM_NOESCAPE flag is NOT set
 * if FNM_NOESCAPE is set,  treat <backslash> as itself
 * Return no match if pattern ends with quoting <backslash>
 */
		case '\\':
			if ((flags & FNM_NOESCAPE) == 0) {
				ppat++;
				mblen_p = METHOD_NATIVE(__lc_collate->cmapp,
					mbtowc)(__lc_collate->cmapp,
					&wc_p, ppat,
					__lc_collate->cmapp->cm_mb_cur_max);
				if (mblen_p == 0)
					return (FNM_NOMATCH);
				if (mblen_p < 0) {
					mblen_p = 1;
					wc_p = *ppat & 0xff;
				}
			}
/*
 * Ordinary character in pattern matches itself in string
 * Need to compare process codes for multibyte languages
 * Continue if pattern character matches string character
 * Return no match if pattern character does not match string character
 */
		default:
		ordinary:
			if (wc_p == wc_s) {
				ppat += mblen_p;
				pstr += mblen_s;
				break;
			} else
				return (FNM_NOMATCH);
/*
 * <asterisk> matches zero or more string characters
 * Cannot match <slash> if FNM_PATHNAME is set
 * Cannot match leading <period> if FNM_PERIOD is set
 * Consecutive <asterisk> are redundant
 *
 * Return success if remaining pattern matches remaining string
 * Otherwise advance to the next string character and try again
 * Return no match if string exhausted and more pattern remains
 */
		case '*':
			while (*++ppat == '*')
				;
			if (*ppat == '\0') {
				if ((flags & FNM_PATHNAME) != 0 &&
				    strchr(pstr, '/') != NULL)
					return (FNM_NOMATCH);
				if (*pstr == '.' && (flags & FNM_PERIOD) != 0)
					if (pstr == string ||
					    (pstr[-1] == '/' &&
						(flags & FNM_PATHNAME) != 0))
						return (FNM_NOMATCH);
				return (0);
			}
			while (*pstr != '\0') {
				stat = __fnmatch_std(phdl, ppat, string, pstr,
							flags);
				if (stat != FNM_NOMATCH)
					return (stat);
				if (*pstr == '/') {
					if ((flags & FNM_PATHNAME) != 0)
						return (FNM_NOMATCH);
				} else if (*pstr == '.' &&
				    (flags & FNM_PERIOD) != 0)
					if (pstr == string ||
					    (pstr[-1] == '/' &&
						(flags & FNM_PATHNAME) != 0))
						return (FNM_NOMATCH);
				mblen_s = mblen(pstr,
					__lc_collate->cmapp->cm_mb_cur_max);
				if (mblen_s > 0)
					pstr += mblen_s;
				else
					pstr++;
			}
			return (FNM_NOMATCH);
/*
 * <question-mark> matches any single character
 * Cannot match <slash> if FNM_PATHNAME is set
 * Cannot match leading <period> if FNM_PERIOD is set
 *
 * Return no match if string is exhausted
 * Otherwise continue with next pattern and string character
 */
		case '?':
			if (wc_s == '/') {
				if ((flags & FNM_PATHNAME) != 0)
					return (FNM_NOMATCH);
			} else if (wc_s == '.' && (flags & FNM_PERIOD) != 0)
				if (pstr == string || (pstr[-1] == '/' &&
				    (flags & FNM_PATHNAME) != 0))
					return (FNM_NOMATCH);
			if (wc_s != (wchar_t)'\0') {
				if (mblen_s >= 0)
					pstr += mblen_s;
				else
					pstr++;
				ppat++;
				break;
			} else
				return (FNM_NOMATCH);
/*
 * <left-bracket> begins a [bracket expression] which matches single collating
 * element
 * [bracket expression] cannot match <slash> if FNM_PATHNAME is set
 * [bracket expression] cannot match leading <period> if FNM_PERIOD is set
 */
		case '[':
			if (wc_s == '/') {
				if ((flags & FNM_PATHNAME) != 0)
					return (FNM_NOMATCH);
			} else if (wc_s == '.' && (flags & FNM_PERIOD) != 0)
				if (pstr == string || (pstr[-1] == '/' &&
				    (flags & FNM_PATHNAME) != 0))
					return (FNM_NOMATCH);
/*
 * Determine unique collating value of next collating element
 */
			ucoll_s = _mbucoll(phdl, (char *)pstr, (char **)&pse);
			if ((ucoll_s < MIN_UCOLL) || (ucoll_s > MAX_UCOLL))
				return (FNM_NOMATCH);
/*
 * Compare unique collating value to [bracket expression]
 *   > 0  no match
 *   = 0  match found
 *   < 0  error, treat [] as individual characters
 */
			stat = bracket(phdl, ppat + 1, &ppat, wc_s, ucoll_s,
					flags);
			if (stat == 0)
				pstr = pse;
			else if (stat > 0)
				return (FNM_NOMATCH);
			else
				goto ordinary;
			break;
		}
	}
/*
 * <NUL> following end-of-pattern
 * Return success if string is also at <NUL>
 * Return no match if string not at <NUL>
 */
	if (*pstr == '\0')
		return (0);
	else
		return (FNM_NOMATCH);
}


/*
 * bracket()    - Determine if [bracket] matches filename character
 *
 *	pdhl	 - ptr to __lc_collate structure
 *	ppat	 - ptr to position of '[' in pattern
 *	wc_s	 - process code of next filename character
 *	ucoll_s	 - unique collating weight of next filename character
 *	flags	 - fnmatch() flags, see <fnmatch.h>
 */

static int
bracket(_LC_collate_t *phdl, const char *ppat, const char **pend, wchar_t wc_s,
		wchar_t ucoll_s, int flags)
{
	int	neg;		/* nonmatching [] expression	*/
	int	dash;		/* <hyphen> found for a-z range expr	*/
	int	prev_min_ucoll;	/* low end of range expr		*/
	wchar_t	min_ucoll;	/* minimum unique collating value	*/
	wchar_t	max_ucoll;	/* maximum unique collating value	*/
	const char *pb;		/* ptr to [bracket] pattern		*/
	const char *pi;		/* ptr to international [] expression	*/
	char	*piend;		/* ptr to character after intl [] expr	*/
	int	found;		/* match found flag			*/
	wchar_t	wc_b;		/* bracket process code			*/
	int	mblen_b;	/* next bracket character length	*/
	char	type;		/* international [] type =:.		*/
	wchar_t	*pwgt;		/* ptr to collation weight table	*/
	wchar_t	pcoll;		/* primary collation weight		*/
	wint_t	i;		/* process code loop index		*/
	char	class[CLASS_SIZE+1]; /* character class with <NUL>	*/
	char	*pclass;	/* class[] ptr				*/
/*
 * Leading <exclamation-mark> designates nonmatching [bracket expression]
 */
	pb = ppat;
	neg = 0;
	if (*pb == '!') {
		pb++;
		neg++;
	}
/*
 * Loop through each [] collating element comparing unique collating values
 */
	dash = 0;
	found = 0;
	prev_min_ucoll = 0;
	min_ucoll = 0;
	max_ucoll = 0;
	while ((mblen_b = METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
			__lc_collate->cmapp, &wc_b, pb,
			__lc_collate->cmapp->cm_mb_cur_max)) != 0) {
/*
 * Use next byte if invalid multibyte character
 */
		if (mblen_b < 0) {
			mblen_b = 1;
			wc_b = *pb & 0xff;
		}
/*
 * Final <right-bracket> so return status based upon whether match was found
 * Return character after final ] if match is found
 * Ordinary character if first character of [barcket expression]
 */
		if (wc_b == ']')
			if ((neg == 0 && pb > ppat) ||
			    (neg != 0 && pb > ppat + 1)) {
				if ((found ^ neg) == 0)
					return (FNM_NOMATCH);
				*pend = ++pb;
				return (0);
			}
/*
 * Return error if embedded <slash> found and FNM_PATHNAME is set
 */
		else if (wc_b == '/')
			if ((flags && FNM_PATHNAME) != 0)
				return (-1);
/*
 * Decode next [] element
 */
		if (dash == 0)
			prev_min_ucoll = min_ucoll;
		switch (wc_b) {
/*
 * <hyphen> deliniates a range expression unless it is first character of []
 * or it immediately follows another <hyphen> and is therefore an end point
 */
		case '-':
			if (dash == 0 && !((neg == 0 && pb == ppat) ||
			    (neg != 0 && pb == ppat + 1) || (pb[1] == ']'))) {
				dash++;
				pb++;
				continue;
			}
/*
 * ordinary character - compare unique collating weight of collating symbol
 */
		default:
		ordinary:
			if ((min_ucoll = _mbucoll(phdl, (char *)pb,
						(char **)&pb)) == ucoll_s)
				found = 1;
			max_ucoll = min_ucoll;
			break;
/*
 * <left-bracket> initiates one of the following internationalization
 *   character expressions
 *   [: :] character class
 *   [= =] equivalence character class
 *   [. .] collation symbol
 *
 * it is treated as itself if not followed by appropriate special character
 * it is treated as itself if any error is encountered
 */
		case '[':
			pi = pb + 2;
			if ((type = pb[1]) == ':') {
				pclass = class;
				while (1) {
					if (*pi == '\0')
						return (-1);
					if (*pi == ':' && pi[1] == ']')
						break;
					if (pclass >= &class[CLASS_SIZE])
						return (-1);
					*pclass++ = *pi++;
				}
				if (pclass == class)
					return (-1);
				*pclass = '\0';
				if (METHOD_NATIVE(__lc_ctype, iswctype)
				    (__lc_ctype, wc_s, wctype(class)) != 0)
					found = 1;
				min_ucoll = 0;
				pb = pi + 2;
				break;
			}
/*
 * equivalence character class
 *   get process code of character, error if invalid or <NUL>
 *   treat as collation symbol if not entire contents of [= =]
 *   locate address of collation weight table
 *   get unique collation weight, error if none
 *   set found flag if unique collation weight matches that of string
 *   collating element
 *   if no match, compare unique collation weight of all equivalent characters
 */
			else if (type == '=') {
				mblen_b = METHOD_NATIVE(__lc_collate->cmapp,
					mbtowc)(__lc_collate->cmapp, &wc_b, pi,
					__lc_collate->cmapp->cm_mb_cur_max);
				if (mblen_b <= 0)
					return (-1);
				if (pi[mblen_b] != type)
					goto coll_sym;
				if (pi[mblen_b + 1] != ']')
					return (-1);
				pwgt = (wchar_t *)__wccollwgt(wc_b);
				min_ucoll = pwgt[MAX_NORDS];
				if ((min_ucoll < MIN_UCOLL) ||
				    (min_ucoll > MAX_UCOLL))
					return (-1);
				max_ucoll = min_ucoll;
				pcoll = *pwgt;
				for (i = MIN_PC; i <= MAX_PC; i++)
					if (*(pwgt = (wchar_t *)__wccollwgt(i))
					    == pcoll) {
						if (pwgt[MAX_NORDS] == ucoll_s)
							found = 1;
						if (pwgt[MAX_NORDS] < min_ucoll)
							min_ucoll =
								pwgt[MAX_NORDS];
						if (pwgt[MAX_NORDS] > max_ucoll)
							max_ucoll =
								pwgt[MAX_NORDS];
					}
				pb = pi + mblen_b + 2;
				break;
			}
/*
 * collation symbol
 *   locate address of collation weight table, error if none
 *   verify collation symbol is entire contents of [. .] expression, error if
 *   not get unique collation weight, error if none
 *   set found flag if collation weight matches that of string collating element
 */
			else if (type == '.') {
coll_sym:
				min_ucoll = _mbucoll(phdl, (char *)pi,
							(char **)&piend);
				if ((min_ucoll < MIN_UCOLL) ||
				    (min_ucoll > MAX_UCOLL) ||
				    (*piend != type) || (piend[1] != ']'))
					return (-1);
				if (min_ucoll == ucoll_s)
					found = 1;
				max_ucoll = min_ucoll;
				pb = piend + 2;
				break;
			} else
				goto ordinary;
		} /* end of switch */
/*
 * Check for the completion of a range expression and determine
 * whether string collating element falls between end points
 */
		if (dash != 0) {
			dash = 0;
			if (prev_min_ucoll == 0 || prev_min_ucoll > max_ucoll)
				return (-1);
			if (ucoll_s >= prev_min_ucoll && ucoll_s <= max_ucoll)
				found = 1;
			min_ucoll = 0;
		}
	} /* end of while */
/*
 * Return < 0 since <NUL> was found
 */
	return (-1);
}
