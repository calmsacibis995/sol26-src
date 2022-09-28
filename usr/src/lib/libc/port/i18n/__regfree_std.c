/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ident	"@(#)__regfree_std.c	1.8	96/07/02 SMI"

/*
static char sccsid[] = "@(#)44	1.2.1.1 "
"src/bos/usr/ccs/lib/libc/__regfree_std.c, bos, bos410 5/25/92 14:04:23";
*/
/*
 * COMPONENT_NAME: (LIBCPAT) Standard C Library Pattern Functions
 *
 * FUNCTIONS: __regfree_std
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

#include <sys/localedef.h>
#include <regex.h>
#include <unistd.h>

/* ******************************************************************** */
/* __regfree_std() - Free Compiled RE Pattern Dynamic Memory		*/
/* ******************************************************************** */

void
__regfree_std(_LC_collate_t *hdl, regex_t *preg)
{
	if (preg->re_comp != NULL) {
		free(preg->re_comp);
		preg->re_comp = NULL;
	}
	if (preg->re_sc != NULL) {
		free(preg->re_sc);
		preg->re_sc = NULL;
	}
}
