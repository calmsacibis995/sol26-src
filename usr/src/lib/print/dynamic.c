/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident  "@(#)dynamic.c 1.4     96/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <dlfcn.h>

#include <print/misc.h>


#define	MAX_LIB	 20

#define	LPATH  "/usr/lib:/usr/lib/print"


/*
 *	Dynamic LIB stuff ...
 */

#ifndef RTLD_GLOBAL	/* for systems without this */
#define	RTLD_GLOBAL	0
#endif

/*
 * opens the right network name service dynamic library
 */
static void *
dynamic_lib_open(const char *path, const char *name)
{
	char	*tmp,
		**path_list;
	void *handle;

	if (name == NULL)
		return (NULL);

	if (path == NULL)
		tmp = strdup(LPATH);
	else
		tmp = strdup(path);

	path_list = (char **)strsplit(tmp, ":");

	while (*path_list != NULL) {
		char buf[BUFSIZ];

		sprintf(buf, "%s/lib%s.so", *path_list++, name);
		if ((handle = dlopen(buf, RTLD_LAZY|RTLD_GLOBAL)) != NULL)
			break;
	}

	free(tmp);

	return (handle);
}


/*
 * Returns a pointer for the named function in the library for that
 * name service.
 */
void *
dynamic_function(const char *lib, const char *func)
{
	static void *hcache[MAX_LIB];
	int i;
	int opened_lib = 0;
	void *fpt;

	if (hcache[0] == NULL)
		hcache[0] = dlopen(NULL, RTLD_LAZY|RTLD_GLOBAL);

	if (hcache[1] == NULL)
		hcache[1] = dynamic_lib_open(NULL, "print");

	/* look for func in dynamic libs */
	for (i = 0; i < MAX_LIB; i++) {
		if (hcache[i] == NULL) {
			hcache[i] = dynamic_lib_open(NULL, lib);
			opened_lib++;
		}

		if ((fpt = dlsym(hcache[i], func)) != NULL)
			return (fpt);

		if (opened_lib != 0)
			break;
	}

	return (NULL);
}
