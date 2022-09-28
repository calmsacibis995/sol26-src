/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__nl_langinfo_std.c 1.14	96/08/14  SMI"

/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: __nl_langinfo_std
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1989 , 1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */
#include <sys/localedef.h>
#include <string.h>
#include <langinfo.h>
#include <locale.h>
#include <limits.h>
#include <thread.h>


extern int	__xpg4;
extern _LC_time_t	__C_time_object;
extern int _thr_main();
extern int * _tsdalloc(thread_key_t *key, int size);

#define	IS_C_LOCALE(hdl)	((hdl) == &__C_time_object)
#define	MAXANSLENGTH 128

/*
*  FUNCTION: __nl_langinfo_std
*
*  DESCRIPTION:
*  Returns the locale database string which corresponds to the specified
*  nl_item.
*/

char *
__nl_langinfo_std(_LC_locale_t *hdl, nl_item item)
{
	char *ptr1, *ptr2;
	static char string[MAXANSLENGTH];
	int  len;
	static thread_key_t	nl_key = 0;
	char * langinfobuf  = (_thr_main() ? string :
				(char *)_tsdalloc(&nl_key, MAXANSLENGTH));
	static const char	*xpg4_d_t_fmt = "%a %b %e %H:%M:%S %Y";

	if (item >= _NL_NUM_ITEMS || item < 0) {
		*langinfobuf = 0x00;
		return (langinfobuf);
	}

	if (item == CRNCYSTR) {
		ptr1 = (hdl)->nl_info[item];
		ptr2 = langinfobuf;
		if (hdl->nl_lconv->p_cs_precedes == CHAR_MAX || ptr1[0] == '\0')
			return ((char *) "");

	if (hdl->nl_lconv->p_cs_precedes == 1)
	    ptr2[0] = '-';
	else if (hdl->nl_lconv->p_cs_precedes == 0)
	    ptr2[0] = '+';

	strncpy(&ptr2[1], ptr1, MAXANSLENGTH - 1);
	ptr2[MAXANSLENGTH] = '\0';
	return (ptr2);
	} else if (item == D_T_FMT) {
		if (__xpg4 != 0) {		/* XPG4 mode */
			if (IS_C_LOCALE(hdl->lc_time)) {
				return ((char *)xpg4_d_t_fmt);
			}
		}
		if (hdl->nl_info[D_T_FMT] == NULL) {
			return ((char *)"");
		} else {
			return (hdl->nl_info[D_T_FMT]);
		}
	} else if ((hdl)->nl_info[item] == NULL)
			return ((char *)"");
	else
		return ((hdl)->nl_info[item]);
}
