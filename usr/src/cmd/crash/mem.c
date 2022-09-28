#ident	"@(#)mem.c	1.2	95/11/13 SMI"

/*
 *		Copyright (C) 1995  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

#include <stddef.h>
#include <dlfcn.h>
#include <sys/types.h>
#include "mem.h"

/*
 * These routines are versions of malloc, realloc, and calloc that call an
 * installed handler if they get an error, allowing the error-handling to
 * be done just once instead of after every memory allocation.
 *
 * Routine set_alloc_err_func() returns the current error handler and
 * installs a new one.
 */

static alloc_err_func_t alloc_err_func = NULL;

alloc_err_func_t
set_alloc_err_func(alloc_err_func_t new)
{
	alloc_err_func_t old = alloc_err_func;
	alloc_err_func = new;
	return (old);
}

void *
malloc(size_t size)
{
	typedef void *(*malloc_func_t)(size_t);
	static malloc_func_t real_malloc = NULL;
	char *p;

	/* If we don't already have a ptr to the real malloc, get it. */
	if (!real_malloc) {
		real_malloc = (malloc_func_t) dlsym(RTLD_NEXT, "malloc");
		if (!real_malloc)
			abort();
	}

	/*
	 * Call the real malloc; if it fails, call the error routine if there
	 * is one; if it returns true, then try again.
	 */
	do {
		p = (*real_malloc)(size);
		if (p)
			return (p);
	} while (alloc_err_func && (*alloc_err_func)(size));

	/* Either there was no err func, or it returned false.  Fail. */
	return (NULL);
}

void *
realloc(void *old, size_t size)
{
	typedef void *(*realloc_func_t)(void *, size_t);
	static realloc_func_t real_realloc = NULL;
	char *p;

	/* If we don't already have a ptr to the real realloc, get it. */
	if (!real_realloc) {
		real_realloc = (realloc_func_t) dlsym(RTLD_NEXT, "realloc");
		if (!real_realloc)
			abort();
	}

	/*
	 * Call the real realloc; if it fails, call the error routine if there
	 * is one; if it returns true, then try again.
	 */
	do {
		p = (*real_realloc)(old, size);
		if (p)
			return (p);
	} while (alloc_err_func && (*alloc_err_func)(size));

	/* Either there was no err func, or it returned false.  Fail. */
	return (NULL);
}
