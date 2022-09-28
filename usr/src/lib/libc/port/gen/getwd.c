/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getwd.c	1.4	1.4 SMI"

/*LINTLIBRARY*/
#include "synonyms.h"

#include <sys/param.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

char *
getwd(char *pathname)
{
	char *c;
	long val;


	if ((val = pathconf(".", _PC_PATH_MAX)) == -1)
		val = MAXPATHLEN + 1;

	if ((c = getcwd(pathname, val)) == NULL) {
		if (errno == EACCES)
			(void) strcpy(pathname,
				"getwd: a parent directory cannot be read");
		else if (errno == ERANGE)
			(void) strcpy(pathname, "getwd: buffer too small");
		else
			(void) strcpy(pathname, "getwd: failure occurred");
		return (0);
	}

	return (c);
}
