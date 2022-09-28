/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)colval.c 1.8	96/07/11  SMI"


/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)68	1.4.2.2  "
	"src/bos/usr/ccs/lib/libc/colval.c, bos, bos410 1/12/93 11:12:54";

#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: _getcolval, _mbucoll, _mbce_lower
 *
 * ORIGINS: 27
 *
 * IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 * combined with the aggregated modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1991,1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */
#pragma alloca

#include <limits.h>
#include <sys/localedef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#include <errno.h>


/* _getcolval - determine n'th collation weight of collating symbol. */

int
_getcolval(_LC_collate_t *hdl, int *colval, wchar_t wc, char *str, int order)
{
	int i;
	int count;

	/* As a safety net, just in case mbtowc() didn't catch invalid WC. */
	if (wc > hdl->co_wc_max || wc < hdl->co_wc_min) {
		errno = EINVAL;
		wc &= 0x7f;
	}

	/* Get the collation value for the wide character, wc. */
	if (hdl->co_nord < _COLL_WEIGHTS_INLINE)
		*colval = hdl->co_coltbl[wc].ct_wgt.n[order];
	else
		*colval = hdl->co_coltbl[wc].ct_wgt.p[order];

	/* Check if there are any collation elements for this character. */
	if (hdl->co_coltbl[wc].ct_collel != (_LC_collel_t *)NULL) {
		i = 0;
		while (hdl->co_coltbl[wc].ct_collel[i].ce_sym !=
		    (_const char *)NULL) {
			/*
			 * Get the length of the collation element that
			 * starts with this character.
			 */
			count = strlen(hdl->co_coltbl[wc].ct_collel[i].ce_sym);

			/*
			 * If there is a match, get the collation elements
			 * value and return the number of characters that
			 * make up the collation value.
			 */
			if (! (strncmp(str,
			    hdl->co_coltbl[wc].ct_collel[i].ce_sym, count))) {
				if (hdl->co_nord < _COLL_WEIGHTS_INLINE)
					*colval =
			hdl->co_coltbl[wc].ct_collel[i].ce_wgt.n[order];
				else
					*colval =
			hdl->co_coltbl[wc].ct_collel[i].ce_wgt.p[order];
				return (count);
			}

			/*
			 * This collation element did not match, go to
			 * the next.
			 */
			i++;
		}
	}

	/*
	 * No collation elements, or none that matched, return 0 additional
	 * characters.
	 */
	return (0);
}


/* _mbucoll - determine unique collating weight of collating symbol. */

int
_mbucoll(_LC_collate_t *hdl, char *str, char **next_char)
{
	int ucoll;	/* collating symbol unique weight	*/
	int wclen;	/* # bytes in first character		*/
	wchar_t wc;	/* first character process code		*/

	wclen = METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(__lc_collate->cmapp,
			&wc, str, __lc_collate->cmapp->cm_mb_cur_max);
	if (wclen < 0)
		wc = *str++ & 0xff;
	else
		str += wclen;

	*next_char = str + _getcolval(hdl, &ucoll, wc, str, (int)hdl->co_nord);

	return (ucoll);
}


#ifdef	notdef
/* This function commented out because we are not using this as of 7/1/1996. */

/*
 * _mbce_lower    - convert multibyte collating element to lowercase
 *                - return status indicating if old/new are different
 *
 *                - for each character in collating element
 *                - convert from file code to proces code
 *		  - convert process code to lower case
 *		  - convert lower case process code back to file code
 *		  - set status if upper/lower process code different
 *                - add terminating NULL
 */

int
_mbce_lower(char *pstr, size_t n, char *plstr)
{
	char *pend;	/* ptr to end of conversion string	*/
	int stat;	/* return status			*/
	wchar_t wc;	/* original string process code		*/
	wchar_t wcl;	/* lowercase string process code	*/

	stat = 0;
	for (pend = pstr + n; pstr < pend; ) {
		pstr += METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
				__lc_collate->cmapp, &wc, pstr,
				__lc_collate->cmapp->cm_mb_cur_max);
		wcl = METHOD_NATIVE(__lc_ctype, towlower)(__lc_ctype,
				(wint_t)wc);
		plstr += METHOD_NATIVE(__lc_collate->cmapp, wctomb)(
				__lc_collate->cmapp, plstr, wcl);
		if (wcl != wc)
			stat++;
	}
	*plstr = '\0';

	return (stat);
}
#endif
