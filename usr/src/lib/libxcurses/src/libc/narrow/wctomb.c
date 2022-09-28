/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wctomb.c 1.1	96/01/17 SMI"

/*
 * Copyright 1992, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/narrow/rcs/wctomb.c 1.3 1995/01/20 00:34:51 fredw Exp $";
#endif
#endif

#include <mks.h>

#ifdef M_I18N_MB
#error "wctomb.c should only be used in singlebyte environments."
#endif

int
m_sb_wctomb(char *s, wchar_t wc)
{
	if (s == NULL) {
		return (0);
	} else {
		*(char *)s = (wchar_t) wc;
		return (1);
	}
}
