/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)__strcoll_sb.c 1.6	96/07/02  SMI"


/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)66	1.4.2.2  "
	"src/bos/usr/ccs/lib/libc/__strcoll_sb.c, bos, bos410 1/12/93 11:10:33";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: __strcoll_sb
 *
 * ORIGINS: 27
 *
 * IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 * combined with the aggregated modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1989,1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */
#pragma alloca

#include <sys/localedef.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

extern char *do_replacement(_LC_collate_t *, const char *, int, char *);
extern int forward_collate_sb(_LC_collate_t *, const char *, const char *, int);
extern int forw_pos_collate_sb(_LC_collate_t *, const char *, const char *,
			int);
extern int backward_collate_sb(_LC_collate_t *, const char *, const char *,
			int);
extern int back_pos_collate_sb(_LC_collate_t *, const char *, const char *,
			int);


int
__strcoll_sb(_LC_collate_t *hdl, const char *str1, const char *str2)
{
	char *str1_rep = (char *)NULL;
	char *str2_rep = (char *)NULL;
	char *str1_ptr;
	char *str2_ptr;
	int cur_order;	/* current order being collated */
	short sort_mod;	/* the current order's modification params */
	int rc;		/* generic return code */

	/* see if str1 and str2 are the same string */
	if (str1 == str2)
		return (0);

	/* if str1 and str2 are null, they are equal */
	if (*str1 == '\0' && *str2 == '\0')
		return (0);

	for (cur_order = 0; cur_order <= hdl->co_nord; cur_order++) {
		/* get the sort modifier for this order */
		if (hdl->co_nord < _COLL_WEIGHTS_INLINE)
			sort_mod = hdl->co_sort.n[cur_order];
		else
			sort_mod = hdl->co_sort.p[cur_order];

		/* If this order uses replacement strings, set them up. */
		if (hdl->co_nsubs && (sort_mod & _COLL_SUBS_MASK)) {
			/* alloca 2 * strlen(str) iff it hasn't been done. */
			if (!str1_rep)
				str1_rep = (char *)alloca(strlen(str1) * 2 +
								20);
			str1_ptr = do_replacement(hdl, str1, cur_order,
							str1_rep);
			if (!str2_rep)
				str2_rep = (char *)alloca(strlen(str2) * 2 +
								20);
			str2_ptr = do_replacement(hdl, str2, cur_order,
							str2_rep);
		} else {
			str1_ptr = (char *)str1;
			str2_ptr = (char *)str2;
		}

		/*
		 * Check for direction of collation for this order.
		 * If neither forward nor backward are specified, then
		 * this is to be done by character(machine order).
		 */
		if (sort_mod == 0) {
			rc = strcmp(str1_ptr, str2_ptr);
			return (rc);
		} else if (sort_mod & _COLL_BACKWARD_MASK) {
			if (sort_mod & _COLL_POSITION_MASK)
				rc = back_pos_collate_sb(hdl, str1_ptr,
							str2_ptr, cur_order);
			else
				rc = backward_collate_sb(hdl, str1_ptr,
							str2_ptr, cur_order);
		} else {
			if (sort_mod & _COLL_POSITION_MASK)
				rc = forw_pos_collate_sb(hdl, str1_ptr,
							str2_ptr, cur_order);
			else
				rc = forward_collate_sb(hdl, str1_ptr,
							str2_ptr, cur_order);
		}

		/*
		 * If the strings are not equal, we can leave.
		 * Otherwise continue on to next order
		 */
		if (rc != 0)
			return (rc);
	}

	/* Must be equal, return 0. */
	return (0);
}
