/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident  "@(#)ns_cmn_order.c 1.5     96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <stdarg.h>

#include <print/list.h>
#include <print/ns.h>

#define	MAX_CALLS	50	/* cntr to detect loops */

/*
 *	Dynamic LIB stuff ...
 */

#ifndef RTLD_GLOBAL	/* for systems without this */
#define	RTLD_GLOBAL	0
#endif

/*
 * This is here for systems without XFN support.  This currently includes:
 *	anything that isn't running Soalris 2.3 or later.
 * It returns a list of name services to use for lookups.  XFN is prefered.
 * resulting lists could be:
 *		"user", ["printcap"], "xfn"
 *		"user", ["printcap"], "files", "nis"
 *		"user", ["printcap"], "files", "nisplus"
 */
char **
ns_order()
{
	static char **list = NULL;

	if (list == NULL) {
		list = (char **)list_append((void **)list, (void *)NS_SVC_USER);
#ifdef HAVE_XFN
		list = (char **)list_append((void **)list, (void *)NS_SVC_XFN);
#else
		list = (char **)list_append((void **)list, (void *)NS_SVC_ETC);
		list = (char **)list_append((void **)list, (void *)NS_SVC_NIS);
		list = (char **)list_append((void **)list,
						(void *)NS_SVC_NISPLUS);
#endif
	}
	return (list);
}
