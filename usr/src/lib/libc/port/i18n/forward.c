/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)forward.c 1.7	96/07/11  SMI"


/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)22	1.2.1.1  "
	"src/bos/usr/ccs/lib/libc/forward.c, bos, bos410 5/25/92 14:08:13";
#endif
*/
/*
 *   COMPONENT_NAME: LIBCSTR
 *
 *   FUNCTIONS: forward_collate_std,forward_collate_sb
 *
 *   ORIGINS: 27
 *
 *
 *   (C) COPYRIGHT International Business Machines Corp. 1991, 1992
 *   All Rights Reserved
 *   Licensed Materials - Property of IBM
 *   US Government Users Restricted Rights - Use, duplication or
 *   disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <sys/localedef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <widec.h>


int
forward_collate_std(_LC_collate_t *hdl, const char *str1, const char *str2,
			int order)
{
	wchar_t wc;
	int rc;
	int str1_colval;
	int str2_colval;

	/* Go thru all of the characters until a null is hit. */
	while ((*str1 != '\0') && (*str2 != '\0')) {
		/*
		 * Convert to a wc, if it is an invalid wc, assume a length of
		 * 1 and continue.  If the collating value is 0 (non-collating)
		 * get the collating value of the next character.
		 */
		do {
			if ((rc = (METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
			    __lc_collate->cmapp, &wc, str1,
			    __lc_collate->cmapp->cm_mb_cur_max))) == -1) {
				errno = EINVAL;
				wc = (wchar_t)*str1++ & 0x00ff;
			} else
				str1 += rc;
			str1 += _getcolval(hdl, &str1_colval, wc, str1, order);
		} while ((str1_colval == 0) && (*str1 != '\0'));

		do {
			if ((rc = (METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
			    __lc_collate->cmapp, &wc, str2,
			    __lc_collate->cmapp->cm_mb_cur_max))) == -1) {
				errno = EINVAL;
				wc = (wchar_t)*str2++ & 0x00ff;
			} else
				str2 += rc;
			str2 += _getcolval(hdl, &str2_colval, wc, str2, order);
		} while ((str2_colval == 0) && (*str2 != '\0'));

		/*
		 * If the collation values are not equal, then we have gone
		 * far enough and may return.
		 */
		if (str1_colval < str2_colval)
			return (-1);
		else if (str1_colval > str2_colval)
			return (1);
	}

	/*
	 * To get here, str1 and/or str2 are NULL and they have been equal so
	 * far.  If one of them is non-null, check if the remaining
	 * characters are non-collating.  If they are, then the strings are
	 * equal for this order.
	 */
	if (*str1 != '\0') {
		/*
		 * If str1 is non-null and there are collating characters
		 * left, then str1 is greater than str2.
		 */
		do {
			if ((rc = (METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
			    __lc_collate->cmapp, &wc, str1,
			    __lc_collate->cmapp->cm_mb_cur_max))) == -1) {
				errno = EINVAL;
				wc = (wchar_t)*str1++ & 0x00ff;
			} else
				str1 += rc;
			str1 += _getcolval(hdl, &str1_colval, wc, str1, order);
		} while ((str1_colval == 0) && (*str1 != '\0'));
		if (str1_colval != 0)
			return (1);
	} else if (*str2 != '\0') {
		/*
		 * If str2 is non-null and there are collating characters
		 * left, then str1 is less than str2.
		 */
		do {
			if ((rc = (METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
			    __lc_collate->cmapp, &wc, str2,
			    __lc_collate->cmapp->cm_mb_cur_max))) == -1) {
				errno = EINVAL;
				wc = (wchar_t)*str2++ & 0x00ff;
			} else
				str2 += rc;
			str2 += _getcolval(hdl, &str2_colval, wc, str2, order);
		} while ((str2_colval == 0) && (*str2 != '\0'));
		if (str2_colval != 0)
			return (-1);
	}

	/* If we get to here, they are equal. */
	return (0);
}


int
forward_collate_sb(_LC_collate_t *hdl, const char *str1, const char *str2,
			int order)
{
	wchar_t wc;
	int rc;
	int str1_colval;
	int str2_colval;

	/* Go thru all of the characters until a null is hit. */
	while ((*str1 != '\0') && (*str2 != '\0')) {
		/*
		 * Convert to a wc, if it is an invalid wc, assume a length of
		 * 1 and continue. If the collating value is 0 (non-collating)
		 * get the collating value of the next character.
		 */
		do {
			wc = (wchar_t)*str1++ & 0x00ff;
			str1 += _getcolval(hdl, &str1_colval, wc, str1, order);
		} while ((str1_colval == 0) && (*str1 != '\0'));

		do {
			wc = (wchar_t)*str2++ & 0x00ff;
			str2 += _getcolval(hdl, &str2_colval, wc, str2, order);
		} while ((str2_colval == 0) && (*str2 != '\0'));

		/*
		 * If the collation values are not equal, then we have gone far
		 * enough and may return.
		 */
		if (str1_colval < str2_colval)
			return (-1);
		else if (str1_colval > str2_colval)
			return (1);
	}

	/*
	 * To get here, str1 and/or str2 are NULL and they have been equal so
	 * far.  If one of them is non-null, check if the remaining
	 * characters are non-collating.  If they are, then the strings are
	 * equal for this order.
	 */
	if (*str1 != '\0') {
		/*
		 * If str1 is non-null and there are collating characters
		 * left, then str1 is greater than str2.
		 */
		do {
			wc = (wchar_t)*str1++ & 0x00ff;
			str1 += _getcolval(hdl, &str1_colval, wc, str1, order);
		} while ((str1_colval == 0) && (*str1 != '\0'));
		if (str1_colval != 0)
			return (1);
	} else if (*str2 != '\0') {
		/*
		 * If str2 is non-null and there are collating characters
		 * left, then str1 is less than str2.
		 */
		do {
			wc = (wchar_t) *str2++ & 0x00ff;
			str2 += _getcolval(hdl, &str2_colval, wc, str2, order);
		} while ((str2_colval == 0) && (*str2 != '\0'));
		if (str2_colval != 0)
			return (-1);
	}

	/* If we get to here, they are equal. */
	return (0);
}
